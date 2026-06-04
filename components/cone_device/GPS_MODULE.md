# GPS 模块 / GPS Module

当前 `cone_device::gps_module` 在 PlatformIO Arduino 主线上提供一个经过
实测验证的 UART/NMEA 实现，默认配置对齐 SR2631Z3 单北斗模组：

- `uart_port = 1`
- `tx_pin = 17`
- `rx_pin = 18`
- `baud_rate = 115200`

The current `cone_device::gps_module` ships with a validated UART/NMEA
implementation for the PlatformIO Arduino firmware. Its default configuration
matches the SR2631Z3 single-BeiDou module listed above.

## Wiring

默认接线 / Default wiring:

| ESP32 pin | Module pin | Notes |
| --- | --- | --- |
| `GPIO17` | `RX` | ESP TX -> module RX |
| `GPIO18` | `TX` | ESP RX <- module TX |
| `3.3V` or `5V` | `VCC` | match your module board requirements |
| `GND` | `GND` | common ground |

注意 TX/RX 需要交叉连接，不要 TX 接 TX。

Cross-connect TX/RX. Do not wire TX to TX.

## Build Flags

当前主线通过 PlatformIO 宏控制模块启用：

```text
CONE_DEVICE_ENABLE_GPS
```

The current main branch enables or disables the module with the PlatformIO
macro above, not with ESP-IDF Kconfig.

## Runtime Behavior

- `setup_gps(config)` creates a `HardwareSerial` instance for `uart_port`.
- `tick_gps()` performs non-blocking reads and parses completed NMEA lines.
- `gps_status()` returns a snapshot copy of the current state.
- `stale_after_ms` clears `has_fix` after the last valid fix ages out.

The parser currently accepts both `$BDRMC` / `$BDGGA` and `$GPRMC` / `$GPGGA`
sentences. That keeps the generic API useful for SR2631Z3 while remaining
compatible with more common GPS talker IDs.

## Supported Fields

`GpsStatus` currently updates:

- `enabled`
- `initialized`
- `has_fix`
- `latitude`
- `longitude`
- `last_fix_age_ms`
- `last_error`

`accuracy_m` remains reserved for a later hardware-specific estimate.

## Example

```cpp
#include "cone_device/gps_module.h"

void setup() {
  cone_device::GpsModuleConfig config;
  cone_device::setup_gps(config);
}

void loop() {
  cone_device::tick_gps();

  if (cone_device::data_valid()) {
    auto status = cone_device::gps_status();
    Serial.printf("lat=%.6f lon=%.6f age=%lu\n",
                  status.latitude,
                  status.longitude,
                  static_cast<unsigned long>(status.last_fix_age_ms));
  }
}
```

## Bench Test App

Repository includes a standalone UART bench app:

```text
apps/gps-test-pio
```

Build it with:

```powershell
cd apps/gps-test-pio
pio run -e esp32-s3-devkitc-1
```

Use this app when validating wiring, baud rate, or raw NMEA output before
integrating the module into `apps/edge-cone-node`.

## Troubleshooting

`Lines received: 0`

- check power and shared ground
- confirm TX/RX crossover
- confirm the module baud rate really is `115200`
- try another UART port if the board reserves UART1 pins

`Fix: NO` with data flowing

- move outdoors or near a window
- allow extra time for a cold start
- inspect raw NMEA output to confirm the module is emitting `BD` or `GP`
  talker IDs

`ANTENNA OPEN`

Some SR2631Z3 boards report this vendor message even with a passive ceramic
antenna. Treat it as a hardware note, not an automatic proof that wiring is
broken.
