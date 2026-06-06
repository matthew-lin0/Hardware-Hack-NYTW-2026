#include <Arduino.h>
#include <Servo.h>
#include <STM32FreeRTOS.h>
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
Servo scanServo;
BabyMonitor monitor;

SensorSnapshot snapshot;
SemaphoreHandle_t snapshotMutex;
volatile bool servoTestRequested = false;
constexpr uint8_t ScanAngles[Config::ServoScanSteps] = {0, 30, 60, 90, 120, 150,
                                                        180, 150, 120, 90, 60, 30};

const __FlashStringHelper* severityName(AlertSeverity severity) {
  switch (severity) {
    case AlertSeverity::Info:
      return F("info");
    case AlertSeverity::Warning:
      return F("warning");
    case AlertSeverity::Critical:
      return F("critical");
    case AlertSeverity::None:
    default:
      return F("none");
  }
}

void printJsonFloat(const char* key, float value, bool comma = true) {
  Serial.print('"');
  Serial.print(key);
  Serial.print(F("\":"));
  if (isnan(value)) {
    Serial.print(F("null"));
  } else {
    Serial.print(value, 3);
  }
  if (comma) {
    Serial.print(',');
  }
}

void printJsonBool(const char* key, bool value, bool comma = true) {
  Serial.print('"');
  Serial.print(key);
  Serial.print(F("\":"));
  Serial.print(value ? F("true") : F("false"));
  if (comma) {
    Serial.print(',');
  }
}

void printEvent(const __FlashStringHelper* event, const __FlashStringHelper* status) {
  Serial.print(F("{\"type\":\"event\",\"event\":\""));
  Serial.print(event);
  Serial.print(F("\",\"status\":\""));
  Serial.print(status);
  Serial.println(F("\"}"));
}

void printTelemetry(const SensorSnapshot& current, const AlertState& alert) {
  Serial.print(F("{\"type\":\"telemetry\","));
  Serial.print(F("\"ms\":"));
  Serial.print(millis());
  Serial.print(',');
  printJsonFloat("temperature_c", current.temperatureC);
  printJsonFloat("distance_cm", current.distanceCm);
  Serial.print(F("\"scan_angle_deg\":"));
  Serial.print(current.scanAngleDeg);
  Serial.print(',');
  Serial.print(F("\"scan_step\":"));
  Serial.print(current.scanStep);
  Serial.print(',');
  Serial.print(F("\"scan_cycle\":"));
  Serial.print(current.scanCycle);
  Serial.print(',');
  printJsonFloat("ax_g", current.axG);
  printJsonFloat("ay_g", current.ayG);
  printJsonFloat("az_g", current.azG);
  printJsonFloat("gx_dps", current.gxDps);
  printJsonFloat("gy_dps", current.gyDps);
  printJsonFloat("gz_dps", current.gzDps);
  printJsonFloat("tilt_deg", current.tiltDeg);
  printJsonBool("presence_detected", current.presenceDetected);
  printJsonBool("distance_changed", current.distanceChanged);
  printJsonBool("scan_cycle_changed", current.scanCycleChanged);
  printJsonBool("tilt_detected", current.tiltDetected);
  printJsonBool("spike_detected", current.spikeDetected);
  Serial.print(F("\"alert_severity\":\""));
  Serial.print(severityName(alert.severity));
  Serial.print(F("\",\"alert_reason\":\""));
  Serial.print(alert.reason);
  Serial.println(F("\"}"));
}

bool commandHas(const String& command, const char* token) {
  return command.indexOf(token) >= 0;
}

AlertSeverity parseSeverity(const String& command) {
  if (commandHas(command, "critical")) {
    return AlertSeverity::Critical;
  }
  if (commandHas(command, "warning")) {
    return AlertSeverity::Warning;
  }
  if (commandHas(command, "info")) {
    return AlertSeverity::Info;
  }
  return AlertSeverity::None;
}

void handleCommand(const String& command) {
  if (command.length() == 0) {
    return;
  }

  if (commandHas(command, "ping")) {
    printEvent(F("pong"), F("ok"));
    return;
  }

  if (commandHas(command, "speaker") || commandHas(command, "beep")) {
    const AlertSeverity severity = parseSeverity(command);
    speaker.play(severity == AlertSeverity::None ? AlertSeverity::Info : severity);
    printEvent(F("speaker"), F("ok"));
    return;
  }

  if (commandHas(command, "servo_test")) {
    servoTestRequested = true;
    printEvent(F("servo_test"), F("queued"));
    return;
  }

  printEvent(F("command"), F("unknown"));
}

