#pragma once

#include <cstdint>
#include <string>

namespace cone_device {

enum class WarningLightTarget : uint8_t {
  kAll = 0x00,
  kYellow = 0x01,
  kGreen = 0x02,
  kRed = 0x03,
  kBuzzer = 0x04,
  kYellowBuzzer = 0x05,
  kGreenBuzzer = 0x06,
  kRedBuzzer = 0x07,
  kDocumentedGlobalAlias = 0x08,
};

enum class WarningLightAction : uint8_t {
  kOff = 0x00,
  kOn = 0x01,
  kFlash = 0x02,
};

struct WarningLightConfig {
  bool enabled = true;
  int uart_port = 2;
  int tx_pin = 19;
  int rx_pin = 20;
  uint32_t baud_rate = 115200;
  uint8_t start_byte = 0xA0;
};

struct WarningLightStatus {
  bool enabled = false;
  bool initialized = false;
  int uart_port = -1;
  int tx_pin = -1;
  int rx_pin = -1;
  uint32_t baud_rate = 0;
  uint8_t start_byte = 0xA0;
  WarningLightTarget last_target = WarningLightTarget::kAll;
  WarningLightAction last_action = WarningLightAction::kOff;
  std::string last_frame_hex;
  uint32_t command_count = 0;
  std::string last_error;
};

bool setup_warning_light(const WarningLightConfig& config);
void tick_warning_light();
void deinit_warning_light();
WarningLightStatus warning_light_status();

bool warning_light_set(WarningLightTarget target, WarningLightAction action);
bool warning_light_on(WarningLightTarget target);
bool warning_light_off(WarningLightTarget target);
bool warning_light_flash(WarningLightTarget target);

uint8_t warning_light_checksum(uint8_t start_byte,
                               WarningLightTarget target,
                               WarningLightAction action);
std::string warning_light_frame_hex(uint8_t start_byte,
                                    WarningLightTarget target,
                                    WarningLightAction action);

const char* warning_light_target_key(WarningLightTarget target);
const char* warning_light_action_key(WarningLightAction action);
bool parse_warning_light_target(const std::string& value,
                                WarningLightTarget& target);
bool parse_warning_light_action(const std::string& value,
                                WarningLightAction& action);

}  // namespace cone_device
