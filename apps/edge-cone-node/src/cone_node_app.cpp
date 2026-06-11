#include "cone_node_app.h"

#include "cone_device/camera_module.h"
#include "cone_device/gps_module.h"
#include "cone_device/pwm_servo.h"
#include "cone_device/telemetry_encoder.h"
#include "cone_device/ultrasonic_array.h"
#include "cone_device/warning_light.h"
#include "cone_node_bsp.h"

#include <Arduino.h>

#if __has_include("cone_node.local.h")
#include "cone_node.local.h"
#endif

#ifndef CONE_NODE_ID
#define CONE_NODE_ID "cone-demo-001"
#endif

#ifndef CONE_NODE_TELEMETRY_INTERVAL_MS
#define CONE_NODE_TELEMETRY_INTERVAL_MS 3000
#endif

#ifndef CONE_NODE_IMAGE_UPLOAD_INTERVAL_MS
#define CONE_NODE_IMAGE_UPLOAD_INTERVAL_MS 4000
#endif

#ifndef CONE_NODE_PAN_ROAM_STEP_INTERVAL_MS
#define CONE_NODE_PAN_ROAM_STEP_INTERVAL_MS 50
#endif

#ifndef CONE_NODE_PAN_MANUAL_TIMEOUT_MS
#define CONE_NODE_PAN_MANUAL_TIMEOUT_MS 60000
#endif

#ifndef CONE_NODE_WARNING_MANUAL_PAUSE_MS
#define CONE_NODE_WARNING_MANUAL_PAUSE_MS 30000
#endif

namespace {
constexpr const char* kTag = "cone_node_app";
constexpr uint16_t kPanMaxHeadingDeg = 360;
constexpr uint16_t kServoMaxAngleDeg = 180;
constexpr float kWarningNoticeDistanceM = 2.0f;
constexpr float kWarningWarningDistanceM = 1.0f;
constexpr float kWarningCriticalDistanceM = 0.5f;

struct ImageUploadContext {
  TelemetryUploader* uploader = nullptr;
  std::string image_url;
};

struct PanServoAngles {
  uint16_t base_angle_deg = 0;
  uint16_t top_angle_deg = 0;
};

bool upload_frame_writer(const uint8_t* data, size_t size, void* context) {
  auto* upload_context = static_cast<ImageUploadContext*>(context);
  if (upload_context == nullptr || upload_context->uploader == nullptr) {
    return false;
  }
  return upload_context->uploader->upload_image(
      data, size, upload_context->image_url);
}

std::string bool_json(bool value) {
  return value ? "true" : "false";
}

PanServoAngles pan_angles_for_heading(uint16_t heading_deg) {
  if (heading_deg > kPanMaxHeadingDeg) {
    heading_deg = kPanMaxHeadingDeg;
  }
  if (heading_deg <= kServoMaxAngleDeg) {
    return {heading_deg, 0};
  }
  return {kServoMaxAngleDeg,
          static_cast<uint16_t>(heading_deg - kServoMaxAngleDeg)};
}

WarningAutomationLevel warning_level_for_distance(bool has_distance,
                                                  float distance_m) {
  if (!has_distance || distance_m > kWarningNoticeDistanceM) {
    return WarningAutomationLevel::kSafe;
  }
  if (distance_m > kWarningWarningDistanceM) {
    return WarningAutomationLevel::kNotice;
  }
  if (distance_m > kWarningCriticalDistanceM) {
    return WarningAutomationLevel::kWarning;
  }
  return WarningAutomationLevel::kCritical;
}

cone_device::WarningLightTarget warning_target_for_level(
    WarningAutomationLevel level) {
  switch (level) {
    case WarningAutomationLevel::kNotice:
      return cone_device::WarningLightTarget::kYellow;
    case WarningAutomationLevel::kWarning:
      return cone_device::WarningLightTarget::kRed;
    case WarningAutomationLevel::kCritical:
      return cone_device::WarningLightTarget::kRedBuzzer;
    case WarningAutomationLevel::kSafe:
    case WarningAutomationLevel::kUnknown:
      return cone_device::WarningLightTarget::kGreen;
  }
  return cone_device::WarningLightTarget::kGreen;
}

cone_device::WarningLightAction warning_action_for_level(
    WarningAutomationLevel level) {
  switch (level) {
    case WarningAutomationLevel::kSafe:
      return cone_device::WarningLightAction::kOn;
    case WarningAutomationLevel::kNotice:
    case WarningAutomationLevel::kWarning:
    case WarningAutomationLevel::kCritical:
      return cone_device::WarningLightAction::kFlash;
    case WarningAutomationLevel::kUnknown:
      return cone_device::WarningLightAction::kOff;
  }
  return cone_device::WarningLightAction::kOff;
}
}  // namespace

