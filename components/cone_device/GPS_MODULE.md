# GPS 模块 — SR2631Z3 单北斗定位驱动

## 硬件概述

| 项目 | 规格 |
|------|------|
| 模组型号 | SR2631Z3（星瑞达） |
| 定位系统 | 单北斗（BDS-B1I / BDS-B1C） |
| 通信接口 | UART TTL（3.3V / 5V 兼容） |
| 默认波特率 | 115200 bps |
| 输出协议 | NMEA 0183（报文头 `$BD`） |
| 更新频率 | 1–10 Hz（默认 1 Hz） |
| 灵敏度 | 捕获 -161 dBm，跟踪 -158 dBm |
| 首次定位 | 冷启动 < 23 s（典型值） |
| 定位精度 | < 2.0 m（CEP50） |

> 详细电气参数请参考模组规格书 `SR2631Z3规格书.pdf`。

---

## 引脚连接

| ESP32-S3 引脚 | 模组引脚 | 说明 |
|---------------|----------|------|
| GPIO17 (UART1 TX) | RX | ESP → 模组（数据发送） |
| GPIO18 (UART1 RX) | TX | 模组 → ESP（数据接收） |
| VCC | VCC | 3.3V–5V 供电 |
| GND | GND | 共地 |

接线示意：

```
┌──────────────┐         ┌──────────────┐
│   ESP32-S3   │         │   SR2631Z3   │
│              │         │              │
│  GPIO17/TX ──┼─────────┤ RX           │
│              │         │              │
│  GPIO18/RX ──┼─────────┤ TX           │
│              │         │              │
│  3.3V ───────┼─────────┤ VCC          │
│              │         │              │
│  GND ────────┼─────────┤ GND          │
└──────────────┘         └──────────────┘
```

---

## 软件架构

### 文件结构

```
components/cone_device/
├── include/
│   └── cone_device/
│       └── gps_module.h       # 公共头文件（配置结构体、状态结构体、API）
├── src/
│   └── gps_module.cpp          # 驱动实现（UART 初始化 + NMEA 解析）
├── GPS_MODULE.md               # 本说明文档
└── CMakeLists.txt              # 构建注册（已包含 gps_module.cpp）
```

### 设计思路

- **三层分离**：配置 (`GpsModuleConfig`) → 状态 (`GpsStatus`) → 接口 (`setup_*`, `tick_*`, `get_*`)
- **仅解析 `$BD` 前缀**：因模组是单北斗，所有 NMEA 报文头为 `$BD`（`$BDRMC`、`$BDGGA` 等），不匹配 `$GP` 或 `$GL`
- **双源容错**：主解析 `$BDRMC`（含 A/V 状态标志），备选 `$BDGGA`（通过 fix quality 验证）
- **非阻塞轮询**：`tick_gps()` 使用零超时 UART 读取，不阻塞主循环

---

## API 参考

### 数据结构

```cpp
namespace cone_device {

struct GpsModuleConfig {
  int uart_port = 1;         // UART 端口号（UART1）
  int tx_pin = 17;           // TX 引脚（模组 RX 端）
  int rx_pin = 18;           // RX 引脚（模组 TX 端）
  uint32_t baud_rate = 115200; // 波特率
  uint32_t stale_after_ms = 5000; // 数据超时阈值（保留字段）
};

struct GpsStatus {
  bool enabled = false;      // 模块是否启用
  bool initialized = false;  // 硬件是否成功初始化
  bool has_fix = false;      // 是否已定位（数据有效标志）
  double longitude = 0.0;    // 经度（十进制角度）
  double latitude = 0.0;     // 纬度（十进制角度）
  float accuracy_m = 0.0f;   // 精度估计（米）
  uint32_t last_fix_age_ms = 0; // 距上次定位的毫秒数
  std::string last_error;    // 最近一次错误描述
};

}
```

### 函数接口

| 函数 | 说明 |
|------|------|
| `setup_gps(cfg)` | 初始化 UART、配置引脚、启动驱动；成功返回 `true` |
| `tick_gps()` | 非阻塞轮询 UART、解析 NMEA 报文、更新状态（应周期性调用） |
| `deinit_gps()` | 卸载 UART 驱动、清除状态 |
| `gps_status()` | 返回完整 `GpsStatus` 结构体（含经纬度、定位标志、错误信息等） |
| `get_lat()` | 便捷接口：返回当前纬度（十进制角度） |
| `get_lon()` | 便捷接口：返回当前经度（十进制角度） |
| `data_valid()` | 便捷接口：返回当前数据是否有效（`has_fix`） |

