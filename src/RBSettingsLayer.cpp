#include "RBSettingsLayer.hpp"

#include "AudioEncoder.hpp"
#include "Recorder.hpp"

bool RBSettingsLayer::setup()  {
  this->setTitle("Replay Buffer Settings");

  auto *menu = cocos2d::CCMenu::create();
  menu->setPosition(0, 0);
  menu->setID("settings-menu"_spr);
  this->m_mainLayer->addChild(menu);

  bool isRecording = geode::Mod::get()->getSavedValue<bool>("is-recording"_spr);
  auto *buttonSprite = ButtonSprite::create(isRecording ? "Stop Recording" : "Start Recording");
  auto *recordButton = CCMenuItemSpriteExtra::create(
    buttonSprite,
    this,
    menu_selector(RBSettingsLayer::onRecordButton)
    );
  buttonSprite->m_BGSprite = cocos2d::extension::CCScale9Sprite::createWithSpriteFrameName(!isRecording ? "GJ_button_01.png" : "GJ_button_06.png");
  recordButton->setPosition(140, 68);
  menu->addChild(recordButton);

  auto *hwAccelToggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(RBSettingsLayer::onHWAccelToggle), 1.0);
  hwAccelToggle->setPosition(252.5 + 10, 122.5);
  hwAccelToggle->setID("settings-hw-accel"_spr);
  hwAccelToggle->setScale(0.5);
  hwAccelToggle->toggle(geode::Mod::get()->getSavedValue<bool>("settings-hw-accel"_spr));
  menu->addChild(hwAccelToggle);

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

  auto *textBox = geode::TextInput::create(100, "", "bigFont.fnt");
  textBox->setPosition(80, 200 - 30);
  textBox->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  textBox->setID("settings-width"_spr);
  textBox->setScale(0.5);
  textBox->setString(std::to_string(geode::Mod::get()->getSavedValue<int>("settings-width"_spr)));
  this->m_mainLayer->addChild(textBox);

  textBox = geode::TextInput::create(100, "", "bigFont.fnt");
  textBox->setPosition(80, 200 - 50);
  textBox->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  textBox->setID("settings-height"_spr);
  textBox->setScale(0.5);
  textBox->setString(std::to_string(geode::Mod::get()->getSavedValue<int>("settings-height"_spr)));
  this->m_mainLayer->addChild(textBox);

  textBox = geode::TextInput::create(100, "", "bigFont.fnt");
  textBox->setPosition(80, 200 - 70);
  textBox->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  textBox->setID("settings-framerate"_spr);
  textBox->setScale(0.5);
  textBox->setString(std::to_string(geode::Mod::get()->getSavedValue<int>("settings-framerate"_spr)));
  this->m_mainLayer->addChild(textBox);

  textBox = geode::TextInput::create(100, "", "bigFont.fnt");
  textBox->setPosition(220, 200 - 30);
  textBox->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  textBox->setID("settings-bitrate"_spr);
  textBox->setScale(0.5);
  textBox->setString(std::to_string(geode::Mod::get()->getSavedValue<int>("settings-bitrate"_spr)));
  this->m_mainLayer->addChild(textBox);

  textBox = geode::TextInput::create(100, "", "bigFont.fnt");
  textBox->setPosition(220, 200 - 50);
  textBox->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  textBox->setID("settings-length"_spr);
  textBox->setScale(0.5);
  textBox->setString(std::to_string(geode::Mod::get()->getSavedValue<int>("settings-length"_spr)));
  this->m_mainLayer->addChild(textBox);

  textBox = geode::TextInput::create(100, "", "bigFont.fnt");
  textBox->setPosition(80, 200 - 90);
  textBox->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  textBox->setID("settings-audio-id-1"_spr);
  textBox->setScale(0.5);
  textBox->setString(std::to_string(geode::Mod::get()->getSavedValue<int>("settings-audio-id-1"_spr)));
  this->m_mainLayer->addChild(textBox);

  textBox = geode::TextInput::create(100, "", "bigFont.fnt");
  textBox->setPosition(220, 200 - 90);
  textBox->setAnchorPoint(cocos2d::CCPoint(0.0, 1.0));
  textBox->setID("settings-audio-id-2"_spr);
  textBox->setScale(0.5);
  textBox->setString(std::to_string(geode::Mod::get()->getSavedValue<int>("settings-audio-id-2"_spr)));
  this->m_mainLayer->addChild(textBox);

  buttonSprite = ButtonSprite::create("Save Settings");
  auto *button = CCMenuItemSpriteExtra::create(
    buttonSprite,
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
  auto buttonSprite = dynamic_cast<ButtonSprite *>(button->getNormalImage());
  bool isRecording = geode::Mod::get()->getSavedValue<bool>("is-recording"_spr);
  buttonSprite->m_BGSprite = cocos2d::extension::CCScale9Sprite::createWithSpriteFrameName(isRecording ? "GJ_button_01.png" : "GJ_button_06.png");
  buttonSprite->setString(isRecording ? "Start Recording" : "Stop Recording");
  geode::Mod::get()->setSavedValue<bool>("is-recording"_spr, !isRecording);

  auto *menu = this->m_mainLayer->getChildByID("settings-menu"_spr);
  dynamic_cast<CCMenuItemSpriteExtra *>(menu->getChildByID("settings-save"_spr))->setEnabled(isRecording);

  if (!isRecording) {
    auto result = Recorder::getInstance()->start();
    if (result.isErr()) {
      buttonSprite = dynamic_cast<ButtonSprite *>(button->getNormalImage());
      buttonSprite->m_BGSprite = cocos2d::extension::CCScale9Sprite::createWithSpriteFrameName("GJ_button_01.png");
      buttonSprite->setString("Start Recording");
      dynamic_cast<CCMenuItemSpriteExtra *>(menu->getChildByID("settings-save"_spr))->setEnabled(true);
      geode::Mod::get()->setSavedValue<bool>("is-recording"_spr, false);
      FLAlertLayer::create("Error", result.unwrapErr(), "OK")->show();
    }
  } else {
    Recorder::getInstance()->stop();
  }
}

