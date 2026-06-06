# Hardware Hack NYTW 2026

STM32-based nursery environment and movement monitor prototype for the NYTW hardware hack.

This is a learning prototype, not a medical device or safety-critical baby monitor. Keep all
electronics, wires, batteries, and small parts outside any crib or sleep area.

## Hardware

- STM32G4 Nucleo-64 development board, default target `NUCLEO-G474RE`
- MPU-6050 accelerometer and gyroscope
- HC-SR04 ultrasonic distance sensor
- MCP9808 temperature sensor
- Adafruit STEMMA speaker
- Breadboard, jumper wires, LEDs, and resistors

## Project Layout

```text
.
├── .github/workflows/build.yml
├── src/
│   ├── config.h
│   ├── main.cpp
│   ├── sensors/
│   │   ├── mcp9808.h
│   │   ├── mcp9808.cpp
│   │   ├── mpu6050.h
│   │   ├── mpu6050.cpp
│   │   ├── hcsr04.h
│   │   └── hcsr04.cpp
│   ├── audio/
│   │   ├── speaker.h
│   │   └── speaker.cpp
│   └── monitor/
│       ├── baby_monitor.h
│       └── baby_monitor.cpp
├── platformio.ini
└── README.md
```

## Behavior

- MCP9808 warns at 22 C and marks critical at 25 C.
- MPU-6050 detects tilt at 40 degrees and acceleration spikes.
- HC-SR04 marks the presence zone active when something is within 5-55 cm.
- Speaker plays info, warning, and critical tone patterns.
- `baby_monitor` ranks alerts so critical conditions win over warning and info states.

Edit pins, thresholds, and task periods in `src/config.h`.

## Build

Install PlatformIO, then run:

```bash
pio run
```

Upload to the board:

```bash
pio run --target upload
```

Open serial logs:

```bash
pio device monitor
```

## Wiring Notes

The MCP9808 and MPU-6050 use I2C. Connect SDA/SCL to the Nucleo board's I2C pins and power the
modules from 3.3 V unless the breakout board documentation says otherwise.

Many HC-SR04 modules output a 5 V echo signal. Use a voltage divider or level shifter before
connecting Echo to an STM32 GPIO pin.

