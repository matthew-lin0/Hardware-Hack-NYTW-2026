#!/usr/bin/env python3
"""Tiny desktop dashboard for the STM32 bring-up CSV serial stream."""

from __future__ import annotations

import argparse
import csv
import queue
import sys
import threading
import time
from pathlib import Path
from tkinter import BOTH, END, LEFT, RIGHT, StringVar, Tk, filedialog, messagebox, ttk

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover - user environment check
    print("Missing pyserial. Install with: python3 -m pip install --user pyserial", file=sys.stderr)
    raise


CSV_FIELDS = [
    "sample_ms",
    "temp_status",
    "temp_c",
    "temp_f",
    "motion_status",
    "tilt_deg",
    "tilt",
    "spike",
    "distance_status",
    "distance_cm",
    "presence",
    "distance_changed",
    "alert",
    "reason",
]


def default_port() -> str | None:
    for port in list_ports.comports():
        description = f"{port.description} {port.hwid}".lower()
        if "stlink" in description or "0483:374e" in description:
            return port.device
    for port in list_ports.comports():
        if "usbmodem" in port.device:
            return port.device
    return None


def parse_csv_line(line: str) -> dict[str, str] | None:
    if not line.startswith("csv,"):
        return None

    row = next(csv.reader([line]))
    values = row[1:]
    if len(values) != len(CSV_FIELDS):
        return None
    return dict(zip(CSV_FIELDS, values))


class SerialReader(threading.Thread):
    def __init__(self, port: str, baud: int, events: "queue.Queue[tuple[str, object]]") -> None:
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.events = events
        self.stop_event = threading.Event()
        self.serial = None

    def run(self) -> None:
        try:
            with serial.Serial(self.port, self.baud, timeout=1) as stream:
                self.serial = stream
                self.events.put(("status", f"Connected to {self.port} @ {self.baud}"))
                while not self.stop_event.is_set():
                    raw = stream.readline()
                    if not raw:
                        continue
                    line = raw.decode("utf-8", errors="replace").strip()
                    parsed = parse_csv_line(line)
                    if parsed:
                        self.events.put(("sample", parsed))
                    else:
                        self.events.put(("log", line))
        except Exception as exc:  # pragma: no cover - UI feedback path
            self.events.put(("status", f"Serial error: {exc}"))

    def stop(self) -> None:
        self.stop_event.set()


class Dashboard:
    def __init__(self, port: str, baud: int) -> None:
        self.root = Tk()
        self.root.title("NYTW STM32 Sensor Dashboard")
        self.root.geometry("760x560")
        self.events: "queue.Queue[tuple[str, object]]" = queue.Queue()
        self.reader = SerialReader(port, baud, self.events)
        self.samples: list[dict[str, str]] = []
        self.vars = {field: StringVar(value="-") for field in CSV_FIELDS}
        self.status = StringVar(value="Starting serial reader...")

        self._build_ui()
        self.root.protocol("WM_DELETE_WINDOW", self.close)

    def _build_ui(self) -> None:
        outer = ttk.Frame(self.root, padding=12)
        outer.pack(fill=BOTH, expand=True)

        ttk.Label(outer, textvariable=self.status).pack(anchor="w")

        cards = ttk.Frame(outer)
        cards.pack(fill="x", pady=(12, 8))
        for label, field in [
            ("Temperature C", "temp_c"),
            ("Temperature F", "temp_f"),
            ("Alert", "alert"),
            ("Distance cm", "distance_cm"),
            ("Tilt deg", "tilt_deg"),
        ]:
            card = ttk.LabelFrame(cards, text=label, padding=10)
            card.pack(side=LEFT, fill="x", expand=True, padx=4)
            ttk.Label(card, textvariable=self.vars[field], font=("TkDefaultFont", 18, "bold")).pack()

        details = ttk.LabelFrame(outer, text="Latest Sensor Reading", padding=10)
        details.pack(fill="x", pady=8)

        for index, field in enumerate(CSV_FIELDS):
            row = index // 2
            column = (index % 2) * 2
            ttk.Label(details, text=f"{field}:").grid(row=row, column=column, sticky="e", padx=4, pady=2)
            ttk.Label(details, textvariable=self.vars[field]).grid(
                row=row, column=column + 1, sticky="w", padx=4, pady=2
            )

        controls = ttk.Frame(outer)
        controls.pack(fill="x", pady=8)
        ttk.Button(controls, text="Save CSV", command=self.save_csv).pack(side=LEFT)
        ttk.Button(controls, text="Quit", command=self.close).pack(side=RIGHT)

        self.log = ttk.Treeview(outer, columns=("time", "temp", "alert", "distance"), show="headings", height=10)
        self.log.heading("time", text="sample_ms")
        self.log.heading("temp", text="temp_c")
        self.log.heading("alert", text="alert")
        self.log.heading("distance", text="distance_cm")
        self.log.pack(fill=BOTH, expand=True, pady=(8, 0))

    def start(self) -> None:
        self.reader.start()
        self.root.after(100, self.poll_events)
        self.root.mainloop()

    def poll_events(self) -> None:
        while True:
            try:
                event, payload = self.events.get_nowait()
            except queue.Empty:
                break

            if event == "status":
                self.status.set(str(payload))
            elif event == "sample":
                self.update_sample(payload)  # type: ignore[arg-type]

        self.root.after(100, self.poll_events)

    def update_sample(self, sample: dict[str, str]) -> None:
        self.samples.append({"wall_time": f"{time.time():.3f}", **sample})
        for field, value in sample.items():
            self.vars[field].set(value or "-")

        self.log.insert(
            "",
            0,
            values=(
                sample.get("sample_ms", ""),
                sample.get("temp_c", ""),
                sample.get("alert", ""),
                sample.get("distance_cm", ""),
            ),
        )
        for item in self.log.get_children()[250:]:
            self.log.delete(item)

    def save_csv(self) -> None:
        if not self.samples:
            messagebox.showinfo("No samples", "No CSV samples have been received yet.")
            return

        default = Path.home() / "Downloads" / "nytw_sensor_readings.csv"
        path = filedialog.asksaveasfilename(
            defaultextension=".csv",
            initialfile=default.name,
            initialdir=str(default.parent),
            filetypes=[("CSV files", "*.csv"), ("All files", "*.*")],
        )
        if not path:
            return

        fields = ["wall_time", *CSV_FIELDS]
        with open(path, "w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=fields)
            writer.writeheader()
            writer.writerows(self.samples)
        messagebox.showinfo("Saved", f"Saved {len(self.samples)} samples to {path}")

    def close(self) -> None:
        self.reader.stop()
        self.root.destroy()


def main() -> int:
    parser = argparse.ArgumentParser(description="View STM32 sensor CSV serial output.")
    parser.add_argument("--port", default=default_port(), help="Serial port, defaults to detected ST-LINK port.")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    if not args.port:
        print("No ST-LINK serial port found. Plug in the STM32 and try again.", file=sys.stderr)
        return 1

    Dashboard(args.port, args.baud).start()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
