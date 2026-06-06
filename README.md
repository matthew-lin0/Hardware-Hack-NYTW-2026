# Hardware Hack NYTW 2026

STM32-based privacy-preserving nursery environment and activity monitor prototype for the NYTW
hardware hack.

This is a learning prototype, not a medical device or safety-critical baby monitor. Keep all
electronics, wires, batteries, and small parts outside any crib or sleep area.

The prototype has two layers:

- Firmware on the microcontroller reads sensors, drives LEDs/speaker, and exchanges newline JSON
  over Serial.
- A host app on a laptop or Raspberry Pi parses telemetry, smooths values, classifies readable
  states, logs data, and sends demo commands back to the board.

## Hardware

- STM32G4 Nucleo-64 development board, default target `NUCLEO-G474RE`
- MPU-6050 accelerometer and gyroscope
- HC-SR04 ultrasonic distance sensor
- MCP9808 temperature sensor
- SG90 or compatible servo for ultrasonic scanning
- Adafruit STEMMA speaker
- Breadboard, jumper wires, LEDs, and resistors

## Project Layout

```text
.
├── host/
│   ├── monitor.py
│   └── requirements.txt
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
- HC-SR04 is mounted on a servo and scans a 180 degree field in 30 degree increments.
- A scan cycle uses 12 readings in a smooth ping-pong pattern:
  `0, 30, 60, 90, 120, 150, 180, 150, 120, 90, 60, 30`.
- After every 12-reading cycle, the firmware compares the new scan against the previous scan and
  flags a change when any matching point differs by at least 15 cm.
- The current scan point marks the presence zone active when something is within 5-55 cm.
- Speaker plays info, warning, and critical tone patterns.
- Speaker output is explicitly silenced when the alert state clears.
- `baby_monitor` ranks alerts so critical conditions win over warning and info states.
- Firmware emits one JSON object per line for host parsing.

Edit pins, thresholds, scan settings, and task periods in `src/config.h`.

## Serial Protocol

Firmware telemetry is newline-delimited JSON:

```json
{"type":"telemetry","ms":12345,"temperature_c":23.125,"distance_cm":31.200,"scan_angle_deg":90,"scan_step":3,"scan_cycle":4,"ax_g":0.010,"ay_g":0.030,"az_g":0.990,"gx_dps":0.120,"gy_dps":0.000,"gz_dps":-0.240,"tilt_deg":2.100,"presence_detected":true,"distance_changed":false,"scan_cycle_changed":false,"tilt_detected":false,"spike_detected":false,"alert_severity":"warning","alert_reason":"temperature warm"}
```

Startup and command responses are event packets:

```json
{"type":"event","event":"mpu6050","status":"ready"}
```

The host can send command packets back to firmware:

```json
{"cmd":"ping"}
{"cmd":"speaker","severity":"warning"}
{"cmd":"servo_test"}
```

Supported speaker severities are `info`, `warning`, and `critical`.

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

## Host App

Install host dependencies:

```bash
python3 -m pip install -r host/requirements.txt
```

Run the monitor. It will auto-pick a likely serial port if possible:

```bash
python3 host/monitor.py
```

Or pass a port explicitly:

```bash
python3 host/monitor.py --port /dev/tty.usbmodem1101
```

The app prints smoothed temperature, distance, tilt, readable state labels, and alert reasons. It
also prints the current scanner angle/cycle state and appends structured records to
`logs/monitor.jsonl` by default.

Interactive commands while it is running:

```text
ping
servo_test
beep info
beep warning
beep critical
quit
```

## Wiring Notes

The MCP9808 and MPU-6050 use I2C. This firmware sets I2C explicitly to the Arduino header pins:

- SDA: `D14` / `PB9`
- SCL: `D15` / `PB8`
- Power: `3.3V`
- Ground: `GND`

Power the modules from 3.3 V unless the breakout board documentation says otherwise.

Many HC-SR04 modules output a 5 V echo signal. Use a voltage divider or level shifter before
connecting Echo to an STM32 GPIO pin.
