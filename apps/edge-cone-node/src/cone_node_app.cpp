#include "cone_node_app.h"

#include "cone_device/camera_module.h"
#include "cone_device/gps_module.h"
#include "cone_device/telemetry_encoder.h"
#include "cone_device/ultrasonic_array.h"

#include <Arduino.h>

#ifndef CONE_NODE_ID
#define CONE_NODE_ID "cone-demo-001"
#endif

#ifndef CONE_NODE_TELEMETRY_INTERVAL_MS
#define CONE_NODE_TELEMETRY_INTERVAL_MS 3000
#endif

namespace {
constexpr const char* kTag = "cone_node_app";
}  // namespace

void ConeNodeApp::setup() {
  cone_device::GpsModuleConfig gps_config;
  cone_device::UltrasonicArrayConfig ultrasonic_config;
  cone_device::CameraModuleConfig camera_config;

  const bool gps_ok = cone_device::setup_gps(gps_config);
  const bool ultrasonic_ok = cone_device::setup_ultrasonic_array(ultrasonic_config);
  const bool camera_ok = cone_device::setup_camera(camera_config);

  initialized_ = gps_ok || ultrasonic_ok || camera_ok;
  Serial.printf("[%s] hardware setup complete gps=%d ultrasonic=%d camera=%d\n",
                kTag,
                gps_ok,
                ultrasonic_ok,
                camera_ok);
}

void ConeNodeApp::tick() {
  if (!initialized_) {
    return;
  }

  cone_device::tick_gps();
  cone_device::tick_ultrasonic_array();
  cone_device::tick_camera();

  const uint32_t now = ::millis();
  if (now - last_publish_ms_ >= CONE_NODE_TELEMETRY_INTERVAL_MS) {
    last_publish_ms_ = now;
    publish_telemetry();
  }
}

void ConeNodeApp::publish_telemetry() {
  cone_device::TelemetrySnapshot snapshot;
  snapshot.cone_id = CONE_NODE_ID;
  snapshot.uptime_ms = ::millis();
  snapshot.gps = cone_device::gps_status();
  snapshot.ultrasonic = cone_device::ultrasonic_array_status();
  snapshot.camera = cone_device::camera_status();

  const std::string payload = cone_device::encode_telemetry_json(snapshot);

  // Network upload is intentionally a later adapter. Keep this app-level
  // boundary stable while Wi-Fi/MQTT/HTTP implementation is assigned.
  Serial.printf("[%s] telemetry ready: %s\n", kTag, payload.c_str());
}
