#!/usr/bin/env python3
"""SP01 G1 USB Serial/JTAG boot probe.

Captures the ESP32-S3 boot log over the native USB Serial/JTAG CDC port with the
port held open across reset, and reports the running application version.

Why this exists
---------------
The IDF USB Serial/JTAG console discards output when no host has the port open.
A terminal opened after boot therefore shows nothing, and a firmware whose
app_main() returns shows nothing ever again. Both look identical to a dead
board. This script opens the port first, then resets, so the boot log is
captured from the first byte.

The App version line it reports is the ground truth for which commit is
actually running. Always compare it against the GIT_SHA file in the flash
bundle before trusting any bring-up test result.

Usage
-----
    python tools/g1_usb_probe.py                 # COM3, 20 s
    python tools/g1_usb_probe.py --port COM6
    python tools/g1_usb_probe.py --seconds 40 --hex

If the automatic reset pulse does not take, press the board RST/EN button while
the script is still running. The port stays open across a physical reset.

Requires: pyserial. Machine wiring must remain disconnected during G1.
"""

from __future__ import annotations

import argparse
import re
import sys
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.exit("pyserial not installed. Run: python -m pip install --user pyserial")


BAUD = 115200  # ignored by native USB CDC; kept for UART-bridge compatibility

# Lines worth calling out in the summary.
MARKERS = (
    ("app_version", re.compile(r"App version:\s*(\S+)")),
    ("compile_time", re.compile(r"Compile time:\s*(.+?)\s*$")),
    ("project", re.compile(r"Project name:\s*(\S+)")),
    ("idf", re.compile(r"ESP-IDF:\s*(\S+)")),
    ("app_offset", re.compile(r"Loaded app from partition at offset (\S+)")),
    ("flash_io", re.compile(r"flash io:\s*(\S+)")),
)


def list_ports() -> dict[str, str]:
    return {p.device: p.description for p in serial.tools.list_ports.comports()}


def pulse_reset(port: serial.Serial, order: str) -> None:
    """Attempt a reset via the CDC control lines.

    The ESP32-S3 ROM decodes DTR/RTS transitions as reset and download-mode
    requests. The exact sequence varies by board revision, so two orders are
    offered. Neither is guaranteed to reset every board; the physical RST
    button is the reliable path.
    """
    port.dtr = False
    port.rts = False
    time.sleep(0.2)
    port.reset_input_buffer()

    if order == "rts":
        port.rts = True
        time.sleep(0.2)
        port.rts = False
    else:  # "dtr"
        port.dtr = True
        time.sleep(0.2)
        port.dtr = False
    time.sleep(0.05)


def main() -> int:
    ap = argparse.ArgumentParser(description="SP01 G1 USB Serial/JTAG boot probe")
    ap.add_argument("--port", default="COM3", help="serial port (default COM3)")
    ap.add_argument("--seconds", type=float, default=20.0, help="capture window")
    ap.add_argument("--hex", action="store_true", help="also print hex for each chunk")
    ap.add_argument("--reset", choices=("rts", "dtr", "none"), default="rts",
                    help="control-line reset attempt (default rts)")
    ap.add_argument("--log", default="", help="also append output to this file")
    args = ap.parse_args()

    logfh = open(args.log, "a", encoding="utf-8") if args.log else None

    def emit(line: str) -> None:
        print(line, flush=True)
        if logfh:
            logfh.write(line + "\n")
            logfh.flush()

    before = list_ports()
    emit("before: %r" % before)
    if args.port not in before:
        emit("WARNING: %s not enumerated before open" % args.port)

    port = None
    total = 0
    text = ""
    verdict = "NO DATA"

    try:
        port = serial.Serial(args.port, BAUD, timeout=0.05)
        if args.reset != "none":
            pulse_reset(port, args.reset)

        t0 = time.time()
        while time.time() - t0 < args.seconds:
            try:
                chunk = port.read(4096)
            except serial.SerialException as exc:
                verdict = "PORT DISAPPEARED (app re-enumerated USB)"
                emit("!! %s" % exc)
                break
            if chunk:
                total += len(chunk)
                text += chunk.decode("utf-8", errors="replace")
                stamp = time.time() - t0
                if args.hex:
                    emit("[%7.3f] %s" % (stamp, chunk.hex(" ")))
                emit("[%7.3f] %r" % (stamp, chunk))
        else:
            verdict = "DATA RECEIVED" if total else "SILENT, PORT ALIVE"

    except KeyboardInterrupt:
        verdict = "ABORTED BY USER"
    except Exception as exc:  # noqa: BLE001 - probe must never die dirty
        verdict = "ERROR: %r" % exc
    finally:
        try:
            if port is not None and port.is_open:
                port.close()
        except Exception:  # noqa: BLE001
            pass

    after = list_ports()
    emit("after: %r" % after)
    if before != after:
        emit("NOTE: port set changed across the run - the app re-enumerated USB")

    emit("")
    emit("bytes: %d  verdict: %s" % (total, verdict))

    found = {}
    for name, rx in MARKERS:
        m = rx.search(text)
        if m:
            found[name] = m.group(1)
    if found:
        emit("--- identity ---")
        for name, _ in MARKERS:
            if name in found:
                emit("%-13s %s" % (name + ":", found[name]))
        emit("")
        emit("Compare app_version against the GIT_SHA file in the flash bundle.")
        emit("If it does not match, the flash did not take - fix that before")
        emit("interpreting any other result.")

    if "Returned from app_main()" in text:
        emit("")
        emit("NOTE: app_main() returned. This firmware prints nothing further,")
        emit("by design. Later silence on this port is expected, not a fault.")

    if verdict == "SILENT, PORT ALIVE":
        emit("")
        emit("No bytes captured. Try: a physical RST/EN press while running,")
        emit("--reset dtr, or --reset none with a manual reset.")

    if logfh:
        logfh.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
