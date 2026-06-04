/**
 * @file main.cpp
 * @brief GPS Module Test — PlatformIO + Arduino Framework
 *
 * Reads NMEA sentences from the SR2631Z3 (single-BeiDou) GPS module
 * over UART and prints parsed position / fix information to the console.
 *
 * Default pins:  TX=17, RX=18, UART1, 115200 baud
 * Override via platformio.ini build_flags.
 *
 * Hardware connections (ESP32-S3 → SR2631Z3):
 *   GPIO17 (TX) → RX
 *   GPIO18 (RX) → TX
 *   VCC         → 3.3V / 5V
 *   GND         → GND
 */

#include <Arduino.h>
#include <cstring>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Default configuration (overridable via build_flags)
// ---------------------------------------------------------------------------
#ifndef GPS_TX_PIN
#define GPS_TX_PIN 17
#endif
#ifndef GPS_RX_PIN
#define GPS_RX_PIN 18
#endif
#ifndef GPS_BAUD
#define GPS_BAUD 115200
#endif

// ---------------------------------------------------------------------------
// NMEA line buffer
// ---------------------------------------------------------------------------
static constexpr size_t kLineBufSize = 256;
static char s_line_buf[kLineBufSize];
static size_t s_line_pos = 0;

// ---------------------------------------------------------------------------
// Parsed state
// ---------------------------------------------------------------------------
static bool     s_has_fix     = false;
static double   s_lat         = 0.0;
static double   s_lon         = 0.0;
static int      s_sats        = 0;
static float    s_altitude    = 0.0f;
static int      s_fix_quality = 0;
static uint32_t s_last_fix_ms = 0;
static uint32_t s_line_count  = 0;
// [GPGGA, GPRMC, BDGGA, BDRMC]
static uint32_t s_sentence_count[4] = {};

// ---------------------------------------------------------------------------
// NMEA field helpers
// ---------------------------------------------------------------------------

static const char* field_start(const char* line, int idx) {
  int cur = 0;
  while (*line) {
    if (cur == idx) return line;
    if (*line == ',') ++cur;
    ++line;
  }
  return (cur == idx) ? line : nullptr;
}

static bool parse_lat(const char* field, const char* ns, double& out) {
  if (!field || !*field) return false;
  char* end = nullptr;
  double v = strtod(field, &end);
  if (end == field) return false;
  int d = static_cast<int>(v / 100.0);
  double m = v - d * 100.0;
  out = d + m / 60.0;
  if (ns && *ns == 'S') out = -out;
  return true;
}

static bool parse_lon(const char* field, const char* ew, double& out) {
  if (!field || !*field) return false;
  char* end = nullptr;
  double v = strtod(field, &end);
  if (end == field) return false;
  int d = static_cast<int>(v / 100.0);
  double m = v - d * 100.0;
  out = d + m / 60.0;
  if (ew && *ew == 'W') out = -out;
  return true;
}

// ---------------------------------------------------------------------------
// Per-sentence parsers
// ---------------------------------------------------------------------------

static void parse_gga(const char* line) {
  // $--GGA,time,lat,NS,lon,EW,quality,numSats,HDOP,alt,M,sep,M,...
  const char* lat_f  = field_start(line, 2);
  const char* ns_f   = field_start(line, 3);
  const char* lon_f  = field_start(line, 4);
  const char* ew_f   = field_start(line, 5);
  const char* q_f    = field_start(line, 6);
  const char* sats_f = field_start(line, 7);
  const char* alt_f  = field_start(line, 9);

  if (!q_f) return;

  int q = (*q_f >= '0' && *q_f <= '9') ? (*q_f - '0') : 0;
  s_fix_quality = q;
  s_has_fix = (q > 0);
  if (q == 0) return;

  if (!parse_lat(lat_f, ns_f, s_lat)) return;
  if (!parse_lon(lon_f, ew_f, s_lon)) return;

  if (sats_f) {
    char* end = nullptr;
    int n = static_cast<int>(strtol(sats_f, &end, 10));
    if (end != sats_f) s_sats = n;
  }
  if (alt_f) {
    char* end = nullptr;
    float a = strtof(alt_f, &end);
    if (end != alt_f) s_altitude = a;
  }
  s_last_fix_ms = millis();
}

