#include "cone_node_settings.h"

#include "cone_node_bsp.h"

#include <Arduino.h>
#include <Preferences.h>

namespace {
constexpr const char* kPrefsNamespace = "cone-node";
constexpr const char* kSceneKey = "scene";
constexpr const char* kMaskKey = "us-mask";
constexpr const char* kPanHeadingKey = "pan-head";
constexpr const char* kRevisionKey = "rev";

uint8_t enabled_mask(const ConeNodeSettings& settings) {
  uint8_t mask = 0;
  for (size_t i = 0; i < cone_device::kUltrasonicChannelCount; ++i) {
    if (settings.ultrasonic_enabled[i]) {
      mask |= static_cast<uint8_t>(1U << i);
    }
  }
  return mask;
}

void apply_mask(ConeNodeSettings& settings, uint8_t mask) {
  for (size_t i = 0; i < cone_device::kUltrasonicChannelCount; ++i) {
    settings.ultrasonic_enabled[i] = (mask & (1U << i)) != 0;
  }
}

void append_bool(std::string& out, bool value) {
  out += value ? "true" : "false";
}
}  // namespace

const char* scene_to_string(UltrasonicScene scene) {
  switch (scene) {
    case UltrasonicScene::kOneWay:
      return "one_way";
    case UltrasonicScene::kWorkZone:
      return "work_zone";
    case UltrasonicScene::kIntersection:
      return "intersection";
    case UltrasonicScene::kCustom:
      return "custom";
  }
  return "custom";
}

UltrasonicScene scene_from_string(const std::string& value) {
  if (value == "one_way") {
    return UltrasonicScene::kOneWay;
  }
  if (value == "work_zone") {
    return UltrasonicScene::kWorkZone;
  }
  if (value == "intersection") {
    return UltrasonicScene::kIntersection;
  }
  return UltrasonicScene::kCustom;
}

const char* pan_mode_to_string(PanMode mode) {
  switch (mode) {
    case PanMode::kRoam:
      return "roam";
    case PanMode::kManual:
      return "manual";
  }
  return "roam";
}

bool pan_mode_from_string(const std::string& value, PanMode& mode) {
  if (value == "roam") {
    mode = PanMode::kRoam;
    return true;
  }
  if (value == "manual") {
    mode = PanMode::kManual;
    return true;
  }
  return false;
}

const char* warning_automation_level_to_string(WarningAutomationLevel level) {
  switch (level) {
    case WarningAutomationLevel::kUnknown:
      return "unknown";
    case WarningAutomationLevel::kSafe:
      return "safe";
    case WarningAutomationLevel::kNotice:
      return "notice";
    case WarningAutomationLevel::kWarning:
      return "warning";
    case WarningAutomationLevel::kCritical:
      return "critical";
  }
  return "unknown";
}

ConeNodeSettings default_settings() {
  ConeNodeSettings settings;
  apply_scene_template(settings, UltrasonicScene::kIntersection);
  settings.pan_heading_deg = 0;
  settings.revision = 1;
  settings.config_changed = false;
  return settings;
}

bool load_settings(ConeNodeSettings& settings) {
  settings = default_settings();
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, true)) {
    return false;
  }

  const uint8_t scene_value = prefs.getUChar(kSceneKey, static_cast<uint8_t>(settings.scene));
  settings.scene = static_cast<UltrasonicScene>(scene_value);
  apply_mask(settings, prefs.getUChar(kMaskKey, enabled_mask(settings)));
  settings.pan_heading_deg = prefs.getUShort(kPanHeadingKey, settings.pan_heading_deg);
  if (settings.pan_heading_deg > 360) {
    settings.pan_heading_deg = 0;
  }
  settings.revision = prefs.getUInt(kRevisionKey, settings.revision);
  settings.config_changed = false;
  prefs.end();
  return true;
}

bool save_settings(const ConeNodeSettings& settings) {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    return false;
  }
  prefs.putUChar(kSceneKey, static_cast<uint8_t>(settings.scene));
  prefs.putUChar(kMaskKey, enabled_mask(settings));
  prefs.putUShort(kPanHeadingKey, settings.pan_heading_deg);
  prefs.putUInt(kRevisionKey, settings.revision);
  prefs.end();
  return true;
}

void apply_scene_template(ConeNodeSettings& settings, UltrasonicScene scene) {
  settings.scene = scene;
  for (size_t i = 0; i < cone_device::kUltrasonicChannelCount; ++i) {
    settings.ultrasonic_enabled[i] = false;
  }

  switch (scene) {
    case UltrasonicScene::kOneWay:
      settings.ultrasonic_enabled[0] = true;
      break;
    case UltrasonicScene::kWorkZone:
      settings.ultrasonic_enabled[0] = true;
      settings.ultrasonic_enabled[1] = true;
      break;
    case UltrasonicScene::kIntersection:
      for (size_t i = 0; i < cone_device::kUltrasonicChannelCount; ++i) {
        settings.ultrasonic_enabled[i] = true;
      }
      break;
    case UltrasonicScene::kCustom:
      break;
  }
}

std::string settings_json(const ConeNodeSettings& settings) {
  std::string out = "{";
  out += "\"scene\":\"";
  out += scene_to_string(settings.scene);
  out += "\",\"revision\":";
  out += std::to_string(settings.revision);
  out += ",\"directions\":{";
  for (size_t i = 0; i < cone_device::kUltrasonicChannelCount; ++i) {
    if (i > 0) {
      out += ",";
    }
    out += "\"";
    out += cone_node_bsp::kDirectionNames[i];
    out += "\":";
    append_bool(out, settings.ultrasonic_enabled[i]);
  }
  out += "},\"pan\":{\"heading_deg\":";
  out += std::to_string(settings.pan_heading_deg);
  out += "}}";
  return out;
}

std::string telemetry_config_json(const ConeNodeSettings& settings) {
  std::string out = "\"ultrasonic_config\":";
  out += settings_json(settings);
  if (settings.config_changed) {
    out += ",\"config_event\":\"updated\"";
  }
  return out;
}
