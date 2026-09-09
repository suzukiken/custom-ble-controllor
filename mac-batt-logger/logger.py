#!/usr/bin/env python3
"""
Receive BattMon Xiao telemetry and append to a TSV log.

Prefers custom GATT (read+notify). Falls back to Nordic UART notify when
macOS is still serving a cached NUS-only GATT table after a reflash.

Custom:
  7f5f0001-… / 7f5f0002-…  (read + notify)
NUS:
  6e400003-…  (notify), 6e400002-… (write poll)
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

CUSTOM_SERVICE = UUID("7f5f0001-7a4b-4c8f-9e2d-1b3c5a7e9f01")
CUSTOM_CHAR = "7f5f0002-7a4b-4c8f-9e2d-1b3c5a7e9f01"
NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
DEFAULT_NAME = "BattMon Xiao"
LINE_RE = re.compile(
    r"uptime_s=(\d+)\s+percent=(\d+)\s+voltage_mv=(\d+)",
    re.IGNORECASE,
)
READ_EVERY_S = 5.0


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
        self._last_key: tuple[int, int, int] | None = None
        self.last_sample: tuple[int, int, int] | None = None

    def write_sample(
        self, uptime_s: int, percent: int, voltage_mv: int, *, source: str
    ) -> None:
        key = (uptime_s, percent, voltage_mv)
        self.last_sample = key
        if key == self._last_key:
            return
        self._last_key = key
        row = f"{host_iso()}\t{uptime_s}\t{percent}\t{voltage_mv}\n"
        with self.path.open("a", encoding="utf-8") as f:
            f.write(row)
            f.flush()
        self.latest_path.write_text(
            "host_iso\tuptime_s\tpercent\tvoltage_mv\n" + row,
            encoding="utf-8",
        )
        print(f"{row.rstrip()}  # {source}", flush=True)


def handle_payload(sink: FileSink, raw: bytes | bytearray, source: str) -> None:
    text = raw.decode("utf-8", errors="replace")
    for part in text.replace("\r", "\n").split("\n"):
        line = part.strip()
        if not line:
            continue
        parsed = parse_line(line)
        if parsed is None:
            print(f"# unparsed ({source}): {line!r}", flush=True)
            continue
        sink.write_sample(*parsed, source=source)


async def find_device(
    name: str, timeout: float, address: str | None
) -> BLEDevice:
    target = name.casefold()
    short = "battmon"
    custom = str(CUSTOM_SERVICE).lower()

    if address:
        device = await BleakScanner.find_device_by_address(address, timeout=timeout)
        if device is None:
            raise RuntimeError(f"No device at address {address!r}")
        print(f"Found {device.name or '(no name)'}  address={device.address}", flush=True)
        return device

    print(f"Scanning for {name!r} (timeout {timeout:.0f}s)…", flush=True)
    seen: dict[str, tuple[str, str]] = {}

    def match(device: BLEDevice, adv: AdvertisementData) -> bool:
        label = device_label(device, adv)
        uuids = {_norm_uuid(u) for u in (adv.service_uuids or [])}
        seen[device.address] = (label or "(no name)", ",".join(sorted(uuids)) or "-")
        if label.casefold() == target or short in label.casefold():
            return True
        return custom in uuids

    device = await BleakScanner.find_device_by_filter(match, timeout=timeout)
    if device is None:
        if seen:
            print("# devices seen:", flush=True)
            for addr, (label, uuids) in sorted(seen.items()):
                print(f"#   {addr}  name={label!r}  uuids={uuids}", flush=True)
        raise RuntimeError(f"Device {name!r} not found")
    print(f"Found {device.name or '(no name)'}  address={device.address}", flush=True)
    return device


def char_uuids(client: BleakClient) -> set[str]:
    out: set[str] = set()
    for svc in client.services:
        for char in svc.characteristics:
            out.add(str(char.uuid).lower())
    return out


async def run(
    name: str, out: Path, scan_timeout: float, address: str | None
) -> None:
    sink = FileSink(out)

    while True:
        device = await find_device(name, scan_timeout, address)

        def on_notify(_: BleakGATTCharacteristic, data: bytearray) -> None:
            handle_payload(sink, data, "notify")

        print(f"Connecting… log → {out}", flush=True)
        try:
            async with BleakClient(device, timeout=30.0) as client:
                if not client.is_connected:
                    raise RuntimeError("connect failed")

                print("# GATT:", flush=True)
                for svc in client.services:
                    print(f"#  service {svc.uuid}", flush=True)
                    for char in svc.characteristics:
                        print(
                            f"#    char {char.uuid} props={char.properties}",
                            flush=True,
                        )

                uuids = char_uuids(client)
                has_custom = CUSTOM_CHAR.lower() in uuids
                has_nus = NUS_TX.lower() in uuids

                if not has_custom and has_nus:
                    print(
                        "# warning: only Nordic UART is visible — macOS may be "
                        "caching old GATT. Toggle Bluetooth OFF/ON (or Forget "
                        "BattMon), reflash, then reconnect. Meanwhile using NUS.",
                        flush=True,
                    )

                if has_custom:
                    try:
                        await client.start_notify(CUSTOM_CHAR, on_notify)
                        print("Notify on custom char enabled.", flush=True)
                    except Exception as exc:  # noqa: BLE001
                        print(f"# custom notify failed: {exc!r}", flush=True)
                    try:
                        raw = await client.read_gatt_char(CUSTOM_CHAR)
                        print(f"# first read {raw!r}", flush=True)
                        handle_payload(sink, raw, "read")
                    except Exception as exc:  # noqa: BLE001
                        print(f"# custom read failed: {exc!r}", flush=True)

                if has_nus:
                    try:
                        await client.start_notify(NUS_TX, on_notify)
                        print("Notify on NUS TX enabled.", flush=True)
                    except Exception as exc:  # noqa: BLE001
                        print(f"# NUS notify failed: {exc!r}", flush=True)
                    try:
                        await client.write_gatt_char(NUS_RX, b"poll\n", response=False)
                        print("NUS poll written.", flush=True)
                    except Exception as exc:  # noqa: BLE001
                        print(f"# NUS poll failed: {exc!r}", flush=True)

                if not has_custom and not has_nus:
                    raise RuntimeError("No known telemetry characteristics")

                elapsed = 0.0
                while client.is_connected:
                    await asyncio.sleep(READ_EVERY_S)
                    elapsed += READ_EVERY_S
                    if has_custom:
                        try:
                            raw = await client.read_gatt_char(CUSTOM_CHAR)
                            handle_payload(sink, raw, "read")
                        except Exception as exc:  # noqa: BLE001
                            print(f"# read failed: {exc!r}", flush=True)
                            break
                    elif has_nus:
                        try:
                            await client.write_gatt_char(
                                NUS_RX, b"poll\n", response=False
                            )
                        except Exception as exc:  # noqa: BLE001
                            print(f"# NUS poll failed: {exc!r}", flush=True)
                            break
                    if int(elapsed) % 30 == 0:
                        print(f"# still connected… {int(elapsed)}s", flush=True)
                if sink.last_sample is not None:
                    u, p, v = sink.last_sample
                    print(
                        f"# session end — last uptime_s={u} percent={p} "
                        f"voltage_mv={v} (≈ {u / 3600:.2f} h)",
                        flush=True,
                    )
        except asyncio.CancelledError:
            raise
        except Exception as exc:  # noqa: BLE001
            if sink.last_sample is not None:
                u, p, v = sink.last_sample
                print(
                    f"# last known sample before error: uptime_s={u} "
                    f"percent={p} voltage_mv={v}",
                    flush=True,
                )
            print(f"# disconnect/error: {exc!r}; retry in 5s", flush=True)
            await asyncio.sleep(5.0)


def main() -> int:
    p = argparse.ArgumentParser(description="BattMon Xiao BLE logger for macOS")
    p.add_argument("-o", "--output", type=Path, default=Path("batt-monitor-log.tsv"))
    p.add_argument("-n", "--name", default=DEFAULT_NAME)
    p.add_argument("-a", "--address", default=None)
    p.add_argument("--scan-timeout", type=float, default=30.0)
    args = p.parse_args()
    try:
        asyncio.run(run(args.name, args.output, args.scan_timeout, args.address))
    except KeyboardInterrupt:
        print("\nStopped.", flush=True)
        return 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
