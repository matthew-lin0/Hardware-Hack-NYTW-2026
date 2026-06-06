#include "mcp9808.h"

namespace {
constexpr uint8_t RegisterManufacturerId = 0x06;
constexpr uint8_t RegisterAmbientTemp = 0x05;
constexpr uint16_t ExpectedManufacturerId = 0x0054;

bool read16(TwoWire& wire, uint8_t address, uint8_t reg, uint16_t& value) {
  wire.beginTransmission(address);
  wire.write(reg);
  if (wire.endTransmission(false) != 0) {
    return false;
  }

  if (wire.requestFrom(address, static_cast<uint8_t>(2)) != 2) {
    return false;
  }

  value = static_cast<uint16_t>(wire.read()) << 8;
  value |= static_cast<uint16_t>(wire.read());
  return true;
}
}  // namespace

Mcp9808::Mcp9808(TwoWire& wire) : wire_(wire) {}

bool Mcp9808::begin(uint8_t address) {
  address_ = address;
  uint16_t manufacturerId = 0;
  return read16(wire_, address_, RegisterManufacturerId, manufacturerId) &&
         manufacturerId == ExpectedManufacturerId;
}

bool Mcp9808::readTemperatureC(float& temperatureC) {
  uint16_t raw = 0;
  if (!read16(wire_, address_, RegisterAmbientTemp, raw)) {
    return false;
  }

  raw &= 0x1FFF;
  temperatureC = (raw & 0x1000) ? 256.0f - static_cast<float>(raw & 0x0FFF) / 16.0f
                               : static_cast<float>(raw) / 16.0f;
  return true;
}

