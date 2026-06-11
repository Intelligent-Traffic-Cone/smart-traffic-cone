#pragma once

// Copy this file to cone_node.local.h and fill in your local WiFi/cloud values.
// cone_node.local.h is ignored by git.

#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

#ifndef CONE_CLOUD_BASE_URL
#define CONE_CLOUD_BASE_URL "http://192.168.1.23:8000"
#endif

#ifndef CONE_NODE_ID
#define CONE_NODE_ID "cone-demo-001"
#endif

#ifndef CONE_NODE_IMAGE_UPLOAD_INTERVAL_MS
#define CONE_NODE_IMAGE_UPLOAD_INTERVAL_MS 4000
#endif
