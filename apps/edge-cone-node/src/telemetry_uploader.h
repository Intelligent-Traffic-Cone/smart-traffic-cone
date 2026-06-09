#pragma once

#include <cstdint>
#include <string>

class TelemetryUploader {
 public:
  bool setup();
  bool upload(const std::string& payload);

  const char* status() const;
  uint32_t consecutive_failures() const;

 private:
  uint32_t consecutive_failures_ = 0;
};
