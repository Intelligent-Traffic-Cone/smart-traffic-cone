#pragma once

#include "telemetry_uploader.h"

#include <cstdint>

class ConeNodeApp {
 public:
  void setup();
  void tick();

 private:
  void publish_telemetry();

  bool initialized_ = false;
  uint32_t last_publish_ms_ = 0;
  TelemetryUploader uploader_;
};
