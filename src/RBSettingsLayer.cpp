#include "RBSettingsLayer.hpp"
#include "Recorder.hpp"
#include "AudioRecorder.hpp"

bool RBSettingsLayer::setup()  {
  this->setTitle("Replay Buffer Settings");

  auto *menu = cocos2d::CCMenu::create();
  menu->setPosition(0, 0);
  menu->setID("settings-menu"_spr);
  this->m_mainLayer->addChild(menu);

  bool is_recording = geode::Mod::get()->getSavedValue<bool>("is-recording"_spr);
  auto *button_sprite = ButtonSprite::create(is_recording ? "Stop Recording" : "Start Recording");
  auto *record_button = CCMenuItemSpriteExtra::create(
    button_sprite,
    this,
    menu_selector(RBSettingsLayer::onRecordButton)
    );
  button_sprite->m_BGSprite = cocos2d::extension::CCScale9Sprite::createWithSpriteFrameName(!is_recording ? "GJ_button_01.png" : "GJ_button_06.png");
  record_button->setPosition(140, 68);
  menu->addChild(record_button);

  auto *hw_accel_toggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(RBSettingsLayer::onHWAccelToggle), 1.0);
  hw_accel_toggle->setPosition(252.5 + 10, 122.5);
  hw_accel_toggle->setID("settings-hw-accel"_spr);
  hw_accel_toggle->setScale(0.5);
  hw_accel_toggle->toggle(geode::Mod::get()->getSavedValue<bool>("settings-hw-accel"_spr));
  menu->addChild(hw_accel_toggle);

  auto *label = cocos2d::CCLabelBMFont::create("Width", "bigFont.fnt");
  label->setPosition(10, 200 - 30);
  label->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  label->setScale(0.5);
  this->m_mainLayer->addChild(label);

  label = cocos2d::CCLabelBMFont::create("Height", "bigFont.fnt");
  label->setPosition(10, 200 - 50);
  label->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  label->setScale(0.5);
  this->m_mainLayer->addChild(label);

  label = cocos2d::CCLabelBMFont::create("FPS", "bigFont.fnt");
  label->setPosition(10, 200 - 70);
  label->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  label->setScale(0.5);
  this->m_mainLayer->addChild(label);

  label = cocos2d::CCLabelBMFont::create("Bitrate", "bigFont.fnt");
  label->setPosition(150, 200 - 30);
  label->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  label->setScale(0.5);
  this->m_mainLayer->addChild(label);

  label = cocos2d::CCLabelBMFont::create("Length", "bigFont.fnt");
  label->setPosition(150, 200 - 50);
  label->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  label->setScale(0.5);
  this->m_mainLayer->addChild(label);

  label = cocos2d::CCLabelBMFont::create("Use GPU", "bigFont.fnt");
  label->setPosition(150, 200 - 70);
  label->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  label->setScale(0.5);
  this->m_mainLayer->addChild(label);

  label = cocos2d::CCLabelBMFont::create("Audio 1", "bigFont.fnt");
  label->setPosition(10, 200 - 90);
  label->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  label->setScale(0.5);
  this->m_mainLayer->addChild(label);

  label = cocos2d::CCLabelBMFont::create("Audio 2", "bigFont.fnt");
  label->setPosition(150, 200 - 90);
  label->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  label->setScale(0.5);
  this->m_mainLayer->addChild(label);

  auto *text_box = geode::TextInput::create(100, "", "bigFont.fnt");
  text_box->setPosition(80, 200 - 30);
  text_box->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  text_box->setID("settings-width"_spr);
  text_box->setScale(0.5);
  text_box->setString(std::to_string(geode::Mod::get()->getSavedValue<int>("settings-width"_spr)));
  this->m_mainLayer->addChild(text_box);

  text_box = geode::TextInput::create(100, "", "bigFont.fnt");
  text_box->setPosition(80, 200 - 50);
  text_box->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  text_box->setID("settings-height"_spr);
  text_box->setScale(0.5);
  text_box->setString(std::to_string(geode::Mod::get()->getSavedValue<int>("settings-height"_spr)));
  this->m_mainLayer->addChild(text_box);

  text_box = geode::TextInput::create(100, "", "bigFont.fnt");
  text_box->setPosition(80, 200 - 70);
  text_box->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  text_box->setID("settings-framerate"_spr);
  text_box->setScale(0.5);
  text_box->setString(std::to_string(geode::Mod::get()->getSavedValue<int>("settings-framerate"_spr)));
  this->m_mainLayer->addChild(text_box);

  text_box = geode::TextInput::create(100, "", "bigFont.fnt");
  text_box->setPosition(220, 200 - 30);
  text_box->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  text_box->setID("settings-bitrate"_spr);
  text_box->setScale(0.5);
  text_box->setString(std::to_string(geode::Mod::get()->getSavedValue<int>("settings-bitrate"_spr)));
  this->m_mainLayer->addChild(text_box);

  text_box = geode::TextInput::create(100, "", "bigFont.fnt");
  text_box->setPosition(220, 200 - 50);
  text_box->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  text_box->setID("settings-length"_spr);
  text_box->setScale(0.5);
  text_box->setString(std::to_string(geode::Mod::get()->getSavedValue<int>("settings-length"_spr)));
  this->m_mainLayer->addChild(text_box);

  text_box = geode::TextInput::create(100, "", "bigFont.fnt");
  text_box->setPosition(80, 200 - 90);
  text_box->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  text_box->setID("settings-audio-id-1"_spr);
  text_box->setScale(0.5);
  text_box->setString(std::to_string(geode::Mod::get()->getSavedValue<int>("settings-audio-id-1"_spr)));
  this->m_mainLayer->addChild(text_box);

  text_box = geode::TextInput::create(100, "", "bigFont.fnt");
  text_box->setPosition(220, 200 - 90);
  text_box->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  text_box->setID("settings-audio-id-2"_spr);
  text_box->setScale(0.5);
  text_box->setString(std::to_string(geode::Mod::get()->getSavedValue<int>("settings-audio-id-2"_spr)));
  this->m_mainLayer->addChild(text_box);

  button_sprite = ButtonSprite::create("Save Settings");
  auto *button = CCMenuItemSpriteExtra::create(
    button_sprite,
    this,
    menu_selector(RBSettingsLayer::onSaveSettings)
    );
  button->setPosition(140, 25);
  button->setID("settings-save"_spr);
  menu->addChild(button);

  // button_sprite = ButtonSprite::create("OK");
  // button = CCMenuItemSpriteExtra::create(
  //   button_sprite,
  //   this,
  //   menu_selector(RBSettingsLayer::onDismiss)
  //   );
  // button->setPosition(160, 200 - 170);
  // button->setContentSize(cocos2d::CCSize(40, 20));
  // button->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  // button->setID("settings-dismiss"_spr);
  // menu->addChild(button);

  button = CCMenuItemSpriteExtra::create(
    cocos2d::CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png"),
    this,
    menu_selector(RBSettingsLayer::onInfoIcon)
    );
  button->setPosition(270, 200);
  button->setContentSize(cocos2d::CCSize(20, 20));
  menu->addChild(button);

  return true;
}

