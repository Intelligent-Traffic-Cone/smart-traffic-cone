#include <Arduino.h>

#include <cstdlib>
#include <cstring>

#ifndef GPS_UART_NUM
#define GPS_UART_NUM 1
#endif

#ifndef GPS_TX_PIN
#define GPS_TX_PIN 17
#endif

#ifndef GPS_RX_PIN
#define GPS_RX_PIN 18
#endif

#ifndef GPS_BAUD
#define GPS_BAUD 115200
#endif

namespace {

constexpr size_t kLineBufSize = 256;

HardwareSerial g_gps_serial(GPS_UART_NUM);
char g_line_buffer[kLineBufSize];
size_t g_line_pos = 0;
bool g_discarding_line = false;

bool g_has_fix = false;
double g_latitude = 0.0;
double g_longitude = 0.0;
int g_satellites = 0;
float g_altitude_m = 0.0f;
int g_fix_quality = 0;
uint32_t g_last_fix_ms = 0;
uint32_t g_line_count = 0;
uint32_t g_sentence_count[4] = {};

const char* field_start(const char* line, int index) {
  int current = 0;
  while (*line) {
    if (current == index) return line;
    if (*line == ',') ++current;
    ++line;
  }
  return (current == index) ? line : nullptr;
}

bool parse_latitude(const char* field, const char* ns, double& out) {
  if (!field || !*field) return false;

  char* end = nullptr;
  const double value = strtod(field, &end);
  if (end == field) return false;

  const int degrees = static_cast<int>(value / 100.0);
  const double minutes = value - degrees * 100.0;
  out = degrees + minutes / 60.0;
  if (ns && *ns == 'S') out = -out;
  return true;
}

bool parse_longitude(const char* field, const char* ew, double& out) {
  if (!field || !*field) return false;

  char* end = nullptr;
  const double value = strtod(field, &end);
  if (end == field) return false;

  const int degrees = static_cast<int>(value / 100.0);
  const double minutes = value - degrees * 100.0;
  out = degrees + minutes / 60.0;
  if (ew && *ew == 'W') out = -out;
  return true;
}

bool is_supported_talker(const char* talker) {
  return std::strncmp(talker, "GP", 2) == 0 || std::strncmp(talker, "BD", 2) == 0;
}

void parse_gga(const char* line) {
  const char* lat_field = field_start(line, 2);
  const char* ns_field = field_start(line, 3);
  const char* lon_field = field_start(line, 4);
  const char* ew_field = field_start(line, 5);
  const char* quality_field = field_start(line, 6);
  const char* satellites_field = field_start(line, 7);
  const char* altitude_field = field_start(line, 9);

  if (!quality_field) return;

  g_fix_quality = (*quality_field >= '0' && *quality_field <= '9')
      ? (*quality_field - '0')
      : 0;
  g_has_fix = g_fix_quality > 0;
  if (!g_has_fix) return;

  if (!parse_latitude(lat_field, ns_field, g_latitude)) return;
  if (!parse_longitude(lon_field, ew_field, g_longitude)) return;

  if (satellites_field) {
    char* end = nullptr;
    const long satellites = strtol(satellites_field, &end, 10);
    if (end != satellites_field) g_satellites = static_cast<int>(satellites);
  }

  if (altitude_field) {
    char* end = nullptr;
    const float altitude = strtof(altitude_field, &end);
    if (end != altitude_field) g_altitude_m = altitude;
  }

  g_last_fix_ms = millis();
}

void parse_rmc(const char* line) {
  const char* status_field = field_start(line, 2);
  const char* lat_field = field_start(line, 3);
  const char* ns_field = field_start(line, 4);
  const char* lon_field = field_start(line, 5);
  const char* ew_field = field_start(line, 6);

  if (!status_field) return;

  if (*status_field == 'V') {
    g_has_fix = false;
    return;
  }
  if (*status_field != 'A') return;

  if (!parse_latitude(lat_field, ns_field, g_latitude)) return;
  if (!parse_longitude(lon_field, ew_field, g_longitude)) return;

  g_has_fix = true;
  g_last_fix_ms = millis();
}

void process_nmea_line(const char* line) {
  ++g_line_count;
  if (line[0] != '$' || std::strlen(line) < 6) return;

  const char* talker = line + 1;
  const char* type = line + 3;
  if (!is_supported_talker(talker)) return;

  if (std::strncmp(type, "GGA", 3) == 0) {
    ++g_sentence_count[std::strncmp(talker, "GP", 2) == 0 ? 0 : 2];
    parse_gga(line);
  } else if (std::strncmp(type, "RMC", 3) == 0) {
    ++g_sentence_count[std::strncmp(talker, "GP", 2) == 0 ? 1 : 3];
    parse_rmc(line);
  }
}

void feed_uart() {
  while (g_gps_serial.available() > 0) {
    const char c = static_cast<char>(g_gps_serial.read());

    if (g_discarding_line) {
      if (c == '\n') {
        g_discarding_line = false;
        g_line_pos = 0;
      }
      continue;
    }

    if (c == '\r') continue;

    if (c == '\n') {
      g_line_buffer[g_line_pos] = '\0';
      if (g_line_pos > 5) {
        process_nmea_line(g_line_buffer);
      }
      g_line_pos = 0;
      continue;
    }

    if (g_line_pos < kLineBufSize - 1) {
      g_line_buffer[g_line_pos++] = c;
    } else {
      g_line_pos = 0;
      g_discarding_line = true;
    }
  }
}

void print_status() {
  Serial.println("----------------------------------------");
  Serial.printf("Lines received : %lu\n", static_cast<unsigned long>(g_line_count));
  Serial.printf("  GPGGA        : %lu\n",
                static_cast<unsigned long>(g_sentence_count[0]));
  Serial.printf("  GPRMC        : %lu\n",
                static_cast<unsigned long>(g_sentence_count[1]));
  Serial.printf("  BDGGA        : %lu\n",
                static_cast<unsigned long>(g_sentence_count[2]));
  Serial.printf("  BDRMC        : %lu\n",
                static_cast<unsigned long>(g_sentence_count[3]));
  Serial.printf("Fix quality    : %d\n", g_fix_quality);
  Serial.printf("Satellites     : %d\n", g_satellites);
  Serial.printf("Fix            : %s\n", g_has_fix ? "YES" : "NO");

  if (g_has_fix) {
    const uint32_t age_ms = millis() - g_last_fix_ms;
    Serial.printf("Latitude       : %.6f\n", g_latitude);
    Serial.printf("Longitude      : %.6f\n", g_longitude);
    Serial.printf("Altitude       : %.1f m\n", g_altitude_m);
    Serial.printf("Fix age        : %lu ms\n",
                  static_cast<unsigned long>(age_ms));
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("GPS Module Test - PIO + Arduino");
  Serial.printf("UART%d TX=%d RX=%d baud=%lu\n",
                GPS_UART_NUM,
                GPS_TX_PIN,
                GPS_RX_PIN,
                static_cast<unsigned long>(GPS_BAUD));

  g_gps_serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  g_gps_serial.setTimeout(0);

  Serial.println("UART ready. Waiting for NMEA data...");
}

void loop() {
  feed_uart();

  static uint32_t last_status_ms = 0;
  const uint32_t now = millis();
  if (now - last_status_ms >= 2000) {
    last_status_ms = now;
    print_status();
  }

  delay(50);
}
