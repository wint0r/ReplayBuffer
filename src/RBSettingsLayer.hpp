#ifndef REPLAYBUFFER_RBSETTINGSLAYER_HPP
#define REPLAYBUFFER_RBSETTINGSLAYER_HPP

#include <Geode/Geode.hpp>

class RBSettingsLayer : public geode::Popup<> {
  bool setup() override;

  void onRecordButton(CCObject *sender);
  void onHWAccelToggle(CCObject *sender);
  void onSaveSettings(CCObject *sender);
  void onDismiss(CCObject *sender);
  void onInfoIcon(CCObject *sender);

public:
  static RBSettingsLayer *create();
};

#endif