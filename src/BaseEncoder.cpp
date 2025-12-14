#include "BaseEncoder.hpp"

#include <ranges>

void BaseEncoder::trimBuffer() {
  int64_t maxDurationPts = av_rescale_q(m_maxDuration + 1, { 1, 1 }, m_codecCtx->time_base);
  int64_t cutoff = m_packetBuffer.back()->pts - maxDurationPts;

  while (m_packetBuffer.front()->pts < cutoff) {
    av_packet_free(&m_packetBuffer.front());
    m_packetBuffer.pop_front();
  }
}

void BaseEncoder::pushPacket(AVPacket *pkt) {
  //std::lock_guard lock(m_packetBufferMutex); // praying commenting this doesn't crash the game randomly
  m_packetBuffer.push_back(av_packet_clone(pkt));
  av_packet_unref(pkt);
  this->trimBuffer();
}

BaseEncoder::BaseEncoder() : m_codec(nullptr), m_codecCtx(nullptr), m_frame(nullptr), m_packet(nullptr), m_startTime(0),
                             m_running(false),
                             m_maxDuration(0) {
}

BaseEncoder::~BaseEncoder() {
}

void BaseEncoder::start() {
  m_running = true;
  m_startTime = m_timer.stop();
  m_thread = std::thread(&BaseEncoder::threadProc, this);
}

void BaseEncoder::stop() {
  m_running = false;
}

void BaseEncoder::joinThread() {
  m_thread.join();
}

void BaseEncoder::clearPacketBuffer() {
  for (auto &packet : m_packetBuffer) {
    av_packet_free(&packet);
  }
  m_packetBuffer.clear();
}

const std::deque<AVPacket *> &BaseEncoder::getPacketBuffer() {
  return m_packetBuffer;
}

bool BaseEncoder::isPacketAvailable() const {
  return !m_packetBuffer.empty();
}

void BaseEncoder::setMaxDuration(int duration) {
  m_maxDuration = duration;
}

int BaseEncoder::getMaxDuration() {
  return m_maxDuration;
}

AVCodecContext * BaseEncoder::getCodecContext() {
  return m_codecCtx;
}

int64_t BaseEncoder::getMinimumPTS() const {
  auto filteredBufferPTS = m_packetBuffer | std::views::filter([] (AVPacket *pkt) { return pkt->pts != AV_NOPTS_VALUE; });
  auto filteredBufferDTS = m_packetBuffer | std::views::filter([] (AVPacket *pkt) { return pkt->dts != AV_NOPTS_VALUE; });
  auto ptsMin = std::ranges::min_element(
    filteredBufferPTS,
    [](AVPacket *a, AVPacket *b) { return a->pts < b->pts; }
    );
  auto dtsMin = std::ranges::min_element(
    filteredBufferDTS,
    [](AVPacket *a, AVPacket *b) { return a->dts < b->dts; }
    );
  if (!filteredBufferDTS) {
   return (*ptsMin)->pts;
  }
  return std::min((*ptsMin)->pts, (*dtsMin)->dts);
}

//void BaseEncoder::lockBuffer() {
//  m_packetBufferMutex.lock();
//}

//void BaseEncoder::unlockBuffer() {
//  m_packetBufferMutex.unlock();
//}
