# GPS Module — SR2631Z3 Single-BeiDou Positioning Driver

## Hardware Overview

| Item | Specification |
|------|---------------|
| Module Model | SR2631Z3 (StarChip/星瑞达) |
| Positioning System | BDS-only (BDS-B1I / BDS-B1C) |
| Interface | UART TTL (3.3 V / 5 V tolerant) |
| Default Baud Rate | **115200 bps** |
| Output Protocol | NMEA 0183 — talker ID `$BD` |
| Update Rate | 1–10 Hz (default 1 Hz) |
| Sensitivity | Acquisition −161 dBm, Tracking −158 dBm |
| TTFF (Cold Start) | < 23 s (typical) |
| Positioning Accuracy | < 2.0 m (CEP50) |
| Antenna Type | Passive ceramic patch (25 × 25 × 4 mm) |

> **Note:** The module ships with a **passive ceramic patch antenna** (soldered on-board or attached via IPX). It does not require an external active antenna. The `ANTENNA OPEN` warning described in § *Troubleshooting* is a module-level detection threshold that can usually be ignored for passive antennas.

---

## Pin Connections

| ESP32-S3 Pin | Module Pin | Direction | Description |
|-------------|------------|-----------|-------------|
| GPIO17 (UART1 TX) | RX | ESP → Module | Data transmission |
| GPIO18 (UART1 RX) | TX | Module → ESP | Data reception |
| VCC (3.3 V / 5 V) | VCC | — | Supply voltage |
| GND | GND | — | Common ground |

Wiring diagram:

```
┌──────────────┐              ┌──────────────┐
│   ESP32-S3   │              │   SR2631Z3   │
│              │              │              │
│  GPIO17/TX ──┼──────────────┤ RX           │
│              │              │              │
│  GPIO18/RX ──┼──────────────┤ TX           │
│              │              │              │
│  3.3 V ──────┼──────────────┤ VCC          │
│              │              │              │
│  GND ────────┼──────────────┤ GND          │
└──────────────┘              └──────────────┘
```

> ⚠️ Cross over TX ↔ RX (ESP TX → module RX, ESP RX ← module TX). Do **not** wire TX→TX.

---

## Software Architecture

### File Layout

```
components/cone_device/
├── include/
│   └── cone_device/
│       └── gps_module.h         # Public API (config + status structs, functions)
├── src/
│   └── gps_module.cpp           # Driver implementation (UART init + NMEA parser)
├── GPS_MODULE.md                # This document
└── CMakeLists.txt               # Build registration (includes gps_module.cpp)
```

### Design Decisions

| Decision | Rationale |
|----------|-----------|
| **$BD-only parsing** | Module is single-BeiDou; all sentences carry the `$BD` talker ID (`$BDRMC`, `$BDGGA`, etc.). `$GP` / `$GL` / `$GA` sentences are ignored. |
| **Dual-sentence fallback** | Primary parser: `$BDRMC` (provides A/V status flag). Fallback: `$BDGGA` (validated via fix quality field). Redundancy protects against intermittent sentence corruption. |
| **Non-blocking tick** | `tick_gps()` polls UART with a zero-timeout read; the caller controls scheduling frequency (recommended: 10 Hz). |
| **Line-oriented accumulator** | An internal ring buffer assembles raw UART bytes into `\n`-delimited NMEA lines before parsing. Lines exceeding 128 bytes are discarded. |
| **Compile-time gating** | The entire driver (including static state) can be removed at build time via Kconfig symbol `CONFIG_CONE_DEVICE_ENABLE_GPS`. |

---

## Kconfig

Enable the GPS module through the project configuration:

```
idf.py menuconfig
  → Component config  →  Cone device modules  →  [*] Enable GPS module
```

Or set directly in `sdkconfig`:

```
CONFIG_CONE_DEVICE_ENABLE_GPS=y
```

When disabled:
- `setup_gps()` returns `false`
- `GpsStatus::last_error` is set to `"disabled"`
- All other functions are safe no-ops (state remains zero-initialised)

---

## API Reference

### Configuration Structure

