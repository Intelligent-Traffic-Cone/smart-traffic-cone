#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class TelemetryUploader {
 public:
  bool setup();
  bool upload(const std::string& payload);
  bool upload_image(const uint8_t* data, size_t size, std::string& image_url);
  void tick();

  const char* status() const;
  uint32_t consecutive_failures() const;
  std::string status_json() const;

 private:
  bool ensure_wifi_connected();
  std::string endpoint(const char* suffix) const;
  bool parse_image_url(const std::string& payload, std::string& image_url) const;

  uint32_t consecutive_failures_ = 0;
  uint32_t last_wifi_attempt_ms_ = 0;
  bool wifi_started_ = false;
  bool wifi_ip_logged_ = false;
};
