#include "telemetry_uploader.h"

#include <Arduino.h>

namespace {
constexpr const char* kTag = "telemetry_uploader";
}

bool TelemetryUploader::setup() {
  consecutive_failures_ = 0;
  Serial.printf("[%s] HTTP upload adapter not configured; using serial output\n", kTag);
  return true;
}

bool TelemetryUploader::upload(const std::string& payload) {
  Serial.printf("[%s] telemetry ready: %s\n", kTag, payload.c_str());
  return true;
}

const char* TelemetryUploader::status() const {
  return consecutive_failures_ == 0 ? "serial_only" : "upload_degraded";
}

uint32_t TelemetryUploader::consecutive_failures() const {
  return consecutive_failures_;
}
