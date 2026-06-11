#pragma once

#include "cone_device/ultrasonic_array.h"

#include <cstdint>
#include <string>

enum class UltrasonicScene : uint8_t {
  kOneWay = 0,
  kWorkZone = 1,
  kIntersection = 2,
  kCustom = 3,
};

enum class PanMode : uint8_t {
  kRoam = 0,
  kManual = 1,
};

enum class WarningAutomationLevel : uint8_t {
  kUnknown = 0,
  kSafe = 1,
  kNotice = 2,
  kWarning = 3,
  kCritical = 4,
};

struct ConeNodeSettings {
  UltrasonicScene scene = UltrasonicScene::kIntersection;
  bool ultrasonic_enabled[cone_device::kUltrasonicChannelCount] = {
      true,
      true,
      true,
      true,
  };
  uint16_t pan_heading_deg = 0;
  uint32_t revision = 1;
  bool config_changed = false;
};

const char* scene_to_string(UltrasonicScene scene);
UltrasonicScene scene_from_string(const std::string& value);
const char* pan_mode_to_string(PanMode mode);
bool pan_mode_from_string(const std::string& value, PanMode& mode);
const char* warning_automation_level_to_string(WarningAutomationLevel level);
ConeNodeSettings default_settings();
bool load_settings(ConeNodeSettings& settings);
bool save_settings(const ConeNodeSettings& settings);
void apply_scene_template(ConeNodeSettings& settings, UltrasonicScene scene);
std::string settings_json(const ConeNodeSettings& settings);
std::string telemetry_config_json(const ConeNodeSettings& settings);
