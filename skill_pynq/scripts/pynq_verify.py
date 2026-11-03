#!/usr/bin/env python3
"""Generic one-layer verification for an HLS/BD accelerator on PYNQ.

Two modes:

  board   : load an overlay, push real buffers through the IP, compare with a reference
  offline : compare two .npy dumps (e.g. taken over JTAG) with the same statistics

The reference is whatever you computed in numpy; this script only answers
"does the hardware produce the same numbers, and if not, by how much".

    python pynq_verify.py --bit d.bit --hwh d.hwh --map conv.json \
        --input in.npy --weights w.npy --bias b.npy --ref ref.npy --tol 1e-2

    python pynq_verify.py --offline --got board_out.npy --ref ref.npy
"""
import argparse
import json
import sys

import numpy as np


def load_map(path):
    with open(path, "r", encoding="utf-8") as fh:
        return json.load(fh)


def stats(got, ref):
    got = np.asarray(got, dtype=np.float64).ravel()
    ref = np.asarray(ref, dtype=np.float64).ravel()
    n = min(got.size, ref.size)
    if got.size != ref.size:
        print("WARN size mismatch: got %d ref %d (comparing first %d)"
              % (got.size, ref.size, n))
    g, r = got[:n], ref[:n]
    if n < 2 or np.std(g) == 0 or np.std(r) == 0:
        corr = float("nan")
    else:
        corr = float(np.corrcoef(g, r)[0, 1])
    err = float(np.max(np.abs(g - r))) if n else float("nan")
    idx = int(np.argmax(np.abs(g - r))) if n else -1
    return corr, err, idx


def verify_offline(args):
    got = np.load(args.got)
    ref = np.load(args.ref)
    corr, err, idx = stats(got, ref)
    print("corr        = %.6f" % corr)
    print("max_abs_err = %.6g  at index %d" % (err, idx))
    ok = (corr == corr and corr >= 0.95)
    print("RESULT: " + ("OK" if ok else "MISMATCH"))
    return 0 if ok else 1


def verify_board(args):
    from pynq import Overlay, allocate  # only needed on the board

    ol = Overlay(args.bit)
    ip = ol.__getattr__(args.map_ip)
    m = load_map(args.map)

    inp = np.load(args.input)
    wts = np.load(args.weights) if args.weights else None
    bis = np.load(args.bias) if args.bias else None
    ref = np.load(args.ref)

    # one buffer per distinct pointer role, kept alive until the end
    bufs = {}
    def buf(name, arr):
        b = allocate(shape=arr.shape, dtype=arr.dtype)
        b[:] = arr
        b.flush()
        bufs[name] = b
        return b

    in_buf = buf("in", inp)
    w_buf = buf("w", wts) if wts is not None else None
    b_buf = buf("b", bis) if bis is not None else None
    out_arr = np.zeros(ref.shape, dtype=ref.dtype)
    out_buf = allocate(shape=out_arr.shape, dtype=out_arr.dtype)
    out_buf[:] = 0

    ap = m["ctrl"]["ap_ctrl"]
    if ip.read(ap) != 4:
        print("IP is not idle (ap_ctrl=%d); reset the design first" % ip.read(ap))
        return 2

    for name, off in m["ctrl"].items():
        if name == "ap_ctrl":
            continue
        val = args.scalar.get(name)
        if val is not None:
            ip.write(off, int(val))

    def ptr(name, obj):
        if name in m["data"]:
            ip.write(m["data"][name], obj.device_address)

    ptr("in1", in_buf)
    if w_buf is not None:
        ptr("w1", w_buf)
    if b_buf is not None:
        ptr("b", b_buf)
    ptr("out1", out_buf)

    ip.write(ap, 1)
    for _ in range(args.timeout):
        state = ip.read(ap)
        if state & 0x2:
            break
    for _ in range(args.timeout):
        if ip.read(ap) & 0x4:
            break
    out_buf.invalidate()
    out_buf[:]  # copy back from the device
    got = np.asarray(out_buf)

    corr, err, idx = stats(got, ref)
    print("corr        = %.6f" % corr)
    print("max_abs_err = %.6g  at index %d" % (err, idx))
    ok = (corr == corr and corr >= 0.95 and err <= args.tol)
    print("RESULT: " + ("OK" if ok else "MISMATCH"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--offline", action="store_true")
    ap.add_argument("--got")
    ap.add_argument("--ref")
    ap.add_argument("--bit")
    ap.add_argument("--hwh")
    ap.add_argument("--map")
    ap.add_argument("--map-ip", default=None, help="IP instance name in the overlay")
    ap.add_argument("--input")
    ap.add_argument("--weights")
    ap.add_argument("--bias")
    ap.add_argument("--tol", type=float, default=1e-2)
    ap.add_argument("--timeout", type=int, default=10000000)
    ap.add_argument("--scalar", action="append", default=[],
                    help="name=value for scalars in the register map")
    args = ap.parse_args()

    args.scalar = dict(kv.split("=", 1) for kv in args.scalar)
    if args.map and not args.map_ip:
        args.map_ip = load_map(args.map).get("ip", args.map_ip)
    return verify_offline(args) if args.offline else verify_board(args)


if __name__ == "__main__":
    sys.exit(main())
