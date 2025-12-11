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
#include "Recorder.hpp"
#include "VideoEncoder.hpp"
#include <imgui-cocos.hpp>

using namespace geode::prelude;

static bool g_showSettingsMenu = false;

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
    g_showSettingsMenu = true;
  }
};

class $modify(ReplayBuffer_PauseLayer, PauseLayer) {
  void customSetup() override {
    PauseLayer::customSetup();

    auto *menu = this->getChildByID("left-button-menu");
    auto *button = CCMenuItemSpriteExtra::create(
      CircleButtonSprite::createWithSprite("icon.png"_spr, 0.8),
      this,
      menu_selector(ReplayBuffer_MenuLayer::onSettingsButton)
      );
    menu->addChild(button);
  }

  void onSettingsButton(CCObject *) {
    g_showSettingsMenu = true;
  }
};

class $modify(ReplayBuffer_EditLevelLayer, EditLevelLayer) {
  bool init(GJGameLevel *level) {
    if (!EditLevelLayer::init(level)) {
      return false;
    }

    auto *menu = this->getChildByID("level-actions-menu");
    auto *button = CCMenuItemSpriteExtra::create(
      CircleButtonSprite::createWithSprite("icon.png"_spr, 0.8),
      this,
      menu_selector(ReplayBuffer_EditLevelLayer::onSettingsButton)
      );
    menu->addChild(button);
    menu->updateLayout();

    return true;
  }

  void onSettingsButton(CCObject *) {
    g_showSettingsMenu = true;
  }
};

class $modify(ReplayBuffer_LevelInfoLayer, LevelInfoLayer) {
  bool init(GJGameLevel *level, bool challenge) {
    if (!LevelInfoLayer::init(level, challenge)) {
      return false;
    }

    auto *menu = this->getChildByID("left-side-menu");
    auto *button = CCMenuItemSpriteExtra::create(
      CircleButtonSprite::createWithSprite("icon.png"_spr, 0.8),
      this,
      menu_selector(ReplayBuffer_LevelInfoLayer::onSettingsButton)
      );
    menu->addChild(button);
    menu->updateLayout();

    return true;
  }

  void onSettingsButton(CCObject *) {
    g_showSettingsMenu = true;
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
    auto *button = CCMenuItemSpriteExtra::create(
      CircleButtonSprite::createWithSprite("icon.png"_spr, 0.5),
      this,
      menu_selector(ReplayBuffer_EndLevelLayer::onSettingsButton)
      );
    button->setPosition(150, -90);
    menu->addChild(button);
  }

  void onSettingsButton(CCObject *) {
    g_showSettingsMenu = true;
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
    Mod::get()->setSavedValue<int>("settings-audio-amt"_spr, 2);
    Mod::get()->setSavedValue<std::string>("settings-output-dir"_spr, "please select an output folder");
  }

  Mod::get()->setSavedValue<bool>("is-recording"_spr, false);

