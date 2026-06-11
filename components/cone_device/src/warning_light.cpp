#include "cone_device/warning_light.h"

#include <Arduino.h>

#include <cstdio>
#include <new>

#ifndef CONE_DEVICE_ENABLE_WARNING_LIGHT
#define CONE_DEVICE_ENABLE_WARNING_LIGHT 1
#endif

namespace cone_device {
namespace {

WarningLightConfig g_config;
WarningLightStatus g_status;
HardwareSerial* g_serial = nullptr;

void release_serial() {
  if (g_serial == nullptr) {
    return;
  }
  g_serial->end();
  delete g_serial;
  g_serial = nullptr;
}

uint8_t target_value(WarningLightTarget target) {
  return static_cast<uint8_t>(target);
}

uint8_t action_value(WarningLightAction action) {
  return static_cast<uint8_t>(action);
}

bool is_supported_target(WarningLightTarget target) {
  return target != WarningLightTarget::kDocumentedGlobalAlias;
}

void remember_command(WarningLightTarget target, WarningLightAction action) {
  g_status.last_target = target;
  g_status.last_action = action;
  g_status.last_frame_hex =
      warning_light_frame_hex(g_config.start_byte, target, action);
  g_status.command_count += 1;
}

}  // namespace

bool setup_warning_light(const WarningLightConfig& config) {
#if CONE_DEVICE_ENABLE_WARNING_LIGHT
  release_serial();
  g_config = config;
  g_status = {};
  g_status.enabled = config.enabled;
  g_status.uart_port = config.uart_port;
  g_status.tx_pin = config.tx_pin;
  g_status.rx_pin = config.rx_pin;
  g_status.baud_rate = config.baud_rate;
  g_status.start_byte = config.start_byte;

  if (!config.enabled) {
    g_status.last_error = "disabled";
    return false;
  }
  if (config.uart_port < 0 || config.uart_port > 2) {
    g_status.last_error = "invalid uart_port";
    return false;
  }
  if (config.tx_pin < 0) {
    g_status.last_error = "tx_pin is required";
    return false;
  }

  g_serial = new (std::nothrow) HardwareSerial(
      static_cast<uint8_t>(config.uart_port));
  if (g_serial == nullptr) {
    g_status.last_error = "serial allocation failed";
    return false;
  }

  g_serial->begin(config.baud_rate, SERIAL_8N1, config.rx_pin, config.tx_pin);
  g_serial->setTimeout(0);
  while (g_serial->available() > 0) {
    g_serial->read();
  }

  g_status.initialized = true;
  g_status.last_error.clear();
  return true;
#else
  (void)config;
  g_config = {};
  g_status = {};
  g_status.last_error = "disabled";
  release_serial();
  return false;
#endif
}

void tick_warning_light() {
#if CONE_DEVICE_ENABLE_WARNING_LIGHT
  if (g_serial == nullptr) {
    return;
  }
  while (g_serial->available() > 0) {
    g_serial->read();
  }
#endif
}

void deinit_warning_light() {
  release_serial();
  g_config = {};
  g_status = {};
}

WarningLightStatus warning_light_status() {
  return g_status;
}

bool warning_light_set(WarningLightTarget target, WarningLightAction action) {
#if CONE_DEVICE_ENABLE_WARNING_LIGHT
  if (!g_status.initialized || g_serial == nullptr) {
    g_status.last_error = "uninitialized";
    return false;
  }
  if (!is_supported_target(target)) {
    g_status.last_error = "unsupported target";
    return false;
  }

  const uint8_t frame[4] = {
      g_config.start_byte,
      target_value(target),
      action_value(action),
      warning_light_checksum(g_config.start_byte, target, action),
  };

  const size_t written = g_serial->write(frame, sizeof(frame));
  g_serial->flush();
  remember_command(target, action);
  if (written != sizeof(frame)) {
    g_status.last_error = "serial write failed";
    return false;
  }
  g_status.last_error.clear();
  return true;
#else
  (void)target;
  (void)action;
  g_status.last_error = "disabled";
  return false;
#endif
}

bool warning_light_on(WarningLightTarget target) {
  return warning_light_set(target, WarningLightAction::kOn);
}

bool warning_light_off(WarningLightTarget target) {
  return warning_light_set(target, WarningLightAction::kOff);
}

bool warning_light_flash(WarningLightTarget target) {
  return warning_light_set(target, WarningLightAction::kFlash);
}

uint8_t warning_light_checksum(uint8_t start_byte,
                               WarningLightTarget target,
                               WarningLightAction action) {
  return static_cast<uint8_t>(
      start_byte + target_value(target) + action_value(action));
}

std::string warning_light_frame_hex(uint8_t start_byte,
                                    WarningLightTarget target,
                                    WarningLightAction action) {
  char buffer[12];
  std::snprintf(buffer,
                sizeof(buffer),
                "%02X %02X %02X %02X",
                start_byte,
                target_value(target),
                action_value(action),
                warning_light_checksum(start_byte, target, action));
  return buffer;
}

const char* warning_light_target_key(WarningLightTarget target) {
  switch (target) {
    case WarningLightTarget::kAll:
      return "all";
    case WarningLightTarget::kYellow:
      return "yellow";
    case WarningLightTarget::kGreen:
      return "green";
    case WarningLightTarget::kRed:
      return "red";
    case WarningLightTarget::kBuzzer:
      return "buzzer";
    case WarningLightTarget::kYellowBuzzer:
      return "yellow_buzzer";
    case WarningLightTarget::kGreenBuzzer:
      return "green_buzzer";
    case WarningLightTarget::kRedBuzzer:
      return "red_buzzer";
    case WarningLightTarget::kDocumentedGlobalAlias:
      return "documented_global_alias";
  }
  return "unknown";
}

const char* warning_light_action_key(WarningLightAction action) {
  switch (action) {
    case WarningLightAction::kOff:
      return "off";
    case WarningLightAction::kOn:
      return "on";
    case WarningLightAction::kFlash:
      return "flash";
  }
  return "unknown";
}

bool parse_warning_light_target(const std::string& value,
                                WarningLightTarget& target) {
  if (value == "all") {
    target = WarningLightTarget::kAll;
  } else if (value == "yellow") {
    target = WarningLightTarget::kYellow;
  } else if (value == "green") {
    target = WarningLightTarget::kGreen;
  } else if (value == "red") {
    target = WarningLightTarget::kRed;
  } else if (value == "buzzer") {
    target = WarningLightTarget::kBuzzer;
  } else if (value == "yellow_buzzer") {
    target = WarningLightTarget::kYellowBuzzer;
  } else if (value == "green_buzzer") {
    target = WarningLightTarget::kGreenBuzzer;
  } else if (value == "red_buzzer") {
    target = WarningLightTarget::kRedBuzzer;
  } else {
    return false;
  }
  return true;
}

bool parse_warning_light_action(const std::string& value,
                                WarningLightAction& action) {
  if (value == "off") {
    action = WarningLightAction::kOff;
  } else if (value == "on") {
    action = WarningLightAction::kOn;
  } else if (value == "flash") {
    action = WarningLightAction::kFlash;
  } else {
    return false;
  }
  return true;
}

}  // namespace cone_device
