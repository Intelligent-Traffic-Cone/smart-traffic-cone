#pragma once

#include <cstdint>
#include <string>

namespace cone_device {

struct GpsModuleConfig {
  int uart_port = 1;
  int tx_pin = 17;
  int rx_pin = 18;
  uint32_t baud_rate = 115200;
  uint32_t stale_after_ms = 5000;
};

struct GpsStatus {
  bool enabled = false;
  bool initialized = false;
  bool has_fix = false;
  double longitude = 0.0;
  double latitude = 0.0;
  float accuracy_m = 0.0f;
  uint32_t last_fix_age_ms = 0;
  std::string last_error;
};

bool setup_gps(const GpsModuleConfig& config);
void tick_gps();
void deinit_gps();
GpsStatus gps_status();

inline double get_lat() { return gps_status().latitude; }
inline double get_lon() { return gps_status().longitude; }
inline bool data_valid() { return gps_status().has_fix; }

}  // namespace cone_device