template <typename Mutator>
void updateSnapshot(Mutator mutator) {
  if (xSemaphoreTake(snapshotMutex, portMAX_DELAY) == pdTRUE) {
    mutator(snapshot);
    xSemaphoreGive(snapshotMutex);
  }
}

SensorSnapshot copySnapshot() {
  SensorSnapshot copy;
  if (xSemaphoreTake(snapshotMutex, portMAX_DELAY) == pdTRUE) {
    copy = snapshot;
    xSemaphoreGive(snapshotMutex);
  }
  return copy;
}

void tempTask(void*) {
  for (;;) {
    float temperatureC = NAN;
    if (tempSensor.readTemperatureC(temperatureC)) {
      if (temperatureC < -40.0f || temperatureC > 125.0f) {
        vTaskDelay(pdMS_TO_TICKS(Config::TempPeriodMs));
        continue;
      }
      updateSnapshot([&](SensorSnapshot& state) { state.temperatureC = temperatureC; });
    }
    vTaskDelay(pdMS_TO_TICKS(Config::TempPeriodMs));
  }
}

void distanceTask(void*) {
  float previousCycle[Config::ServoScanSteps];
  float currentCycle[Config::ServoScanSteps];
  bool hasPreviousCycle = false;
  uint8_t scanStep = 0;
  uint32_t scanCycle = 0;

  for (uint8_t i = 0; i < Config::ServoScanSteps; ++i) {
    previousCycle[i] = NAN;
    currentCycle[i] = NAN;
  }

  for (;;) {
    if (servoTestRequested) {
      servoTestRequested = false;
      printEvent(F("servo_test"), F("starting"));
      for (uint8_t i = 0; i < Config::ServoScanSteps; ++i) {
        const uint8_t angleDeg = ScanAngles[i];
        scanServo.write(angleDeg);
        updateSnapshot([&](SensorSnapshot& state) {
          state.scanAngleDeg = angleDeg;
          state.scanStep = i;
        });
        vTaskDelay(pdMS_TO_TICKS(Config::ServoSettleMs));
      }
      printEvent(F("servo_test"), F("done"));
    }

    const uint8_t angleDeg = ScanAngles[scanStep];
    scanServo.write(angleDeg);
    vTaskDelay(pdMS_TO_TICKS(Config::ServoSettleMs));

    float distanceCm = NAN;
    if (distanceSensor.readDistanceCm(distanceCm, Config::HcSr04TimeoutUs)) {
      const bool presenceDetected =
          distanceCm >= Config::PresenceMinCm && distanceCm <= Config::PresenceMaxCm;
      currentCycle[scanStep] = distanceCm;
      bool distanceChanged = false;

      if (hasPreviousCycle && !isnan(previousCycle[scanStep]) &&
          fabs(distanceCm - previousCycle[scanStep]) >= Config::DistanceChangeAlertCm) {
        distanceChanged = true;
      }

      updateSnapshot([&](SensorSnapshot& state) {
        state.distanceCm = distanceCm;
        state.scanAngleDeg = angleDeg;
        state.scanStep = scanStep;
        state.scanCycle = scanCycle;
        state.presenceDetected = presenceDetected;
        state.distanceChanged = distanceChanged;
      });
    }

    scanStep++;
    if (scanStep >= Config::ServoScanSteps) {
      bool scanCycleChanged = false;
      if (hasPreviousCycle) {
        for (uint8_t i = 0; i < Config::ServoScanSteps; ++i) {
          if (!isnan(currentCycle[i]) && !isnan(previousCycle[i]) &&
              fabs(currentCycle[i] - previousCycle[i]) >= Config::DistanceChangeAlertCm) {
            scanCycleChanged = true;
            break;
          }
        }
      }

      for (uint8_t i = 0; i < Config::ServoScanSteps; ++i) {
        previousCycle[i] = currentCycle[i];
        currentCycle[i] = NAN;
      }

      hasPreviousCycle = true;
      scanStep = 0;
      scanCycle++;

      updateSnapshot([&](SensorSnapshot& state) {
        state.scanCycle = scanCycle;
        state.scanCycleChanged = scanCycleChanged;
      });
    }

    vTaskDelay(pdMS_TO_TICKS(Config::DistancePeriodMs));
  }
}

