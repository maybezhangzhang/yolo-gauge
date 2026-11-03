#!/usr/bin/env python3
"""Render a framebuffer dump taken over JTAG into a PNG.

The firmware draws into DDR at 0x20100000 as packed 24-bit BGR, 1024x600.
`tools/flash.ps1` dumps it with `mrd -bin -file`, which produces 4 bytes per
32-bit word unless the dump size is trimmed - so this script simply takes the
first w*h*3 bytes and tolerates trailing padding.

Usage:
    python decode_framebuffer.py fb.bin out.png [--w 1024] [--h 600]
"""
import argparse
import sys

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    sys.exit("Pillow is required: pip install pillow")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bin")
    ap.add_argument("out")
    ap.add_argument("--w", type=int, default=1024)
    ap.add_argument("--h", type=int, default=600)
    args = ap.parse_args()

    with open(args.bin, "rb") as fh:
        data = fh.read()

    need = args.w * args.h * 3
    if len(data) < need:
        sys.exit("dump is %d bytes, need %d (wrong base address or length?)"
                 % (len(data), need))

    # the framebuffer is BGR in memory, PIL wants RGB
    rgb = bytearray(data[:need])
    rgb[0::3], rgb[2::3] = data[2:need:3], data[0:need:3]

    Image.frombytes("RGB", (args.w, args.h), bytes(rgb)).save(args.out)
    print("wrote %s (%dx%d) from %s" % (args.out, args.w, args.h, args.bin))


if __name__ == "__main__":
    main()
