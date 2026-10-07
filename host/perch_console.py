#!/usr/bin/env python3
"""PERCH host console. One-way reader for the Shrike-fi census stream.

Does not send commands. Ignores ESP-IDF log lines. Prints a rolling
SSID table and the latest occupancy summary.
"""

from __future__ import annotations

import argparse
import json
import sys
import time

try:
    import serial
except ImportError:
    serial = None


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="PERCH census viewer")
    p.add_argument("--port", required=True, help="UART or USB-CDC device")
    p.add_argument("--baud", type=int, default=115200)
    return p.parse_args()


def handle(obj: dict, ssids: dict) -> None:
    kind = obj.get("t")
    if kind == "hello":
        print(
            f"PERCH {obj.get('fw')} on {obj.get('board')}  "
            f"channels={obj.get('channels')} dwell={obj.get('dwell_ms')} ms  "
            f"salted={obj.get('salted')}"
        )
        return
    if kind != "census":
        return
    for row in obj.get("ssids") or []:
        key = row.get("bss") or row.get("ssid")
        ssids[key] = row
    stamp = time.strftime("%H:%M:%S")
    print(
        f"[{stamp}] ch {obj.get('ch')}  "
        f"mgmt/s {obj.get('mgmt_rate')}  "
        f"beacons {obj.get('beacons')}  probes {obj.get('probes')}  "
        f"stations {obj.get('stations')}"
    )
    counts = obj.get("ch_counts") or []
    if counts:
        peak = max(counts) or 1
        bar = " ".join(
            f"{i + 1}:{int(8 * n / peak) * '|' or '.'}" for i, n in enumerate(counts)
        )
        print(f"  dwell  {bar}")
    print(f"  {'SSID':<24} {'ch':>3} {'rssi':>5}  flags")
    for row in sorted(ssids.values(), key=lambda r: r.get("rssi") or -127, reverse=True):
        name = row.get("ssid") or "(hidden)"
        flags = []
        if row.get("hidden"):
            flags.append("hidden")
        if row.get("ht"):
            flags.append("ht")
        print(f"  {name[:24]:<24} {row.get('ch', 0):>3} {row.get('rssi', 0):>5}  {','.join(flags)}")
    print()


def main() -> int:
    args = parse_args()
    if serial is None:
        print("pyserial is required: pip install pyserial", file=sys.stderr)
        return 1
    ssids: dict = {}
    with serial.Serial(args.port, args.baud, timeout=1) as ser:
        while True:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            if not line.startswith("{"):
                continue
            try:
                obj = json.loads(line)
            except json.JSONDecodeError:
                continue
            handle(obj, ssids)


if __name__ == "__main__":
    raise SystemExit(main())
