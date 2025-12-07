#include "PixelBufferManager.hpp"
#include <Geode/cocos/CCDirector.h>

PixelBufferManager::PixelBufferManager() {
  for (unsigned int &pbo : this->pbos) {
    pbo = 0;
  }
  this->pbo_idx = 0;
  this->first_frame = true;
  this->buffer_size = -1;

  glGenBuffers(2, this->pbos);
  this->refresh_size();
}

PixelBufferManager::~PixelBufferManager() {
  glDeleteBuffers(2, this->pbos);
}

void PixelBufferManager::capture_frame() {
  // glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[pbo_idx]);
  // glReadPixels(0, 0, this->frame_size.width, this->frame_size.height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  // glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  // prev_pbo_idx = this->pbo_idx;
  // pbo_idx = (pbo_idx + 1) % PBO_AMOUNT;
  glBindBuffer(GL_PIXEL_PACK_BUFFER, this->pbos[this->pbo_idx]);
  glReadPixels(0, 0, this->frame_size.width, this->frame_size.height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  this->pbo_idx = this->pbo_idx ^ 1;

  if (!this->first_frame) {
    std::lock_guard<std::mutex> lock(this->frame_mutex);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, this->pbos[this->pbo_idx]);
    auto *data = static_cast<uint8_t *>(glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));
    if (data != nullptr) {
      std::copy_n(data, this->buffer_size, this->last_frame_data.begin());
    }
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  } else {
    this->first_frame = false;
  }
}

void PixelBufferManager::refresh_size() {
  auto *director = cocos2d::CCDirector::sharedDirector();
  this->frame_size = director->getOpenGLView()->getFrameSize();
  this->buffer_size = static_cast<size_t>(this->frame_size.width * this->frame_size.height * 4);
  this->last_frame_data.resize(this->buffer_size);
  for (const GLuint pbo : this->pbos) {
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, this->buffer_size, nullptr, GL_STREAM_READ);
  }
}

void PixelBufferManager::lock_frame() {
  this->frame_lock = std::unique_lock<std::mutex>(this->frame_mutex);
}

void PixelBufferManager::unlock_frame() {
  this->frame_lock.unlock();
}

uint8_t * PixelBufferManager::get_current_frame() {
  return this->last_frame_data.data();
}

// uint8_t *PixelBufferManager::get_current_frame() {
//   glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[prev_pbo_idx]);
//   auto *data = static_cast<uint8_t *>(glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));
//   if (data == nullptr) {
//     Frame *frame = new Frame();
//     frame->frame_size = this->frame_size;
//     frame->frame_data.assign(last_frame_data.begin(), last_frame_data.end());
//     return frame;
//   }
//
//   Frame *frame = new Frame();
//   frame->frame_size = this->frame_size;
//   frame->frame_data.assign(data, data + this->buffer_size);
//   last_frame_data.assign(data, data + this->buffer_size);
//   glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
//   glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
//   return frame;
// }
