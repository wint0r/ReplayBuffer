#include "ReplayBuffer.hpp"
#include <ranges>

int64_t ReplayBuffer::pts_to_ms(int64_t pts, AVRational time_base) {
  return av_rescale_q(pts, time_base, AVRational{1, 1000});
}

ReplayBuffer::ReplayBuffer(size_t max_duration) {
  this->max_duration = max_duration;
  this->video_stream_idx = -1;
  geode::log::info("replay buffer started with {} seconds limit", max_duration / 1000);
}

ReplayBuffer::~ReplayBuffer() {
  clear();
}

void ReplayBuffer::add_packet(AVPacket *pkt) {
  AVPacket* clone = av_packet_clone(pkt);

  bool is_keyframe = (pkt->flags & AV_PKT_FLAG_KEY);
  bool is_video = (pkt->stream_index == this->video_stream_idx);

  int64_t timestamp_ms = pts_to_ms(pkt->pts, this->time_bases[pkt->stream_index]);

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

  size_t video_start_idx = 0;
  bool found_keyframe = false;

  for (size_t i = 0; i < this->buffer.size(); i++) {
    if (this->buffer[i].is_keyframe) {
      video_start_idx = i;
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
    return geode::Err("could not write header, error: {}", err_str);
  }

  int64_t start_timestamp_ms = this->buffer[video_start_idx].timestamp_ms;
  std::map<int, int64_t> start_indices;
  std::map<int, std::vector<BufferedPacket>> packets_per_stream;
  for (int64_t i = 0; i < this->buffer.size(); i++) {
    auto &packet = this->buffer[i];
    packets_per_stream[packet.stream_idx].push_back(packet);
    bool is_within_limits_time = packet.timestamp_ms >= start_timestamp_ms;
    bool is_within_limits_audio = (is_within_limits_time && packet.stream_idx != video_stream_idx);
    bool is_within_limits_video = (is_within_limits_time && packet.is_keyframe);
    bool is_within_limits = (is_within_limits_video || is_within_limits_audio);
    if ((is_within_limits && start_indices.count(packet.stream_idx) == 0)
      || (!is_within_limits_time && packet.is_keyframe)) {
      start_indices[packet.stream_idx] = packets_per_stream[packet.stream_idx].size() - 1;
    }
  }

  for (auto &[stream_idx, stream_buffer] : packets_per_stream) {
    int64_t timestamp_offset;

    auto filtered_buffer_pts = stream_buffer | std::views::filter([] (BufferedPacket &packet) { return packet.packet->pts != AV_NOPTS_VALUE; });
    auto filtered_buffer_dts = stream_buffer | std::views::filter([] (BufferedPacket &packet) { return packet.packet->dts != AV_NOPTS_VALUE; });
    auto pts_it = std::ranges::min_element(
      filtered_buffer_pts,
      [] (BufferedPacket &a, BufferedPacket &b) {
        return a.packet->pts < b.packet->pts;
      });
    auto dts_it = std::ranges::min_element(
      filtered_buffer_dts,
      [] (BufferedPacket &a, BufferedPacket &b) {
        return a.packet->dts < b.packet->dts;
      });
    if (dts_it->packet->dts == AV_NOPTS_VALUE) {
      timestamp_offset = pts_it->packet->pts;
    } else {
      timestamp_offset = std::min(pts_it->packet->pts, dts_it->packet->dts);
    }

    for (size_t i = start_indices[stream_idx]; i < stream_buffer.size(); i++) {
      AVPacket *orig_pkt = stream_buffer[i].packet;
      AVPacket *pkt = av_packet_clone(orig_pkt);

      int old_stream_idx = pkt->stream_index;
      int new_stream_idx = stream_indices[old_stream_idx];
      int64_t offset = timestamp_offset;

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
      av_packet_rescale_ts(pkt, this->time_bases[old_stream_idx], out_stream->time_base);

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
  }

  av_write_trailer(out_ctx);
  avio_closep(&out_ctx->pb);
  avformat_free_context(out_ctx);

  if (ret >= 0) {
    return geode::Ok();
  } else {
    char err_str[64];
    av_make_error_string(err_str, 64, ret);
    return geode::Err("could not write to file, error: {}", err_str);
  }
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