#pragma once

#include <Arduino.h>

#include <array>
#include <cstdint>
#include <string>

namespace cone_device {

constexpr size_t kUltrasonicChannelCount = 4;

struct UltrasonicChannelConfig {
  bool enabled = false;
  int trigger_pin = -1;
  int echo_pin = -1;
  uint32_t timeout_us = 30000;
};

struct UltrasonicArrayConfig {
  std::array<UltrasonicChannelConfig, kUltrasonicChannelCount> channels = {};
  uint32_t sample_interval_ms = 500;
  uint32_t stale_after_ms = 2000;
  uint8_t filter_sample_count = 5;
};

struct UltrasonicChannelStatus {
  bool present = false;
  bool timed_out = false;
  float distance_m = 0.0f;
  uint32_t sample_age_ms = 0;
  std::string last_error;
};

struct UltrasonicArrayStatus {
  bool enabled = false;
  bool initialized = false;
  std::array<UltrasonicChannelStatus, kUltrasonicChannelCount> channels = {};
  std::string last_error;
};

class UltrasonicArray {
public:
  UltrasonicArray(uint8_t trigPin, uint8_t echoPin, uint32_t timeoutUs = 30000);

  void begin();

  // 单次读取距离，单位 cm
  // 返回 -1 表示超时或无有效回波
  float readDistanceCmOnce();

  // 多次采样平均滤波
  // sampleCount 为 0 或没有有效回波时返回 -1
  float readDistanceCmFiltered(uint8_t sampleCount = 5);

private:
  uint8_t trigPin_;
  uint8_t echoPin_;
  uint32_t timeoutUs_;
};

// Preserve the existing telemetry snapshot API while the hardware uses
// a single HC-SR04 channel.
bool setup_ultrasonic_array(const UltrasonicArrayConfig& config);
void tick_ultrasonic_array();
void deinit_ultrasonic_array();
UltrasonicArrayStatus ultrasonic_array_status();

}  // namespace cone_device
