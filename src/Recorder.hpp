#ifndef REPLAYBUFFER_RECORDER_HPP
#define REPLAYBUFFER_RECORDER_HPP

#include "ReplayBuffer.hpp"

struct Recorder {
  bool m_firstInit;
  std::shared_ptr<ReplayBuffer> m_replayBuffer;

  Recorder();
  ~Recorder();

  static std::shared_ptr<Recorder> getInstance();

  geode::Result<> start();
  void stop();
  geode::Result<std::string> clip();
};


#endif