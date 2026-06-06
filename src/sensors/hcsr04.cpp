#include "hcsr04.h"

void HcSr04::begin(uint8_t trigPin, uint8_t echoPin) {
  trigPin_ = trigPin;
  echoPin_ = echoPin;
  pinMode(trigPin_, OUTPUT);
  pinMode(echoPin_, INPUT);
  digitalWrite(trigPin_, LOW);
}

bool HcSr04::readDistanceCm(float& distanceCm, uint32_t timeoutUs) {
  digitalWrite(trigPin_, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin_, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin_, LOW);

  const unsigned long durationUs = pulseIn(echoPin_, HIGH, timeoutUs);
  if (durationUs == 0) {
    return false;
  }

  distanceCm = static_cast<float>(durationUs) * 0.0343f / 2.0f;
  return true;
}

