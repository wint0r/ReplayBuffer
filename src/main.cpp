#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/cocos/CCDirector.h>
#include "PixelBufferManager.hpp"
#include <queue>
#include <ranges>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/CCEGLViewProtocol.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include "AudioEncoder.hpp"
#include "ReplayBuffer.hpp"
#include "RBSettingsLayer.hpp"
#include "Recorder.hpp"
#include "VideoEncoder.hpp"

using namespace geode::prelude;

class $modify(ReplayBuffer_MenuLayer, MenuLayer) {
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
    Recorder::getInstance()->clip();
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

    return true;
  }

  void onClipButton(CCObject *) {
    Recorder::getInstance()->clip();
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

    return true;
  }

  void onClipButton(CCObject *) {
    Recorder::getInstance()->clip();
  }
};

class $modify(ReplayBuffer_CCEGLViewProtocol, cocos2d::CCEGLViewProtocol) {
  void setFrameSize(float width, float height) override {
    CCEGLViewProtocol::setFrameSize(width, height);

    auto encoders = Recorder::getInstance()->m_replayBuffer->getEncoders();
    for (auto &encoder: encoders | std::views::values) {
      if (encoder->isVideo()) {
        auto videoEncoder = std::dynamic_pointer_cast<VideoEncoder>(encoder);
        videoEncoder->setSrcResolution(width, height);
      }
    }
  }
};

class $modify(ReplayBuffer_CCScheduler, cocos2d::CCScheduler) {
  void update(float dt) override {
    CCScheduler::update(dt);
    Recorder::getInstance()->m_replayBuffer->update();
  }
};

class $modify(ReplayBuffer_EndLevelLayer, EndLevelLayer) {
  void customSetup() override {
    EndLevelLayer::customSetup();

    auto *menu = this->m_sideMenu;
    if (Mod::get()->getSavedValue<bool>("is-recording"_spr)) {
      auto *sprite = CCSprite::createWithSpriteFrameName("GJ_shareBtn_001.png");
      sprite->setScale(0.5f);
      auto *button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(ReplayBuffer_EndLevelLayer::onClipButton));
      button->setPosition(150, -90);
      menu->addChild(button);
    }
  }

  void onClipButton(CCObject *) {
    Recorder::getInstance()->clip();
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
    Mod::get()->setSavedValue<int>("settings-audio-id-2"_spr, 0);
    auto deviceList = AudioEncoder::getDeviceList();
    int defaultDesktopID = 0;
    for (int i = 0; i < deviceList.size(); i++) {
      if (deviceList[i].contains("[loopback]")) {
        defaultDesktopID = i;
        break;
      }
    }
    Mod::get()->setSavedValue<int>("settings-audio-id-1"_spr, defaultDesktopID);
    Mod::get()->setSavedValue<int>("settings-length"_spr, 300);
  }

  Mod::get()->setSavedValue<bool>("is-recording"_spr, false);
}
