#include "cone_device/gps_module.h"

#include <cctype>
#include <cmath>
#include <cstring>

#include "driver/uart.h"
#include "esp_log.h"
#include "sdkconfig.h"

namespace cone_device {
namespace {

static const char* kTag = "cone_device.gps";

// UART ring-buffer and internal line-buffer sizes.
constexpr size_t kUartBufSize = 1024;
constexpr size_t kLineBufSize = 128;

// ---------------------------------------------------------------------------
// Internal driver state
// ---------------------------------------------------------------------------
struct GpsState {
  GpsStatus status;
  int uart_port = -1;           // stored UART port from config
  bool uart_initialized = false;

  // Partial-line accumulator.
  char line_buffer[kLineBufSize];
  size_t line_pos = 0;
};

GpsState g_state;

// ---------------------------------------------------------------------------
// NMEA string helpers
// ---------------------------------------------------------------------------

// Return a pointer to the start of the N-th comma-separated field (0-based),
// or nullptr when the field does not exist.
const char* field_start(const char* line, int field_index) {
  int cur = 0;
  while (*line) {
    if (cur == field_index) return line;
    if (*line == ',') ++cur;
    ++line;
  }
  return (cur == field_index) ? line : nullptr;
}

// Convert an NMEA-format latitude field ("DDMM.MMMMM") into decimal degrees.
// Returns false on parse failure.
bool parse_latitude(const char* field, const char* ns, double& out) {
  if (!field || !*field) return false;

  char* end = nullptr;
  double val = strtod(field, &end);
  if (end == field) return false;

  int degrees = static_cast<int>(val / 100.0);
  double minutes = val - degrees * 100.0;
  out = degrees + minutes / 60.0;
  if (ns && *ns == 'S') out = -out;
  return true;
}

// Convert an NMEA-format longitude field ("DDDMM.MMMMM") into decimal degrees.
// Returns false on parse failure.
bool parse_longitude(const char* field, const char* ew, double& out) {
  if (!field || !*field) return false;

  char* end = nullptr;
  double val = strtod(field, &end);
  if (end == field) return false;

  int degrees = static_cast<int>(val / 100.0);
  double minutes = val - degrees * 100.0;
  out = degrees + minutes / 60.0;
  if (ew && *ew == 'W') out = -out;
  return true;
}

// ---------------------------------------------------------------------------
// Per-sentence parsers
// ---------------------------------------------------------------------------

// $BDRMC — Recommended Minimum.
//   Fields: 0=$BDRMC, 1=time, 2=status(A/V), 3=lat, 4=NS, 5=lon, 6=EW, ...
void parse_rmc(const char* line) {
  const char* status_f  = field_start(line, 2);
  const char* lat_f     = field_start(line, 3);
  const char* ns_f      = field_start(line, 4);
  const char* lon_f     = field_start(line, 5);
  const char* ew_f      = field_start(line, 6);

  if (!status_f) return;

  // Validity.
  if (*status_f == 'A') {
    g_state.status.has_fix = true;
  } else if (*status_f == 'V') {
    g_state.status.has_fix = false;
    return;   // No position data when the status is void.
  } else {
    return;
  }

  // Position.
  double lat = 0.0, lon = 0.0;
  if (!parse_latitude(lat_f, ns_f, lat)) return;
  if (!parse_longitude(lon_f, ew_f, lon)) return;

  g_state.status.latitude  = lat;
  g_state.status.longitude = lon;
}

// $BDGGA — Fix data.
//   Fields: 0=$BDGGA, 1=time, 2=lat, 3=NS, 4=lon, 5=EW, 6=fix_quality, ...
void parse_gga(const char* line) {
  const char* lat_f  = field_start(line, 2);
  const char* ns_f   = field_start(line, 3);
  const char* lon_f  = field_start(line, 4);
  const char* ew_f   = field_start(line, 5);
  const char* q_f    = field_start(line, 6);

  if (!q_f) return;

  // Fix quality: 0 = invalid, 1+ = valid.
  if (*q_f == '0') {
    g_state.status.has_fix = false;
    return;
  }

  double lat = 0.0, lon = 0.0;
  if (!parse_latitude(lat_f, ns_f, lat)) return;
  if (!parse_longitude(lon_f, ew_f, lon)) return;

  g_state.status.latitude  = lat;
  g_state.status.longitude = lon;
  g_state.status.has_fix   = true;
}

// ---------------------------------------------------------------------------
// Sentence router
// ---------------------------------------------------------------------------

void process_nmea_line(const char* line) {
  // Only interested in BeiDou sentences ($BDxxx).
  if (line[0] != '$' || line[1] != 'B' || line[2] != 'D') return;

  const char* type = line + 3;  // skip "$BD"

  if      (type[0] == 'R' && type[1] == 'M' && type[2] == 'C') parse_rmc(line);
  else if (type[0] == 'G' && type[1] == 'G' && type[2] == 'A') parse_gga(line);
  // Other $BD sentences ($BDGSA, $BDGSV, …) are ignored on purpose.
}

// ---------------------------------------------------------------------------
// UART data feeder
// ---------------------------------------------------------------------------

void feed_uart_data(const char* data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    const char c = data[i];

    if (c == '\n') {
      // Complete line — null-terminate and process.
      g_state.line_buffer[g_state.line_pos] = '\0';
      if (g_state.line_pos > 4) {   // minimum plausible: "$BDxx"
        process_nmea_line(g_state.line_buffer);
      }
      g_state.line_pos = 0;
    } else if (c == '\r') {
      // Ignore carriage-return.
    } else if (g_state.line_pos < kLineBufSize - 1) {
      g_state.line_buffer[g_state.line_pos++] = c;
    } else {
      // Line too long — discard and reset to avoid truncation garbage.
      g_state.line_pos = 0;
    }
  }
}

}  // anonymous namespace

