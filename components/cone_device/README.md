# cone_device

Reusable PlatformIO Arduino library for smart traffic cone hardware-facing
modules.

This library follows the project ownership rules:

- Apps own product orchestration and business meaning.
- Libraries own reusable hardware capabilities.
- BSP configuration owns concrete pins and board resources.
- Disabled modules still link and report `disabled`.

Current modules are generic interfaces only:

- `gps_module`: location, accuracy, freshness, and health snapshot.
- `ultrasonic_array`: four-channel distance snapshot and timeout state.
- `camera_module`: camera availability and frame capture status.
- `telemetry_encoder`: stable JSON encoder for cloud upload payloads.

Concrete device models are intentionally not fixed yet.