static void parse_rmc(const char* line) {
  // $--RMC,time,status(A/V),lat,NS,lon,EW,speed,course,date,...
  const char* st_f  = field_start(line, 2);
  const char* lat_f = field_start(line, 3);
  const char* ns_f  = field_start(line, 4);
  const char* lon_f = field_start(line, 5);
  const char* ew_f  = field_start(line, 6);

  if (!st_f) return;

  if (*st_f == 'A') {
    s_has_fix = true;
    if (!parse_lat(lat_f, ns_f, s_lat)) return;
    if (!parse_lon(lon_f, ew_f, s_lon)) return;
    s_last_fix_ms = millis();
  } else if (*st_f == 'V') {
    s_has_fix = false;
  }
}

// ---------------------------------------------------------------------------
// Sentence router
// ---------------------------------------------------------------------------

static void process_nmea(const char* line) {
  ++s_line_count;
  if (line[0] != '$' || strlen(line) < 6) return;

  const char* talker = line + 1;   // "GP" or "BD"
  const char* type   = line + 3;   // "GGA", "RMC", ...

  if      (strncmp(talker, "GP", 2) == 0 && strncmp(type, "GGA", 3) == 0) {
    ++s_sentence_count[0];
    parse_gga(line);
  }
  else if (strncmp(talker, "GP", 2) == 0 && strncmp(type, "RMC", 3) == 0) {
    ++s_sentence_count[1];
    parse_rmc(line);
  }
  else if (strncmp(talker, "BD", 2) == 0 && strncmp(type, "GGA", 3) == 0) {
    ++s_sentence_count[2];
    parse_gga(line);
  }
  else if (strncmp(talker, "BD", 2) == 0 && strncmp(type, "RMC", 3) == 0) {
    ++s_sentence_count[3];
    parse_rmc(line);
  }
}

// ---------------------------------------------------------------------------
// UART feeder
// ---------------------------------------------------------------------------

static void feed_uart() {
  while (Serial1.available()) {
    char c = static_cast<char>(Serial1.read());
    if (c == '\n') {
      s_line_buf[s_line_pos] = '\0';
      if (s_line_pos > 5) {
        process_nmea(s_line_buf);
      }
      s_line_pos = 0;
    } else if (c == '\r') {
      // ignore
    } else if (s_line_pos < kLineBufSize - 1) {
      s_line_buf[s_line_pos++] = c;
    } else {
      s_line_pos = 0;  // overflow guard
    }
  }
}

// ---------------------------------------------------------------------------
// Status printer
// ---------------------------------------------------------------------------

static void print_status() {
  Serial.println(F("═══════════════════════════════════════"));
  Serial.printf("Lines received : %lu\n", (unsigned long)s_line_count);
  Serial.printf("  GPGGA        : %lu\n", (unsigned long)s_sentence_count[0]);
  Serial.printf("  GPRMC        : %lu\n", (unsigned long)s_sentence_count[1]);
  Serial.printf("  BDGGA        : %lu\n", (unsigned long)s_sentence_count[2]);
  Serial.printf("  BDRMC        : %lu\n", (unsigned long)s_sentence_count[3]);
  Serial.printf("Fix quality    : %d  (0=no, 1=GPS, 2=DGPS)\n", s_fix_quality);
  Serial.printf("Satellites     : %d\n", s_sats);
  Serial.printf("Fix            : %s\n", s_has_fix ? "YES" : "NO");

  if (s_has_fix) {
    uint32_t age = millis() - s_last_fix_ms;
    Serial.printf("Latitude       : %.6f\n", s_lat);
    Serial.printf("Longitude      : %.6f\n", s_lon);
    Serial.printf("Altitude       : %.1f m\n", s_altitude);
    Serial.printf("Fix age        : %lu ms\n", (unsigned long)age);
  }
  Serial.println(F("───────────────────────────────────────"));
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println(F("========================================"));
  Serial.println(F("  GPS Module Test — PIO + Arduino"));
  Serial.println(F("========================================"));
  Serial.printf("UART1  TX=%d  RX=%d  %lu baud\n",
                GPS_TX_PIN, GPS_RX_PIN, (unsigned long)GPS_BAUD);

  // Initialise GPS UART (HardwareSerial1)
  Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial1.setTimeout(0);  // non-blocking reads

  Serial.println(F("UART ready — waiting for GPS data..."));
  Serial.println(F("(if nothing appears, check wiring and baud rate)"));
  Serial.println();
}

void loop() {
  // Read all available UART data (non-blocking)
  feed_uart();

  // Print status every ~2 seconds
  static uint32_t last_status = 0;
  uint32_t now = millis();
  if (now - last_status >= 2000) {
    last_status = now;
    print_status();
  }

  delay(50);
}