```cpp
namespace cone_device {

struct GpsModuleConfig {
    int uart_port = 1;           // UART peripheral number (0/1/2)
    int tx_pin = 17;             // GPIO driving module RX
    int rx_pin = 18;             // GPIO receiving module TX
    uint32_t baud_rate = 115200; // Bus baud rate
    uint32_t stale_after_ms = 5000; // Reserved: data staleness threshold
};

}
```

### Status Structure

```cpp
struct GpsStatus {
    bool enabled = false;        // Module is enabled (Kconfig)
    bool initialized = false;    // UART hardware initialised successfully
    bool has_fix = false;        // Position data is currently valid
    double longitude = 0.0;      // Decimal degrees (WGS-84)
    double latitude = 0.0;       // Decimal degrees (WGS-84)
    float accuracy_m = 0.0f;     // Estimated positional accuracy in metres
    uint32_t last_fix_age_ms = 0; // Milliseconds since last valid fix
    std::string last_error;      // Most recent error description
};

}
```

### Functions

| Function | Description |
|----------|-------------|
| `setup_gps(config)` | Initialise UART, configure pins, start driver. Returns `true` on success. |
| `tick_gps()` | Non-blocking poll: read UART, parse NMEA, update internal state. Call periodically (every 50–100 ms). |
| `deinit_gps()` | Uninstall UART driver, reset internal state to defaults. |
| `gps_status()` | Return a copy of the current `GpsStatus` (thread-safe by value). |
| `get_lat()` | Convenience inline — return current latitude in decimal degrees. |
| `get_lon()` | Convenience inline — return current longitude in decimal degrees. |
| `data_valid()` | Convenience inline — shorthand for `gps_status().has_fix`. |

> **Thread safety:** The internal state is a single module-level struct accessed only from `tick_gps()`. The accessor `gps_status()` returns a **copy**, so the caller does not need to synchronise if it only reads the returned value. Mixing `tick_gps()` from one task and `gps_status()` from another is safe as long as the copy is consumed before the next tick.

---

## NMEA Sentence Reference

### `$BDRMC` — Recommended Minimum (preferred)

```
$BDRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a*hh
         ↑       ↑ ↑         ↑          ↑
        time  status lat      NS  lon     EW ...
```

| Field | Content | Values |
|-------|---------|--------|
| 2 | Status | `A` = data valid; `V` = void (no fix) |
| 3 | Latitude | NMEA `DDMM.MMMMM` |
| 4 | N/S indicator | `N` / `S` |
| 5 | Longitude | NMEA `DDDMM.MMMMM` |
| 6 | E/W indicator | `E` / `W` |

### `$BDGGA` — Fix Data (fallback)

```
$BDGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,xx,xxxx*hh
                ↑         ↑          ↑ ↑
               lat       NS  lon     EW  fix_quality ...
```

| Field | Content | Values |
|-------|---------|--------|
| 6 | Fix quality | `0` = invalid; `1` = GPS fix; `2` = DGPS fix |
| 7 | Satellites used | Count (0–99) |

### Sentences NOT parsed

The driver ignores the following standard NMEA sentences emitted by the module:

| Sentence | Content | Reason |
|----------|---------|--------|
| `$BDVTG` | Course over ground | Not needed for positioning |
| `$BDGSA` | DOP & active satellites | Not parsed (no `$BDGSA` handler) |
| `$BDGSV` | Satellites in view | Not parsed (verbose, multi-sentence) |
| `$BDGLL` | Geographic position (lat/lon) | Duplicates `$BDRMC` / `$BDGGA` |
| `$BDZDA` | Date & time | Not parsed (no RTC synchronisation) |
| `$GPTXT` | Text message (manufacturer) | May carry `ANTENNA OPEN` warnings |

---

## Coordinate Conversion

The module outputs position in **NMEA format** (degrees + decimal minutes). The driver transparently converts to **decimal degrees (WGS-84)**.

| Format | Example | Conversion |
|--------|---------|------------|
| NMEA latitude | `2240.61563` | `22 + 40.61563 ÷ 60 = 22.67693°` |
| NMEA longitude | `11359.86512` | `113 + 59.86512 ÷ 60 = 113.99775°` |
| Driver output | `get_lat()` / `get_lon()` | Already in decimal degrees |

