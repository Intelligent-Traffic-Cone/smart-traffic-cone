#include "cone_device/ultrasonic_array.h"

namespace cone_device {

UltrasonicArray::UltrasonicArray(uint8_t trigPin, uint8_t echoPin)
    : trigPin_(trigPin), echoPin_(echoPin) {}

void UltrasonicArray::begin() {
  pinMode(trigPin_, OUTPUT);
  pinMode(echoPin_, INPUT);
  digitalWrite(trigPin_, LOW);
}

float UltrasonicArray::readDistanceCmOnce() {
  digitalWrite(trigPin_, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin_, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin_, LOW);

  const unsigned long duration = pulseIn(echoPin_, HIGH, kEchoTimeoutUs);
  if (duration == 0) {
    return -1.0f;
  }

  return (duration * 0.0343f) / 2.0f;
}

float UltrasonicArray::readDistanceCmFiltered(uint8_t sampleCount) {
  if (sampleCount == 0) {
    return -1.0f;
  }

  float sum = 0.0f;
  uint8_t validCount = 0;

  for (uint8_t i = 0; i < sampleCount; ++i) {
    const float distance = readDistanceCmOnce();
    if (distance > 0.0f) {
      sum += distance;
      ++validCount;
    }
    delay(30);
  }

  if (validCount == 0) {
    return -1.0f;
  }

  return sum / validCount;
}

}  // namespace cone_device