void motionTask(void*) {
  for (;;) {
    MotionReading reading;
    if (motionSensor.read(reading, Config::TiltThresholdDeg, Config::SpikeThresholdG)) {
      updateSnapshot([&](SensorSnapshot& state) {
        state.axG = reading.axG;
        state.ayG = reading.ayG;
        state.azG = reading.azG;
        state.gxDps = reading.gxDps;
        state.gyDps = reading.gyDps;
        state.gzDps = reading.gzDps;
        state.tiltDeg = reading.tiltDeg;
        state.tiltDetected = reading.tiltDetected;
        state.spikeDetected = reading.spikeDetected;
      });
    }
    vTaskDelay(pdMS_TO_TICKS(Config::MotionPeriodMs));
  }
}

void monitorTask(void*) {
  AlertSeverity lastSeverity = AlertSeverity::None;
  uint32_t lastAlertMs = 0;
  uint32_t lastTelemetryMs = 0;

  for (;;) {
    const SensorSnapshot current = copySnapshot();
    const AlertState alert = monitor.evaluate(current);

    digitalWrite(Config::LedOkPin, alert.severity == AlertSeverity::None ? HIGH : LOW);
    digitalWrite(Config::LedWarningPin, alert.severity == AlertSeverity::Warning ? HIGH : LOW);
    digitalWrite(Config::LedCriticalPin, alert.severity == AlertSeverity::Critical ? HIGH : LOW);

    const uint32_t nowMs = millis();
    if (nowMs - lastTelemetryMs >= Config::TelemetryPeriodMs) {
      printTelemetry(current, alert);
      lastTelemetryMs = nowMs;
    }

    if (alert.severity == AlertSeverity::None) {
      if (lastSeverity != AlertSeverity::None) {
        speaker.play(AlertSeverity::None);
      }
    } else {
      if (alert.severity != lastSeverity || nowMs - lastAlertMs >= Config::AlertCooldownMs) {
        speaker.play(alert.severity);
        lastAlertMs = nowMs;
      }
    }

    lastSeverity = alert.severity;
    vTaskDelay(pdMS_TO_TICKS(Config::MonitorPeriodMs));
  }
}

void serialCommandTask(void*) {
  String line;
  line.reserve(96);

  for (;;) {
    while (Serial.available() > 0) {
      const char c = static_cast<char>(Serial.read());
      if (c == '\n' || c == '\r') {
        handleCommand(line);
        line = "";
      } else if (line.length() < 160) {
        line += c;
      } else {
        line = "";
        printEvent(F("command"), F("too_long"));
      }
    }
    vTaskDelay(pdMS_TO_TICKS(25));
  }
}
}  // namespace

void setup() {
  Serial.begin(Config::SerialBaud);
  delay(1000);

  pinMode(Config::LedOkPin, OUTPUT);
  pinMode(Config::LedWarningPin, OUTPUT);
  pinMode(Config::LedCriticalPin, OUTPUT);

  Wire.setSDA(Config::I2cSdaPin);
  Wire.setSCL(Config::I2cSclPin);
  Wire.begin();
  Wire.setClock(Config::I2cClockHz);
  speaker.begin(Config::SpeakerPin);
  distanceSensor.begin(Config::HcSr04TrigPin, Config::HcSr04EchoPin);
  scanServo.attach(Config::ServoPin);
  scanServo.write(ScanAngles[0]);

  printEvent(F("boot"), F("starting"));
  printEvent(F("mcp9808"), tempSensor.begin(Config::Mcp9808Address) ? F("ready") : F("missing"));
  printEvent(F("mpu6050"), motionSensor.begin(Config::Mpu6050Address) ? F("ready") : F("missing"));

  snapshotMutex = xSemaphoreCreateMutex();
  xTaskCreate(tempTask, "temp", 256, nullptr, 1, nullptr);
  xTaskCreate(distanceTask, "distance", 256, nullptr, 1, nullptr);
  xTaskCreate(motionTask, "motion", 256, nullptr, 1, nullptr);
  xTaskCreate(monitorTask, "monitor", 512, nullptr, 2, nullptr);
  xTaskCreate(serialCommandTask, "serial", 384, nullptr, 1, nullptr);
  vTaskStartScheduler();
}

void loop() {}
