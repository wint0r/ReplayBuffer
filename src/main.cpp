#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/cocos/platform/win32/CCEGLView.h>
#include <Geode/cocos/CCDirector.h>
#include "PixelBufferManager.hpp"
#include <queue>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include "ReplayBuffer.hpp"
#include "VideoRecorder.hpp"
#include "AudioRecorder.hpp"
#include "RBSettingsLayer.hpp"
#include "Recorder.hpp"

using namespace geode::prelude;

class $modify(ReplayBuffer_MenuLayer, MenuLayer) {
  struct Fields {
    bool initialised_recording = false;
  };
  bool init() override {
    if (!MenuLayer::init()) {
      return false;
    }

    auto *button = CCMenuItemSpriteExtra::create(
      CircleButtonSprite::createWithSprite("icon.png"_spr, 0.8),
      this,
      menu_selector(ReplayBuffer_MenuLayer::onSettingsButton)
      );
    auto menu = this->getChildByID("bottom-menu");
    menu->addChild(button);
    menu->updateLayout();

    return true;
  }

  void onSettingsButton(CCObject *) {
    RBSettingsLayer::create()->show();
  }
};

class $modify(ReplayBuffer_PauseLayer, PauseLayer) {
  void customSetup() override {
    PauseLayer::customSetup();

    auto *menu = this->getChildByID("left-button-menu");
    if (Mod::get()->getSavedValue<bool>("is-recording"_spr)) {
      auto *sprite = CCSprite::createWithSpriteFrameName("GJ_shareBtn_001.png");
      sprite->setScale(0.5f);
      auto *button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(ReplayBuffer_PauseLayer::onClipButton));
      menu->addChild(button);
    }

    auto *button = CCMenuItemSpriteExtra::create(
      CircleButtonSprite::createWithSprite("icon.png"_spr, 0.8),
      this,
      menu_selector(ReplayBuffer_MenuLayer::onSettingsButton)
      );
    menu->addChild(button);
  }

  void onClipButton(CCObject *) {
    Recorder::get_instance()->clip();
  }

  void onSettingsButton(CCObject *) {
    RBSettingsLayer::create()->show();
  }
};

class $modify(ReplayBuffer_EditLevelLayer, EditLevelLayer) {
  bool init(GJGameLevel *level) {
    if (!EditLevelLayer::init(level)) {
      return false;
    }

    auto *menu = this->getChildByID("level-actions-menu");
    if (Mod::get()->getSavedValue<bool>("is-recording"_spr)) {
      auto *sprite = CCSprite::createWithSpriteFrameName("GJ_shareBtn_001.png");
      sprite->setScale(0.8f);
      auto *button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(ReplayBuffer_EditLevelLayer::onClipButton));
      menu->addChild(button);
      menu->updateLayout();
    }
  }

  void onClipButton(CCObject *) {
    Recorder::get_instance()->clip();
  }
};

class $modify(ReplayBuffer_LevelInfoLayer, LevelInfoLayer) {
  bool init(GJGameLevel *level, bool challenge) {
    if (!LevelInfoLayer::init(level, challenge)) {
      return false;
    }

    auto *menu = this->getChildByID("left-side-menu");
    if (Mod::get()->getSavedValue<bool>("is-recording"_spr)) {
      auto *sprite = CCSprite::createWithSpriteFrameName("GJ_shareBtn_001.png");
      sprite->setScale(0.8f);
      auto *button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(ReplayBuffer_LevelInfoLayer::onClipButton));
      menu->addChild(button);
      menu->updateLayout();
    }
  }

  void onClipButton(CCObject *) {
    Recorder::get_instance()->clip();
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
    Mod::get()->setSavedValue<int>("settings-length"_spr, 300);
  }

  Mod::get()->setSavedValue<bool>("is-recording"_spr, false);
}
