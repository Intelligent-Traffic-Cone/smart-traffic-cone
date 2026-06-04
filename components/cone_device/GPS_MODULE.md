# GPS 模块 — SR2631Z3 单北斗定位驱动
# GPS Module — SR2631Z3 Single-BeiDou Positioning Driver

---

## 硬件概述 / Hardware Overview

| 项目 Item | 规格 Specification |
|-----------|-------------------|
| 模组型号 Module Model | SR2631Z3（星瑞达 StarChip） |
| 定位系统 Positioning System | 单北斗 BDS-only（BDS-B1I / BDS-B1C） |
| 通信接口 Interface | UART TTL（3.3 V / 5 V 兼容 tolerant） |
| 默认波特率 Default Baud Rate | **115200 bps** |
| 输出协议 Output Protocol | NMEA 0183 — 报文头 talker ID `$BD` |
| 更新频率 Update Rate | 1–10 Hz（默认 default 1 Hz） |
| 灵敏度 Sensitivity | 捕获 Acquisition −161 dBm，跟踪 Tracking −158 dBm |
| 冷启动首次定位 TTFF (Cold Start) | < 23 s（典型值 typical） |
| 定位精度 Positioning Accuracy | < 2.0 m (CEP50) |
| 天线类型 Antenna Type | 无源陶瓷贴片 Passive ceramic patch（25 × 25 × 4 mm） |

> **注 Note：** 本模块搭载**无源陶瓷贴片天线**（板载焊接或 IPX 接口），无需外接有源天线。下文*故障排查*一节中提到的 `ANTENNA OPEN` 警告是模块级的检测阈值，对于无源天线通常可忽略。
>
> The module ships with a **passive ceramic patch antenna** (soldered on-board or attached via IPX). It does not require an external active antenna. The `ANTENNA OPEN` warning in § *Troubleshooting* is a module-level detection threshold that can usually be ignored for passive antennas.

---

## 引脚连接 / Pin Connections

| ESP32-S3 引脚 Pin | 模组引脚 Module Pin | 方向 Direction | 说明 Description |
|-------------------|---------------------|----------------|------------------|
| GPIO17 (UART1 TX) | RX | ESP → 模组 Module | 数据发送 Data transmission |
| GPIO18 (UART1 RX) | TX | 模组 Module → ESP | 数据接收 Data reception |
| VCC（3.3 V / 5 V） | VCC | — | 供电 Supply voltage |
| GND | GND | — | 共地 Common ground |

接线示意 / Wiring diagram：

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

> ⚠️ 注意交叉连接 TX ↔ RX（ESP TX → 模组 RX，ESP RX ← 模组 TX）。**不要** TX 接 TX。
>
> Cross over TX ↔ RX (ESP TX → module RX, ESP RX ← module TX). Do **not** wire TX→TX.

---

## 软件架构 / Software Architecture

### 文件结构 / File Layout

```
components/cone_device/
├── include/
│   └── cone_device/
│       └── gps_module.h         # 公共 API（配置 + 状态结构体、函数）
                                  # Public API (config + status structs, functions)
├── src/
│   └── gps_module.cpp           # 驱动实现（UART 初始化 + NMEA 解析器）
                                  # Driver implementation (UART init + NMEA parser)
├── GPS_MODULE.md                # 本文档 This document
└── CMakeLists.txt               # 构建注册（含 gps_module.cpp）
                                  # Build registration (includes gps_module.cpp)
```

### 设计决策 / Design Decisions

| 决策 Decision | 理由 Rationale |
|---------------|---------------|
| **仅解析 `$BD` 前缀 Only `$BD` parsing** | 模块为单北斗；所有语句均携带 `$BD` 报文头，忽略 `$GP` / `$GL` / `$GA` 语句。Module is single-BeiDou; all sentences carry `$BD` talker ID. |
| **双语句容错 Dual-sentence fallback** | 主解析 `$BDRMC`（含 A/V 状态标志），备用 `$BDGGA`（通过 fix quality 验证），冗余防止单句损坏。Primary: `$BDRMC` (A/V flag). Fallback: `$BDGGA` (fix quality). Redundancy against corruption. |
| **非阻塞轮询 Non-blocking tick** | `tick_gps()` 使用零超时 UART 读取，调用方控制调度频率（建议 10 Hz）。`tick_gps()` polls with zero-timeout read; caller controls frequency (recommended 10 Hz). |
| **行累加器 Line-oriented accumulator** | 内部环形缓冲区将原始 UART 字节按 `\n` 分隔组装成 NMEA 行再解析，超过 128 字节的行被丢弃。Internal ring buffer assembles raw bytes into `\n`-delimited lines; lines > 128 bytes are discarded. |
| **编译时开关 Compile-time gating** | 整个驱动（含静态状态）可通过 Kconfig `CONFIG_CONE_DEVICE_ENABLE_GPS` 在构建时移除。Entire driver gated by Kconfig symbol. |

