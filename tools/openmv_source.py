#!/usr/bin/env python3
"""Switch the OpenMV script between live sensor mode and replay-from-file mode.

The board's camera link expects the OpenMV to stream 320x240 frames over UART3 at
921600 baud and to wait for a 'K' acknowledge after each frame.  F:\main.py owns
that loop; its SOURCE constant selects where the frames come from:

    SOURCE = 'sensor'   live image from the lens
    SOURCE = 'file'     replays FILE_PATH (a .bmp stored on the OpenMV disk)

A board that shows a fixed photograph which is clearly not what the lens sees is
almost always running the 'file' variant.  This script prints the current setting
and can flip it, optionally restarting the script over the USB serial console.

Usage
-----
    python openmv_source.py                         # show current mode
    python openmv_source.py --set sensor            # live camera
    python openmv_source.py --set file              # replay stored bmp
    python openmv_source.py --set sensor --restart --port COM14

Notes
-----
* The file on the OpenMV disk is GBK-encoded, so it is edited as bytes and the
  edit is verified before anything is written back.
* --restart sends Ctrl-C and then re-executes main.py through the MicroPython
  REPL, which is how the script gets picked up without pressing the RST button.
* The board only uses the camera when the SD card holds no testN.bmp files; put
  a test1.bmp on the card to make it boot in static-image mode instead.
"""

import argparse
import re
import sys
from pathlib import Path

DEFAULT_MOUNT = Path("F:/main.py")
MODES = ("sensor", "file")


def read_source(path: Path) -> str:
    text = path.read_bytes().decode("gbk", "replace")
    m = re.search(r"^\s*SOURCE\s*=\s*['\"](file|sensor)['\"]", text, re.M)
    return m.group(1) if m else "<not found>"


def patch_source(path: Path, mode: str) -> bool:
    raw = path.read_bytes()
    new, n = re.subn(
        rb"(^\s*SOURCE\s*=\s*['\"])(file|sensor)(['\"])",
        lambda m: m.group(1) + mode.encode() + m.group(3),
        raw,
        flags=re.M,
    )
    if n == 0:
        print("ERROR: no SOURCE = '...' line found in %s" % path)
        return False
    if n > 1:
        print("WARNING: %d SOURCE lines matched, patching all of them" % n)
    if new == raw:
        print("already set to '%s', file untouched" % mode)
        return True
    backup = path.with_suffix(".py.bak")
    if not backup.exists():
        backup.write_bytes(raw)
        print("backup written to %s" % backup)
    path.write_bytes(new)
    got = read_source(path)
    if got != mode:
        print("ERROR: verification failed, file now says '%s'" % got)
        return False
    print("patched %s -> SOURCE = '%s'" % (path, got))
    return True


def restart(port: str, baud: int = 115200, wait_s: float = 12.0) -> None:
    try:
        import serial  # pyserial
    except ImportError:
        print("ERROR: pyserial is required for --restart (pip install pyserial)")
        return

    try:
        ser = serial.Serial(port, baud, timeout=0.3, write_timeout=2)
    except Exception as exc:  # noqa: BLE001 - report the OS error verbatim
        print("ERROR: cannot open %s (%s); close OpenMV IDE / PuTTY first" % (port, exc))
        return

    try:
        ser.write(b"\x03")          # Ctrl-C: break out of the frame loop
        ser.read(4096)
        ser.write(b"exec(open('main.py').read())\r")
        print("--- output from %s ---" % port)
        deadline = wait_s
        import time

        t0 = time.time()
        while time.time() - t0 < deadline:
            chunk = ser.read(4096)
            if chunk:
                sys.stdout.write(chunk.decode("utf-8", "replace"))
                sys.stdout.flush()
    finally:
        ser.close()
    print("\n--- restart command sent ---")


def main() -> int:
    ap = argparse.ArgumentParser(description="Inspect or switch the OpenMV frame source")
    ap.add_argument("--path", type=Path, default=DEFAULT_MOUNT,
                    help="path to the OpenMV main.py (default: F:/main.py)")
    ap.add_argument("--set", choices=MODES, help="switch SOURCE to this mode")
    ap.add_argument("--restart", action="store_true",
                    help="restart main.py on the OpenMV after patching")
    ap.add_argument("--port", default="COM14", help="OpenMV USB serial port (default COM14)")
    ap.add_argument("--baud", type=int, default=115200, help="port baud for --restart")
    args = ap.parse_args()

    if not args.path.exists():
        print("ERROR: %s not found - is the OpenMV disk mounted?" % args.path)
        return 1

    before = read_source(args.path)
    print("current SOURCE = '%s'  (%s)" % (before, args.path))

    ok = True
    if args.set:
        ok = patch_source(args.path, args.set)

    if ok and args.restart:
        restart(args.port, args.baud)

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
