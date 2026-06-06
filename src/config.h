#pragma once

#include <Arduino.h>

namespace Config {
constexpr uint32_t SerialBaud = 115200;

constexpr uint8_t LedOkPin = LED_BUILTIN;
constexpr uint8_t LedWarningPin = D5;
constexpr uint8_t LedCriticalPin = D6;

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

constexpr uint32_t TempPeriodMs = 1000;
constexpr uint32_t DistancePeriodMs = 250;
constexpr uint32_t MotionPeriodMs = 100;
constexpr uint32_t MonitorPeriodMs = 250;
constexpr uint32_t AlertCooldownMs = 1500;
}  // namespace Config

