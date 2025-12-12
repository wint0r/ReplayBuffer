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

void SetupImGuiStyle();

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

  static int outputWidth, outputHeight, outputFramerate, outputBitrate, outputLength, outputTrackCount;
  static std::vector<int> audioTracks;
  static std::array<char, 256> outputDir;
  static bool isUsingGPU;
  static std::vector<std::string> deviceList;
  static std::string errorString, clipPath;
  static std::vector<const char *> deviceListCStr;
  ImGuiCocos::get().setup([] {
    deviceList = AudioEncoder::getDeviceList();
    deviceListCStr.reserve(deviceList.size());
    for (const auto &deviceName : deviceList) {
      deviceListCStr.push_back(deviceName.c_str());
    }
    int audioTrackAmount = Mod::get()->getSavedValue<int>("settings-audio-amt"_spr);
    audioTracks.resize(audioTrackAmount);
    outputWidth = Mod::get()->getSavedValue<int>("settings-width"_spr);
    outputHeight = Mod::get()->getSavedValue<int>("settings-height"_spr);
    outputFramerate = Mod::get()->getSavedValue<int>("settings-framerate"_spr);
    outputBitrate = Mod::get()->getSavedValue<int>("settings-bitrate"_spr);
    outputLength = Mod::get()->getSavedValue<int>("settings-length"_spr);
    outputTrackCount = audioTrackAmount;
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

    SetupImGuiStyle();
  }).draw([&] {
    if (g_showSettingsMenu) {
      ImGui::Begin("replay buffer settings", &g_showSettingsMenu, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

      ImGui::PushItemWidth(200);
      ImGui::InputInt("width", &outputWidth, 0);
      ImGui::InputInt("height", &outputHeight, 0);
      ImGui::InputInt("framerate", &outputFramerate, 0);
      ImGui::InputInt("bitrate (kbps)", &outputBitrate, 0);
      ImGui::InputInt("length (seconds)", &outputLength, 0);
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
      ImGui::PopItemWidth();

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
          if (outputTrackCount != audioTrackAmount) {
            audioTracks.resize(outputTrackCount);
            if (outputTrackCount > audioTrackAmount) {
              for (int i = audioTrackAmount; i <= outputTrackCount; i++) {
                audioTracks[i - 1] = 0;
              }
            }
            audioTrackAmount = outputTrackCount;
          }
          Mod::get()->setSavedValue<int>("settings-width"_spr, outputWidth);
          Mod::get()->setSavedValue<int>("settings-height"_spr, outputHeight);
          Mod::get()->setSavedValue<int>("settings-framerate"_spr, outputFramerate);
          Mod::get()->setSavedValue<bool>("settings-hw-accel"_spr, isUsingGPU);
          Mod::get()->setSavedValue<int>("settings-bitrate"_spr, outputBitrate);
          Mod::get()->setSavedValue<int>("settings-length"_spr, outputLength);
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

void SetupImGuiStyle() {
	// Moonlight style by Madam-Herta from ImThemes
	ImGuiStyle &style = ImGui::GetStyle();

	style.Alpha = 1.0f;
	style.DisabledAlpha = 0.5f;
	style.WindowPadding = ImVec2(12.0f, 12.0f);
	style.WindowRounding = 11.5f;
	style.WindowBorderSize = 0.0f;
	style.WindowMinSize = ImVec2(20.0f, 20.0f);
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style.WindowMenuButtonPosition = ImGuiDir_Right;
	style.ChildRounding = 0.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupRounding = 0.0f;
	style.PopupBorderSize = 1.0f;
	style.FramePadding = ImVec2(20.0f, 3.4f);
	style.FrameRounding = 11.9f;
	style.FrameBorderSize = 0.0f;
	style.ItemSpacing = ImVec2(4.3f, 5.5f);
	style.ItemInnerSpacing = ImVec2(7.1f, 1.8f);
	style.CellPadding = ImVec2(12.1f, 9.2f);
	style.IndentSpacing = 0.0f;
	style.ColumnsMinSpacing = 4.9f;
	style.ScrollbarSize = 11.6f;
	style.ScrollbarRounding = 15.9f;
	style.GrabMinSize = 3.7f;
	style.GrabRounding = 20.0f;
	style.TabRounding = 0.0f;
	style.TabBorderSize = 0.0f;
	style.ColorButtonPosition = ImGuiDir_Right;
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

	style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.27450982f, 0.31764707f, 0.4509804f, 1.0f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.09411765f, 0.101960786f, 0.11764706f, 1.0f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.11372549f, 0.1254902f, 0.15294118f, 1.0f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.09803922f, 0.105882354f, 0.12156863f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.972549f, 1.0f, 0.49803922f, 1.0f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.972549f, 1.0f, 0.49803922f, 1.0f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.79607844f, 0.49803922f, 1.0f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.18039216f, 0.1882353f, 0.19607843f, 1.0f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.15294118f, 0.15294118f, 0.15294118f, 1.0f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.14117648f, 0.16470589f, 0.20784314f, 1.0f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.105882354f, 0.105882354f, 0.105882354f, 1.0f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
	style.Colors[ImGuiCol_Separator] = ImVec4(0.12941177f, 0.14901961f, 0.19215687f, 1.0f);
	style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.15686275f, 0.18431373f, 0.2509804f, 1.0f);
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.15686275f, 0.18431373f, 0.2509804f, 1.0f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.14509805f, 0.14509805f, 0.14509805f, 1.0f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.972549f, 1.0f, 0.49803922f, 1.0f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_Tab] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.1254902f, 0.27450982f, 0.57254905f, 1.0f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.52156866f, 0.6f, 0.7019608f, 1.0f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.039215688f, 0.98039216f, 0.98039216f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.88235295f, 0.79607844f, 0.56078434f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.95686275f, 0.95686275f, 0.95686275f, 1.0f);
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.09803922f, 0.105882354f, 0.12156863f, 1.0f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.9372549f, 0.9372549f, 0.9372549f, 1.0f);
	style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.49803922f, 0.5137255f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.26666668f, 0.2901961f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.49803922f, 0.5137255f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.19607843f, 0.1764706f, 0.54509807f, 0.5019608f);
	style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.19607843f, 0.1764706f, 0.54509807f, 0.5019608f);
}
