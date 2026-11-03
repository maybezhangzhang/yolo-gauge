#!/usr/bin/env python3
"""Throughput measurement for a register-driven accelerator on PYNQ.

Measures steady-state latency with buffers allocated once, reports ms/iter and
items/s, and re-checks that the output of the last iteration equals the first one
(a change means the IP is stateful or the buffers are being recycled).

    python pynq_bench.py --bit d.bit --hwh d.hwh --map conv.json --input in.npy --iters 50
"""
import argparse
import gc
import json
import sys
import time

import numpy as np


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bit", required=True)
    ap.add_argument("--map", required=True)
    ap.add_argument("--map-ip", default=None)
    ap.add_argument("--input", required=True)
    ap.add_argument("--weights")
    ap.add_argument("--bias")
    ap.add_argument("--out-shape", required=True,
                    help="comma separated, e.g. 32,320,320")
    ap.add_argument("--iters", type=int, default=50)
    ap.add_argument("--warmup", type=int, default=3)
    ap.add_argument("--scalar", action="append", default=[])
    args = ap.parse_args()

    from pynq import Overlay, allocate  # board only

    with open(args.map, "r", encoding="utf-8") as fh:
        m = json.load(fh)
    ip_name = args.map_ip or m.get("ip")
    scalars = dict(kv.split("=", 1) for kv in args.scalar)

    ol = Overlay(args.bit)
    ip = ol.__getattr__(ip_name)
    ap_off = m["ctrl"]["ap_ctrl"]

    inp = np.load(args.input)
    in_buf = allocate(shape=inp.shape, dtype=inp.dtype)
    in_buf[:] = inp
    in_buf.flush()

    w_buf = b_buf = None
    if args.weights:
        w = np.load(args.weights)
        w_buf = allocate(shape=w.shape, dtype=w.dtype)
        w_buf[:] = w
        w_buf.flush()
    if args.bias:
        b = np.load(args.bias)
        b_buf = allocate(shape=b.shape, dtype=b.dtype)
        b_buf[:] = b
        b_buf.flush()

    shape = tuple(int(x) for x in args.out_shape.split(","))
    out_buf = allocate(shape=shape, dtype=np.int32 if args.weights else np.float32)

    if "in1" in m["data"]:
        ip.write(m["data"]["in1"], in_buf.device_address)    if w_buf is not None and "w1" in m["data"]:
        ip.write(m["data"]["w1"], w_buf.device_address)
    if b_buf is not None and "b" in m["data"]:
        ip.write(m["data"]["b"], b_buf.device_address)
    if "out1" in m["data"]:
        ip.write(m["data"]["out1"], out_buf.device_address)

    for name, off in m["ctrl"].items():
        if name != "ap_ctrl" and name in scalars:
            ip.write(off, int(scalars[name]))

    def run_once():
        ip.write(ap_off, 1)
        for _ in range(10_000_000):
            st = ip.read(ap_off)
            if st & 0x2:
                break
        for _ in range(10_000_000):
            if ip.read(ap_off) & 0x4:
                break
        out_buf.invalidate()

    gc.disable()
    for _ in range(args.warmup):
        run_once()
    out_buf.invalidate()
    first = np.array(out_buf, copy=True)

    t0 = time.perf_counter()
    for _ in range(args.iters):
        run_once()
    dt = time.perf_counter() - t0
    gc.enable()

    out_buf.invalidate()
    last = np.array(out_buf, copy=True)

    ms = dt * 1000.0 / args.iters
    print("iterations     : %d" % args.iters)
    print("total          : %.3f s" % dt)
    print("per iteration  : %.3f ms  (%.2f it/s)" % (ms, 1000.0 / ms if ms else float("inf")))
    same = np.array_equal(first, last)
    print("first == last  : %s" % same)
    print("RESULT: " + ("OK" if same else "STATEFUL / BUFFER REUSE PROBLEM"))
    return 0 if same else 1


if __name__ == "__main__":
    sys.exit(main())
