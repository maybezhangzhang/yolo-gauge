#!/usr/bin/env python3
"""Derive the dial calibration (gauge_scale.txt) from a YOLO label file.

Classes must be in the order used by the firmware: 0=base, 1=maximum,
2=minimum, 3=tip.

Angles use image coordinates: 0 deg = +x (right), 90 deg = +y (down), i.e. a
normal dial grows clockwise on screen.

Usage:
    python scale_from_labels.py <label.txt> [--min 0] [--max 16] [--unit kgf/cm2]

It prints both the JSON-ish summary and a ready-to-paste gauge_scale.txt.
"""
import argparse
import math
import sys

NAMES = {0: "base", 1: "maximum", 2: "minimum", 3: "tip"}


def load(path):
    boxes = {}
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            parts = line.split()
            if len(parts) < 5:
                continue
            cls = int(float(parts[0]))
            cx, cy, w, h = (float(v) for v in parts[1:5])
            boxes[cls] = (cx, cy, w, h)
    return boxes


def angle(cx, cy, bx, by):
    return math.degrees(math.atan2(cy - by, cx - bx)) % 360.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("label")
    ap.add_argument("--min", type=float, default=0.0, dest="vmin")
    ap.add_argument("--max", type=float, default=16.0, dest="vmax")
    ap.add_argument("--unit", default="kgf/cm2")
    args = ap.parse_args()

    b = load(args.label)
    missing = [NAMES[k] for k in (0, 2, 1) if k not in b]
    if missing:
        sys.exit("missing classes: " + ", ".join(missing))

    bx, by = b[0][0], b[0][1]
    amin = angle(b[2][0], b[2][1], bx, by)      # minimum mark
    amax = angle(b[1][0], b[1][1], bx, by)      # maximum mark
    span = (amax - amin) % 360.0

    print("base    (%.4f, %.4f)" % (bx, by))
    print("minimum %.2f deg" % amin)
    print("maximum %.2f deg" % amax)
    print("span    %.2f deg" % span)

    if 3 in b:
        tip = angle(b[3][0], b[3][1], bx, by)
        pos = (tip - amin) % 360.0
        frac = pos / span if span else 0.0
        value = args.vmin + (args.vmax - args.vmin) * frac
        print("tip     %.2f deg  frac=%.4f  value=%.3f %s"
              % (tip, frac, value, args.unit))

    print("\n--- gauge_scale.txt ---")
    print("min   = %g" % args.vmin)
    print("max   = %g" % args.vmax)
    print("unit  = %s" % args.unit)
    print("clockwise = 1")
    print("angle_min = %.1f" % amin)
    print("angle_max = %.1f" % amax)


if __name__ == "__main__":
    main()
