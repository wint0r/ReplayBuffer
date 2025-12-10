#include "PixelBufferManager.hpp"
#include <Geode/cocos/CCDirector.h>

PixelBufferManager::PixelBufferManager() {
  for (unsigned int &pbo : m_pbos) {
    pbo = 0;
  }
  m_pboIdx = 0;
  m_firstFrame = true;
  m_bufferSize = -1;

  glGenBuffers(2, m_pbos);
}

PixelBufferManager::~PixelBufferManager() {
  glDeleteBuffers(2, m_pbos);
}

void PixelBufferManager::captureFrame() {
  glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbos[m_pboIdx]);
  glReadPixels(0, 0, m_frameWidth, m_frameHeight, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  m_pboIdx = m_pboIdx ^ 1;

  if (!m_firstFrame) {
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbos[m_pboIdx]);
    auto *data = static_cast<uint8_t *>(glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));
    if (data != nullptr) {
      std::copy_n(data, m_bufferSize, m_lastFrameData.begin());
    }
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  } else {
    m_firstFrame = false;
  }
}

void PixelBufferManager::changeSize(int width, int height) {
  m_frameWidth = width;
  m_frameHeight = height;
  m_bufferSize = static_cast<size_t>(width * height * 4);
  m_lastFrameData.resize(m_bufferSize);
  for (const GLuint pbo : m_pbos) {
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, m_bufferSize, nullptr, GL_STREAM_READ);
  }
}

uint8_t *PixelBufferManager::getCurrentFrame() {
  return m_lastFrameData.data();
}
