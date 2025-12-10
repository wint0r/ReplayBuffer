#ifndef REPLAYBUFFER_REPLAYBUFFER_HPP
#define REPLAYBUFFER_REPLAYBUFFER_HPP

#include <deque>
#include <memory>
#include <mutex>
#include <vector>
#include "BaseEncoder.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

class ReplayBuffer {
  std::map<int, std::shared_ptr<BaseEncoder>> m_encoders;

public:
  ReplayBuffer();
  ~ReplayBuffer();

  template<typename Encoder>
  void addStream(int idx);
  std::shared_ptr<BaseEncoder> getStreamEncoder(int idx);

  void start();
  void stop();
  void update();
  void clear();
  void saveToFile(const std::filesystem::path &filename);
  void setDuration(int64_t newDuration);
  const std::map<int, std::shared_ptr<BaseEncoder>> &getEncoders();
};

template<typename Encoder>
void ReplayBuffer::addStream(int idx) {
  static_assert(std::is_base_of<BaseEncoder, Encoder>::value, "must be an encoder");
  m_encoders[idx] = std::make_shared<Encoder>();
}

#endif
