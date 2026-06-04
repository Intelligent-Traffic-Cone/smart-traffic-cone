#include "cone_device/camera_module.h"

#ifndef CONE_DEVICE_ENABLE_CAMERA
#define CONE_DEVICE_ENABLE_CAMERA 1
#endif

namespace cone_device {
namespace {
CameraStatus g_status;
}

bool setup_camera(const CameraModuleConfig&) {
#if CONE_DEVICE_ENABLE_CAMERA
  g_status.enabled = true;
  g_status.initialized = true;
  g_status.last_error.clear();
  return true;
#else
  g_status = {};
  g_status.last_error = "disabled";
  return false;
#endif
}

void tick_camera() {
#if CONE_DEVICE_ENABLE_CAMERA
  // Concrete capture implementation is added by the camera hardware owner.
#endif
}

void deinit_camera() {
  g_status = {};
}

CameraStatus camera_status() {
  return g_status;
}

}  // namespace cone_device
