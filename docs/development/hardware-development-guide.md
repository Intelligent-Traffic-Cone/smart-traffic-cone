# Hardware Development Guide

## Goals

- Keep product orchestration in `apps/edge-cone-node`.
- Keep reusable sensor and actuator behavior in `components/cone_device`.
- Keep concrete pins, UART ports, shared buses, and board variants in BSP
  configuration when those facts are known.
- Keep hardware interfaces generic until module models are confirmed.

## Module Shape

Each Hardware Module should expose:

- `setup_<module>(config)`: initialize driver resources.
- `tick_<module>()`: advance non-blocking work.
- `<module>_status()`: return a snapshot value, not mutable internal state.
- `deinit_<module>()`: release driver and board resources.

The first modules are:

- `gps_module`: location and freshness.
- `ultrasonic_array`: four distance channels.
- `camera_module`: camera availability and frame status.
- `pwm_servo`: SG90-style PWM servo output.
- `telemetry_encoder`: structured cloud upload payload.

## PlatformIO Flags

Each optional module owns a PlatformIO build flag:

```text
CONE_DEVICE_ENABLE_GPS
CONE_DEVICE_ENABLE_ULTRASONIC_ARRAY
CONE_DEVICE_ENABLE_CAMERA
CONE_DEVICE_ENABLE_WARNING_LIGHT
CONE_DEVICE_ENABLE_PWM_SERVO
```

Disabled modules must still link and return a status with `last_error` set to
`disabled`. Set these flags in `apps/edge-cone-node/platformio.ini`.

## BSP and Wiring

Do not hardcode confirmed pins in app logic. Record wiring in a BSP layer or
app-level config when hardware is finalized:

| Hardware | Current status | Notes |
| --- | --- | --- |
| ESP32 controller | planned | exact board variant pending |
| Positioning module | SR2631Z3-compatible UART/NMEA path | defaults are UART1 TX17 RX18 @ 115200; override in config if board differs |
| Ultrasonic modules | four-channel interface | model pending |
| Camera | generic camera interface | model pending |
| Warning light | UART command module | V1 uses TX19/RX20 @ 115200 |
| Pan servos | two SG90-compatible PWM outputs | bottom servo GPIO42, top servo GPIO43, 50Hz PWM |

## Test Requirements

- Run `pio run -e esp32dev` in `apps/edge-cone-node` with all modules enabled.
- Run `pio run -e esp32-s3-devkitc-1` in `apps/gps-test-pio` when changing GPS
  parsing, UART defaults, or wiring guidance.
- Run at least one PlatformIO environment or config with modules disabled to
  confirm stubs link.
- For real hardware PRs, document wiring, sample logs, and failure modes.
