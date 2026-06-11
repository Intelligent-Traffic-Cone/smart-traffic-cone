#pragma once

#include "cone_device/gps_module.h"
#include "cone_device/pwm_servo.h"
#include "cone_device/ultrasonic_array.h"
#include "cone_device/warning_light.h"

#include <array>

namespace cone_node_bsp {

constexpr int kGpsUartPort = 1;
constexpr int kGpsTxPin = 47;
constexpr int kGpsRxPin = 48;
constexpr uint32_t kGpsBaudRate = 115200;

constexpr int kWarningLightUartPort = 2;
constexpr int kWarningLightTxPin = 19;
constexpr int kWarningLightRxPin = 20;
constexpr uint32_t kWarningLightBaudRate = 115200;

constexpr int kPanBaseServoPin = 42;
constexpr int kPanTopServoPin = 43;
constexpr uint8_t kPanBaseServoLedcChannel = 2;
constexpr uint8_t kPanTopServoLedcChannel = 3;

constexpr const char* kDirectionNames[cone_device::kUltrasonicChannelCount] = {
    "front",
    "rear",
    "left",
    "right",
};

constexpr std::array<cone_device::UltrasonicChannelConfig,
                     cone_device::kUltrasonicChannelCount>
    kUltrasonicChannels = {{
        {true, 1, 2, 30000},
        {true, 14, 21, 30000},
        {true, 38, 39, 30000},
        {true, 40, 41, 30000},
    }};

inline cone_device::GpsModuleConfig gps_config() {
  cone_device::GpsModuleConfig config;
  config.uart_port = kGpsUartPort;
  config.tx_pin = kGpsTxPin;
  config.rx_pin = kGpsRxPin;
  config.baud_rate = kGpsBaudRate;
  return config;
}

inline cone_device::WarningLightConfig warning_light_config() {
  cone_device::WarningLightConfig config;
  config.enabled = true;
  config.uart_port = kWarningLightUartPort;
  config.tx_pin = kWarningLightTxPin;
  config.rx_pin = kWarningLightRxPin;
  config.baud_rate = kWarningLightBaudRate;
  config.start_byte = 0xA0;
  return config;
}

inline cone_device::PwmServoConfig pan_servo_config(uint16_t base_angle_deg,
                                                    uint16_t top_angle_deg) {
  cone_device::PwmServoConfig config;
  config.channels[0].enabled = true;
  config.channels[0].pin = kPanBaseServoPin;
  config.channels[0].ledc_channel = kPanBaseServoLedcChannel;
  config.channels[0].initial_angle_deg = base_angle_deg;
  config.channels[1].enabled = true;
  config.channels[1].pin = kPanTopServoPin;
  config.channels[1].ledc_channel = kPanTopServoLedcChannel;
  config.channels[1].initial_angle_deg = top_angle_deg;
  config.pwm_frequency_hz = 50;
  config.ledc_resolution_bits = 14;
  config.min_pulse_us = 500;
  config.max_pulse_us = 2500;
  return config;
}

}  // namespace cone_node_bsp
