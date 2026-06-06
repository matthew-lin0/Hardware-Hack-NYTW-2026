#include "mpu6050.h"

namespace {
constexpr uint8_t RegisterWhoAmI = 0x75;
constexpr uint8_t RegisterPowerMgmt1 = 0x6B;
constexpr uint8_t RegisterAccelXoutH = 0x3B;
constexpr uint8_t ExpectedWhoAmI = 0x68;
constexpr float AccelScale = 16384.0f;
constexpr float GyroScale = 131.0f;

int16_t toInt16(uint8_t high, uint8_t low) {
  return static_cast<int16_t>((static_cast<uint16_t>(high) << 8) | low);
}
}  // namespace

Mpu6050::Mpu6050(TwoWire& wire) : wire_(wire) {}

bool Mpu6050::begin(uint8_t address) {
  address_ = address;
  uint8_t whoAmI = 0;
  if (!readRegisters(RegisterWhoAmI, &whoAmI, 1) || whoAmI != ExpectedWhoAmI) {
    return false;
  }

  return writeRegister(RegisterPowerMgmt1, 0x00);
}

bool Mpu6050::read(MotionReading& reading, float tiltThresholdDeg, float spikeThresholdG) {
  uint8_t data[14] = {};
  if (!readRegisters(RegisterAccelXoutH, data, sizeof(data))) {
    return false;
  }

  const int16_t ax = toInt16(data[0], data[1]);
  const int16_t ay = toInt16(data[2], data[3]);
  const int16_t az = toInt16(data[4], data[5]);
  const int16_t gx = toInt16(data[8], data[9]);
  const int16_t gy = toInt16(data[10], data[11]);
  const int16_t gz = toInt16(data[12], data[13]);

  reading.axG = static_cast<float>(ax) / AccelScale;
  reading.ayG = static_cast<float>(ay) / AccelScale;
  reading.azG = static_cast<float>(az) / AccelScale;
  reading.gxDps = static_cast<float>(gx) / GyroScale;
  reading.gyDps = static_cast<float>(gy) / GyroScale;
  reading.gzDps = static_cast<float>(gz) / GyroScale;

  const float horizontalG = sqrt(reading.axG * reading.axG + reading.ayG * reading.ayG);
  reading.tiltDeg = atan2(horizontalG, fabs(reading.azG)) * 180.0f / PI;
  reading.tiltDetected = reading.tiltDeg >= tiltThresholdDeg;

  const float magnitudeG = sqrt(reading.axG * reading.axG + reading.ayG * reading.ayG +
                                reading.azG * reading.azG);
  reading.spikeDetected = fabs(magnitudeG - 1.0f) >= spikeThresholdG;
  return true;
}

bool Mpu6050::writeRegister(uint8_t reg, uint8_t value) {
  wire_.beginTransmission(address_);
  wire_.write(reg);
  wire_.write(value);
  return wire_.endTransmission() == 0;
}

bool Mpu6050::readRegisters(uint8_t reg, uint8_t* data, size_t length) {
  wire_.beginTransmission(address_);
  wire_.write(reg);
  if (wire_.endTransmission(false) != 0) {
    return false;
  }

  if (wire_.requestFrom(address_, static_cast<uint8_t>(length)) != length) {
    return false;
  }

  for (size_t i = 0; i < length; ++i) {
    data[i] = wire_.read();
  }
  return true;
}