---

## Kconfig 配置 / Kconfig

通过项目配置启用 GPS 模块 / Enable through project configuration：

```
idf.py menuconfig
  → Component config  →  Cone device modules  →  [*] Enable GPS module
```

或直接在 `sdkconfig` 中设置 / Or set directly in `sdkconfig`：

```
CONFIG_CONE_DEVICE_ENABLE_GPS=y
```

禁用时的行为 / When disabled：

- `setup_gps()` 返回 `false` / returns `false`
- `GpsStatus::last_error` 设为 `"disabled"`
- 所有其他函数安全空转（状态保持零初始化） / All other functions are safe no-ops

---

## API 参考 / API Reference

### 配置结构体 / Configuration Structure

```cpp
namespace cone_device {

struct GpsModuleConfig {
    int uart_port = 1;           // UART 外设编号 (0/1/2)
                                 // UART peripheral number
    int tx_pin = 17;             // 驱动模组 RX 的 GPIO
                                 // GPIO driving module RX
    int rx_pin = 18;             // 接收模组 TX 的 GPIO
                                 // GPIO receiving module TX
    uint32_t baud_rate = 115200; // 总线波特率 Bus baud rate
    uint32_t stale_after_ms = 5000; // 预留：数据过期阈值 (reserved)
};

}
```

### 状态结构体 / Status Structure

```cpp
struct GpsStatus {
    bool enabled = false;        // 模块是否已启用（Kconfig）
                                 // Module is enabled
    bool initialized = false;    // UART 硬件是否成功初始化
                                 // UART hardware initialised successfully
    bool has_fix = false;        // 当前定位数据是否有效
                                 // Position data is currently valid
    double longitude = 0.0;      // 经度，十进制角度（WGS-84）
                                 // Decimal degrees (WGS-84)
    double latitude = 0.0;       // 纬度，十进制角度（WGS-84）
                                 // Decimal degrees (WGS-84)
    float accuracy_m = 0.0f;     // 估计定位精度（米）
                                 // Estimated positional accuracy in metres
    uint32_t last_fix_age_ms = 0; // 距上次有效定位的毫秒数
                                  // ms since last valid fix
    std::string last_error;      // 最近一次错误描述
                                 // Most recent error description
};

}
```

### 函数接口 / Functions

| 函数 Function | 说明 Description |
|--------------|-----------------|
| `setup_gps(config)` | 初始化 UART、配置引脚、启动驱动；成功返回 `true`。Initialise UART, configure pins, start driver. Returns `true` on success. |
| `tick_gps()` | 非阻塞轮询：读取 UART、解析 NMEA、更新内部状态。每 50–100 ms 调用一次。Non-blocking poll: read UART, parse NMEA, update state. Call every 50–100 ms. |
| `deinit_gps()` | 卸载 UART 驱动、复位内部状态为默认值。Uninstall UART driver, reset state to defaults. |
| `gps_status()` | 返回当前 `GpsStatus` 的副本（值语义，线程安全）。Return a copy of current `GpsStatus` (thread-safe by value). |
| `get_lat()` | 便捷内联函数 — 返回当前纬度（十进制角度）。Return current latitude in decimal degrees. |
| `get_lon()` | 便捷内联函数 — 返回当前经度（十进制角度）。Return current longitude in decimal degrees. |
| `data_valid()` | 便捷内联函数 — `gps_status().has_fix` 的简写。Shorthand for `gps_status().has_fix`. |

> **线程安全 Thread safety：** 内部状态是模块级结构体，仅由 `tick_gps()` 写入。`gps_status()` 返回**副本**，因此只要调用方在使用副本期间不混用 `tick_gps()`，就无需加锁。
>
> Internal state is module-level, written only by `tick_gps()`. `gps_status()` returns a **copy**; no synchronisation needed if the caller consumes the copy before the next tick.

