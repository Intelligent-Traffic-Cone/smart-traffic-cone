# Smart Traffic Cone Context

This file defines shared project language.

## Language

**Smart Traffic Cone**:
A traffic cone with ESP32 networking, positioning, distance sensing, camera
status, and cloud telemetry upload capability.
_Avoid_: static cone, ordinary road cone, unrelated IoT node

**Edge Cone Node**:
The ESP32 firmware runtime inside one Smart Traffic Cone. It collects hardware
snapshots and uploads telemetry to the cloud.
_Avoid_: cloud node, vehicle client, browser demo

**Telemetry**:
One structured report from an Edge Cone Node containing location, ultrasonic
channels, camera status, device health, and raw extension fields.
_Avoid_: log line, UI event, unstructured sensor dump

**Road Event**:
A cloud-managed temporary road condition such as construction, accident
isolation, road closure, low visibility, or abnormal cone boundary.
_Avoid_: raw alert, single sensor reading

**Alert**:
A risk or device-health item generated from telemetry or operator input that
requires review, confirmation, or handling.
_Avoid_: Road Event, vehicle warning

**Vehicle Warning**:
A cloud-published warning contract for map or vehicle-side systems after a Road
Event is fresh enough or operator-confirmed.
_Avoid_: in-firmware alarm, local buzzer, dispatch task

**Vehicle Navigation Session**:
A vehicle-side navigation run created by the Raspberry Pi simulator before
driving. It carries route endpoints, dispatch risk segments, and later dynamic
advice polling.
_Avoid_: Road Event, raw map route, one-time warning

**Lane Guidance**:
Cloud-generated lane-level advice for the vehicle simulator, such as keeping
lane or preparing a left merge near a Smart Traffic Cone boundary.
_Avoid_: full autonomous control, firmware risk rule, map display layer

**Device Health**:
The freshness and operational state of GPS, ultrasonic channels, camera,
network, battery, and upload quality.
_Avoid_: risk level, road event status

**Hardware Module**:
A reusable firmware module with `setup`, `tick`, `status`, and `deinit`
operations. Hardware Modules do not own product risk semantics.
_Avoid_: app workflow, cloud service, one-off pin script

**BSP Configuration**:
The board and wiring facts that map generic Hardware Modules to concrete pins,
ports, and shared resources.
_Avoid_: product behavior, sensor algorithm

## Decisions

- The repository root is a workspace and does not own a single build system.
- ESP32 firmware is implemented as a PlatformIO Arduino project under
  `apps/edge-cone-node`.
- Most hardware model names remain unspecified; interfaces stay generic until
  the team confirms concrete modules. The GPS path now has a validated
  SR2631Z3-compatible UART/NMEA implementation behind the generic interface.
- The first runnable vehicle side is a Raspberry Pi simulator under
  `apps/pi-vehicle-simulator`, using HTTP polling against the cloud API.
