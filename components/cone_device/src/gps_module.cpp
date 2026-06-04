#include "cone_device/gps_module.h"

#include <Arduino.h>

#include <cstdlib>
#include <cstring>
#include <new>

#ifndef CONE_DEVICE_ENABLE_GPS
#define CONE_DEVICE_ENABLE_GPS 1
#endif

namespace cone_device {
namespace {

constexpr size_t kLineBufSize = 128;

struct GpsState {
  GpsStatus status;
  HardwareSerial* serial = nullptr;
  uint32_t stale_after_ms = 0;
  uint32_t last_fix_ms = 0;
  char line_buffer[kLineBufSize];
  size_t line_pos = 0;
  bool discarding_line = false;
};

GpsState g_state;

const char* field_start(const char* line, int field_index) {
  int current = 0;
  while (*line) {
    if (current == field_index) return line;
    if (*line == ',') ++current;
    ++line;
  }
  return (current == field_index) ? line : nullptr;
}

bool parse_latitude(const char* field, const char* ns, double& out) {
  if (!field || !*field) return false;

  char* end = nullptr;
  const double value = strtod(field, &end);
  if (end == field) return false;

  const int degrees = static_cast<int>(value / 100.0);
  const double minutes = value - degrees * 100.0;
  out = degrees + minutes / 60.0;
  if (ns && *ns == 'S') out = -out;
  return true;
}

bool parse_longitude(const char* field, const char* ew, double& out) {
  if (!field || !*field) return false;

  char* end = nullptr;
  const double value = strtod(field, &end);
  if (end == field) return false;

  const int degrees = static_cast<int>(value / 100.0);
  const double minutes = value - degrees * 100.0;
  out = degrees + minutes / 60.0;
  if (ew && *ew == 'W') out = -out;
  return true;
}

bool is_supported_talker(const char* talker) {
  return std::strncmp(talker, "BD", 2) == 0 || std::strncmp(talker, "GP", 2) == 0;
}

void update_fix_age(uint32_t now) {
  if (g_state.last_fix_ms == 0) {
    g_state.status.last_fix_age_ms = 0;
    return;
  }

  const uint32_t age = now - g_state.last_fix_ms;
  g_state.status.last_fix_age_ms = age;
  if (g_state.status.has_fix && g_state.stale_after_ms > 0 &&
      age > g_state.stale_after_ms) {
    g_state.status.has_fix = false;
  }
}

void record_fix(double latitude, double longitude, uint32_t now) {
  g_state.status.latitude = latitude;
  g_state.status.longitude = longitude;
  g_state.status.has_fix = true;
  g_state.status.last_fix_age_ms = 0;
  g_state.status.last_error.clear();
  g_state.last_fix_ms = now;
}

void parse_rmc(const char* line) {
  const char* status_field = field_start(line, 2);
  const char* lat_field = field_start(line, 3);
  const char* ns_field = field_start(line, 4);
  const char* lon_field = field_start(line, 5);
  const char* ew_field = field_start(line, 6);

  if (!status_field) return;

  if (*status_field == 'V') {
    g_state.status.has_fix = false;
    update_fix_age(::millis());
    return;
  }
  if (*status_field != 'A') return;

  double latitude = 0.0;
  double longitude = 0.0;
  if (!parse_latitude(lat_field, ns_field, latitude)) return;
  if (!parse_longitude(lon_field, ew_field, longitude)) return;

  record_fix(latitude, longitude, ::millis());
}

void parse_gga(const char* line) {
  const char* lat_field = field_start(line, 2);
  const char* ns_field = field_start(line, 3);
  const char* lon_field = field_start(line, 4);
  const char* ew_field = field_start(line, 5);
  const char* quality_field = field_start(line, 6);

  if (!quality_field) return;

  const bool has_fix = *quality_field >= '1' && *quality_field <= '9';
  if (!has_fix) {
    g_state.status.has_fix = false;
    update_fix_age(::millis());
    return;
  }

  double latitude = 0.0;
  double longitude = 0.0;
  if (!parse_latitude(lat_field, ns_field, latitude)) return;
  if (!parse_longitude(lon_field, ew_field, longitude)) return;

  record_fix(latitude, longitude, ::millis());
}

void process_nmea_line(const char* line) {
  if (line[0] != '$' || std::strlen(line) < 6) return;

  const char* talker = line + 1;
  const char* type = line + 3;
  if (!is_supported_talker(talker)) return;

  if (std::strncmp(type, "RMC", 3) == 0) {
    parse_rmc(line);
  } else if (std::strncmp(type, "GGA", 3) == 0) {
    parse_gga(line);
  }
}

void feed_serial() {
  if (g_state.serial == nullptr) return;

  while (g_state.serial->available() > 0) {
    const char c = static_cast<char>(g_state.serial->read());

    if (g_state.discarding_line) {
      if (c == '\n') {
        g_state.discarding_line = false;
        g_state.line_pos = 0;
      }
      continue;
    }

    if (c == '\r') continue;

    if (c == '\n') {
      g_state.line_buffer[g_state.line_pos] = '\0';
      if (g_state.line_pos > 5) {
        process_nmea_line(g_state.line_buffer);
      }
      g_state.line_pos = 0;
      continue;
    }

    if (g_state.line_pos < kLineBufSize - 1) {
      g_state.line_buffer[g_state.line_pos++] = c;
    } else {
      g_state.line_pos = 0;
      g_state.discarding_line = true;
    }
  }
}

void release_serial() {
  if (g_state.serial == nullptr) return;
  g_state.serial->end();
  delete g_state.serial;
  g_state.serial = nullptr;
}
}

bool setup_gps(const GpsModuleConfig& config) {
#if CONE_DEVICE_ENABLE_GPS
  release_serial();
  g_state = {};
  g_state.status.enabled = true;
  g_state.stale_after_ms = config.stale_after_ms;

  if (config.uart_port < 0 || config.uart_port > 2) {
    g_state.status.last_error = "invalid uart_port";
    return false;
  }
  if (config.tx_pin < 0 || config.rx_pin < 0) {
    g_state.status.last_error = "tx_pin and rx_pin are required";
    return false;
  }

  g_state.serial = new (std::nothrow) HardwareSerial(
      static_cast<uint8_t>(config.uart_port));
  if (g_state.serial == nullptr) {
    g_state.status.last_error = "serial allocation failed";
    return false;
  }

  g_state.serial->begin(config.baud_rate, SERIAL_8N1, config.rx_pin, config.tx_pin);
  g_state.serial->setTimeout(0);
  while (g_state.serial->available() > 0) {
    g_state.serial->read();
  }

  g_state.status.initialized = true;
  g_state.status.last_error.clear();
  return true;
#else
  g_state = {};
  g_state.status.last_error = "disabled";
  return false;
#endif
}

void tick_gps() {
#if CONE_DEVICE_ENABLE_GPS
  if (!g_state.status.initialized || g_state.serial == nullptr) {
    return;
  }

  feed_serial();
  update_fix_age(::millis());
#endif
}

void deinit_gps() {
  release_serial();
  g_state = {};
}

GpsStatus gps_status() {
#if CONE_DEVICE_ENABLE_GPS
  update_fix_age(::millis());
#endif
  return g_state.status;
}

}  // namespace cone_device