---

## NMEA 语句参考 / NMEA Sentence Reference

### `$BDRMC` — 推荐最小定位信息（首选 / Preferred）

```
$BDRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a*hh
         ↑       ↑ ↑         ↑          ↑
        time  status lat      NS  lon     EW ...
```

| 字段 Field | 内容 Content | 值 Values |
|------------|-------------|-----------|
| 2 | 状态 Status | `A` = 数据有效 valid；`V` = 无效 void (no fix) |
| 3 | 纬度 Latitude | NMEA `DDMM.MMMMM` |
| 4 | 南北指示 N/S indicator | `N` / `S` |
| 5 | 经度 Longitude | NMEA `DDDMM.MMMMM` |
| 6 | 东西指示 E/W indicator | `E` / `W` |

### `$BDGGA` — 定位数据（备用 / Fallback）

```
$BDGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,xx,xxxx*hh
                ↑         ↑          ↑ ↑
               lat       NS  lon     EW  fix_quality ...
```

| 字段 Field | 内容 Content | 值 Values |
|------------|-------------|-----------|
| 6 | 定位质量 Fix quality | `0` = 无效 invalid；`1` = GPS 定位 fix；`2` = DGPS 差分定位 |
| 7 | 使用卫星数 Satellites used | 计数 Count (0–99) |

### 未解析的语句 / Sentences NOT Parsed

驱动忽略模块发射的以下标准 NMEA 语句 / The driver ignores the following standard NMEA sentences：

| 语句 Sentence | 内容 Content | 原因 Reason |
|--------------|-------------|-------------|
| `$BDVTG` | 对地航向 Course over ground | 定位不需要 Not needed for positioning |
| `$BDGSA` | DOP 与活动卫星 DOP & active satellites | 未实现 No handler |
| `$BDGSV` | 可见卫星 Satellites in view | 未实现（多句、冗长）Not parsed (verbose, multi-sentence) |
| `$BDGLL` | 地理位置（经纬度） Geographic position | 与 `$BDRMC` / `$BDGGA` 重复 Duplicates |
| `$BDZDA` | 日期与时间 Date & time | 未解析（无 RTC 同步）Not parsed (no RTC) |
| `$GPTXT` | 厂商文本消息 Manufacturer text | 可能携带 `ANTENNA OPEN` 警告 May carry antenna warnings |

---

## 坐标转换 / Coordinate Conversion

模块输出为 **NMEA 格式**（度 + 十进制分）。驱动自动转换为 **十进制度（WGS-84）**。

The module outputs in **NMEA format** (degrees + decimal minutes). The driver transparently converts to **decimal degrees (WGS-84)**.

| 格式 Format | 示例 Example | 转换 Conversion |
|-------------|-------------|----------------|
| NMEA 纬度 latitude | `2240.61563` | `22 + 40.61563 ÷ 60 = 22.67693°` |
| NMEA 经度 longitude | `11359.86512` | `113 + 59.86512 ÷ 60 = 113.99775°` |
| 驱动输出 Driver output | `get_lat()` / `get_lon()` | 已是十进制度 Already decimal degrees |

---

## 使用示例 / Usage Examples

### 基础循环（边缘节点模式 / Edge Node Pattern）

```cpp
#include "cone_device/gps_module.h"

extern "C" void app_main() {
    // 默认配置：UART1, GPIO17 TX, GPIO18 RX, 115200 波特
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
        vTaskDelay(pdMS_TO_TICKS(100));  // 10 Hz 轮询 tick
    }
}
```

### 访问完整状态 / Access Full Status

```cpp
auto st = cone_device::gps_status();
if (st.has_fix) {
    printf("lat=%.6f  lon=%.6f  acc=%.1f m\n",
           st.latitude, st.longitude, st.accuracy_m);
} else {
    ESP_LOGW("app", "No fix: %s", st.last_error.c_str());
}
```

### 自定义引脚 / Custom Pin Assignment

```cpp
cone_device::GpsModuleConfig cfg;
cfg.uart_port = 2;    // UART2
cfg.tx_pin = 4;       // TX → GPIO4
cfg.rx_pin = 5;       // RX → GPIO5
cfg.baud_rate = 9600; // 较低波特率 Slower bus
cone_device::setup_gps(cfg);
```

