#include "telemetry_uploader.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#if __has_include("cone_node.local.h")
#include "cone_node.local.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef CONE_CLOUD_BASE_URL
#define CONE_CLOUD_BASE_URL "http://127.0.0.1:8000"
#endif

#ifndef CONE_NODE_ID
#define CONE_NODE_ID "cone-demo-001"
#endif

namespace {
constexpr const char* kTag = "telemetry_uploader";
constexpr uint32_t kWifiRetryIntervalMs = 5000;
constexpr uint32_t kHttpTimeoutMs = 3000;

std::string json_escape(const String& value) {
  std::string out;
  out.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value.charAt(i);
    if (c == '"' || c == '\\') {
      out += '\\';
    }
    out += c;
  }
  return out;
}
}  // namespace

bool TelemetryUploader::setup() {
  consecutive_failures_ = 0;
  WiFi.mode(WIFI_STA);
  return ensure_wifi_connected();
}

void TelemetryUploader::tick() {
  ensure_wifi_connected();
}

bool TelemetryUploader::upload(const std::string& payload) {
  if (!ensure_wifi_connected()) {
    consecutive_failures_ += 1;
    Serial.printf("[%s] telemetry skipped: wifi offline\n", kTag);
    return false;
  }

  HTTPClient http;
  const std::string url = endpoint("/api/cones/" CONE_NODE_ID "/telemetry");
  http.setTimeout(kHttpTimeoutMs);
  if (!http.begin(url.c_str())) {
    consecutive_failures_ += 1;
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(
      const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(payload.data())),
      payload.size());
  const bool ok = code >= 200 && code < 300;
  if (ok) {
    consecutive_failures_ = 0;
  } else {
    consecutive_failures_ += 1;
    Serial.printf("[%s] telemetry upload failed http=%d\n", kTag, code);
  }
  http.end();
  return ok;
}

bool TelemetryUploader::upload_image(const uint8_t* data, size_t size, std::string& image_url) {
  image_url.clear();
  if (data == nullptr || size == 0) {
    consecutive_failures_ += 1;
    return false;
  }
  if (!ensure_wifi_connected()) {
    consecutive_failures_ += 1;
    Serial.printf("[%s] image skipped: wifi offline\n", kTag);
    return false;
  }

  HTTPClient http;
  const std::string url = endpoint("/api/cones/" CONE_NODE_ID "/images");
  http.setTimeout(kHttpTimeoutMs);
  if (!http.begin(url.c_str())) {
    consecutive_failures_ += 1;
    return false;
  }
  http.addHeader("Content-Type", "image/jpeg");
  const int code = http.POST(const_cast<uint8_t*>(data), size);
  const String response = http.getString();
  const bool ok = code >= 200 && code < 300 && parse_image_url(response.c_str(), image_url);
  if (ok) {
    consecutive_failures_ = 0;
  } else {
    consecutive_failures_ += 1;
    Serial.printf("[%s] image upload failed http=%d body=%s\n", kTag, code, response.c_str());
  }
  http.end();
  return ok;
}

const char* TelemetryUploader::status() const {
  if (WiFi.status() == WL_CONNECTED) {
    return consecutive_failures_ == 0 ? "online" : "upload_degraded";
  }
  return wifi_started_ ? "wifi_connecting" : "wifi_unconfigured";
}

uint32_t TelemetryUploader::consecutive_failures() const {
  return consecutive_failures_;
}

std::string TelemetryUploader::status_json() const {
  std::string out = "{";
  out += "\"network_status\":\"";
  out += status();
  out += "\",\"ip\":\"";
  out += json_escape(WiFi.localIP().toString());
  out += "\",\"rssi\":";
  out += std::to_string(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
  out += ",\"failures\":";
  out += std::to_string(consecutive_failures_);
  out += "}";
  return out;
}

bool TelemetryUploader::ensure_wifi_connected() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifi_ip_logged_) {
      wifi_ip_logged_ = true;
      Serial.printf("[%s] wifi connected ip=%s local_config=http://%s/\n",
                    kTag,
                    WiFi.localIP().toString().c_str(),
                    WiFi.localIP().toString().c_str());
    }
    return true;
  }

  if (String(WIFI_SSID).isEmpty()) {
    wifi_started_ = false;
    wifi_ip_logged_ = false;
    return false;
  }

  const uint32_t now = millis();
  if (wifi_started_ && now - last_wifi_attempt_ms_ < kWifiRetryIntervalMs) {
    return false;
  }

  wifi_started_ = true;
  wifi_ip_logged_ = false;
  last_wifi_attempt_ms_ = now;
  Serial.printf("[%s] connecting wifi ssid=%s\n", kTag, WIFI_SSID);
  WiFi.disconnect(false, false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  return WiFi.status() == WL_CONNECTED;
}

std::string TelemetryUploader::endpoint(const char* suffix) const {
  std::string base = CONE_CLOUD_BASE_URL;
  while (!base.empty() && base.back() == '/') {
    base.pop_back();
  }
  base += suffix;
  return base;
}

bool TelemetryUploader::parse_image_url(const std::string& payload, std::string& image_url) const {
  const std::string key = "\"image_url\"";
  const size_t key_pos = payload.find(key);
  if (key_pos == std::string::npos) {
    return false;
  }
  const size_t colon = payload.find(':', key_pos + key.size());
  if (colon == std::string::npos) {
    return false;
  }
  const size_t first_quote = payload.find('"', colon + 1);
  if (first_quote == std::string::npos) {
    return false;
  }
  const size_t second_quote = payload.find('"', first_quote + 1);
  if (second_quote == std::string::npos || second_quote <= first_quote + 1) {
    return false;
  }
  image_url = payload.substr(first_quote + 1, second_quote - first_quote - 1);
  if (!image_url.empty() && image_url[0] == '/') {
    std::string base = CONE_CLOUD_BASE_URL;
    while (!base.empty() && base.back() == '/') {
      base.pop_back();
    }
    image_url = base + image_url;
  }
  return true;
}
