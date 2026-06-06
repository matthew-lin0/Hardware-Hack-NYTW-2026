#include "baby_monitor.h"

#include "../config.h"

AlertState BabyMonitor::evaluate(const SensorSnapshot& snapshot) const {
  AlertState state;

  if (!isnan(snapshot.temperatureC)) {
    if (snapshot.temperatureC >= Config::TempCriticalC) {
      state = maxSeverity(state, {AlertSeverity::Critical, "temperature critical"});
    } else if (snapshot.temperatureC >= Config::TempWarningC) {
      state = maxSeverity(state, {AlertSeverity::Warning, "temperature warm"});
    }
  }

  if (snapshot.spikeDetected) {
    state = maxSeverity(state, {AlertSeverity::Warning, "movement spike"});
  }

  if (snapshot.tiltDetected) {
    state = maxSeverity(state, {AlertSeverity::Warning, "tilt detected"});
  }

  if (snapshot.distanceChanged) {
    state = maxSeverity(state, {AlertSeverity::Info, "distance changed"});
  }

  if (!snapshot.presenceDetected && !isnan(snapshot.distanceCm)) {
    state = maxSeverity(state, {AlertSeverity::Info, "presence zone clear"});
  }

  return state;
}

AlertState BabyMonitor::maxSeverity(AlertState current, AlertState candidate) {
  return static_cast<uint8_t>(candidate.severity) > static_cast<uint8_t>(current.severity)
             ? candidate
             : current;
}

