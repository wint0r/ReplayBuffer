#include "Recorder.hpp"
#include "VideoRecorder.hpp"
#include <Geode/Geode.hpp>
using namespace geode::prelude;

Recorder::Recorder() {
  first_init = false;
}

Recorder::~Recorder() {
  this->stop();
}

std::shared_ptr<Recorder> Recorder::get_instance() {
  static std::shared_ptr<Recorder> instance = std::make_shared<Recorder>();
  return instance;
}

Result<> Recorder::start() {
  auto recorder = VideoRecorder::get_instance();
  int width = Mod::get()->getSavedValue<int>("settings-width"_spr);
  int height = Mod::get()->getSavedValue<int>("settings-height"_spr);
  int framerate = Mod::get()->getSavedValue<int>("settings-framerate"_spr);
  bool hw_accel = Mod::get()->getSavedValue<bool>("settings-hw-accel"_spr);
  int bitrate = Mod::get()->getSavedValue<int>("settings-bitrate"_spr) * 1000;

  if (!this->first_init) {
    auto res = recorder->init_av(width, height, framerate, hw_accel, bitrate);
    if (res.isErr()) {
      return Err("error while initialising ffmpeg for video recording: {}", res.err());
    }

    desktop_recorder = std::make_shared<AudioRecorder>();
    res = desktop_recorder->init(Mod::get()->getSavedValue<int>("settings-audio-id-1"_spr));
    if (res.isErr()) {
      return Err("error while initialising desktop audio recorder/ffmpeg: {}", res.err());
    }

    mic_recorder = std::make_shared<AudioRecorder>();
    res = mic_recorder->init(Mod::get()->getSavedValue<int>("settings-audio-id-2"_spr));
    if (res.isErr()) {
      return Err("error while initialising microphone audio recorder/ffmpeg: {}", res.err());
    }

    replay_buffer = std::make_shared<ReplayBuffer>(Mod::get()->getSavedValue<int>("settings-length"_spr) * 1000);
    recorder->init_hook();
    this->first_init = true;
  } else {
    auto res = recorder->reinit_av(width, height, framerate, hw_accel, bitrate);
    if (res.isErr()) {
      return Err("error while initialising ffmpeg for video recording: {}", res.err());
    }

    res = desktop_recorder->reinit(Mod::get()->getSavedValue<int>("settings-audio-id-1"_spr));
    if (res.isErr()) {
      return Err("error while initialising desktop audio recorder/ffmpeg: {}", res.err());
    }

    res = mic_recorder->reinit(Mod::get()->getSavedValue<int>("settings-audio-id-2"_spr));
    if (res.isErr()) {
      return Err("error while initialising microphone audio recorder/ffmpeg: {}", res.err());
    }
  }

  recorder->start_recording(replay_buffer);
  desktop_recorder->start_recording(replay_buffer, 1);
  mic_recorder->start_recording(replay_buffer, 2);

  return Ok();
}

void Recorder::stop() {
  auto recorder = VideoRecorder::get_instance();
  recorder->stop_recording();
  mic_recorder->stop_recording();
  desktop_recorder->stop_recording();
  replay_buffer->clear();
}
