#pragma once

#include <Arduino.h>

namespace Config {
constexpr uint32_t SerialBaud = 115200;

constexpr bool EnableMcp9808 = true;
constexpr bool EnableMpu6050 = true;
constexpr bool EnableHcSr04 = true;
constexpr bool EnableSpeaker = true;
constexpr bool EnableI2cScan = true;
constexpr bool EnableCsvOutput = true;

constexpr uint8_t LedOkPin = LED_BUILTIN;
constexpr uint8_t LedWarningPin = D5;
constexpr uint8_t LedCriticalPin = D6;

constexpr uint32_t I2cSdaPin = PB9;  // Arduino header D14 / SDA
constexpr uint32_t I2cSclPin = PB8;  // Arduino header D15 / SCL
constexpr uint32_t I2cClockHz = 100000;

constexpr uint8_t HcSr04TrigPin = D8;
constexpr uint8_t HcSr04EchoPin = D9;
constexpr uint32_t HcSr04TimeoutUs = 30000;
constexpr float PresenceMinCm = 5.0f;
constexpr float PresenceMaxCm = 55.0f;
constexpr float DistanceChangeAlertCm = 15.0f;

constexpr uint8_t SpeakerPin = D3;

constexpr uint8_t Mcp9808Address = 0x18;
constexpr float TempWarningC = 22.0f;
constexpr float TempCriticalC = 25.0f;

constexpr uint8_t Mpu6050Address = 0x68;
constexpr float TiltThresholdDeg = 40.0f;
constexpr float SpikeThresholdG = 1.8f;

constexpr uint32_t HeartbeatPeriodMs = 500;
constexpr uint32_t ReportPeriodMs = 1000;
constexpr uint32_t AlertCooldownMs = 1500;
constexpr uint32_t I2cScanPeriodMs = 3000;
}  // namespace Config