void ConeNodeApp::setup() {
  load_settings(settings_);
  pan_mode_ = PanMode::kRoam;
  pan_heading_deg_ = settings_.pan_heading_deg;

  cone_device::CameraModuleConfig camera_config;
  camera_config.capture_interval_ms = CONE_NODE_IMAGE_UPLOAD_INTERVAL_MS;

  const bool gps_ok = cone_device::setup_gps(cone_node_bsp::gps_config());
  configure_ultrasonic();
  const bool ultrasonic_ok = cone_device::ultrasonic_array_status().initialized;
  const bool camera_ok = cone_device::setup_camera(camera_config);
  const bool warning_light_ok =
      cone_device::setup_warning_light(cone_node_bsp::warning_light_config());
  const bool pan_ok = configure_pan_servos();
  const bool uploader_ok = uploader_.setup();

  config_server_.setup(
      &settings_,
      [this](const ConeNodeSettings& next) {
        settings_ = next;
        configure_ultrasonic();
        publish_telemetry();
      },
      [this]() { return status_json(); },
      [this](cone_device::WarningLightTarget target,
             cone_device::WarningLightAction action) {
        return handle_warning_light_command(target, action);
      },
      [this]() { return pan_status_json(); },
      [this](PanMode mode) { return handle_pan_mode_command(mode); },
      [this](uint16_t heading_deg) {
        return handle_pan_heading_command(heading_deg);
      },
      [this](bool enabled) {
        return handle_warning_automation_command(enabled);
      });

  initialized_ = gps_ok || ultrasonic_ok || camera_ok || warning_light_ok ||
                 pan_ok || uploader_ok;
  Serial.printf("[%s] setup complete gps=%d ultrasonic=%d camera=%d warning_light=%d pan=%d uploader=%d\n",
                kTag,
                gps_ok,
                ultrasonic_ok,
                camera_ok,
                warning_light_ok,
                pan_ok,
                uploader_ok);
}

void ConeNodeApp::tick() {
  config_server_.tick();
  uploader_.tick();

  if (!initialized_) {
    return;
  }

  cone_device::tick_gps();
  cone_device::tick_ultrasonic_array();
  cone_device::tick_camera();
  cone_device::tick_warning_light();
  cone_device::tick_pwm_servos();
  tick_pan();
  tick_warning_automation();

  const uint32_t now = ::millis();
  if (now - last_image_publish_ms_ >= CONE_NODE_IMAGE_UPLOAD_INTERVAL_MS) {
    last_image_publish_ms_ = now;
    publish_image();
  }

  if (now - last_publish_ms_ >= CONE_NODE_TELEMETRY_INTERVAL_MS) {
    last_publish_ms_ = now;
    publish_telemetry();
  }
}

void ConeNodeApp::configure_ultrasonic() {
  cone_device::UltrasonicArrayConfig ultrasonic_config;
  ultrasonic_config.channels = cone_node_bsp::kUltrasonicChannels;
  for (size_t i = 0; i < cone_device::kUltrasonicChannelCount; ++i) {
    ultrasonic_config.channels[i].enabled = settings_.ultrasonic_enabled[i];
  }
  ultrasonic_config.sample_interval_ms = 500;
  ultrasonic_config.stale_after_ms = 2000;
  ultrasonic_config.filter_sample_count = 5;
  cone_device::setup_ultrasonic_array(ultrasonic_config);
}

bool ConeNodeApp::configure_pan_servos() {
  const PanServoAngles angles = pan_angles_for_heading(pan_heading_deg_);
  pan_base_angle_deg_ = angles.base_angle_deg;
  pan_top_angle_deg_ = angles.top_angle_deg;
  const bool ok = cone_device::setup_pwm_servos(
      cone_node_bsp::pan_servo_config(pan_base_angle_deg_, pan_top_angle_deg_));
  const auto status = cone_device::pwm_servos_status();
  if (!ok) {
    pan_last_error_ = status.last_error;
  } else {
    pan_last_error_.clear();
  }
  return ok;
}