### 解析的 NMEA 报文

**`$BDRMC`（Recommended Minimum）**

```
$BDRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a*hh
         ↑       ↑  ↑         ↑          ↑
         时间   状态 纬度      N/S       经度     E/W  速度  航向  日期  ...
```

- 字段 2：状态 — `A` = 有效数据；`V` = 无效（卫星丢失等）
- 字段 3、5：纬度和经度（NMEA 格式 `DDMM.MMMMM` / `DDDMM.MMMMM`）
- 字段 4、6：N/S、E/W 半球标志

**`$BDGGA`（Fix Data）**

```
$BDGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,xx,xxxx*hh
                ↑         ↑          ↑ ↑
               纬度      N/S       经度 E/W  fix_quality  ...
```

- 字段 6：fix quality — `0` = 无效；`1` = GPS；`2` = DGPS

---

## 使用示例

### 基础用法

```cpp
#include "cone_device/gps_module.h"

extern "C" void app_main() {
    // 默认配置（UART1, TX=17, RX=18, 115200）
    cone_device::GpsModuleConfig cfg;
    if (!cone_device::setup_gps(cfg)) {
        ESP_LOGE("app", "GPS init failed");
        return;
    }

    while (true) {
        cone_device::tick_gps();  // 非阻塞轮询

        if (cone_device::data_valid()) {
            double lat = cone_device::get_lat();
            double lon = cone_device::get_lon();
            ESP_LOGI("app", "%.6f, %.6f", lat, lon);
        }

        vTaskDelay(pdMS_TO_TICKS(100));  // 10 Hz 轮询
    }
}
```

### 访问完整状态

```cpp
auto status = cone_device::gps_status();
if (status.has_fix) {
    printf("lat=%.6f, lon=%.6f\n", status.latitude, status.longitude);
} else {
    printf("No fix: %s\n", status.last_error.c_str());
}
```

### 使用非默认引脚

```cpp
cone_device::GpsModuleConfig cfg;
cfg.uart_port = 2;   // 改为 UART2
cfg.tx_pin = 4;      // TX → GPIO4
cfg.rx_pin = 5;      // RX → GPIO5
cone_device::setup_gps(cfg);
```

---

## 配置与启用

在 ESP-IDF 项目配置中启用：

```
idf.py menuconfig
  → Component config  →  Cone device modules  →  [*] Enable GPS module
```

或直接在 `sdkconfig` 中添加：

```
CONFIG_CONE_DEVICE_ENABLE_GPS=y
```

> 当模块未启用时，`setup_gps()` 返回 `false`，`last_error` 为 `"disabled"`，所有接口安全空转。

---

## 坐标系说明

模组输出的经纬度为 **WGS-84** 坐标系下的 NMEA 格式：

| 格式 | 示例 | 转换公式 |
|------|------|----------|
| 纬度 NMEA | `2240.61563` | `22 + 40.61563/60 = 22.67693°` |
| 经度 NMEA | `11359.86512` | `113 + 59.86512/60 = 113.99775°` |

本驱动自动完成 NMEA → 十进制角度的转换，开发者直接使用 `get_lat()` / `get_lon()` 即可得到标准经纬度。

---

## 调试建议

1. **检查 UART 连通性**：用逻辑分析仪或串口助手查看模组 TX 脚是否有 NMEA 数据输出
2. **确认波特率**：SR2631Z3 默认 115200，若模组曾被配置为其他波特率需先恢复
3. **查看日志**：驱动使用 `ESP_LOGI` / `ESP_LOGE` 输出初始化和错误信息（tag: `cone_device.gps`）
4. **天线检查**：确保模组已连接有源天线，室外首次定位可能需要 20–60 秒
5. **`data_valid()` 先检查**：始终在使用经纬度之前检查数据是否有效

---

## 兼容性

| 条件 | 支持 |
|------|------|
| ESP-IDF 版本 | v5.x（VFS UART 驱动） |
| 模组型号 | SR2631Z3（星瑞达单北斗） |
| 其他 $BD 模组 | 协议兼容，引脚按实际调整即可 |
| $GP/$GL 混合模组 | 需修改 `process_nmea_line()` 中的前缀判断逻辑 |
