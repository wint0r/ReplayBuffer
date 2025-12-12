#ifndef REPLAYBUFFER_BASEENCODER_HPP
#define REPLAYBUFFER_BASEENCODER_HPP

#include <thread>
#include <mutex>
#include "Timer.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

class BaseEncoder {
protected:
  const AVCodec *m_codec;
  AVCodecContext *m_codecCtx;
  AVFrame *m_frame;
  AVPacket *m_packet;
  std::thread m_thread;
  Timer m_timer;
  int64_t m_startTime;
  bool m_running;
  int m_maxDuration;
  std::deque<AVPacket *> m_packetBuffer;
  std::mutex m_packetBufferMutex;

  virtual void threadProc() = 0;
  void trimBuffer();
  void pushPacket(AVPacket *pkt);

public:
  BaseEncoder();
  virtual ~BaseEncoder();

  virtual void init() = 0;
  virtual void destroy() = 0;
  virtual void start();
  virtual void stop();
  virtual void joinThread();
  virtual void update() = 0;
  virtual bool isVideo() = 0;

  void clearPacketBuffer();

  const std::deque<AVPacket *> &getPacketBuffer();
  bool isPacketAvailable() const;
  void setMaxDuration(int duration);
  int getMaxDuration();
  AVCodecContext *getCodecContext();
  int64_t getMinimumPTS() const;
  void lockBuffer();
  void unlockBuffer();
};


#endif //REPLAYBUFFER_BASEENCODER_HPP