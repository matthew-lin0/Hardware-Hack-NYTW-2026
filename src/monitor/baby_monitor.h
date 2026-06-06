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
  bool presenceDetected = false;
  bool distanceChanged = false;
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

