#include <Arduino.h>
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
BabyMonitor monitor;

SensorSnapshot snapshot;
SemaphoreHandle_t snapshotMutex;

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
      updateSnapshot([&](SensorSnapshot& state) { state.temperatureC = temperatureC; });
    }
    vTaskDelay(pdMS_TO_TICKS(Config::TempPeriodMs));
  }
}

void distanceTask(void*) {
  float previousDistanceCm = NAN;

  for (;;) {
    float distanceCm = NAN;
    if (distanceSensor.readDistanceCm(distanceCm, Config::HcSr04TimeoutUs)) {
      const bool presenceDetected =
          distanceCm >= Config::PresenceMinCm && distanceCm <= Config::PresenceMaxCm;
      const bool distanceChanged =
          !isnan(previousDistanceCm) &&
          fabs(distanceCm - previousDistanceCm) >= Config::DistanceChangeAlertCm;
      previousDistanceCm = distanceCm;

      updateSnapshot([&](SensorSnapshot& state) {
        state.distanceCm = distanceCm;
        state.presenceDetected = presenceDetected;
        state.distanceChanged = distanceChanged;
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

  for (;;) {
    const SensorSnapshot current = copySnapshot();
    const AlertState alert = monitor.evaluate(current);

    digitalWrite(Config::LedOkPin, alert.severity == AlertSeverity::None ? HIGH : LOW);
    digitalWrite(Config::LedWarningPin, alert.severity == AlertSeverity::Warning ? HIGH : LOW);
    digitalWrite(Config::LedCriticalPin, alert.severity == AlertSeverity::Critical ? HIGH : LOW);

    if (alert.severity != AlertSeverity::None &&
        (alert.severity != lastSeverity || millis() - lastAlertMs >= Config::AlertCooldownMs)) {
      Serial.print("alert=");
      Serial.print(static_cast<uint8_t>(alert.severity));
      Serial.print(" reason=");
      Serial.print(alert.reason);
      Serial.print(" tempC=");
      Serial.print(current.temperatureC);
      Serial.print(" distanceCm=");
      Serial.println(current.distanceCm);
      speaker.play(alert.severity);
      lastAlertMs = millis();
    }

    lastSeverity = alert.severity;
    vTaskDelay(pdMS_TO_TICKS(Config::MonitorPeriodMs));
  }
}
}  // namespace

void setup() {
  Serial.begin(Config::SerialBaud);
  delay(1000);

  pinMode(Config::LedOkPin, OUTPUT);
  pinMode(Config::LedWarningPin, OUTPUT);
  pinMode(Config::LedCriticalPin, OUTPUT);

  Wire.begin();
  speaker.begin(Config::SpeakerPin);
  distanceSensor.begin(Config::HcSr04TrigPin, Config::HcSr04EchoPin);

  Serial.println("NYTW baby monitor prototype booting");
  Serial.println(tempSensor.begin(Config::Mcp9808Address) ? "MCP9808 ready" : "MCP9808 missing");
  Serial.println(motionSensor.begin(Config::Mpu6050Address) ? "MPU6050 ready" : "MPU6050 missing");

  snapshotMutex = xSemaphoreCreateMutex();
  xTaskCreate(tempTask, "temp", 256, nullptr, 1, nullptr);
  xTaskCreate(distanceTask, "distance", 256, nullptr, 1, nullptr);
  xTaskCreate(motionTask, "motion", 256, nullptr, 1, nullptr);
  xTaskCreate(monitorTask, "monitor", 512, nullptr, 2, nullptr);
  vTaskStartScheduler();
}

void loop() {}
