#!/usr/bin/env python3
"""Build and upload the DustMover firmware, then check its serial output.

Run from a PC with the ESP32 attached via USB:

    python tools/flash_and_test.py                 # auto-detect port
    python tools/flash_and_test.py --port COM5
    python tools/flash_and_test.py --no-upload     # test the firmware already on the board

Needs PlatformIO Core and pyserial (`pip install pyserial`).
Keep the sensor at rest during the test. The captured output is written to
.pio/flash-test.log.
"""

import argparse
import math
import os
import re
import shutil
import statistics
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BAUD = 230400
NOMINAL_HZ = 100.0
LSB_PER_G = 16384.0
# CP210x, CH340/CH341/CH910x, FTDI, Espressif USB-JTAG
USB_SERIAL_VIDS = {0x10C4, 0x1A86, 0x0403, 0x303A}


def find_pio():
    exe = shutil.which("pio") or shutil.which("platformio")
    if exe:
        return exe
    sub = "Scripts/pio.exe" if os.name == "nt" else "bin/pio"
    candidate = Path.home() / ".platformio" / "penv" / sub
    return str(candidate) if candidate.exists() else None


def find_port():
    from serial.tools import list_ports

    ports = list(list_ports.comports())
    matches = [p for p in ports if p.vid in USB_SERIAL_VIDS]
    if len(matches) == 1:
        return matches[0].device
    print("Serial ports found:")
    for p in ports:
        print(f"  {p.device}  {p.description}  [{p.hwid}]")
    if not ports:
        print("  none. Check the USB data cable and the USB-serial driver "
              "(see docs/TOOLCHAIN.md).")
    sys.exit("Could not pick a port automatically. Pass --port.")


def capture(port, seconds):
    import serial

    ser = serial.Serial()
    ser.port = port
    ser.baudrate = BAUD
    ser.timeout = 0.2
    # Do not hold the board in reset or bootloader mode while opening.
    ser.dtr = False
    ser.rts = False
    ser.open()
    try:
        # Pulse EN through the DevKit auto-reset circuit to capture the boot log.
        ser.rts = True
        time.sleep(0.1)
        ser.reset_input_buffer()
        ser.rts = False
        data = bytearray()
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            data += ser.read(4096)
    finally:
        ser.close()
    return data.decode("utf-8", errors="replace").splitlines()


def analyze(lines):
    """Return a list of (passed, description) checks for captured serial lines."""
    checks = []

    def check(passed, text):
        checks.append((bool(passed), text))

    errors = [line for line in lines if line.startswith("ERROR")]
    check(not errors, "no ERROR lines" + (f" (first: {errors[0]})" if errors else ""))

    ids = re.findall(r"INIT: ICM-42688-P WHO_AM_I=0x([0-9A-Fa-f]{2})", "\n".join(lines))
    check(ids and ids[-1].upper() == "47",
          "WHO_AM_I is 0x47" + (f" (got 0x{ids[-1]})" if ids else " (no WHO_AM_I line)"))

    check(any(line.startswith(("UART streaming started", "LOGGER"))
              for line in lines), "firmware reached the streaming state")

    samples = []
    for line in lines:
        fields = line.split(",")
        if fields[0] != "RAW" or len(fields) != 13:
            continue
        try:
            samples.append([int(value) for value in fields[1:]])
        except ValueError:
            pass
    check(len(samples) >= 2, f"RAW sample lines received ({len(samples)})")
    if len(samples) < 2:
        return checks

    span_us = samples[-1][0] - samples[0][0]
    rate = (len(samples) - 1) * 1e6 / span_us if span_us > 0 else 0.0
    check(0.9 * NOMINAL_HZ <= rate <= 1.1 * NOMINAL_HZ,
          f"sample rate {rate:.1f} Hz is within 10 % of {NOMINAL_HZ:.0f} Hz")

    check(all(b[0] > a[0] for a, b in zip(samples, samples[1:])),
          "timestamps increase monotonically")

    last = samples[-1]
    check(last[5] == 0, f"read_errors is 0 (got {last[5]})")
    check(last[3] == 0 and last[4] == 0,
          f"no coalesced interrupts or queue drops (got {last[3]}, {last[4]})")

    gravity = statistics.median(
        math.sqrt(s[6] ** 2 + s[7] ** 2 + s[8] ** 2) / LSB_PER_G for s in samples)
    check(0.8 <= gravity <= 1.2,
          f"acceleration magnitude at rest {gravity:.2f} g is about 1 g")

    check(any(len({s[i] for s in samples}) > 1 for i in range(6, 12)),
          "sensor values change between samples (not stuck)")
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--port", help="serial port, e.g. COM5 or /dev/ttyUSB0")
    parser.add_argument("--env", default="esp32dev", help="PlatformIO environment")
    parser.add_argument("--seconds", type=float, default=10.0, help="capture duration")
    parser.add_argument("--no-upload", action="store_true",
                        help="skip build and upload, only test")
    args = parser.parse_args()

    try:
        import serial  # noqa: F401
    except ImportError:
        sys.exit("pyserial is missing. Install it with: pip install pyserial")

    port = args.port or find_port()
    print(f"Port: {port}")

    if not args.no_upload:
        pio = find_pio()
        if not pio:
            sys.exit("PlatformIO Core not found. Install it with: pip install platformio")
        command = [pio, "run", "-d", str(ROOT), "-e", args.env,
                   "-t", "upload", "--upload-port", port]
        print("Running:", " ".join(command))
        if subprocess.call(command) != 0:
            sys.exit("Build or upload failed. If the upload could not connect, hold "
                     "the BOOT button while it starts.")

    print(f"Capturing serial output for {args.seconds:g} s ...")
    lines = capture(port, args.seconds)

    log = ROOT / ".pio" / "flash-test.log"
    log.parent.mkdir(exist_ok=True)
    checks = analyze(lines)
    summary = [f"{'PASS' if ok else 'FAIL'}: {text}" for ok, text in checks]
    log.write_text("\n".join(lines + ["", "--- checks ---"] + summary) + "\n",
                   encoding="utf-8")

    print("--- first serial lines ---")
    print("\n".join(lines[:25]))
    print("--- checks ---")
    print("\n".join(summary))
    print(f"Full log: {log}")
    failed = sum(not ok for ok, _ in checks)
    print("RESULT:", "PASS" if not failed else f"FAIL ({failed} check(s) failed)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
