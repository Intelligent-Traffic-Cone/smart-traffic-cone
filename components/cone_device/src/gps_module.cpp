#include "cone_device/gps_module.h"

#ifndef CONE_DEVICE_ENABLE_GPS
#define CONE_DEVICE_ENABLE_GPS 1
#endif

namespace cone_device {
namespace {
GpsStatus g_status;
}

bool setup_gps(const GpsModuleConfig&) {
#if CONE_DEVICE_ENABLE_GPS
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

void tick_gps() {
#if CONE_DEVICE_ENABLE_GPS
  // Concrete GPS parsing is added by the hardware owner for the selected module.
#endif
}

void deinit_gps() {
  g_status = {};
}

GpsStatus gps_status() {
  return g_status;
}

}  // namespace cone_device
