#!/usr/bin/env python3
"""Sanity-check an SD card payload before putting it into the board.

Checks the things that actually cost time when they are wrong:
  * BOOT.BIN / bitstream / ELF exist and have a plausible size
  * gauge_weights contains exactly 42 files with the expected names
  * testN.bmp are consecutive from 1 and are 24-bit BI_RGB
  * gauge_scale.txt (if present) has known keys

Usage:
    python check_sd_payload.py E:\\final_entry\\sd
Exit code 0 = ok, 1 = problems found.
"""
import os
import struct
import sys

EXPECTED_BLOBS = {"conv_forP5", "ResBlock1", "ResBlock2", "ResBlock3",
                  "upsample", "yolo_headP4", "yolo_headP5", "BasicConv1",
                  "BasicConv2", "BasicConv3"}
KNOWN_KEYS = {"min", "max", "unit", "clockwise", "angle_min", "angle_max"}


def bmp_header(path):
    with open(path, "rb") as fh:
        head = fh.read(54)
    if len(head) < 54 or head[:2] != b"BM":
        return None
    size, _, offset, dib = struct.unpack("<III", head[2:14]) + (0,)  # placeholder
    data_off = struct.unpack("<I", head[10:14])[0]
    w, h = struct.unpack("<ii", head[18:26])
    bpp = struct.unpack("<H", head[28:30])[0]
    comp = struct.unpack("<I", head[30:34])[0]
    return dict(size=size, data_off=data_off, w=w, h=abs(h), bpp=bpp, comp=comp)


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    problems = []

    for name, lo in (("BOOT.BIN", 4_000_000), ("fsbl.elf", 500_000),
                     ("gauge_hdmi_true640.elf", 1_000_000),
                     ("system_wrapper_640.bit", 4_000_000)):
        p = os.path.join(root, name)
        if not os.path.isfile(p):
            problems.append("missing " + name)
        elif os.path.getsize(p) < lo:
            problems.append("%s looks truncated (%d bytes)" % (name, os.path.getsize(p)))

    wdir = os.path.join(root, "gauge_weights")
    if not os.path.isdir(wdir):
        problems.append("missing gauge_weights/")
    else:
        bins = [f for f in os.listdir(wdir) if f.lower().endswith(".bin")]
        subs = [d for d in os.listdir(wdir) if os.path.isdir(os.path.join(wdir, d))]
        if bins:
            if len(bins) != 42:
                problems.append("gauge_weights has %d .bin files (expected 42)" % len(bins))
        elif len(subs) != 10 or set(subs) != EXPECTED_BLOBS:
            problems.append("gauge_weights sub-directories unexpected: %s" % sorted(subs))

    n = 0
    while True:
        p = os.path.join(root, "test%d.bmp" % (n + 1))
        if not os.path.isfile(p):
            break
        hdr = bmp_header(p)
        if hdr is None:
            problems.append("test%d.bmp is not a BMP" % (n + 1))
        elif hdr["bpp"] != 24 or hdr["comp"] != 0:
            problems.append("test%d.bmp is %d-bit comp=%d (needs 24-bit BI_RGB)"
                            % (n + 1, hdr["bpp"], hdr["comp"]))
        n += 1
    if n == 0:
        problems.append("no testN.bmp found (firmware would fall back to camera mode)")

    scale = os.path.join(root, "gauge_scale.txt")
    if os.path.isfile(scale):
        with open(scale, "r", encoding="utf-8", errors="replace") as fh:
            for line in fh:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                key = line.split("=", 1)[0].strip().lower()
                if key not in KNOWN_KEYS:
                    problems.append("gauge_scale.txt: unknown key %r" % key)

    print("images found      : %d" % n)
    print("weights directory : %s" % ("ok" if os.path.isdir(wdir) else "MISSING"))
    if problems:
        print("\nPROBLEMS:")
        for p in problems:
            print(" - " + p)
        return 1
    print("\nOK - payload looks consistent")
    return 0


if __name__ == "__main__":
    sys.exit(main())