void ConeNodeApp::tick_pan() {
  const uint32_t now = ::millis();
  if (pan_mode_ == PanMode::kManual) {
    if (last_manual_control_ms_ != 0 &&
        now - last_manual_control_ms_ >= CONE_NODE_PAN_MANUAL_TIMEOUT_MS) {
      pan_mode_ = PanMode::kRoam;
      pan_last_error_.clear();
      last_pan_step_ms_ = now;
    } else {
      return;
    }
  }

  if (pan_mode_ != PanMode::kRoam) {
    return;
  }
  if (last_pan_step_ms_ != 0 &&
      now - last_pan_step_ms_ < CONE_NODE_PAN_ROAM_STEP_INTERVAL_MS) {
    return;
  }
  last_pan_step_ms_ = now;

  int16_t next = static_cast<int16_t>(pan_heading_deg_) + pan_roam_direction_;
  if (next >= static_cast<int16_t>(kPanMaxHeadingDeg)) {
    next = kPanMaxHeadingDeg;
    pan_roam_direction_ = -1;
  } else if (next <= 0) {
    next = 0;
    pan_roam_direction_ = 1;
  }
  apply_pan_heading(static_cast<uint16_t>(next));
}

void ConeNodeApp::tick_warning_automation() {
  const uint32_t now = ::millis();
  if (!warning_automation_enabled_) {
    return;
  }
  if (warning_manual_pause_until_ms_ != 0) {
    const int32_t remaining =
        static_cast<int32_t>(warning_manual_pause_until_ms_ - now);
    if (remaining > 0) {
      return;
    }
    warning_manual_pause_until_ms_ = 0;
    warning_automation_level_ = WarningAutomationLevel::kUnknown;
  }

  bool has_enabled_channel = false;
  for (size_t i = 0; i < cone_device::kUltrasonicChannelCount; ++i) {
    if (settings_.ultrasonic_enabled[i]) {
      has_enabled_channel = true;
      break;
    }
  }
  if (!has_enabled_channel) {
    warning_nearest_distance_m_ = 0.0f;
    warning_nearest_channel_ = -1;
    if (warning_automation_level_ == WarningAutomationLevel::kSafe) {
      warning_automation_last_error_.clear();
      return;
    }
    if (apply_warning_level(WarningAutomationLevel::kSafe)) {
      warning_automation_level_ = WarningAutomationLevel::kSafe;
      warning_automation_last_error_.clear();
    }
    return;
  }

  const auto ultrasonic = cone_device::ultrasonic_array_status();
  if (!ultrasonic.initialized) {
    warning_automation_last_error_ = ultrasonic.last_error.empty()
                                         ? "ultrasonic_unavailable"
                                         : ultrasonic.last_error;
    return;
  }

  bool has_distance = false;
  float nearest_distance_m = 0.0f;
  int nearest_channel = -1;
  for (size_t i = 0; i < cone_device::kUltrasonicChannelCount; ++i) {
    if (!settings_.ultrasonic_enabled[i]) {
      continue;
    }
    const auto& channel = ultrasonic.channels[i];
    if (!channel.present || channel.timed_out) {
      continue;
    }
    if (!has_distance || channel.distance_m < nearest_distance_m) {
      has_distance = true;
      nearest_distance_m = channel.distance_m;
      nearest_channel = static_cast<int>(i);
    }
  }

  warning_nearest_distance_m_ = has_distance ? nearest_distance_m : 0.0f;
  warning_nearest_channel_ = nearest_channel;
  const WarningAutomationLevel next_level =
      warning_level_for_distance(has_distance, nearest_distance_m);
  if (next_level == warning_automation_level_) {
    warning_automation_last_error_.clear();
    return;
  }

  if (apply_warning_level(next_level)) {
    warning_automation_level_ = next_level;
    warning_automation_last_error_.clear();
  }
}

