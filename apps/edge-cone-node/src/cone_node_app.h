#pragma once

#include "config_server.h"
#include "cone_node_settings.h"
#include "cone_device/pwm_servo.h"
#include "cone_device/warning_light.h"
#include "telemetry_uploader.h"

#include <cstdint>
#include <string>

class ConeNodeApp {
 public:
  void setup();
  void tick();

 private:
  void configure_ultrasonic();
  bool configure_pan_servos();
  void tick_pan();
  void tick_warning_automation();
  bool apply_pan_heading(uint16_t heading_deg);
  bool apply_warning_level(WarningAutomationLevel level);
  std::string handle_pan_mode_command(PanMode mode);
  std::string handle_pan_heading_command(uint16_t heading_deg);
  std::string handle_warning_automation_command(bool enabled);
  std::string pan_status_json() const;
  std::string warning_automation_status_json() const;
  void publish_telemetry();
  void publish_image();
  std::string handle_warning_light_command(
      cone_device::WarningLightTarget target,
      cone_device::WarningLightAction action);
  std::string status_json() const;

  bool initialized_ = false;
  uint32_t last_publish_ms_ = 0;
  uint32_t last_image_publish_ms_ = 0;
  uint32_t last_pan_step_ms_ = 0;
  uint32_t last_manual_control_ms_ = 0;
  uint32_t warning_manual_pause_until_ms_ = 0;
  uint16_t pan_heading_deg_ = 0;
  uint16_t pan_base_angle_deg_ = 0;
  uint16_t pan_top_angle_deg_ = 0;
  int8_t pan_roam_direction_ = 1;
  PanMode pan_mode_ = PanMode::kRoam;
  WarningAutomationLevel warning_automation_level_ =
      WarningAutomationLevel::kUnknown;
  float warning_nearest_distance_m_ = 0.0f;
  int warning_nearest_channel_ = -1;
  bool warning_automation_enabled_ = true;
  std::string pan_last_error_;
  std::string warning_automation_last_error_;
  bool image_upload_in_progress_ = false;
  ConeNodeSettings settings_;
  TelemetryUploader uploader_;
  ConfigServer config_server_;
  std::string latest_image_url_;
};
