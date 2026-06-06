#pragma once

#include <Arduino.h>
#include <Wire.h>

struct MotionReading {
  float axG = 0;
  float ayG = 0;
  float azG = 0;
  float gxDps = 0;
  float gyDps = 0;
  float gzDps = 0;
  float tiltDeg = 0;
  bool tiltDetected = false;
  bool spikeDetected = false;
};

class Mpu6050 {
 public:
  explicit Mpu6050(TwoWire& wire = Wire);
  bool begin(uint8_t address);
  bool read(MotionReading& reading, float tiltThresholdDeg, float spikeThresholdG);

 private:
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegisters(uint8_t reg, uint8_t* data, size_t length);

  TwoWire& wire_;
  uint8_t address_ = 0;
};