void RBSettingsLayer::onRecordButton(CCObject *sender) {
  auto *button = dynamic_cast<CCMenuItemSpriteExtra *>(sender);
  auto button_sprite = dynamic_cast<ButtonSprite *>(button->getNormalImage());
  bool is_recording = geode::Mod::get()->getSavedValue<bool>("is-recording"_spr);
  button_sprite->m_BGSprite = cocos2d::extension::CCScale9Sprite::createWithSpriteFrameName(is_recording ? "GJ_button_01.png" : "GJ_button_06.png");
  button_sprite->setString(is_recording ? "Start Recording" : "Stop Recording");
  geode::Mod::get()->setSavedValue<bool>("is-recording"_spr, !is_recording);

  auto *menu = this->m_mainLayer->getChildByID("settings-menu"_spr);
  dynamic_cast<CCMenuItemSpriteExtra *>(menu->getChildByID("settings-save"_spr))->setEnabled(is_recording);

  if (!is_recording) {
    auto result = Recorder::get_instance()->start();
    if (result.isErr()) {
      button_sprite = dynamic_cast<ButtonSprite *>(button->getNormalImage());
      button_sprite->m_BGSprite = cocos2d::extension::CCScale9Sprite::createWithSpriteFrameName("GJ_button_01.png");
      button_sprite->setString("Start Recording");
      dynamic_cast<CCMenuItemSpriteExtra *>(menu->getChildByID("settings-save"_spr))->setEnabled(true);
      geode::Mod::get()->setSavedValue<bool>("is-recording"_spr, false);
      FLAlertLayer::create("Error", result.unwrapErr(), "OK")->show();
    }
  } else {
    Recorder::get_instance()->stop();
  }
}

void RBSettingsLayer::onHWAccelToggle(CCObject *sender) {
  // auto *toggle = geode::cast::typeinfo_cast<CCMenuItemToggler *>(sender);
  // toggle->toggle(!toggle->isToggled());
}

void RBSettingsLayer::onSaveSettings(CCObject *sender) {
  static std::vector<std::string> settings_ids = {
    "settings-width"_spr,
    "settings-height"_spr,
    "settings-framerate"_spr,
    "settings-bitrate"_spr,
    "settings-length"_spr,
    "settings-audio-id-1"_spr,
    "settings-audio-id-2"_spr
  };

  for (auto &settings_id : settings_ids) {
    auto *text_box = dynamic_cast<geode::TextInput *>(this->m_mainLayer->getChildByID(settings_id));
    geode::Mod::get()->setSavedValue(settings_id, std::stoi(text_box->getString()));
  }

  auto *menu = this->m_mainLayer->getChildByID("settings-menu"_spr);
  auto *hw_accel_toggle = dynamic_cast<CCMenuItemToggler *>(menu->getChildByID("settings-hw-accel"_spr));
  geode::Mod::get()->setSavedValue("settings-hw-accel"_spr, hw_accel_toggle->isToggled());
}

void RBSettingsLayer::onDismiss(CCObject *sender) {
  this->onClose(sender);
}

void RBSettingsLayer::onInfoIcon(CCObject *sender) {
  std::string alert_string;
  std::vector<std::string> devices = AudioRecorder::get_device_list();
  for (int i = 0; i < devices.size(); i++) {
    alert_string += "[" + std::to_string(i) + "]: " + devices[i] + "\n";
  }
  FLAlertLayer::create("Audio Device IDs", alert_string, "OK")->show();
}

RBSettingsLayer *RBSettingsLayer::create() {
  auto ret = new RBSettingsLayer();
  if (ret->initAnchored(280.f, 200.f)) {
    ret->autorelease();
    return ret;
  }

  CC_SAFE_DELETE(ret);
  return nullptr;
}
