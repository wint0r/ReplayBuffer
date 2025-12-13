#include "ReplayBuffer.hpp"
#include <ranges>

ReplayBuffer::ReplayBuffer() {
}

ReplayBuffer::~ReplayBuffer() {
  this->stop();
}

std::shared_ptr<BaseEncoder> ReplayBuffer::getStreamEncoder(int idx) {
  return m_encoders[idx];
}

void ReplayBuffer::start() {
  for (const auto &encoder: m_encoders | std::views::values) {
    encoder->start();
  }
}

void ReplayBuffer::stop() {
  for (const auto &encoder: m_encoders | std::views::values) {
    encoder->stop();
  }
}

void ReplayBuffer::update() {
  for (const auto &encoder: m_encoders | std::views::values) {
    encoder->update();
  }
}

void ReplayBuffer::clear() {
  for (const auto &encoder: m_encoders | std::views::values) {
    encoder->clearPacketBuffer();
  }
}

void ReplayBuffer::saveToFile(const std::filesystem::path &filename) {
  const std::string path = filename.string();

  AVFormatContext *formatCtx;
  int ret = avformat_alloc_output_context2(&formatCtx, nullptr, nullptr, path.c_str());
  if (formatCtx == nullptr) {
    char errStr[64];
    av_make_error_string(errStr, 64, ret);
    throw fmt::format("could not allocate output context, error: {}", errStr);
  }

  std::map<int, int> streamMapping;
  std::map<int, AVStream *> streams;
  for (auto &[idx, encoder] : m_encoders) {
    AVStream *outStream = avformat_new_stream(formatCtx, nullptr);
    if (outStream == nullptr) {
      avformat_free_context(formatCtx);
      throw fmt::format("couldn't allocate output stream");
    }

    avcodec_parameters_from_context(outStream->codecpar, encoder->getCodecContext());
    outStream->time_base = encoder->getCodecContext()->time_base;

    streamMapping[idx] = outStream->index;
    streams[idx] = outStream;
  }

  if ((ret = avio_open(&formatCtx->pb, path.c_str(), AVIO_FLAG_WRITE)) < 0) {
    avformat_free_context(formatCtx);

    char errStr[64];
    av_make_error_string(errStr, 64, ret);
    throw fmt::format("could not open file for writing, error: {}", errStr);
  }

  if ((ret = avformat_write_header(formatCtx, nullptr)) < 0) {
    avio_closep(&formatCtx->pb);
    avformat_free_context(formatCtx);

    char errStr[64];
    av_make_error_string(errStr, 64, ret);
    throw fmt::format("could not write header, error: {}", errStr);
  }

  // hack
  int64_t lastPTS = m_encoders[0]->getPacketBuffer().back()->pts;
  int64_t maxDurationPTS = av_rescale_q(m_encoders[0]->getMaxDuration(), { 1, 1 }, m_encoders[0]->getCodecContext()->time_base);
  int64_t usOffsetBase = av_rescale_q(lastPTS - maxDurationPTS, m_encoders[0]->getCodecContext()->time_base, { 1, 1000000 });

  for (auto &[idx, encoder] : m_encoders) {
    encoder->lockBuffer();
    auto buffer = encoder->getPacketBuffer();
    int64_t timestampOffset = av_rescale_q(usOffsetBase, { 1, 1000000 }, encoder->getCodecContext()->time_base);
    //timestampOffset = std::max(timestampOffset, encoder->getMinimumPTS());

    bool seenKeyframe = false;
    for (const auto &orig_pkt : buffer) {
      if (orig_pkt->flags & AV_PKT_FLAG_KEY) {
        seenKeyframe = true;
      }
      if ((!seenKeyframe && encoder->isVideo()) || orig_pkt->pts < timestampOffset) {
        continue;
      }

      AVPacket *pkt = av_packet_clone(orig_pkt);
      int oldStreamIdx = idx;
      int newStreamIdx = streamMapping[oldStreamIdx];
      int64_t offset = timestampOffset;

      if (pkt->pts != AV_NOPTS_VALUE) {
        pkt->pts = pkt->pts - offset;
      }
      if (pkt->dts != AV_NOPTS_VALUE) {
        pkt->dts = pkt->dts - offset;
      }

      if (pkt->dts != AV_NOPTS_VALUE && pkt->pts != AV_NOPTS_VALUE) {
        if (pkt->dts > pkt->pts) {
          pkt->dts = pkt->pts;
        }
      }

      if (pkt->dts == AV_NOPTS_VALUE && pkt->pts != AV_NOPTS_VALUE) {
        pkt->dts = pkt->pts;
      }

      AVStream *outStream = streams[oldStreamIdx];
      av_packet_rescale_ts(pkt, encoder->getCodecContext()->time_base, outStream->time_base);

      if (pkt->dts != AV_NOPTS_VALUE && pkt->pts != AV_NOPTS_VALUE) {
        if (pkt->dts > pkt->pts) {
          pkt->dts = pkt->pts;
        }
      }

      pkt->stream_index = newStreamIdx;

      if ((ret = av_interleaved_write_frame(formatCtx, pkt)) < 0) {
        av_packet_free(&pkt);
        break;
      }

      av_packet_free(&pkt);
    }
    encoder->unlockBuffer();
  }

  av_write_trailer(formatCtx);
  avio_closep(&formatCtx->pb);
  avformat_free_context(formatCtx);
  if (ret < 0) {
    char errStr[64];
    av_make_error_string(errStr, 64, ret);
    throw fmt::format("could not write to file, error: {}", errStr);
  }
}

void ReplayBuffer::setDuration(int64_t newDuration) {
  for (const auto &encoder: m_encoders | std::views::values) {
    encoder->setMaxDuration(newDuration);
  }
}

const std::map<int, std::shared_ptr<BaseEncoder>> &ReplayBuffer::getEncoders() {
  return m_encoders;
}