bool ConeNodeApp::apply_pan_heading(uint16_t heading_deg) {
  if (heading_deg > kPanMaxHeadingDeg) {
    pan_last_error_ = "invalid_heading";
    return false;
  }

  const PanServoAngles angles = pan_angles_for_heading(heading_deg);
  const bool base_ok = cone_device::pwm_servo_set_angle(
      0, angles.base_angle_deg);
  const bool top_ok = cone_device::pwm_servo_set_angle(
      1, angles.top_angle_deg);
  if (!base_ok || !top_ok) {
    const auto status = cone_device::pwm_servos_status();
    pan_last_error_ = status.last_error.empty() ? "servo_write_failed"
                                                : status.last_error;
    return false;
  }

  pan_heading_deg_ = heading_deg;
  pan_base_angle_deg_ = angles.base_angle_deg;
  pan_top_angle_deg_ = angles.top_angle_deg;
  pan_last_error_.clear();
  return true;
}

bool ConeNodeApp::apply_warning_level(WarningAutomationLevel level) {
  const auto status = cone_device::warning_light_status();
  if (!status.initialized) {
    warning_automation_last_error_ = status.last_error.empty()
                                         ? "warning_light_unavailable"
                                         : status.last_error;
    return false;
  }

  if (!cone_device::warning_light_off(cone_device::WarningLightTarget::kAll)) {
    const auto error_status = cone_device::warning_light_status();
    warning_automation_last_error_ = error_status.last_error.empty()
                                         ? "warning_light_off_failed"
                                         : error_status.last_error;
    return false;
  }
  if (!cone_device::warning_light_set(warning_target_for_level(level),
                                      warning_action_for_level(level))) {
    const auto error_status = cone_device::warning_light_status();
    warning_automation_last_error_ = error_status.last_error.empty()
                                         ? "warning_light_set_failed"
                                         : error_status.last_error;
    return false;
  }
  return true;
}

void ConeNodeApp::publish_telemetry() {
  cone_device::TelemetrySnapshot snapshot;
  snapshot.cone_id = CONE_NODE_ID;
  snapshot.uptime_ms = ::millis();
  snapshot.gps = cone_device::gps_status();
  snapshot.ultrasonic = cone_device::ultrasonic_array_status();
  snapshot.camera = cone_device::camera_status();
  snapshot.network_status = uploader_.status();
  snapshot.camera_image_url = latest_image_url_;
  snapshot.upload_failure_count = uploader_.consecutive_failures();
  snapshot.raw_extension_json = telemetry_config_json(settings_);

  const std::string payload = cone_device::encode_telemetry_json(snapshot);
  uploader_.upload(payload);
  settings_.config_changed = false;
}

void ConeNodeApp::publish_image() {
  if (image_upload_in_progress_) {
    return;
  }

  image_upload_in_progress_ = true;
  ImageUploadContext context;
  context.uploader = &uploader_;
  std::string error;
  if (cone_device::camera_capture_jpeg(upload_frame_writer, &context, error)) {
    latest_image_url_ = context.image_url;
    Serial.printf("[%s] image uploaded: %s\n", kTag, latest_image_url_.c_str());
  } else if (!error.empty()) {
    Serial.printf("[%s] image skipped: %s\n", kTag, error.c_str());
  }
  image_upload_in_progress_ = false;
}

std::string ConeNodeApp::handle_pan_mode_command(PanMode mode) {
  pan_mode_ = mode;
  if (mode == PanMode::kManual) {
    last_manual_control_ms_ = ::millis();
  } else {
    last_pan_step_ms_ = ::millis();
    if (pan_heading_deg_ == 0) {
      pan_roam_direction_ = 1;
    } else if (pan_heading_deg_ >= kPanMaxHeadingDeg) {
      pan_roam_direction_ = -1;
    }
  }
  pan_last_error_.clear();

  std::string out = "{\"ok\":true,\"pan\":";
  out += pan_status_json();
  out += "}";
  return out;
}

std::string ConeNodeApp::handle_pan_heading_command(uint16_t heading_deg) {
  if (pan_mode_ != PanMode::kManual) {
    pan_last_error_ = "manual_mode_required";
    std::string out = "{\"ok\":false,\"error\":\"";
    out += pan_last_error_;
    out += "\",\"pan\":";
    out += pan_status_json();
    out += "}";
    return out;
  }

  const bool ok = apply_pan_heading(heading_deg);
  if (ok) {
    last_manual_control_ms_ = ::millis();
    settings_.pan_heading_deg = heading_deg;
    settings_.revision += 1;
    settings_.config_changed = true;
    save_settings(settings_);
  }

  std::string out = "{\"ok\":";
  out += bool_json(ok);
  if (!ok) {
    out += ",\"error\":\"";
    out += pan_last_error_;
    out += "\"";
  }
  out += ",\"pan\":";
  out += pan_status_json();
  out += "}";
  return out;
}

