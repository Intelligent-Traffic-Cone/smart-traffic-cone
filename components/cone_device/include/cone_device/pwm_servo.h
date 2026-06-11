#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace cone_device {

constexpr size_t kPwmServoChannelCount = 2;

struct PwmServoChannelConfig {
  bool enabled = false;
  int pin = -1;
  uint8_t ledc_channel = 0;
  uint16_t initial_angle_deg = 90;
};

struct PwmServoConfig {
  std::array<PwmServoChannelConfig, kPwmServoChannelCount> channels = {};
  uint32_t pwm_frequency_hz = 50;
  uint8_t ledc_resolution_bits = 14;
  uint16_t min_pulse_us = 500;
  uint16_t max_pulse_us = 2500;
};

struct PwmServoChannelStatus {
  bool enabled = false;
  bool attached = false;
  int pin = -1;
  uint8_t ledc_channel = 0;
  uint16_t angle_deg = 0;
  uint16_t pulse_us = 0;
  std::string last_error;
};

struct PwmServoStatus {
  bool enabled = false;
  bool initialized = false;
  uint32_t pwm_frequency_hz = 0;
  uint8_t ledc_resolution_bits = 0;
  uint16_t min_pulse_us = 0;
  uint16_t max_pulse_us = 0;
  std::array<PwmServoChannelStatus, kPwmServoChannelCount> channels = {};
  std::string last_error;
};

bool setup_pwm_servos(const PwmServoConfig& config);
void tick_pwm_servos();
void deinit_pwm_servos();
PwmServoStatus pwm_servos_status();

bool pwm_servo_set_angle(size_t index, uint16_t angle_deg);
uint16_t pwm_servo_pulse_us_for_angle(const PwmServoConfig& config,
                                      uint16_t angle_deg);

}  // namespace cone_device
