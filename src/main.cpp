#include <Arduino.h>
#include <Wire.h>

#include "audio/speaker.h"
#include "config.h"
#include "monitor/baby_monitor.h"
#include "sensors/hcsr04.h"
#include "sensors/mcp9808.h"
#include "sensors/mpu6050.h"

namespace {
Mcp9808 tempSensor;
Mpu6050 motionSensor;
HcSr04 distanceSensor;
Speaker speaker;
BabyMonitor monitor;

bool tempReady = false;
bool motionReady = false;
bool heartbeatOn = false;
float previousDistanceCm = NAN;
uint32_t lastHeartbeatMs = 0;
uint32_t lastReportMs = 0;
uint32_t lastAlertMs = 0;
uint32_t lastI2cScanMs = 0;
AlertSeverity lastAlertSeverity = AlertSeverity::None;

struct SampleReport {
  SensorSnapshot snapshot;
  MotionReading motion;
  const char* tempStatus = "disabled";
  const char* motionStatus = "disabled";
  const char* distanceStatus = "disabled";
  bool tempValid = false;
  bool motionValid = false;
  bool distanceValid = false;
};

const char* enabledLabel(bool enabled) {
  return enabled ? "enabled" : "disabled";
}

const char* severityLabel(AlertSeverity severity) {
  switch (severity) {
    case AlertSeverity::Info:
      return "info";
    case AlertSeverity::Warning:
      return "warning";
    case AlertSeverity::Critical:
      return "critical";
    case AlertSeverity::None:
      return "none";
  }
  return "unknown";
}

void printBool(const char* label, bool value) {
  Serial.print(label);
  Serial.print(value ? "true" : "false");
}

void printHexAddress(uint8_t address) {
  Serial.print("0x");
  if (address < 0x10) {
    Serial.print("0");
  }
  Serial.print(address, HEX);
}

void printI2cScan(uint32_t nowMs) {
  if (!Config::EnableI2cScan || nowMs - lastI2cScanMs < Config::I2cScanPeriodMs) {
    return;
  }

  lastI2cScanMs = nowMs;
  bool foundAny = false;

  Serial.print("i2c_scan:");
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print(" ");
      printHexAddress(address);
      foundAny = true;
    }
  }

  if (!foundAny) {
    Serial.print(" none");
  }
  Serial.println();
}

void printBootStatus() {
  Serial.println();
  Serial.println("boot: NYTW monitor bring-up");
  Serial.print("serial: ");
  Serial.println(Config::SerialBaud);
  Serial.print("mcp9808: ");
  Serial.println(Config::EnableMcp9808 ? (tempReady ? "ready" : "missing") : "disabled");
  Serial.print("mpu6050: ");
  Serial.println(Config::EnableMpu6050 ? (motionReady ? "ready" : "missing") : "disabled");
  Serial.print("hcsr04: ");
  Serial.println(enabledLabel(Config::EnableHcSr04));
  Serial.print("speaker: ");
  Serial.println(enabledLabel(Config::EnableSpeaker));
  Serial.print("i2c_scan: ");
  Serial.println(enabledLabel(Config::EnableI2cScan));
  Serial.println("status: board firmware is alive");
}

void updateHeartbeat(uint32_t nowMs) {
  if (nowMs - lastHeartbeatMs < Config::HeartbeatPeriodMs) {
    return;
  }

  heartbeatOn = !heartbeatOn;
  digitalWrite(Config::LedOkPin, heartbeatOn ? HIGH : LOW);
  lastHeartbeatMs = nowMs;
}

void sampleTemperature(SampleReport& report) {
  if (!Config::EnableMcp9808) {
    return;
  }

  if (!tempReady) {
    tempReady = tempSensor.begin(Config::Mcp9808Address);
  }

  if (!tempReady) {
    report.tempStatus = "missing";
    return;
  }

  float temperatureC = NAN;
  if (!tempSensor.readTemperatureC(temperatureC)) {
    report.tempStatus = "read_error";
    return;
  }

  report.tempStatus = "ready";
  report.tempValid = true;
  report.snapshot.temperatureC = temperatureC;
}

void sampleMotion(SampleReport& report) {
  if (!Config::EnableMpu6050) {
    return;
  }

  if (!motionReady) {
    motionReady = motionSensor.begin(Config::Mpu6050Address);
  }

  if (!motionReady) {
    report.motionStatus = "missing";
    return;
  }

  if (!motionSensor.read(report.motion, Config::TiltThresholdDeg, Config::SpikeThresholdG)) {
    report.motionStatus = "read_error";
    return;
  }

  report.motionStatus = "ready";
  report.motionValid = true;
  report.snapshot.tiltDetected = report.motion.tiltDetected;
  report.snapshot.spikeDetected = report.motion.spikeDetected;
}

void sampleDistance(SampleReport& report) {
  if (!Config::EnableHcSr04) {
    return;
  }

  float distanceCm = NAN;
  if (!distanceSensor.readDistanceCm(distanceCm, Config::HcSr04TimeoutUs)) {
    report.distanceStatus = "timeout";
    report.snapshot.distanceCm = NAN;
    return;
  }

  report.distanceStatus = "ready";
  report.distanceValid = true;
  report.snapshot.distanceCm = distanceCm;
  report.snapshot.presenceDetected =
      distanceCm >= Config::PresenceMinCm && distanceCm <= Config::PresenceMaxCm;
  report.snapshot.distanceChanged =
      !isnan(previousDistanceCm) &&
      fabs(distanceCm - previousDistanceCm) >= Config::DistanceChangeAlertCm;
  previousDistanceCm = distanceCm;
}