// ===========================================================================
// Public API
// ===========================================================================

bool setup_gps(const GpsModuleConfig& config) {
#if CONFIG_CONE_DEVICE_ENABLE_GPS
  g_state = {};
  g_state.status.enabled   = true;
  g_state.uart_port        = config.uart_port;

  uart_config_t uart_config = {
    .baud_rate  = static_cast<int>(config.baud_rate),
    .data_bits  = UART_DATA_8_BITS,
    .parity     = UART_PARITY_DISABLE,
    .stop_bits  = UART_STOP_BITS_1,
    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
  };

  esp_err_t err = uart_param_config(
      static_cast<uart_port_t>(config.uart_port), &uart_config);
  if (err != ESP_OK) {
    g_state.status.last_error = "uart_param_config failed";
    ESP_LOGE(kTag, "uart_param_config returned %d", err);
    return false;
  }

  err = uart_set_pin(
      static_cast<uart_port_t>(config.uart_port),
      config.tx_pin,
      config.rx_pin,
      UART_PIN_NO_CHANGE,
      UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    g_state.status.last_error = "uart_set_pin failed";
    ESP_LOGE(kTag, "uart_set_pin returned %d", err);
    return false;
  }

  err = uart_driver_install(
      static_cast<uart_port_t>(config.uart_port),
      kUartBufSize,   // rx ring buffer
      0,              // tx buffer (not needed)
      0,              // queue size
      nullptr,        // queue handle
      0);             // interrupt flags
  if (err != ESP_OK) {
    g_state.status.last_error = "uart_driver_install failed";
    ESP_LOGE(kTag, "uart_driver_install returned %d", err);
    return false;
  }

  // Flush any stale data that may have accumulated before init.
  uart_flush(static_cast<uart_port_t>(config.uart_port));

  g_state.uart_initialized   = true;
  g_state.status.initialized = true;
  g_state.status.last_error.clear();

  ESP_LOGI(kTag, "GPS UART%d initialised  TX=%d  RX=%d  %lu baud",
           config.uart_port, config.tx_pin, config.rx_pin,
           (unsigned long)config.baud_rate);
  return true;
#else
  g_state                = {};
  g_state.status.last_error = "disabled";
  return false;
#endif
}

void tick_gps() {
#if CONFIG_CONE_DEVICE_ENABLE_GPS
  if (!g_state.uart_initialized) return;

  uint8_t buf[kUartBufSize];
  int len = uart_read_bytes(
      static_cast<uart_port_t>(g_state.uart_port),
      buf,
      kUartBufSize,
      0);   // 0 ticks timeout — non-blocking poll.

  if (len > 0) {
    feed_uart_data(reinterpret_cast<const char*>(buf),
                   static_cast<size_t>(len));
  }
#else
  // NOP when disabled.
#endif
}

void deinit_gps() {
#if CONFIG_CONE_DEVICE_ENABLE_GPS
  if (g_state.uart_initialized) {
    uart_driver_delete(static_cast<uart_port_t>(g_state.uart_port));
    g_state.uart_initialized = false;
  }
#endif
  g_state.status = {};
}

GpsStatus gps_status() {
  return g_state.status;
}

}  // namespace cone_device