---

## 故障排查 / Troubleshooting

### 症状：接收行数为零 / Symptom: Zero lines received (`Lines received: 0`)

| 原因 Cause | 检查 Check |
|-----------|------------|
| 接线错误 Wiring incorrect | 确认 TX↔RX 交叉连接（非 TX→TX）Verify TX↔RX crossover (not TX→TX) |
| 波特率错误 Wrong baud rate | 确认模块波特率与配置一致。多数 SR2631Z3 为 **115200**；不确定时可试 9600。Confirm module baud rate matches config. Most SR2631Z3 ship at **115200**. Try 9600 if unsure. |
| 模块未上电 Module not powered | 测量模块引脚 VCC 电压，检查电源指示灯。Measure VCC at module pin. Look for a PWR LED. |
| UART 端口冲突 UART port conflict | 部分 ESP32-S3 开发板将 UART1 复用至 USB 桥接芯片，可改用 UART2。Some dev boards route UART1 to USB bridge; use UART2 instead. |

### 症状：有数据但无定位 / Symptom: Data received but no fix (`Fix quality: 0`)

| 原因 Cause | 检查 Check |
|-----------|------------|
| 室内操作 Indoor operation | GPS 信号被建筑严重衰减，移至窗边或室外。Signals are severely attenuated indoors. Move near a window or outdoors. |
| 天线问题 Antenna issue | 模块可能报 `$GPTXT,01,01,01,ANTENNA OPEN*25`。**无源陶瓷天线**下此警告可忽略——室外通常仍能定位。Module may report `ANTENNA OPEN`. For **passive ceramic patch antennas**, this warning can be safely ignored outdoors. |
| 冷启动延迟 Cold start delay | 断电后首次定位需要 20–60 s 开阔天空环境。First fix after power loss takes 20–60 s under open sky. |

### 症状：`$GPTXT` 报告 `ANTENNA OPEN`

这是**模块级天线检测**功能。SR2631Z3 监测天线端口是否有源天线所需的 DC 短路特征。无源陶瓷贴片不呈现该短路，故触发开路警告。

This is a **module-level antenna detection** feature. The SR2631Z3 monitors the antenna port for a DC short expected by an active antenna. Passive ceramic patches do not present this short, triggering the open warning.

**影响 Impact：** 无源天线不受影响，模块在卫星信号足够后仍能正常定位。No impact for passive antennas; the module will still navigate once it acquires sufficient satellite signals.

**处理方式 / To suppress：**
- 忽略此消息（推荐，仅为外观提示）。Ignore (recommended, cosmetic only).
- 如需消除，换用有源天线（需外置偏置器）。Use an active antenna with external bias-T.

### 症状：行数非零但解析计数为零 / Symptom: Lines received non-zero but parser counters stay 0

驱动仅处理 **`$BD`** 前缀的语句。若模块曾被配置为输出 `$GP`（GPS）或 `$GL`（GLONASS）语句，或通过 UART 命令更改了报文头，则解析计数保持为 0。

The driver only handles **`$BD`**-prefixed sentences. If the module was reconfigured for `$GP` or `$GL` output, or the talker ID was changed, parser counts will remain 0.

**验证 Verify：** 用 USB-UART 转换器连接模块 TX 引脚，在 115200 波特率下用串口助手查看原始数据。语句应以 `$BD` 开头。

Connect a USB-UART adapter to the module TX pin and inspect raw output at 115200 baud. Sentences should begin with `$BD`.

---

## 修订历史 / Revision History

| 日期 Date | 变更 Changes |
|-----------|-------------|
| 2026-06-04 | 初始文档；基于 ESP32-S3 实体验证（COM9, USB-Serial/JTAG）。新增 `ANTENNA OPEN` 行为说明。Initial documentation; verified against real hardware. Added `ANTENNA OPEN` behaviour notes. |

---

## 参考资料 / References

- [NMEA 0183 标准 Standard](https://www.nmea.org/content/STANDARDS/NMEA_0183_Standard)
- [SR2631Z3 规格书 Datasheet](/docs/product/SR2631Z3规格书.pdf)
- [ESP-IDF UART 驱动文档 Driver Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/uart.html)
- [cone_device 组件 Component](../cone_device/README.md)