void RBSettingsLayer::onHWAccelToggle(CCObject *sender) {
  // auto *toggle = geode::cast::typeinfo_cast<CCMenuItemToggler *>(sender);
  // toggle->toggle(!toggle->isToggled());
}

void RBSettingsLayer::onSaveSettings(CCObject *sender) {
  static std::vector<std::string> settingsIDs = {
    "settings-width"_spr,
    "settings-height"_spr,
    "settings-framerate"_spr,
    "settings-bitrate"_spr,
    "settings-length"_spr,
    "settings-audio-id-1"_spr,
    "settings-audio-id-2"_spr
  };

  for (auto &settingsID : settingsIDs) {
    auto *text_box = dynamic_cast<geode::TextInput *>(this->m_mainLayer->getChildByID(settingsID));
    geode::Mod::get()->setSavedValue(settingsID, std::stoi(text_box->getString()));
  }

  auto *menu = this->m_mainLayer->getChildByID("settings-menu"_spr);
  auto *hwAccelToggle = dynamic_cast<CCMenuItemToggler *>(menu->getChildByID("settings-hw-accel"_spr));
  geode::Mod::get()->setSavedValue("settings-hw-accel"_spr, hwAccelToggle->isToggled());
}

void RBSettingsLayer::onDismiss(CCObject *sender) {
  this->onClose(sender);
}

void RBSettingsLayer::onInfoIcon(CCObject *sender) {
  std::string alertString;
  std::vector<std::string> devices = AudioEncoder::getDeviceList();
  for (int i = 0; i < devices.size(); i++) {
    alertString += "[" + std::to_string(i) + "]: " + devices[i] + "\n";
  }
  FLAlertLayer::create("Audio Device IDs", alertString, "OK")->show();
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
