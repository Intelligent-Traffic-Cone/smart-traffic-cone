#include "cone_device/pwm_servo.h"

#include <Arduino.h>

#ifndef CONE_DEVICE_ENABLE_PWM_SERVO
#define CONE_DEVICE_ENABLE_PWM_SERVO 1
#endif

namespace cone_device {
namespace {

constexpr uint16_t kMaxServoAngleDeg = 180;
constexpr uint8_t kMaxLedcChannel = 15;

PwmServoConfig g_config;
PwmServoStatus g_status;
std::array<bool, kPwmServoChannelCount> g_attached = {};

bool valid_pin(int pin) {
  return pin >= 0;
}

uint32_t max_duty(const PwmServoConfig& config) {
  return (1UL << config.ledc_resolution_bits) - 1UL;
}

uint32_t duty_for_pulse_us(const PwmServoConfig& config, uint16_t pulse_us) {
  const uint64_t duty =
      static_cast<uint64_t>(pulse_us) * max_duty(config) *
      config.pwm_frequency_hz / 1000000ULL;
  return static_cast<uint32_t>(duty);
}

bool validate_config(const PwmServoConfig& config, std::string& error) {
  if (config.pwm_frequency_hz == 0) {
    error = "invalid_frequency";
    return false;
  }
  if (config.ledc_resolution_bits == 0 || config.ledc_resolution_bits > 20) {
    error = "invalid_resolution";
    return false;
  }
  if (config.min_pulse_us >= config.max_pulse_us) {
    error = "invalid_pulse_range";
    return false;
  }
  return true;
}

bool write_angle(size_t index, uint16_t angle_deg) {
  if (index >= kPwmServoChannelCount) {
    g_status.last_error = "invalid_index";
    return false;
  }
  if (angle_deg > kMaxServoAngleDeg) {
    g_status.channels[index].last_error = "invalid_angle";
    g_status.last_error = "invalid_angle";
    return false;
  }

  auto& channel = g_status.channels[index];
  if (!channel.enabled || !channel.attached) {
    channel.last_error = "unavailable";
    g_status.last_error = "unavailable";
    return false;
  }

  const uint16_t pulse_us = pwm_servo_pulse_us_for_angle(g_config, angle_deg);
  ledcWrite(channel.ledc_channel, duty_for_pulse_us(g_config, pulse_us));
  channel.angle_deg = angle_deg;
  channel.pulse_us = pulse_us;
  channel.last_error.clear();
  g_status.last_error.clear();
  return true;
}

}  // namespace

bool setup_pwm_servos(const PwmServoConfig& config) {
#if CONE_DEVICE_ENABLE_PWM_SERVO
  deinit_pwm_servos();
  g_config = config;
  g_status = {};
  g_status.enabled = true;
  g_status.pwm_frequency_hz = config.pwm_frequency_hz;
  g_status.ledc_resolution_bits = config.ledc_resolution_bits;
  g_status.min_pulse_us = config.min_pulse_us;
  g_status.max_pulse_us = config.max_pulse_us;

  std::string error;
  if (!validate_config(config, error)) {
    g_status.last_error = error;
    return false;
  }

  bool any_attached = false;
  bool any_error = false;
  for (size_t i = 0; i < kPwmServoChannelCount; ++i) {
    const auto& cfg = config.channels[i];
    auto& status = g_status.channels[i];
    status.enabled = cfg.enabled;
    status.pin = cfg.pin;
    status.ledc_channel = cfg.ledc_channel;

    if (!cfg.enabled) {
      status.last_error = "disabled";
      continue;
    }
    if (!valid_pin(cfg.pin)) {
      status.last_error = "unconfigured";
      g_status.last_error = "unconfigured";
      any_error = true;
      continue;
    }
    if (cfg.ledc_channel > kMaxLedcChannel) {
      status.last_error = "invalid_ledc_channel";
      g_status.last_error = "invalid_ledc_channel";
      any_error = true;
      continue;
    }
    if (cfg.initial_angle_deg > kMaxServoAngleDeg) {
      status.last_error = "invalid_angle";
      g_status.last_error = "invalid_angle";
      any_error = true;
      continue;
    }

    const double actual_frequency = ledcSetup(
        cfg.ledc_channel, config.pwm_frequency_hz, config.ledc_resolution_bits);
    if (actual_frequency <= 0.0) {
      status.last_error = "ledc_setup_failed";
      g_status.last_error = "ledc_setup_failed";
      any_error = true;
      continue;
    }

    ledcAttachPin(static_cast<uint8_t>(cfg.pin), cfg.ledc_channel);
    g_attached[i] = true;
    status.attached = true;
    if (!write_angle(i, cfg.initial_angle_deg)) {
      any_error = true;
      continue;
    }
    any_attached = true;
  }

  g_status.initialized = any_attached && !any_error;
  if (!any_attached && g_status.last_error.empty()) {
    g_status.last_error = "unconfigured";
  }
  return g_status.initialized;
#else
  (void)config;
  deinit_pwm_servos();
  g_status.last_error = "disabled";
  return false;
#endif
}

void tick_pwm_servos() {}

void deinit_pwm_servos() {
#if CONE_DEVICE_ENABLE_PWM_SERVO
  for (size_t i = 0; i < kPwmServoChannelCount; ++i) {
    if (g_attached[i] && valid_pin(g_config.channels[i].pin)) {
      ledcDetachPin(static_cast<uint8_t>(g_config.channels[i].pin));
    }
  }
#endif
  g_config = {};
  g_status = {};
  g_attached = {};
}

PwmServoStatus pwm_servos_status() {
  return g_status;
}

bool pwm_servo_set_angle(size_t index, uint16_t angle_deg) {
#if CONE_DEVICE_ENABLE_PWM_SERVO
  if (!g_status.initialized) {
    g_status.last_error = "uninitialized";
    return false;
  }
  return write_angle(index, angle_deg);
#else
  (void)index;
  (void)angle_deg;
  g_status.last_error = "disabled";
  return false;
#endif
}

uint16_t pwm_servo_pulse_us_for_angle(const PwmServoConfig& config,
                                      uint16_t angle_deg) {
  if (angle_deg > kMaxServoAngleDeg) {
    angle_deg = kMaxServoAngleDeg;
  }
  const uint32_t span = config.max_pulse_us - config.min_pulse_us;
  return static_cast<uint16_t>(config.min_pulse_us +
                               (span * angle_deg) / kMaxServoAngleDeg);
}

}  // namespace cone_device
