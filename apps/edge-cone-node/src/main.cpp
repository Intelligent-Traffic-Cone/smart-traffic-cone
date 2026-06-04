#include "cone_node_app.h"

#include <Arduino.h>

namespace {
ConeNodeApp app;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("starting smart traffic cone edge node");
  app.setup();
}

void loop() {
  app.tick();
  delay(100);
}
