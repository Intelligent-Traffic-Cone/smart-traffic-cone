#pragma once

#include "cone_device/camera_module.h"
#include "cone_device/gps_module.h"
#include "cone_device/ultrasonic_array.h"

#include <cstdint>
#include <string>

namespace cone_device {

struct TelemetrySnapshot {
  std::string cone_id;
  uint32_t uptime_ms = 0;
  GpsStatus gps;
  UltrasonicArrayStatus ultrasonic;
  CameraStatus camera;
  std::string network_status = "unknown";
  uint32_t upload_failure_count = 0;
};

std::string encode_telemetry_json(const TelemetrySnapshot& snapshot);

}  // namespace cone_device