std::string ConeNodeApp::handle_warning_automation_command(bool enabled) {
  warning_automation_enabled_ = enabled;
  warning_manual_pause_until_ms_ = 0;
  warning_automation_level_ = WarningAutomationLevel::kUnknown;
  warning_automation_last_error_.clear();
  if (enabled) {
    tick_warning_automation();
  }

  std::string out = "{\"ok\":true,\"warning_automation\":";
  out += warning_automation_status_json();
  out += "}";
  return out;
}

std::string ConeNodeApp::pan_status_json() const {
  const auto servo_status = cone_device::pwm_servos_status();
  std::string last_error = pan_last_error_;
  if (last_error.empty()) {
    last_error = servo_status.last_error;
  }

  std::string out = "{";
  out += "\"mode\":\"";
  out += pan_mode_to_string(pan_mode_);
  out += "\",\"heading_deg\":";
  out += std::to_string(pan_heading_deg_);
  out += ",\"base_angle_deg\":";
  out += std::to_string(pan_base_angle_deg_);
  out += ",\"top_angle_deg\":";
  out += std::to_string(pan_top_angle_deg_);
  out += ",\"roam_direction\":";
  out += std::to_string(pan_roam_direction_);
  out += ",\"servo_initialized\":";
  out += bool_json(servo_status.initialized);
  out += ",\"channels\":[";
  for (size_t i = 0; i < cone_device::kPwmServoChannelCount; ++i) {
    if (i > 0) {
      out += ",";
    }
    const auto& channel = servo_status.channels[i];
    out += "{\"index\":";
    out += std::to_string(i);
    out += ",\"enabled\":";
    out += bool_json(channel.enabled);
    out += ",\"attached\":";
    out += bool_json(channel.attached);
    out += ",\"pin\":";
    out += std::to_string(channel.pin);
    out += ",\"angle_deg\":";
    out += std::to_string(channel.angle_deg);
    out += ",\"last_error\":\"";
    out += channel.last_error;
    out += "\"}";
  }
  out += "],\"last_error\":\"";
  out += last_error;
  out += "\"}";
  return out;
}

std::string ConeNodeApp::warning_automation_status_json() const {
  const uint32_t now = ::millis();
  uint32_t pause_remaining_ms = 0;
  if (warning_manual_pause_until_ms_ != 0) {
    const int32_t remaining =
        static_cast<int32_t>(warning_manual_pause_until_ms_ - now);
    if (remaining > 0) {
      pause_remaining_ms = static_cast<uint32_t>(remaining);
    }
  }

  std::string out = "{";
  out += "\"enabled\":";
  out += bool_json(warning_automation_enabled_);
  out += ",\"paused\":";
  out += bool_json(pause_remaining_ms > 0);
  out += ",\"level\":\"";
  out += warning_automation_level_to_string(warning_automation_level_);
  out += "\",\"nearest_distance_m\":";
  out += warning_nearest_channel_ >= 0
             ? std::to_string(warning_nearest_distance_m_)
             : "null";
  out += ",\"nearest_direction\":";
  if (warning_nearest_channel_ >= 0 &&
      warning_nearest_channel_ <
          static_cast<int>(cone_device::kUltrasonicChannelCount)) {
    out += "\"";
    out += cone_node_bsp::kDirectionNames[warning_nearest_channel_];
    out += "\"";
  } else {
    out += "null";
  }
  out += ",\"active_channels\":[";
  bool wrote_channel = false;
  for (size_t i = 0; i < cone_device::kUltrasonicChannelCount; ++i) {
    if (!settings_.ultrasonic_enabled[i]) {
      continue;
    }
    if (wrote_channel) {
      out += ",";
    }
    out += "\"";
    out += cone_node_bsp::kDirectionNames[i];
    out += "\"";
    wrote_channel = true;
  }
  out += "],\"manual_pause_remaining_ms\":";
  out += std::to_string(pause_remaining_ms);
  out += ",\"last_error\":\"";
  out += warning_automation_last_error_;
  out += "\"}";
  return out;
}

