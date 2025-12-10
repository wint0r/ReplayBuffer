#ifndef REPLAYBUFFER_PIXELBUFFERMANAGER_HPP
#define REPLAYBUFFER_PIXELBUFFERMANAGER_HPP

#include <Geode/cocos/platform/CCGL.h>
#include <vector>

class PixelBufferManager {
  GLuint m_pbos[2]{}, m_pboIdx;
  int m_frameWidth, m_frameHeight;
  size_t m_bufferSize;
  std::vector<uint8_t> m_lastFrameData;
  bool m_firstFrame;

public:
  PixelBufferManager();
  ~PixelBufferManager();

  void captureFrame();
  void changeSize(int width, int height);
  uint8_t *getCurrentFrame();
};

#endif
