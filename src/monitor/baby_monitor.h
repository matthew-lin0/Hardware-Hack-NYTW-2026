#pragma once

#include <Arduino.h>

enum class AlertSeverity : uint8_t {
  None = 0,
  Info = 1,
  Warning = 2,
  Critical = 3,
};

struct SensorSnapshot {
  float temperatureC = NAN;
  float distanceCm = NAN;
  uint8_t scanAngleDeg = 0;
  uint8_t scanStep = 0;
  uint32_t scanCycle = 0;
  float axG = NAN;
  float ayG = NAN;
  float azG = NAN;
  float gxDps = NAN;
  float gyDps = NAN;
  float gzDps = NAN;
  float tiltDeg = NAN;
  bool presenceDetected = false;
  bool distanceChanged = false;
  bool scanCycleChanged = false;
  bool tiltDetected = false;
  bool spikeDetected = false;
};

struct AlertState {
  AlertSeverity severity = AlertSeverity::None;
  const char* reason = "normal";
};

class BabyMonitor {
 public:
  AlertState evaluate(const SensorSnapshot& snapshot) const;

 private:
  static AlertState maxSeverity(AlertState current, AlertState candidate);
};
