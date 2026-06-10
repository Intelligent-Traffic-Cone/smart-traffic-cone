#include "cone_device/ultrasonic_array.h"

#ifndef CONE_DEVICE_ENABLE_ULTRASONIC_ARRAY
#define CONE_DEVICE_ENABLE_ULTRASONIC_ARRAY 1
#endif

#include <memory>

namespace cone_device {
namespace {

UltrasonicArrayConfig g_config;
UltrasonicArrayStatus g_status;
std::unique_ptr<UltrasonicArray> g_sensor;
uint32_t g_last_sample_ms = 0;

bool is_configured_pin(int pin) {
  return pin >= 0;
}

void mark_unwired_channels() {
  for (size_t i = 1; i < kUltrasonicChannelCount; ++i) {
    g_status.channels[i] = {};
  }
}

void update_primary_channel_age(UltrasonicArrayStatus& status, uint32_t now_ms) {
  auto& channel = status.channels[0];
  if (!channel.present || g_last_sample_ms == 0) {
    return;
  }

  channel.sample_age_ms = now_ms - g_last_sample_ms;
  if (channel.sample_age_ms > g_config.stale_after_ms) {
    channel.timed_out = true;
    if (channel.last_error.empty()) {
      channel.last_error = "stale";
    }
    if (status.last_error.empty()) {
      status.last_error = "stale";
    }
  }
}

void sample_primary_channel() {
  auto& channel = g_status.channels[0];
  channel.present = true;

  const float distance_cm = g_sensor->readDistanceCmFiltered(g_config.filter_sample_count);
  g_last_sample_ms = ::millis();
  channel.sample_age_ms = 0;

  if (distance_cm < 0.0f) {
    channel.timed_out = true;
    channel.distance_m = 0.0f;
    channel.last_error = "timeout";
    g_status.last_error = "timeout";
    return;
  }

  channel.timed_out = false;
  channel.distance_m = distance_cm / 100.0f;
  channel.last_error.clear();
  g_status.last_error.clear();
}

}  // namespace

UltrasonicArray::UltrasonicArray(uint8_t trigPin, uint8_t echoPin)
    : trigPin_(trigPin), echoPin_(echoPin) {}

void UltrasonicArray::begin() {
  pinMode(trigPin_, OUTPUT);
  pinMode(echoPin_, INPUT);
  digitalWrite(trigPin_, LOW);
}

float UltrasonicArray::readDistanceCmOnce() {
  digitalWrite(trigPin_, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin_, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin_, LOW);

  const unsigned long duration = pulseIn(echoPin_, HIGH, kEchoTimeoutUs);
  if (duration == 0) {
    return -1.0f;
  }

  return (duration * 0.0343f) / 2.0f;
}

float UltrasonicArray::readDistanceCmFiltered(uint8_t sampleCount) {
  if (sampleCount == 0) {
    return -1.0f;
  }

  float sum = 0.0f;
  uint8_t validCount = 0;

  for (uint8_t i = 0; i < sampleCount; ++i) {
    const float distance = readDistanceCmOnce();
    if (distance > 0.0f) {
      sum += distance;
      ++validCount;
    }
    delay(30);
  }

  if (validCount == 0) {
    return -1.0f;
  }

  return sum / validCount;
}

bool setup_ultrasonic_array(const UltrasonicArrayConfig& config) {
#if CONE_DEVICE_ENABLE_ULTRASONIC_ARRAY
  g_config = config;
  g_status = {};
  g_status.enabled = true;
  mark_unwired_channels();

  const auto& primary = g_config.channels[0];
  if (!is_configured_pin(primary.trigger_pin) || !is_configured_pin(primary.echo_pin)) {
    g_status.last_error = "unconfigured";
    return false;
  }

  g_sensor = std::make_unique<UltrasonicArray>(
      static_cast<uint8_t>(primary.trigger_pin),
      static_cast<uint8_t>(primary.echo_pin));
  g_sensor->begin();
  g_status.initialized = true;
  g_status.channels[0].present = true;
  g_last_sample_ms = 0;
  sample_primary_channel();
  return true;
#else
  g_config = {};
  g_status = {};
  g_sensor.reset();
  g_status.last_error = "disabled";
  g_last_sample_ms = 0;
  return false;
#endif
}

void tick_ultrasonic_array() {
#if CONE_DEVICE_ENABLE_ULTRASONIC_ARRAY
  if (!g_sensor) {
    return;
  }

  const uint32_t now = ::millis();
  if (g_last_sample_ms != 0 && now - g_last_sample_ms < g_config.sample_interval_ms) {
    return;
  }

  sample_primary_channel();
#endif
}

void deinit_ultrasonic_array() {
  g_sensor.reset();
  g_config = {};
  g_status = {};
  g_last_sample_ms = 0;
}

UltrasonicArrayStatus ultrasonic_array_status() {
  UltrasonicArrayStatus snapshot = g_status;
  update_primary_channel_age(snapshot, ::millis());
  return snapshot;
}

}  // namespace cone_device
