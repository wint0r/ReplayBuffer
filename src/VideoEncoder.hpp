#ifndef REPLAYBUFFER_VIDEOENCODER_HPP
#define REPLAYBUFFER_VIDEOENCODER_HPP

#include "BaseEncoder.hpp"
#include "PixelBufferManager.hpp"
#include <string>

class VideoEncoder : public BaseEncoder {
  AVBufferRef *m_hwDeviceCtx;
  SwsContext *m_swsCtx;
  int m_srcWidth, m_srcHeight;
  int m_dstWidth, m_dstHeight;
  int m_dstFramerate;
  bool m_isUsingGPU;
  std::string m_encoderName;
  int64_t m_lastFrameTime;
  int64_t m_timeBaseUs;
  int64_t m_dstBitrate;
  std::unique_ptr<PixelBufferManager> m_pixelBufferManager;

public:
  VideoEncoder();
  ~VideoEncoder();

  void init() override;
  void destroy() override;
  void start() override;
  void stop() override;
  void update() override;
  bool isVideo() override;

private:
  const AVCodec *detectCodec();

protected:
  void threadProc() override;

private:
  void initSwsContext();
  void initCodecContext();
  void destroyCodecContext();
  void reinitCodecContext();

public:
  void setSrcResolution(int width, int height);
  void setDstResolution(int width, int height);
  void setUsingGPU(bool isGPU);
  void setDstFramerate(int fps);
  void setDstBitrate(int bitrate);
};

#endif //REPLAYBUFFER_VIDEOENCODER_HPP