#include "ReplayBuffer.hpp"

int64_t ReplayBuffer::pts_to_ms(int64_t pts, AVRational time_base) {
  return av_rescale_q(pts, time_base, AVRational{1, 1000});
}

ReplayBuffer::ReplayBuffer(size_t max_duration) {
  this->max_duration = max_duration;
  this->video_stream_idx = -1;
}

ReplayBuffer::~ReplayBuffer() {
  clear();
}

void ReplayBuffer::add_packet(AVPacket *pkt) {
  AVPacket* clone = av_packet_clone(pkt);

  bool is_keyframe = (clone->flags & AV_PKT_FLAG_KEY) != 0;
  bool is_video = (pkt->stream_index == this->video_stream_idx);

  int64_t timestamp_ms = pts_to_ms(clone->pts, this->time_bases[pkt->stream_index]);

  std::lock_guard<std::mutex> lock(this->mutex);
  this->buffer.push_back({ clone, pkt->stream_index, is_keyframe && is_video, timestamp_ms });
  if (is_video) {
    trim_buffer();
  }
}

void ReplayBuffer::add_stream(int stream_idx, AVCodecContext *ctx, bool video) {
  this->codec_contexts[stream_idx] = ctx;
  this->time_bases[stream_idx] = ctx->time_base;

  if (video) {
    this->video_stream_idx = stream_idx;
  }
}

void ReplayBuffer::trim_buffer() {
  if (this->buffer.empty()) {
    return;
  }

  int64_t newest_timestamp = this->buffer.back().timestamp_ms;
  int64_t cutoff_time = newest_timestamp - this->max_duration;

  size_t first_keyframe_idx = 0;
  bool found_keyframe = false;

  for (size_t i = 0; i < this->buffer.size(); i++) {
    if (this->buffer[i].timestamp_ms >= cutoff_time && this->buffer[i].is_keyframe) {
      first_keyframe_idx = i;
      found_keyframe = true;
      break;
    }
  }

  if (!found_keyframe) {
    for (size_t i = 0; i < this->buffer.size(); i++) {
      if (this->buffer[i].is_keyframe) {
        first_keyframe_idx = i;
        found_keyframe = true;
      }
      if (this->buffer[i].timestamp_ms >= cutoff_time) {
        break;
      }
    }
  }

  if (!found_keyframe || first_keyframe_idx == 0) {
    return;
  }

  for (size_t i = 0; i < first_keyframe_idx; i++) {
    av_packet_free(&this->buffer.front().packet);
    this->buffer.pop_front();
  }
}

