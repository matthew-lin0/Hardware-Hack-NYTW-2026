#pragma once

#include <Arduino.h>

#include "../monitor/baby_monitor.h"

class Speaker {
 public:
  void begin(uint8_t pin);
  void play(AlertSeverity severity);

 private:
  void toneFor(uint16_t frequency, uint16_t durationMs);

  uint8_t pin_ = 0;
};

