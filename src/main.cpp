#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/cocos/platform/win32/CCEGLView.h>
#include <Geode/cocos/CCDirector.h>
#include "PixelBufferManager.hpp"
#include <queue>
#include <Geode/modify/MenuLayer.hpp>
#include "ReplayBuffer.hpp"
#include "VideoRecorder.hpp"
#include "AudioRecorder.hpp"
#include "RBSettingsLayer.hpp"
#include "Recorder.hpp"

using namespace geode::prelude;

class $modify(MyMenuLayer, MenuLayer) {
  struct Fields {
    bool initialised_recording = false;
  };
  bool init() override {
    if (!MenuLayer::init()) {
      return false;
    }

    auto *button = CCMenuItemSpriteExtra::create(CCSprite::createWithSpriteFrameName("GJ_likeBtn_001.png"), this, menu_selector(MyMenuLayer::onButton));
    auto menu = this->getChildByID("bottom-menu");
    menu->addChild(button);
    menu->updateLayout();

    return true;
  }

  void onButton(CCObject *) {
    RBSettingsLayer::create()->show();
  }
};

class $modify(ReplayBuffer_PauseLayer, PauseLayer) {
  void customSetup() override {
    PauseLayer::customSetup();

    auto *sprite = CCSprite::createWithSpriteFrameName("GJ_shareBtn_001.png");
    sprite->setScale(0.5f);

    auto *menu = this->getChildByID("left-button-menu");
    auto *button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(ReplayBuffer_PauseLayer::onClipButton));
    menu->addChild(button);

    sprite = CCSprite::createWithSpriteFrameName("GJ_optionsBtn02_001.png");
    button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(ReplayBuffer_PauseLayer::onSettingsButton));
    menu->addChild(button);
  }

  void onClipButton(CCObject *) {
    auto output_dir = Mod::get()->getSettingValue<std::filesystem::path>("output-dir");
    char buffer[80];
    std::time_t now = std::time(nullptr);
    std::tm* local_time = std::localtime(&now);
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H-%M-%S.mp4", local_time);

    if (Mod::get()->getSavedValue<bool>("is-recording"_spr)) {
      auto result = Recorder::get_instance()->replay_buffer->save_to_file(output_dir / buffer);
      if (result.isErr()) {
        FLAlertLayer::create(
          "Error while saving",
          result.unwrapErr(),
          "OK"
        )->show();
      }
    }
  }

  void onSettingsButton(CCObject *) {
    RBSettingsLayer::create()->show();
  }
};

$on_mod(Loaded) {
  if (!Mod::get()->setSavedValue("set-default-values", true)) {
    auto view_size = CCDirector::sharedDirector()->getOpenGLView()->getFrameSize();
    Mod::get()->setSavedValue<int>("settings-width"_spr, static_cast<int>(view_size.width));
    Mod::get()->setSavedValue<int>("settings-height"_spr, static_cast<int>(view_size.height));
    Mod::get()->setSavedValue<int>("settings-framerate"_spr, 60);
    Mod::get()->setSavedValue<bool>("settings-hw-accel"_spr, true);
    Mod::get()->setSavedValue<int>("settings-bitrate"_spr, 12000);
    Mod::get()->setSavedValue<int>("settings-audio-id-1"_spr, 0);
    auto device_list = AudioRecorder::get_device_list();
    int default_desktop_id = 0;
    for (int i = 0; i < device_list.size(); i++) {
      if (device_list[i].contains("[loopback]")) {
        default_desktop_id = i;
        break;
      }
    }
    Mod::get()->setSavedValue<int>("settings-audio-id-2"_spr, default_desktop_id);
  }

  Mod::get()->setSavedValue<bool>("is-recording"_spr, false);
}
