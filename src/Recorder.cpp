#include "Recorder.hpp"
#include "AudioEncoder.hpp"
#include "VideoEncoder.hpp"
#include <Geode/Geode.hpp>
using namespace geode::prelude;

Recorder::Recorder() {
  m_firstInit = true;
  m_replayBuffer = std::make_shared<ReplayBuffer>();
}

Recorder::~Recorder() {
  this->stop();
}

std::shared_ptr<Recorder> Recorder::getInstance() {
  static std::shared_ptr<Recorder> instance = std::make_shared<Recorder>();
  return instance;
}

Result<> Recorder::start() {
  int width = Mod::get()->getSavedValue<int>("settings-width"_spr);
  int height = Mod::get()->getSavedValue<int>("settings-height"_spr);
  int framerate = Mod::get()->getSavedValue<int>("settings-framerate"_spr);
  bool hwAccel = Mod::get()->getSavedValue<bool>("settings-hw-accel"_spr);
  int bitrate = Mod::get()->getSavedValue<int>("settings-bitrate"_spr) * 1000;
  int length = Mod::get()->getSavedValue<int>("settings-length"_spr);
  int deviceIDs[] = {
    -1,
    Mod::get()->getSavedValue<int>("settings-audio-id-1"_spr),
    Mod::get()->getSavedValue<int>("settings-audio-id-2"_spr)
  };

  try {
    if (m_firstInit) {
      m_replayBuffer->addStream<VideoEncoder>(0);
      m_replayBuffer->addStream<AudioEncoder>(1);
      m_replayBuffer->addStream<AudioEncoder>(2);
    } else {
      for (const auto &[_idx, encoder] : m_replayBuffer->getEncoders()) {
        encoder->joinThread();
      }
      m_replayBuffer->clear();
    }

    m_replayBuffer->setDuration(length);

    for (const auto &[idx, encoder] : m_replayBuffer->getEncoders()) {
      if (encoder->isVideo()) {
        auto frameSize = CCDirector::sharedDirector()->getOpenGLView()->getFrameSize();
        auto videoEncoder = std::dynamic_pointer_cast<VideoEncoder>(encoder);
        videoEncoder->setSrcResolution(frameSize.width, frameSize.height);
        videoEncoder->setDstResolution(width, height);
        videoEncoder->setDstFramerate(framerate);
        videoEncoder->setDstBitrate(bitrate);
        videoEncoder->setUsingGPU(hwAccel);
      } else {
        auto audioEncoder = std::dynamic_pointer_cast<AudioEncoder>(encoder);
        audioEncoder->setDeviceID(deviceIDs[idx]);
      }

      if (m_firstInit) {
        encoder->init();
      }
      encoder->start();
    }

    m_firstInit = false;
  } catch (const std::string &e) {
    return Err(e);
  }

  return Ok();
}

void Recorder::stop() {
  m_replayBuffer->stop();
}

void Recorder::clip() {
  auto output_dir = Mod::get()->getSettingValue<std::filesystem::path>("output-dir");
  char buffer[80];
  std::time_t now = std::time(nullptr);
  std::tm* local_time = std::localtime(&now);
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H-%M-%S.mp4", local_time);

  if (Mod::get()->getSavedValue<bool>("is-recording"_spr)) {
    try {
      auto path = output_dir / buffer;
      m_replayBuffer->saveToFile(path);
      FLAlertLayer::create(
        "Success",
        fmt::format("Clip saved at {}", path.string()),
        "OK"
        )->show();
    } catch (const std::string &e) {
      FLAlertLayer::create(
        "Error while saving",
        e,
        "OK"
        )->show();
    }
  }
}
