#include "cone_device/camera_module.h"

#include <Arduino.h>

#ifndef CONE_DEVICE_ENABLE_CAMERA
#define CONE_DEVICE_ENABLE_CAMERA 1
#endif

#if CONE_DEVICE_ENABLE_CAMERA
#include <esp_camera.h>
#include <esp_err.h>
#endif

namespace cone_device {
namespace {

#if CONE_DEVICE_ENABLE_CAMERA

#ifndef CONE_CAMERA_PIN_PWDN
#define CONE_CAMERA_PIN_PWDN -1
#endif
#ifndef CONE_CAMERA_PIN_RESET
#define CONE_CAMERA_PIN_RESET -1
#endif
#ifndef CONE_CAMERA_PIN_SIOD
#define CONE_CAMERA_PIN_SIOD 4
#endif
#ifndef CONE_CAMERA_PIN_SIOC
#define CONE_CAMERA_PIN_SIOC 5
#endif
#ifndef CONE_CAMERA_PIN_VSYNC
#define CONE_CAMERA_PIN_VSYNC 6
#endif
#ifndef CONE_CAMERA_PIN_HREF
#define CONE_CAMERA_PIN_HREF 7
#endif
#ifndef CONE_CAMERA_PIN_XCLK
#define CONE_CAMERA_PIN_XCLK 15
#endif
#ifndef CONE_CAMERA_PIN_Y2
#define CONE_CAMERA_PIN_Y2 11
#endif
#ifndef CONE_CAMERA_PIN_Y3
#define CONE_CAMERA_PIN_Y3 9
#endif
#ifndef CONE_CAMERA_PIN_Y4
#define CONE_CAMERA_PIN_Y4 8
#endif
#ifndef CONE_CAMERA_PIN_Y5
#define CONE_CAMERA_PIN_Y5 10
#endif
#ifndef CONE_CAMERA_PIN_Y6
#define CONE_CAMERA_PIN_Y6 12
#endif
#ifndef CONE_CAMERA_PIN_Y7
#define CONE_CAMERA_PIN_Y7 18
#endif
#ifndef CONE_CAMERA_PIN_Y8
#define CONE_CAMERA_PIN_Y8 17
#endif
#ifndef CONE_CAMERA_PIN_Y9
#define CONE_CAMERA_PIN_Y9 16
#endif
#ifndef CONE_CAMERA_PIN_PCLK
#define CONE_CAMERA_PIN_PCLK 13
#endif

#ifndef CONE_CAMERA_XCLK_FREQ_HZ
#define CONE_CAMERA_XCLK_FREQ_HZ 20000000
#endif
#ifndef CONE_CAMERA_JPEG_QUALITY
#define CONE_CAMERA_JPEG_QUALITY 12
#endif

#endif

CameraModuleConfig g_config;
CameraStatus g_status;
uint32_t g_last_capture_ms = 0;
bool g_camera_started = false;

#if CONE_DEVICE_ENABLE_CAMERA

struct FrameSizeMapping {
  uint16_t width;
  uint16_t height;
  framesize_t frame_size;
};

constexpr FrameSizeMapping kFrameSizes[] = {
    {96, 96, FRAMESIZE_96X96},
    {160, 120, FRAMESIZE_QQVGA},
    {176, 144, FRAMESIZE_QCIF},
    {240, 176, FRAMESIZE_HQVGA},
    {240, 240, FRAMESIZE_240X240},
    {320, 240, FRAMESIZE_QVGA},
    {400, 296, FRAMESIZE_CIF},
    {480, 320, FRAMESIZE_HVGA},
    {640, 480, FRAMESIZE_VGA},
    {800, 600, FRAMESIZE_SVGA},
    {1024, 768, FRAMESIZE_XGA},
    {1280, 720, FRAMESIZE_HD},
    {1280, 1024, FRAMESIZE_SXGA},
    {1600, 1200, FRAMESIZE_UXGA},
};

framesize_t frame_size_for(uint16_t width, uint16_t height) {
  for (const auto& mapping : kFrameSizes) {
    if (mapping.width == width && mapping.height == height) {
      return mapping.frame_size;
    }
  }
  return FRAMESIZE_VGA;
}

std::string esp_error(const char* prefix, esp_err_t err) {
  std::string message = prefix;
  message += ":";
  message += esp_err_to_name(err);
  return message;
}

camera_config_t build_camera_config(const CameraModuleConfig& config) {
  camera_config_t camera_config = {};
  camera_config.pin_pwdn = CONE_CAMERA_PIN_PWDN;
  camera_config.pin_reset = CONE_CAMERA_PIN_RESET;
  camera_config.pin_xclk = CONE_CAMERA_PIN_XCLK;
  camera_config.pin_sccb_sda = CONE_CAMERA_PIN_SIOD;
  camera_config.pin_sccb_scl = CONE_CAMERA_PIN_SIOC;
  camera_config.pin_d0 = CONE_CAMERA_PIN_Y2;
  camera_config.pin_d1 = CONE_CAMERA_PIN_Y3;
  camera_config.pin_d2 = CONE_CAMERA_PIN_Y4;
  camera_config.pin_d3 = CONE_CAMERA_PIN_Y5;
  camera_config.pin_d4 = CONE_CAMERA_PIN_Y6;
  camera_config.pin_d5 = CONE_CAMERA_PIN_Y7;
  camera_config.pin_d6 = CONE_CAMERA_PIN_Y8;
  camera_config.pin_d7 = CONE_CAMERA_PIN_Y9;
  camera_config.pin_vsync = CONE_CAMERA_PIN_VSYNC;
  camera_config.pin_href = CONE_CAMERA_PIN_HREF;
  camera_config.pin_pclk = CONE_CAMERA_PIN_PCLK;
  camera_config.xclk_freq_hz = CONE_CAMERA_XCLK_FREQ_HZ;
  camera_config.ledc_timer = LEDC_TIMER_0;
  camera_config.ledc_channel = LEDC_CHANNEL_0;
  camera_config.pixel_format = PIXFORMAT_JPEG;
  camera_config.frame_size =
      frame_size_for(config.frame_width, config.frame_height);
  camera_config.jpeg_quality = CONE_CAMERA_JPEG_QUALITY;
  camera_config.fb_count = 1;
  camera_config.fb_location =
      psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  camera_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  camera_config.sccb_i2c_port = -1;
  return camera_config;
}

void record_capture(camera_fb_t* frame, uint32_t now_ms) {
  if (frame == nullptr || frame->buf == nullptr || frame->len == 0) {
    g_status.frame_available = false;
    g_status.last_error = "empty_frame";
    return;
  }

  g_status.frame_available = true;
  g_status.last_frame_age_ms = 0;
  g_status.frame_count += 1;
  g_status.last_error.clear();
  g_last_capture_ms = now_ms;
}

bool capture_frame(CameraJpegFrameWriter writer,
                   void* context,
                   std::string* error) {
  if (!g_status.initialized || !g_camera_started) {
    if (error != nullptr) {
      *error = "camera_uninitialized";
    }
    if (g_status.last_error.empty()) {
      g_status.last_error = "camera_uninitialized";
    }
    return false;
  }

  camera_fb_t* frame = esp_camera_fb_get();
  if (frame == nullptr) {
    if (error != nullptr) {
      *error = "capture_timeout";
    }
    g_status.frame_available = false;
    g_status.last_error = "capture_timeout";
    return false;
  }

  bool ok = true;
  if (frame->buf == nullptr || frame->len == 0) {
    ok = false;
    if (error != nullptr) {
      *error = "empty_frame";
    }
    g_status.last_error = "empty_frame";
  } else if (frame->format != PIXFORMAT_JPEG) {
    ok = false;
    if (error != nullptr) {
      *error = "non_jpeg_frame";
    }
    g_status.last_error = "non_jpeg_frame";
  } else if (writer != nullptr && !writer(frame->buf, frame->len, context)) {
    ok = false;
    if (error != nullptr) {
      *error = "frame_writer_failed";
    }
    g_status.last_error = "frame_writer_failed";
  }

  if (ok) {
    record_capture(frame, ::millis());
  } else {
    g_status.frame_available = false;
  }

  esp_camera_fb_return(frame);
  return ok;
}

void update_frame_age(uint32_t now_ms) {
  if (g_last_capture_ms == 0 || !g_status.frame_available) {
    g_status.last_frame_age_ms = 0;
    return;
  }
  g_status.last_frame_age_ms = now_ms - g_last_capture_ms;
}

#endif

}  // namespace

bool setup_camera(const CameraModuleConfig& config) {
#if CONE_DEVICE_ENABLE_CAMERA
  if (g_camera_started) {
    esp_camera_deinit();
    g_camera_started = false;
  }

  g_config = config;
  g_status = {};
  g_status.enabled = true;
  g_last_capture_ms = 0;

  camera_config_t camera_config = build_camera_config(g_config);
  const esp_err_t init_result = esp_camera_init(&camera_config);
  if (init_result != ESP_OK) {
    g_status.last_error = esp_error("camera_init_failed", init_result);
    return false;
  }

  g_camera_started = true;
  g_status.initialized = true;
  g_status.last_error.clear();
  return true;
#else
  (void)config;
  g_config = {};
  g_status = {};
  g_last_capture_ms = 0;
  g_camera_started = false;
  g_status.last_error = "disabled";
  return false;
#endif
}

void tick_camera() {
#if CONE_DEVICE_ENABLE_CAMERA
  if (!g_status.initialized || !g_camera_started) {
    return;
  }

  const uint32_t now = ::millis();
  update_frame_age(now);
  if (g_last_capture_ms != 0 &&
      now - g_last_capture_ms < g_config.capture_interval_ms) {
    return;
  }

  std::string error;
  capture_frame(nullptr, nullptr, &error);
#endif
}

void deinit_camera() {
#if CONE_DEVICE_ENABLE_CAMERA
  if (g_camera_started) {
    esp_camera_deinit();
  }
#endif
  g_config = {};
  g_status = {};
  g_last_capture_ms = 0;
  g_camera_started = false;
}

CameraStatus camera_status() {
#if CONE_DEVICE_ENABLE_CAMERA
  update_frame_age(::millis());
#endif
  return g_status;
}

bool camera_capture_jpeg(CameraJpegFrameWriter writer,
                         void* context,
                         std::string& error) {
  error.clear();
  if (writer == nullptr) {
    error = "missing_frame_writer";
    return false;
  }

#if CONE_DEVICE_ENABLE_CAMERA
  const bool ok = capture_frame(writer, context, &error);
  if (!ok && error.empty()) {
    error = "capture_failed";
  }
  return ok;
#else
  (void)context;
  error = "disabled";
  g_status.last_error = "disabled";
  return false;
#endif
}

}  // namespace cone_device
