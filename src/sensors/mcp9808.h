#pragma once

#include <Arduino.h>
#include <Wire.h>

class Mcp9808 {
 public:
  explicit Mcp9808(TwoWire& wire = Wire);
  bool begin(uint8_t address);
  bool readTemperatureC(float& temperatureC);

 private:
  TwoWire& wire_;
  uint8_t address_ = 0;
};

