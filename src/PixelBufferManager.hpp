#ifndef REPLAYBUFFER_PIXELBUFFERMANAGER_HPP
#define REPLAYBUFFER_PIXELBUFFERMANAGER_HPP

#include <Geode/cocos/platform/CCGL.h>
#include <vector>

struct Frame {
  //std::vector<uint8_t> frame_data;
  uint8_t *data;
  cocos2d::CCSize frame_size;
};

class PixelBufferManager {
  GLuint pbos[2]{}, pbo_idx;
  cocos2d::CCSize frame_size;
  size_t buffer_size;
  std::vector<uint8_t> last_frame_data;
  std::mutex frame_mutex;
  std::unique_lock<std::mutex> frame_lock;
  bool first_frame;

public:
  PixelBufferManager();
  ~PixelBufferManager();

  void capture_frame();
  void refresh_size();

  void lock_frame();
  void unlock_frame();
  uint8_t *get_current_frame();
};

#endif