geode::Result<> ReplayBuffer::save_to_file(const std::filesystem::path &filename_) {
  const std::string filename = filename_.string();
  if (this->buffer.empty()) {
    return geode::Err("empty buffer");
  }

  size_t start_index = 0;
  bool found_keyframe = false;

  av_log_set_level(AV_LOG_DEBUG);
  for (size_t i = 0; i < this->buffer.size(); i++) {
    if (this->buffer[i].is_keyframe) {
      start_index = i;
      found_keyframe = true;
      break;
    }
  }

  if (!found_keyframe) {
    return geode::Err("no keyframes in buffer");
  }

  AVFormatContext *out_ctx;
  int ret = avformat_alloc_output_context2(&out_ctx, nullptr, nullptr, filename.c_str());
  if (out_ctx == nullptr) {
    char err_str[64];
    av_make_error_string(err_str, 64, ret);
    return geode::Err("could not allocate output context, error: {}", err_str);
  }

  std::map<int, int> stream_indices;
  std::map<int, AVStream *> streams;
  for (auto &[stream_idx, codec_ctx] : this->codec_contexts) {
    AVStream *out_stream = avformat_new_stream(out_ctx, nullptr);
    if (out_stream == nullptr) {
      avformat_free_context(out_ctx);
      return geode::Err("couldn't allocate output stream");
    }

    avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
    out_stream->time_base = codec_ctx->time_base;

    stream_indices[stream_idx] = out_stream->index;
    streams[stream_idx] = out_stream;
  }

  if ((ret = avio_open(&out_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE)) < 0) {
    avformat_free_context(out_ctx);
    char err_str[64];
    av_make_error_string(err_str, 64, ret);
    return geode::Err("could not open file for writing, error: {}", err_str);
  }

  if ((ret = avformat_write_header(out_ctx, nullptr)) < 0) {
    avio_closep(&out_ctx->pb);
    avformat_free_context(out_ctx);
    char err_str[64];
    av_make_error_string(err_str, 64, ret);
    return geode::Err("could not open codec, error: {}", err_str);
  }

  int64_t start_timestamp_ms = this->buffer[start_index].timestamp_ms;
  while (start_index > 0 && this->buffer[start_index - 1].timestamp_ms >= start_timestamp_ms - 100) {
    start_index--;
  }

  std::map<int, int64_t> timestamp_offsets;
  for (size_t i = start_index; i < this->buffer.size(); i++) {
    int stream_idx = this->buffer[i].stream_idx;
    if (!timestamp_offsets.contains(stream_idx)) {
      AVPacket* pkt = this->buffer[i].packet;

      int64_t pts = (pkt->pts != AV_NOPTS_VALUE) ? pkt->pts : 0;
      int64_t dts = (pkt->dts != AV_NOPTS_VALUE) ? pkt->dts : pts;

      timestamp_offsets[stream_idx] = std::min(pts, dts);
    }
  }

  for (size_t i = start_index; i < this->buffer.size(); i++) {
    AVPacket *orig_pkt = this->buffer[i].packet;
    AVPacket *pkt = av_packet_clone(orig_pkt);

    int old_stream_idx = pkt->stream_index;
    int new_stream_idx = stream_indices[old_stream_idx];
    int64_t offset = timestamp_offsets[old_stream_idx];

    if (pkt->pts != AV_NOPTS_VALUE) {
      pkt->pts = orig_pkt->pts - offset;
    }
    if (pkt->dts != AV_NOPTS_VALUE) {
      pkt->dts = orig_pkt->dts - offset;
    }

    if (pkt->dts != AV_NOPTS_VALUE && pkt->pts != AV_NOPTS_VALUE) {
      if (pkt->dts > pkt->pts) {
        pkt->dts = pkt->pts;
      }
    }

    if (pkt->dts == AV_NOPTS_VALUE && pkt->pts != AV_NOPTS_VALUE) {
      pkt->dts = pkt->pts;
    }

    AVStream *out_stream = streams[old_stream_idx];
    av_packet_rescale_ts(pkt, time_bases[old_stream_idx], out_stream->time_base);

    if (pkt->dts != AV_NOPTS_VALUE && pkt->pts != AV_NOPTS_VALUE) {
      if (pkt->dts > pkt->pts) {
        pkt->dts = pkt->pts;
      }
    }

    pkt->stream_index = new_stream_idx;

    if ((ret = av_interleaved_write_frame(out_ctx, pkt)) < 0) {
      av_packet_free(&pkt);
      break;
    }

    av_packet_free(&pkt);
  }

  av_write_trailer(out_ctx);
  avio_closep(&out_ctx->pb);
  avformat_free_context(out_ctx);

  return geode::Ok();
}

void ReplayBuffer::clear() {
  for (auto &pkt : this->buffer) {
    av_packet_free(&pkt.packet);
  }
  this->buffer.clear();
}

int64_t ReplayBuffer::get_buffer_duration() {
  if (this->buffer.size() < 2) {
    return 0;
  }

  return this->buffer.back().timestamp_ms - this->buffer.front().timestamp_ms;
}

void ReplayBuffer::set_duration(int64_t new_duration) {
  this->max_duration = new_duration;
  this->trim_buffer();
}