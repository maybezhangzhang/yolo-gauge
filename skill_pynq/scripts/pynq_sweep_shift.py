#!/usr/bin/env python3
"""Find the integer circular shift along the fastest axis that best matches a reference.

The single most common "the accelerator computes garbage" report is really a
write-back offset of one tile along the fastest axis. This sweeps shifts and reports
the correlation curve, so the answer is a number instead of a suspicion.

    python pynq_sweep_shift.py --got board_out.npy --ref ref.npy --row 320 --max-shift 32
"""
import argparse
import sys

import numpy as np


def corr(a, b):
    a = np.asarray(a, dtype=np.float64).ravel()
    b = np.asarray(b, dtype=np.float64).ravel()
    n = min(a.size, b.size)
    a, b = a[:n], b[:n]
    if n < 2 or np.std(a) == 0 or np.std(b) == 0:
        return float("nan")
    return float(np.corrcoef(a, b)[0, 1])


def shift_rows(arr, s, row):
    """Roll each row of length `row` left by s elements."""
    flat = np.asarray(arr).ravel()
    rows = flat.size // row
    m = flat[: rows * row].reshape(rows, row)
    return np.roll(m, -s, axis=1).ravel()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--got", required=True)
    ap.add_argument("--ref", required=True)
    ap.add_argument("--row", type=int, default=0,
                    help="row length in elements; 0 = infer from the reference shape")
    ap.add_argument("--max-shift", type=int, default=32)
    args = ap.parse_args()

    got = np.load(args.got)
    ref = np.load(args.ref)
    row = args.row or (ref.shape[-1] if ref.ndim >= 2 else ref.size)

    print("row length = %d, elements = %d" % (row, ref.size))
    best = (float("nan"), -1)
    for s in range(0, args.max_shift + 1):
        c = corr(shift_rows(got, s, row), ref)
        flag = ""
        if c == c and (best[0] != best[0] or c > best[0]):
            best = (c, s)
            flag = "  <-- best so far"
        print("shift %2d  corr %.6f%s" % (s, c, flag))

    print("\nbest shift = %d with corr %.6f" % (best[1], best[0]))
    if best[0] == best[0] and best[0] > 0.95:
        print("RESULT: contract offset found - compensate by %d elements per row" % best[1])
        return 0
    print("RESULT: no shift explains the mismatch (0.95 gate) - check layout/order instead")
    return 1


if __name__ == "__main__":
    sys.exit(main())
