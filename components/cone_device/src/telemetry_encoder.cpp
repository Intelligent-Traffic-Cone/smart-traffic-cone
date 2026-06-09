#include "cone_device/telemetry_encoder.h"

#include <cstdio>
#include <sstream>
#include <string>

namespace cone_device {
namespace {
const char* bool_json(bool value) {
  return value ? "true" : "false";
}

const char* status_or(const std::string& error, bool initialized) {
  if (!error.empty()) {
    return error.c_str();
  }
  return initialized ? "normal" : "uninitialized";
}

std::string reported_at_from_uptime(uint32_t uptime_ms) {
  const uint32_t seconds = uptime_ms / 1000;
  const uint32_t minutes = seconds / 60;
  const uint32_t hours = minutes / 60;
  char buffer[32];
  std::snprintf(
      buffer,
      sizeof(buffer),
      "1970-01-01T%02u:%02u:%02uZ",
      static_cast<unsigned>(hours % 24),
      static_cast<unsigned>(minutes % 60),
      static_cast<unsigned>(seconds % 60));
  return buffer;
}
}  // namespace

std::string encode_telemetry_json(const TelemetrySnapshot& snapshot) {
  std::ostringstream out;
  out << "{";
  out << "\"cone_id\":\"" << snapshot.cone_id << "\",";
  out << "\"reported_at\":\"" << reported_at_from_uptime(snapshot.uptime_ms) << "\",";

  out << "\"location\":{";
  out << "\"longitude\":" << (snapshot.gps.has_fix ? std::to_string(snapshot.gps.longitude) : "null") << ",";
  out << "\"latitude\":" << (snapshot.gps.has_fix ? std::to_string(snapshot.gps.latitude) : "null") << ",";
  out << "\"accuracy_m\":" << (snapshot.gps.has_fix ? std::to_string(snapshot.gps.accuracy_m) : "null") << ",";
  out << "\"has_fix\":" << bool_json(snapshot.gps.has_fix);
  out << "},";

  out << "\"ultrasonic\":[";
  for (size_t i = 0; i < kUltrasonicChannelCount; ++i) {
    const auto& channel = snapshot.ultrasonic.channels[i];
    if (i > 0) {
      out << ",";
    }
    out << "{";
    out << "\"channel\":" << i << ",";
    out << "\"distance_m\":";
    if (channel.present && !channel.timed_out) {
      out << channel.distance_m;
    } else {
      out << "null";
    }
    out << ",";
    out << "\"timed_out\":" << bool_json(channel.timed_out || !channel.present) << ",";
    out << "\"sample_age_ms\":" << channel.sample_age_ms;
    out << "}";
  }
  out << "],";

  out << "\"camera\":{";
  out << "\"enabled\":" << bool_json(snapshot.camera.enabled) << ",";
  out << "\"initialized\":" << bool_json(snapshot.camera.initialized) << ",";
  out << "\"frame_available\":" << bool_json(snapshot.camera.frame_available) << ",";
  out << "\"last_frame_age_ms\":" << snapshot.camera.last_frame_age_ms << ",";
  out << "\"frame_count\":" << snapshot.camera.frame_count << ",";
  out << "\"image_url\":null";
  out << "},";

  out << "\"device\":{";
  out << "\"gps_status\":\"" << status_or(snapshot.gps.last_error, snapshot.gps.initialized) << "\",";
  out << "\"ultrasonic_status\":\"" << status_or(snapshot.ultrasonic.last_error, snapshot.ultrasonic.initialized) << "\",";
  out << "\"camera_status\":\"" << status_or(snapshot.camera.last_error, snapshot.camera.initialized) << "\",";
  out << "\"network_status\":\"" << snapshot.network_status << "\",";
  out << "\"battery_percent\":null";
  out << "},";

  out << "\"raw_payload\":{";
  out << "\"firmware_version\":\"0.1.0\",";
  out << "\"uptime_ms\":" << snapshot.uptime_ms << ",";
  out << "\"upload_failure_count\":" << snapshot.upload_failure_count;
  out << "}";
  out << "}";
  return out.str();
}

}  // namespace cone_device
