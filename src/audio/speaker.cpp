#include "speaker.h"

void Speaker::begin(uint8_t pin) {
  pin_ = pin;
  pinMode(pin_, OUTPUT);
  noTone(pin_);
}

void Speaker::play(AlertSeverity severity) {
  switch (severity) {
    case AlertSeverity::Info:
      toneFor(880, 90);
      delay(40);
      toneFor(880, 90);
      break;
    case AlertSeverity::Warning:
      toneFor(740, 140);
      delay(50);
      toneFor(988, 180);
      break;
    case AlertSeverity::Critical:
      toneFor(988, 150);
      delay(70);
      toneFor(988, 150);
      delay(70);
      toneFor(1319, 240);
      break;
    case AlertSeverity::None:
      noTone(pin_);
      break;
  }
}

void Speaker::toneFor(uint16_t frequency, uint16_t durationMs) {
  tone(pin_, frequency, durationMs);
  delay(durationMs);
  noTone(pin_);
}

