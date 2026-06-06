#pragma once

#include <Arduino.h>

class HcSr04 {
 public:
  void begin(uint8_t trigPin, uint8_t echoPin);
  bool readDistanceCm(float& distanceCm, uint32_t timeoutUs);

 private:
  uint8_t trigPin_ = 0;
  uint8_t echoPin_ = 0;
};

