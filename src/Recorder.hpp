#ifndef REPLAYBUFFER_RECORDER_HPP
#define REPLAYBUFFER_RECORDER_HPP

#include "ReplayBuffer.hpp"
#include "AudioRecorder.hpp"

struct Recorder {
  std::shared_ptr<ReplayBuffer> replay_buffer;
  std::shared_ptr<AudioRecorder> desktop_recorder, mic_recorder;
  bool first_init;

  Recorder();
  ~Recorder();

  static std::shared_ptr<Recorder> get_instance();

  geode::Result<> start();
  void stop();
};


#endif