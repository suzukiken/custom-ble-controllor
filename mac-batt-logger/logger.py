#!/usr/bin/env python3
"""
Receive BattMon Xiao Nordic UART telemetry and keep a local text log updated.

Firmware line format:
  uptime_s=<sec> percent=<0-100> voltage_mv=<mv>\\n

Log file (TSV, appended each sample; also rewritten as *-latest.tsv):
  host_iso\\tuptime_s\\tpercent\\tvoltage_mv
"""

from __future__ import annotations

import argparse
import asyncio
import re
import sys
from datetime import datetime, timezone
from pathlib import Path
from uuid import UUID

from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic
from bleak.backends.device import BLEDevice
from bleak.backends.scanner import AdvertisementData

# Nordic UART Service (Adafruit BLEUart)
NUS_SERVICE = UUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
NUS_TX_CHAR = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  # notify (device → host)
DEFAULT_NAME = "BattMon Xiao"
LINE_RE = re.compile(
    r"uptime_s=(\d+)\s+percent=(\d+)\s+voltage_mv=(\d+)",
    re.IGNORECASE,
)


def host_iso() -> str:
    return datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")


def parse_line(text: str) -> tuple[int, int, int] | None:
    m = LINE_RE.search(text)
    if not m:
        return None
    return int(m.group(1)), int(m.group(2)), int(m.group(3))


def _norm_uuid(value: object) -> str:
    return str(value).lower()


def device_label(device: BLEDevice, adv: AdvertisementData | None = None) -> str:
    if device.name:
        return device.name
    if adv and adv.local_name:
        return adv.local_name
    return ""


class FileSink:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.latest_path = path.with_name(path.stem + "-latest" + path.suffix)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        if not self.path.exists():
            self.path.write_text(
                "host_iso\tuptime_s\tpercent\tvoltage_mv\n", encoding="utf-8"
            )

    def write_sample(self, uptime_s: int, percent: int, voltage_mv: int) -> None:
        row = f"{host_iso()}\t{uptime_s}\t{percent}\t{voltage_mv}\n"
        with self.path.open("a", encoding="utf-8") as f:
            f.write(row)
            f.flush()
        self.latest_path.write_text(
            "host_iso\tuptime_s\tpercent\tvoltage_mv\n" + row,
            encoding="utf-8",
        )
        print(row.rstrip(), flush=True)


async def find_device(
    name: str,
    timeout: float,
    address: str | None = None,
) -> BLEDevice:
    target = name.casefold()
    short = "battmon"
    nus = str(NUS_SERVICE).lower()

    if address:
        print(f"Looking up address {address}…", flush=True)
        device = await BleakScanner.find_device_by_address(address, timeout=timeout)
        if device is None:
            raise RuntimeError(f"No device at address {address!r}")
        print(f"Found {device.name or '(no name)'}  address={device.address}", flush=True)
        return device

    print(
        f"Scanning for name {name!r} or NUS UUID (timeout {timeout:.0f}s)…",
        flush=True,
    )
    seen: dict[str, tuple[str, str]] = {}

    def match(device: BLEDevice, adv: AdvertisementData) -> bool:
        label = device_label(device, adv)
        uuids = {_norm_uuid(u) for u in (adv.service_uuids or [])}
        seen[device.address] = (label or "(no name)", ",".join(sorted(uuids)) or "-")
        label_cf = label.casefold()
        # Prefer real BattMon name. macOS often caches an old name (e.g. OneKey)
        # for a re-flashed board — then NUS UUID is the reliable signal.
        if label_cf == target or short in label_cf:
            return True
        return nus in uuids

    device = await BleakScanner.find_device_by_filter(match, timeout=timeout)
    if device is None:
        if seen:
            print("# devices seen during scan:", flush=True)
            for addr, (label, uuids) in sorted(seen.items()):
                print(f"#   {addr}  name={label!r}  uuids={uuids}", flush=True)
        raise RuntimeError(
            f"Device {name!r} not found.\n"
            "Check: UF2 flashed, board powered, blue LED blinking while idle, "
            "macOS Bluetooth on, and Terminal has Bluetooth permission "
            "(System Settings → Privacy & Security → Bluetooth).\n"
            "Retry with --address <uuid> from the scan dump above."
        )
    shown = device.name or "(no name)"
    print(f"Found {shown}  address={device.address}", flush=True)
    if "battmon" not in shown.casefold():
        print(
            "# note: displayed name is not BattMon — macOS may be caching an old "
            "name for this board. Continuing because NUS/BattMon match succeeded.",
            flush=True,
        )
    return device


async def run(
    name: str,
    out: Path,
    scan_timeout: float,
    address: str | None,
) -> None:
    sink = FileSink(out)
    buffer = ""

    while True:
        device = await find_device(name, scan_timeout, address)

        def on_notify(_: BleakGATTCharacteristic, data: bytearray) -> None:
            nonlocal buffer
            buffer += data.decode("utf-8", errors="replace")
            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                line = line.strip()
                if not line:
                    continue
                parsed = parse_line(line)
                if parsed is None:
                    print(f"# unparsed: {line!r}", flush=True)
                    continue
                sink.write_sample(*parsed)

        print(f"Connecting… log → {out}", flush=True)
        try:
            async with BleakClient(device, timeout=30.0) as client:
                if not client.is_connected:
                    raise RuntimeError("connect failed")
                print("Connected. Waiting for notifications…", flush=True)
                await client.start_notify(NUS_TX_CHAR, on_notify)
                while client.is_connected:
                    await asyncio.sleep(1.0)
        except asyncio.CancelledError:
            raise
        except Exception as exc:  # noqa: BLE001 — keep logger alive
            print(f"# disconnect/error: {exc!r}; retry in 5s", flush=True)
            await asyncio.sleep(5.0)


def main() -> int:
    p = argparse.ArgumentParser(description="BattMon Xiao BLE logger for macOS")
    p.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("batt-monitor-log.tsv"),
        help="TSV log path (default: ./batt-monitor-log.tsv)",
    )
    p.add_argument(
        "-n",
        "--name",
        default=DEFAULT_NAME,
        help=f"BLE advertised name (default: {DEFAULT_NAME})",
    )
    p.add_argument(
        "-a",
        "--address",
        default=None,
        help="Connect by CoreBluetooth UUID / address (skip name match)",
    )
    p.add_argument(
        "--scan-timeout",
        type=float,
        default=30.0,
        help="Seconds to scan before giving up on one attempt",
    )
    args = p.parse_args()
    try:
        asyncio.run(run(args.name, args.output, args.scan_timeout, args.address))
    except KeyboardInterrupt:
        print("\nStopped.", flush=True)
        return 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