SampleReport sampleSensors() {
  SampleReport report;
  sampleTemperature(report);
  sampleMotion(report);
  sampleDistance(report);
  return report;
}

void updateAlertOutputs(const AlertState& alert, uint32_t nowMs) {
  const bool showWarning =
      alert.severity == AlertSeverity::Info || alert.severity == AlertSeverity::Warning;
  digitalWrite(Config::LedWarningPin, showWarning ? HIGH : LOW);
  digitalWrite(Config::LedCriticalPin, alert.severity == AlertSeverity::Critical ? HIGH : LOW);

  if (!Config::EnableSpeaker || alert.severity == AlertSeverity::None) {
    lastAlertSeverity = alert.severity;
    return;
  }

  const bool shouldRepeat = nowMs - lastAlertMs >= Config::AlertCooldownMs;
  if (alert.severity != lastAlertSeverity || shouldRepeat) {
    speaker.play(alert.severity);
    lastAlertMs = millis();
  }
  lastAlertSeverity = alert.severity;
}

void printReport(const SampleReport& report, const AlertState& alert, uint32_t nowMs) {
  Serial.println();
  Serial.print("sample_ms: ");
  Serial.println(nowMs);

  Serial.print("mcp9808: ");
  Serial.println(report.tempStatus);
  Serial.print("temp: ");
  if (report.tempValid) {
    const float temperatureF = report.snapshot.temperatureC * 9.0f / 5.0f + 32.0f;
    Serial.print(report.snapshot.temperatureC, 2);
    Serial.print(" C / ");
    Serial.print(temperatureF, 2);
    Serial.println(" F");
  } else {
    Serial.println(report.tempStatus);
  }

  Serial.print("mpu6050: ");
  Serial.println(report.motionStatus);
  Serial.print("motion: ");
  if (report.motionValid) {
    Serial.print("tilt_deg=");
    Serial.print(report.motion.tiltDeg, 1);
    Serial.print(" ");
    printBool("tilt=", report.motion.tiltDetected);
    Serial.print(" ");
    printBool("spike=", report.motion.spikeDetected);
    Serial.print(" ax=");
    Serial.print(report.motion.axG, 2);
    Serial.print(" ay=");
    Serial.print(report.motion.ayG, 2);
    Serial.print(" az=");
    Serial.println(report.motion.azG, 2);
  } else {
    Serial.println(report.motionStatus);
  }

  Serial.print("hcsr04: ");
  Serial.println(report.distanceStatus);
  Serial.print("distance: ");
  if (report.distanceValid) {
    Serial.print(report.snapshot.distanceCm, 1);
    Serial.print(" cm ");
    printBool("presence=", report.snapshot.presenceDetected);
    Serial.print(" ");
    printBool("changed=", report.snapshot.distanceChanged);
    Serial.println();
  } else {
    Serial.println(report.distanceStatus);
  }

  Serial.print("alert: ");
  Serial.print(severityLabel(alert.severity));
  Serial.print(" reason=");
  Serial.println(alert.reason);

  printI2cScan(nowMs);
}
}  // namespace

void setup() {
  Serial.begin(Config::SerialBaud);
  delay(1000);

  pinMode(Config::LedOkPin, OUTPUT);
  pinMode(Config::LedWarningPin, OUTPUT);
  pinMode(Config::LedCriticalPin, OUTPUT);

  Serial.println();
  Serial.println("boot: STM32 serial is online");

  Wire.setSDA(Config::I2cSdaPin);
  Wire.setSCL(Config::I2cSclPin);
  Wire.begin();
  Wire.setClock(Config::I2cClockHz);
  Serial.print("boot: I2C bus initialized at ");
  Serial.print(Config::I2cClockHz);
  Serial.println(" Hz");

  if (Config::EnableSpeaker) {
    speaker.begin(Config::SpeakerPin);
    Serial.println("boot: speaker initialized");
  }

  if (Config::EnableHcSr04) {
    distanceSensor.begin(Config::HcSr04TrigPin, Config::HcSr04EchoPin);
    Serial.println("boot: HC-SR04 pins initialized");
  }

  if (Config::EnableMcp9808) {
    Serial.println("boot: checking MCP9808");
    tempReady = tempSensor.begin(Config::Mcp9808Address);
  }

  if (Config::EnableMpu6050) {
    Serial.println("boot: checking MPU6050");
    motionReady = motionSensor.begin(Config::Mpu6050Address);
  }

  printBootStatus();
}

void loop() {
  const uint32_t nowMs = millis();
  updateHeartbeat(nowMs);

  if (nowMs - lastReportMs < Config::ReportPeriodMs) {
    return;
  }
  lastReportMs = nowMs;

  const SampleReport report = sampleSensors();
  const AlertState alert = monitor.evaluate(report.snapshot);
  updateAlertOutputs(alert, nowMs);
  printReport(report, alert, nowMs);
}
