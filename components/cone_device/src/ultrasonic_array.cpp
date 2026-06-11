#include "cone_device/ultrasonic_array.h"

#ifndef CONE_DEVICE_ENABLE_ULTRASONIC_ARRAY
#define CONE_DEVICE_ENABLE_ULTRASONIC_ARRAY 1
#endif

#include <array>
#include <memory>

namespace cone_device {
namespace {

UltrasonicArrayConfig g_config;
UltrasonicArrayStatus g_status;
std::array<std::unique_ptr<UltrasonicArray>, kUltrasonicChannelCount> g_sensors;
std::array<uint32_t, kUltrasonicChannelCount> g_last_sample_ms = {};

bool is_configured_pin(int pin) {
  return pin >= 0;
}

bool is_channel_configured(const UltrasonicChannelConfig& config) {
  return config.enabled && is_configured_pin(config.trigger_pin) &&
         is_configured_pin(config.echo_pin);
}

void update_channel_age(UltrasonicArrayStatus& status, size_t index, uint32_t now_ms) {
  auto& channel = status.channels[index];
  if (!channel.present) {
    channel.sample_age_ms = 0;
    channel.timed_out = true;
    return;
  }

  if (g_last_sample_ms[index] == 0) {
    return;
  }

  channel.sample_age_ms = now_ms - g_last_sample_ms[index];
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

void sample_channel(size_t index) {
  auto& sensor = g_sensors[index];
  if (!sensor) {
    return;
  }

  auto& channel = g_status.channels[index];
  channel.present = true;

  const float distance_cm = sensor->readDistanceCmFiltered(g_config.filter_sample_count);
  g_last_sample_ms[index] = ::millis();
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

UltrasonicArray::UltrasonicArray(uint8_t trigPin, uint8_t echoPin, uint32_t timeoutUs)
    : trigPin_(trigPin), echoPin_(echoPin), timeoutUs_(timeoutUs) {}

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

  const unsigned long duration = pulseIn(echoPin_, HIGH, timeoutUs_);
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
  g_last_sample_ms = {};
  for (auto& sensor : g_sensors) {
    sensor.reset();
  }

  bool any_configured = false;
  for (size_t i = 0; i < kUltrasonicChannelCount; ++i) {
    auto& channel = g_status.channels[i];
    const auto& channel_config = g_config.channels[i];
    channel.present = false;
    channel.timed_out = true;

    if (!channel_config.enabled) {
      channel.last_error = "disabled";
      continue;
    }

    if (!is_channel_configured(channel_config)) {
      channel.last_error = "unconfigured";
      g_status.last_error = "unconfigured";
      continue;
    }

    g_sensors[i] = std::make_unique<UltrasonicArray>(
        static_cast<uint8_t>(channel_config.trigger_pin),
        static_cast<uint8_t>(channel_config.echo_pin),
        channel_config.timeout_us);
    g_sensors[i]->begin();
    channel.present = true;
    channel.last_error.clear();
    any_configured = true;
  }

  if (!any_configured) {
    g_status.last_error = "unconfigured";
    return false;
  }

  g_status.initialized = true;
  g_status.last_error.clear();
  for (size_t i = 0; i < kUltrasonicChannelCount; ++i) {
    sample_channel(i);
  }
  return any_configured;
#else
  g_config = {};
  g_status = {};
  for (auto& sensor : g_sensors) {
    sensor.reset();
  }
  g_status.last_error = "disabled";
  g_last_sample_ms = {};
  return false;
#endif
}

void tick_ultrasonic_array() {
#if CONE_DEVICE_ENABLE_ULTRASONIC_ARRAY
  const uint32_t now = ::millis();
  for (size_t i = 0; i < kUltrasonicChannelCount; ++i) {
    if (!g_sensors[i]) {
      continue;
    }
    if (g_last_sample_ms[i] != 0 && now - g_last_sample_ms[i] < g_config.sample_interval_ms) {
      continue;
    }
    sample_channel(i);
  }
#endif
}

void deinit_ultrasonic_array() {
  for (auto& sensor : g_sensors) {
    sensor.reset();
  }
  g_config = {};
  g_status = {};
  g_last_sample_ms = {};
}

UltrasonicArrayStatus ultrasonic_array_status() {
  UltrasonicArrayStatus snapshot = g_status;
  const uint32_t now = ::millis();
  for (size_t i = 0; i < kUltrasonicChannelCount; ++i) {
    update_channel_age(snapshot, i, now);
  }
  return snapshot;
}

}  // namespace cone_device
