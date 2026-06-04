#include "cone_device/ultrasonic_array.h"

#ifndef CONE_DEVICE_ENABLE_ULTRASONIC_ARRAY
#define CONE_DEVICE_ENABLE_ULTRASONIC_ARRAY 1
#endif

namespace cone_device {
namespace {
UltrasonicArrayStatus g_status;
}

bool setup_ultrasonic_array(const UltrasonicArrayConfig&) {
#if CONE_DEVICE_ENABLE_ULTRASONIC_ARRAY
  g_status.enabled = true;
  g_status.initialized = true;
  g_status.last_error.clear();
  for (auto& channel : g_status.channels) {
    channel.present = true;
  }
  return true;
#else
  g_status = {};
  g_status.last_error = "disabled";
  return false;
#endif
}

void tick_ultrasonic_array() {
#if CONE_DEVICE_ENABLE_ULTRASONIC_ARRAY
  // Concrete ranging implementation is added after the ultrasonic model is known.
#endif
}

void deinit_ultrasonic_array() {
  g_status = {};
}

UltrasonicArrayStatus ultrasonic_array_status() {
  return g_status;
}

}  // namespace cone_device
