#!/usr/bin/env python3
"""Serial host for the camera-free monitor prototype."""

from __future__ import annotations

import argparse
from contextlib import nullcontext
import json
import queue
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

serial = None
list_ports = None


NUMERIC_FIELDS = (
    "temperature_c",
    "distance_cm",
    "ax_g",
    "ay_g",
    "az_g",
    "gx_dps",
    "gy_dps",
    "gz_dps",
    "tilt_deg",
)


@dataclass
class SmoothState:
    alpha: float
    values: dict[str, float] = field(default_factory=dict)

    def update(self, packet: dict[str, Any]) -> dict[str, float]:
        smoothed: dict[str, float] = {}
        for field_name in NUMERIC_FIELDS:
            value = packet.get(field_name)
            if value is None:
                continue

            value = float(value)
            previous = self.values.get(field_name)
            self.values[field_name] = value if previous is None else (
                self.alpha * value + (1.0 - self.alpha) * previous
            )
            smoothed[field_name] = self.values[field_name]
        return smoothed


def auto_port() -> str:
    require_serial()
    ports = list(list_ports.comports())
    if not ports:
        raise SystemExit("No serial ports found. Pass --port when the board is connected.")

    preferred_tokens = ("usbmodem", "usbserial", "ttyACM", "ttyUSB")
    for port in ports:
        haystack = f"{port.device} {port.description}".lower()
        if any(token.lower() in haystack for token in preferred_tokens):
            return port.device

    return ports[0].device


def classify(packet: dict[str, Any], smoothed: dict[str, float]) -> dict[str, str]:
    temperature = smoothed.get("temperature_c")
    distance = smoothed.get("distance_cm")
    tilt = smoothed.get("tilt_deg")

    if temperature is None:
        thermal = "waiting"
    elif temperature >= 25.0:
        thermal = "too_warm"
    elif temperature >= 22.0:
        thermal = "warm"
    else:
        thermal = "comfortable"

    if distance is None:
        presence = "waiting"
    elif packet.get("presence_detected"):
        presence = "occupied_zone"
    else:
        presence = "zone_clear"

    scan = "changed" if packet.get("scan_cycle_changed") else "stable"

    if packet.get("spike_detected"):
        motion = "movement_spike"
    elif packet.get("tilt_detected"):
        motion = "tilted"
    elif tilt is None:
        motion = "waiting"
    else:
        motion = "settled"

    return {
        "thermal": thermal,
        "presence": presence,
        "scan": scan,
        "motion": motion,
        "alert": str(packet.get("alert_severity", "none")),
    }


def format_status(packet: dict[str, Any], smoothed: dict[str, float], labels: dict[str, str]) -> str:
    temp = smoothed.get("temperature_c")
    distance = smoothed.get("distance_cm")
    tilt = smoothed.get("tilt_deg")
    angle = packet.get("scan_angle_deg")
    cycle = packet.get("scan_cycle")

    temp_text = "--" if temp is None else f"{temp:5.2f} C"
    distance_text = "--" if distance is None else f"{distance:5.1f} cm"
    tilt_text = "--" if tilt is None else f"{tilt:5.1f} deg"
    angle_text = "--" if angle is None else f"{int(angle):3d} deg"
    cycle_text = "--" if cycle is None else str(cycle)

    return (
        f"temp {temp_text} | distance {distance_text} @ {angle_text} | cycle {cycle_text} "
        f"{labels['scan']} | tilt {tilt_text} | "
        f"{labels['thermal']}, {labels['presence']}, {labels['motion']} | "
        f"alert {labels['alert']}: {packet.get('alert_reason', 'normal')}"
    )


def input_worker(commands: queue.Queue[str]) -> None:
    for raw in sys.stdin:
        commands.put(raw.strip())


def command_to_packet(raw: str) -> dict[str, str] | None:
    if not raw:
        return None

    if raw in {"q", "quit", "exit"}:
        return {"cmd": "quit"}

    if raw == "ping":
        return {"cmd": "ping"}

    parts = raw.split()
    if parts[0] in {"beep", "speaker"}:
        severity = parts[1] if len(parts) > 1 else "info"
        if severity not in {"info", "warning", "critical"}:
            print("Use one of: beep info, beep warning, beep critical", file=sys.stderr)
            return None
        return {"cmd": "speaker", "severity": severity}

    print("Commands: ping | beep info | beep warning | beep critical | quit", file=sys.stderr)
    return None


def write_command(ser: serial.Serial, packet: dict[str, str]) -> bool:
    if packet.get("cmd") == "quit":
        return False
    ser.write((json.dumps(packet, separators=(",", ":")) + "\n").encode("utf-8"))
    return True


def require_serial() -> None:
    global serial, list_ports
    if serial is not None and list_ports is not None:
        return

    try:
        import serial as serial_module
        from serial.tools import list_ports as list_ports_module
    except ImportError as exc:
        raise SystemExit(
            "pyserial is required. Install it with: python3 -m pip install -r host/requirements.txt"
        ) from exc

    serial = serial_module
    list_ports = list_ports_module


def open_log(path: Path | None):
    if path is None:
        return nullcontext(None)
    path.parent.mkdir(parents=True, exist_ok=True)
    return path.open("a", encoding="utf-8")


def run(args: argparse.Namespace) -> int:
    require_serial()
    port = args.port or auto_port()
    smoother = SmoothState(alpha=args.alpha)
    commands: queue.Queue[str] = queue.Queue()

    if not args.no_input:
        threading.Thread(target=input_worker, args=(commands,), daemon=True).start()

    with serial.Serial(port, args.baud, timeout=0.2) as ser, open_log(args.log) as log_file:
        print(f"Connected to {port} at {args.baud} baud")
        print("Commands: ping | beep info | beep warning | beep critical | quit")

        running = True
        while running:
            while not commands.empty():
                packet = command_to_packet(commands.get())
                if packet is not None:
                    running = write_command(ser, packet)

            line = ser.readline().decode("utf-8", errors="replace").strip()
            if not line:
                continue

            try:
                packet = json.loads(line)
            except json.JSONDecodeError:
                print(f"raw: {line}")
                continue

            if packet.get("type") == "event":
                print(f"event {packet.get('event')}: {packet.get('status')}")
                continue

            if packet.get("type") != "telemetry":
                print(f"packet: {packet}")
                continue

            smoothed = smoother.update(packet)
            labels = classify(packet, smoothed)
            enriched = {
                "host_time": time.time(),
                "packet": packet,
                "smoothed": smoothed,
                "state": labels,
            }

            print(format_status(packet, smoothed, labels))
            if log_file is not None:
                log_file.write(json.dumps(enriched, separators=(",", ":")) + "\n")
                log_file.flush()

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Read and classify monitor telemetry over serial.")
    parser.add_argument("--port", help="Serial port, for example /dev/tty.usbmodem1101")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--alpha", type=float, default=0.25, help="EMA smoothing factor")
    parser.add_argument("--log", type=Path, default=Path("logs/monitor.jsonl"))
    parser.add_argument("--no-input", action="store_true", help="Disable interactive command input")
    return run(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