  static std::vector<int> settingsValues;
  static std::vector<int> audioTracks;
  static std::array<char, 256> outputDir;
  static bool isUsingGPU;
  static auto deviceList = AudioEncoder::getDeviceList();
  static std::string errorString, clipPath;
  static std::vector<const char *> deviceListCStr;
  ImGuiCocos::get().setup([] {
    deviceListCStr.reserve(deviceList.size());
    for (const auto &deviceName : deviceList) {
      deviceListCStr.push_back(deviceName.c_str());
    }
    settingsValues.resize(6);
    int audioTrackAmount = Mod::get()->getSavedValue<int>("settings-audio-amt"_spr);
    audioTracks.resize(audioTrackAmount);
    settingsValues[0] = Mod::get()->getSavedValue<int>("settings-width"_spr);
    settingsValues[1] = Mod::get()->getSavedValue<int>("settings-height"_spr);
    settingsValues[2] = Mod::get()->getSavedValue<int>("settings-framerate"_spr);
    settingsValues[3] = Mod::get()->getSavedValue<int>("settings-bitrate"_spr);
    settingsValues[4] = Mod::get()->getSavedValue<int>("settings-length"_spr);
    settingsValues[5] = audioTrackAmount;
    for (int i = 1; i <= audioTrackAmount; i++) {
      audioTracks[i - 1] = Mod::get()->getSavedValue<int>("settings-audio-id-"_spr + std::to_string(i));
    }
    isUsingGPU = Mod::get()->getSavedValue<bool>("settings-hw-accel"_spr);

    std::string outputDirSetting = Mod::get()->getSavedValue<std::string>("settings-output-dir"_spr);
    outputDir.fill(0);
    std::strncpy(outputDir.data(), outputDirSetting.c_str(), 256);

    auto &io = ImGui::GetIO();
    auto *font = io.Fonts->AddFontFromFileTTF((Mod::get()->getResourcesDir() / "inter.ttf").string().c_str(), 16.0f);
    io.FontDefault = font;
  }).draw([&] {
    if (g_showSettingsMenu) {
      ImGui::Begin("replay buffer settings", &g_showSettingsMenu);

      ImGui::InputInt("width", &settingsValues[0], 0);
      ImGui::InputInt("height", &settingsValues[1], 0);
      ImGui::InputInt("framerate", &settingsValues[2], 0);
      ImGui::InputInt("bitrate (kbps)", &settingsValues[3], 0);
      ImGui::InputInt("length (seconds)", &settingsValues[4], 0);
      ImGui::Checkbox("hardware acceleration", &isUsingGPU);
      ImGui::BeginDisabled(true);
      //ImGui::InputInt("audio track count (not implemented yet)", &settingsValues[5]);
      ImGui::EndDisabled();
      for (int i = 1; i <= Mod::get()->getSavedValue<int>("settings-audio-amt"_spr); i++) {
        ImGui::Combo(("audio track " + std::to_string(i)).c_str(), &audioTracks[i - 1], deviceListCStr.data(), deviceList.size());
      }
      ImGui::InputText("", outputDir.data(), 256);
      ImGui::SameLine();
      if (ImGui::Button("select folder")) {
        std::string outputDirStr = outputDir.data();
        std::optional<std::filesystem::path> outputPath;
        if (std::filesystem::exists(outputDirStr))
          outputPath = std::filesystem::path(outputDirStr);
        file::pick(file::PickMode::OpenFolder, { outputPath, {} }).listen([] (Result<std::filesystem::path> *event){
          if (event->isOk()) {
            const auto& newPath = event->unwrap();
            outputDir.fill(0);
            std::strncpy(outputDir.data(), newPath.string().c_str(), 256);
          }
        });
      }
      ImGui::SameLine();
      ImGui::Text("output folder");

      bool isRecording = Mod::get()->getSavedValue<bool>("is-recording"_spr);
      if (isRecording) {
        ImGui::BeginDisabled(true);
        ImGui::Button("save");
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("stop recording")) {
          Recorder::getInstance()->stop();
        }
        ImGui::SameLine();
        if (ImGui::Button("clip")) {
          auto result = Recorder::getInstance()->clip();
          if (result.isErr()) {
            errorString = result.unwrapErr();
            ImGui::OpenPopup("error");
          } else {
            clipPath = result.unwrap();
            ImGui::OpenPopup("success");
          }
        }
      } else {
        if (ImGui::Button("save")) {
          int audioTrackAmount = Mod::get()->getSavedValue<int>("settings-audio-amt"_spr);
          int parsedTrackAmount = settingsValues[5];
          if (parsedTrackAmount != audioTrackAmount) {
            audioTracks.resize(parsedTrackAmount);
            if (parsedTrackAmount > audioTrackAmount) {
              for (int i = audioTrackAmount; i <= parsedTrackAmount; i++) {
                audioTracks[i - 1] = 0;
              }
            }
            audioTrackAmount = parsedTrackAmount;
          }
          Mod::get()->setSavedValue<int>("settings-width"_spr, settingsValues[0]);
          Mod::get()->setSavedValue<int>("settings-height"_spr, settingsValues[1]);
          Mod::get()->setSavedValue<int>("settings-framerate"_spr, settingsValues[2]);
          Mod::get()->setSavedValue<bool>("settings-hw-accel"_spr, isUsingGPU);
          Mod::get()->setSavedValue<int>("settings-bitrate"_spr, settingsValues[3]);
          Mod::get()->setSavedValue<int>("settings-length"_spr, settingsValues[4]);
          Mod::get()->setSavedValue<int>("settings-audio-amt"_spr, audioTrackAmount);
          for (int i = 1; i <= audioTrackAmount; i++) {
            Mod::get()->setSavedValue<int>("settings-audio-id-"_spr + std::to_string(i), audioTracks[i - 1]);
          }
        }
        ImGui::SameLine();
        if (ImGui::Button("start recording")) {
          auto result = Recorder::getInstance()->start();
          if (result.isErr()) {
            errorString = result.unwrapErr();
            ImGui::OpenPopup("error");
          }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(true);
        ImGui::Button("clip");
        ImGui::EndDisabled();
      }

      if (ImGui::BeginPopupModal("error")) {
        ImGui::Text("%s", errorString.c_str());
        ImGui::Separator();
        if (ImGui::Button("ok")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      if (ImGui::BeginPopupModal("success")) {
        ImGui::Text("clip saved at %s", clipPath.c_str());
        ImGui::Separator();
        if (ImGui::Button("ok")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      ImGui::End();
    }
  });
}
