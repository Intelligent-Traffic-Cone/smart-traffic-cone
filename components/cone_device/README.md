# cone_device

Reusable PlatformIO Arduino library for smart traffic cone hardware-facing
modules.

This library follows the project ownership rules:

- Apps own product orchestration and business meaning.
- Libraries own reusable hardware capabilities.
- BSP configuration owns concrete pins and board resources.
- Disabled modules still link and report `disabled`.

Current modules are generic interfaces only:

- `gps_module`: location, accuracy, freshness, and UART-fed NMEA snapshot.
- `ultrasonic_array`: four-channel distance snapshot and timeout state.
- `camera_module`: camera availability and frame capture status.
- `warning_light`: serial command API for yellow, green, red, buzzer, combined
  outputs, and global control.
- `pwm_servo`: two-channel SG90-style PWM servo output for 0-180 degree
  positioning.
- `telemetry_encoder`: stable JSON encoder for cloud upload payloads.

Most concrete device models are intentionally not fixed yet. The GPS path now
includes a validated PlatformIO Arduino implementation with SR2631Z3-compatible
defaults while keeping the public API generic. See `GPS_MODULE.md` for wiring,
runtime behavior, and the standalone test app.
