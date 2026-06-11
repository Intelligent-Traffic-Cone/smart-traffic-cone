#pragma once

#include "cone_device/warning_light.h"
#include "cone_node_settings.h"

#include <cstdint>
#include <functional>
#include <string>

class ConfigServer {
 public:
  using SettingsChangedCallback = std::function<void(const ConeNodeSettings&)>;
  using StatusProvider = std::function<std::string()>;
  using WarningLightCommandHandler =
      std::function<std::string(cone_device::WarningLightTarget,
                                cone_device::WarningLightAction)>;
  using PanStatusProvider = std::function<std::string()>;
  using PanModeCommandHandler = std::function<std::string(PanMode)>;
  using PanHeadingCommandHandler = std::function<std::string(uint16_t)>;
  using WarningAutomationCommandHandler = std::function<std::string(bool)>;

  void setup(ConeNodeSettings* settings,
             SettingsChangedCallback on_settings_changed,
             StatusProvider status_provider,
             WarningLightCommandHandler warning_light_handler,
             PanStatusProvider pan_status_provider,
             PanModeCommandHandler pan_mode_handler,
             PanHeadingCommandHandler pan_heading_handler,
             WarningAutomationCommandHandler warning_automation_handler);
  void tick();

 private:
  ConeNodeSettings* settings_ = nullptr;
  SettingsChangedCallback on_settings_changed_;
  StatusProvider status_provider_;
  WarningLightCommandHandler warning_light_handler_;
  PanStatusProvider pan_status_provider_;
  PanModeCommandHandler pan_mode_handler_;
  PanHeadingCommandHandler pan_heading_handler_;
  WarningAutomationCommandHandler warning_automation_handler_;
};