std::string ConeNodeApp::handle_warning_light_command(
    cone_device::WarningLightTarget target,
    cone_device::WarningLightAction action) {
  const bool ok = cone_device::warning_light_set(target, action);
  if (ok) {
    warning_manual_pause_until_ms_ =
        ::millis() + CONE_NODE_WARNING_MANUAL_PAUSE_MS;
  }
  const auto status = cone_device::warning_light_status();
  std::string out = "{";
  out += "\"ok\":";
  out += bool_json(ok);
  out += ",\"frame\":\"";
  out += status.last_frame_hex;
  out += "\"";
  if (!ok) {
    out += ",\"error\":\"";
    out += status.last_error;
    out += "\"";
  }
  out += "}";
  return out;
}

std::string ConeNodeApp::status_json() const {
  const auto gps = cone_device::gps_status();
  const auto ultrasonic = cone_device::ultrasonic_array_status();
  const auto camera = cone_device::camera_status();
  const auto warning_light = cone_device::warning_light_status();
  std::string out = "{";
  out += "\"cone_id\":\"" CONE_NODE_ID "\",";
  out += "\"uptime_ms\":";
  out += std::to_string(::millis());
  out += ",\"network\":";
  out += uploader_.status_json();
  out += ",\"pan\":";
  out += pan_status_json();
  out += ",\"warning_automation\":";
  out += warning_automation_status_json();
  out += ",\"gps\":{\"initialized\":";
  out += bool_json(gps.initialized);
  out += ",\"has_fix\":";
  out += bool_json(gps.has_fix);
  out += ",\"latitude\":";
  out += gps.has_fix ? std::to_string(gps.latitude) : "null";
  out += ",\"longitude\":";
  out += gps.has_fix ? std::to_string(gps.longitude) : "null";
  out += "},\"camera\":{\"initialized\":";
  out += bool_json(camera.initialized);
  out += ",\"frame_available\":";
  out += bool_json(camera.frame_available);
  out += ",\"frame_count\":";
  out += std::to_string(camera.frame_count);
  out += ",\"latest_image_url\":\"";
  out += latest_image_url_;
  out += "\"},\"ultrasonic\":{\"initialized\":";
  out += bool_json(ultrasonic.initialized);
  out += ",\"channels\":[";
  for (size_t i = 0; i < cone_device::kUltrasonicChannelCount; ++i) {
    if (i > 0) {
      out += ",";
    }
    const auto& channel = ultrasonic.channels[i];
    out += "{\"direction\":\"";
    out += cone_node_bsp::kDirectionNames[i];
    out += "\",\"enabled\":";
    out += bool_json(settings_.ultrasonic_enabled[i]);
    out += ",\"present\":";
    out += bool_json(channel.present);
    out += ",\"timed_out\":";
    out += bool_json(channel.timed_out);
    out += ",\"distance_m\":";
    out += channel.present && !channel.timed_out ? std::to_string(channel.distance_m) : "null";
    out += "}";
  }
  out += "]},\"config\":";
  out += settings_json(settings_);
  out += ",\"warning_light\":{\"enabled\":";
  out += bool_json(warning_light.enabled);
  out += ",\"initialized\":";
  out += bool_json(warning_light.initialized);
  out += ",\"uart_port\":";
  out += std::to_string(warning_light.uart_port);
  out += ",\"tx_pin\":";
  out += std::to_string(warning_light.tx_pin);
  out += ",\"rx_pin\":";
  out += std::to_string(warning_light.rx_pin);
  out += ",\"baud_rate\":";
  out += std::to_string(warning_light.baud_rate);
  out += ",\"last_target\":\"";
  out += cone_device::warning_light_target_key(warning_light.last_target);
  out += "\",\"last_action\":\"";
  out += cone_device::warning_light_action_key(warning_light.last_action);
  out += "\",\"last_frame_hex\":\"";
  out += warning_light.last_frame_hex;
  out += "\",\"command_count\":";
  out += std::to_string(warning_light.command_count);
  out += ",\"last_error\":\"";
  out += warning_light.last_error;
  out += "\"}";
  out += "}";
  return out;
}
