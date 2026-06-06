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

- The firmware starts in a simple bring-up loop, so missing sensors print as missing/timeout and
  do not stop the board.
- The onboard LED blinks as a heartbeat when firmware is alive.
- MCP9808 warns at 22 C and marks critical at 25 C.
- MPU-6050 detects tilt at 40 degrees and acceleration spikes.
- HC-SR04 marks the presence zone active when something is within 5-55 cm.
- Speaker plays info, warning, and critical tone patterns when enabled.
- `baby_monitor` ranks alerts so critical conditions win over warning and info states.

Edit pins, thresholds, feature flags, and report timing in `src/config.h`.

Feature flags in `src/config.h` let you test one part at a time:

```cpp
constexpr bool EnableMcp9808 = true;
constexpr bool EnableMpu6050 = true;
constexpr bool EnableHcSr04 = true;
constexpr bool EnableSpeaker = true;
constexpr bool EnableI2cScan = true;
constexpr bool EnableCsvOutput = true;
```

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

If PlatformIO does not auto-pick the port, use the ST-LINK port directly:

```bash
pio device monitor --port /dev/cu.usbmodem1103 --baud 115200
```

## Bring-Up Flow

1. Plug in only the STM32 over USB.
2. Build and upload:

   ```bash
   pio run --target upload
   ```

3. Open serial monitor:

   ```bash
   pio device monitor --port /dev/cu.usbmodem1103 --baud 115200
   ```

4. Confirm:

   ```text
   boot: NYTW monitor bring-up
   status: board firmware is alive
   ```

5. Add one sensor at a time and watch its status change from `missing`, `timeout`, or `disabled`
   to `ready`.

Expected serial shape:

```text
sample_ms: 12000
mcp9808: ready
temp: 23.50 C / 74.30 F
mpu6050: missing
motion: missing
hcsr04: timeout
distance: timeout
alert: warning reason=temperature warm
csv,12000,ready,23.50,74.30,missing,,0,0,timeout,,0,0,warning,temperature warm
i2c_scan: 0x18
```

CSV line fields:

```text
sample_ms,temp_status,temp_c,temp_f,motion_status,tilt_deg,tilt,spike,distance_status,distance_cm,presence,distance_changed,alert,reason
```

## Desktop Dashboard

The repo includes a small local dashboard that reads the CSV serial lines and displays the latest
sensor readings.

Run it with:

```bash
python3 tools/serial_dashboard.py
```

Or specify the ST-LINK port directly:

```bash
python3 tools/serial_dashboard.py --port /dev/cu.usbmodem103 --baud 115200
```

The dashboard can save received samples as a CSV file.

## Wiring Notes

The MCP9808 and MPU-6050 use I2C. This firmware sets I2C explicitly to the Arduino header pins:

- SDA: `D14` / `PB9`
- SCL: `D15` / `PB8`
- Power: `3.3V`
- Ground: `GND`

Power the modules from 3.3 V unless the breakout board documentation says otherwise.

Many HC-SR04 modules output a 5 V echo signal. Use a voltage divider or level shifter before
connecting Echo to an STM32 GPIO pin.