---

## Usage Examples

### Basic Loop (Edge Node Pattern)

```cpp
#include "cone_device/gps_module.h"

extern "C" void app_main() {
    // Default config: UART1, GPIO17 TX, GPIO18 RX, 115200 baud
    cone_device::GpsModuleConfig cfg;
    if (!cone_device::setup_gps(cfg)) {
        ESP_LOGE("app", "GPS init failed");
        return;
    }

    while (true) {
        cone_device::tick_gps();

        if (cone_device::data_valid()) {
            ESP_LOGI("app", "Position: %.6f, %.6f",
                     cone_device::get_lat(),
                     cone_device::get_lon());
        }
        vTaskDelay(pdMS_TO_TICKS(100));  // 10 Hz tick
    }
}
```

### Access Full Status

```cpp
auto st = cone_device::gps_status();
if (st.has_fix) {
    printf("lat=%.6f  lon=%.6f  acc=%.1f m\n",
           st.latitude, st.longitude, st.accuracy_m);
} else {
    ESP_LOGW("app", "No fix: %s", st.last_error.c_str());
}
```

### Custom Pin Assignment

```cpp
cone_device::GpsModuleConfig cfg;
cfg.uart_port = 2;    // UART2
cfg.tx_pin = 4;       // TX → GPIO4
cfg.rx_pin = 5;       // RX → GPIO5
cfg.baud_rate = 9600; // Slower bus
cone_device::setup_gps(cfg);
```

---

## Troubleshooting

### Symptom: Zero lines received (`Lines received: 0`)

| Cause | Check |
|-------|-------|
| Wiring incorrect | Verify TX↔RX crossover (not TX→TX) |
| Wrong baud rate | Confirm module baud rate matches config. Most SR2631Z3 modules ship at **115200**. If unsure, try 9600. |
| Module not powered | Measure VCC at module pin. Look for a PWR LED. |
| UART port conflict | Some ESP32-S3 dev boards route UART1 to USB bridge; use UART2 instead. |

### Symptom: Data received but no fix (`Fix quality: 0`)

| Cause | Check |
|-------|-------|
| Indoor operation | GPS signals are severely attenuated by building materials. Move near a window or outdoors. |
| Antenna issue | The module may report `$GPTXT,01,01,01,ANTENNA OPEN*25`. For **passive ceramic patch antennas** (this module's default), this warning can be safely ignored — the module often still acquires a fix outdoors. |
| Cold start delay | First fix after power loss can take 20–60 s under open sky. |

### Symptom: `$GPTXT` with `ANTENNA OPEN`

This is a **module-level antenna detection** feature. The SR2631Z3 monitors the antenna port for a DC short expected by an active antenna. Passive ceramic patches do not present this short, triggering the open warning.

**Impact:** None for passive antennas. The module will still navigate once it acquires sufficient satellite signals. To suppress:

- Ignore the message (recommended, cosmetic only).
- Use an active antenna with an external bias-T if the application demands it.

### Symptom: `Lines received` is non-zero but parser counters stay 0

The driver only handles **`$BD`**-prefixed sentences. If the module was reconfigured to output `$GP` (GPS) or `$GL` (GLONASS) sentences, or if the talker ID was changed via UART commands, the parsed counts will remain 0.

**Verify:** Connect a USB-UART adapter to the module TX pin and inspect raw output with a terminal emulator at 115200 baud. Sentences should begin with `$BD`.

---

## Revision History

| Date | Changes |
|------|---------|
| 2026-06-04 | Initial documentation; verified against real hardware (ESP32-S3, COM9, USB-Serial/JTAG). Added `ANTENNA OPEN` behaviour notes. |

---

## References

- [NMEA 0183 Standard](https://www.nmea.org/content/STANDARDS/NMEA_0183_Standard)
- [SR2631Z3 Datasheet](/docs/product/SR2631Z3规格书.pdf)
- [ESP-IDF UART Driver Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/uart.html)
- [cone_device Component](../cone_device/README.md)
