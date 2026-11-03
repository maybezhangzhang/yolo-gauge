# PYNQ notes — what actually breaks, in the order it breaks

Written from a bare-metal Zynq project that was later re-framed for PYNQ. The same
five things go wrong every time.

## 1. Buffers

`numpy.zeros()` is **not** DMA memory. On PYNQ always `pynq.allocate(shape, dtype)`;
the buffer is contiguous, physically mapped, and its cache is handled by the runtime.

Three specific traps:

* **Keep every buffer referenced until the transfer is done.** If the only reference
  dies while the DMA is in flight, the allocator may hand the same physical pages to
  the next allocation. Symptom: works once, then garbage.
* **`flush()` before the accelerator reads, `invalidate()` before Python reads.**
  Forgetting either produces "sometimes correct" results that depend on what ran before.
* **Alignment.** Most AXI masters need 16–64 byte alignment; the allocator gives it,
  but sub-views (`arr[3:]`) do not. Never pass a slice.

## 2. The done/idle handshake

`ap_ctrl` bits: `start=1`, `done=2`, `idle=4`. Many HLS IPs raise `done` one cycle
**before** `idle`. Writing the next `start` in that window is accepted by the register
file and then silently dropped by the FSM — the PS waits forever, or for the length of
its timeout, which is exactly how "it hangs for 19 seconds every few frames" happens.

Rule: wait for `done`, then wait for `idle`, then touch the IP again.

## 3. Layout contracts

Before blaming quantisation, test these four hypotheses against a numpy reference:

1. row stride (is the output width `ow` or `ow + pad`?)
2. channel stride (channel-major vs pixel-major)
3. element order (NCHW vs NHWC, and anchor-first vs pixel-first in detection heads)
4. a whole-output circular shift along the fastest axis

Hypothesis 4 is the sneaky one: tile-based HLS code often writes back one tile
displaced, and the network still "runs" — it just produces nonsense. `pynq_sweep_shift.py`
answers it in one command; anything below 0.95 correlation after the best shift means
it is one of 1–3 instead.

## 4. Timing

* Python register writes cost ~10–100 µs each on PYNQ. A 20-register setup therefore
  costs 0.2–2 ms — as much as a small layer. Prefer one `write` of a packed 64-bit
  pointer over two 32-bit writes, and never poll a slow bus in a tight loop.
* Measure with `time.perf_counter()` around N iterations, `gc.disable()` in the loop,
  and a warm-up. Report ms/iter, not "it feels fast".
* Check that the first and last iteration produce identical output; a difference means
  the IP is stateful or the buffers are being recycled (`pynq_bench.py` does this).

## 5. Overlay loading

* `.bit` and `.hwh` must share the basename and come from the same build. A mismatched
  `.hwh` loads happily and then exposes the wrong register map.
* If the overlay was built for a different board, expect `RuntimeError: Cannot find
  matching device` — that is the platform, not your design.
* `Overlay()` is slow (0.5–2 s). Load once at start-up; do not reload per frame.

## 6. When the numbers still do not match

Work downward, never sideways:

1. compare **one layer**, with the exact same weights file the board uses;
2. dump the input the board actually consumed (not what you think you sent);
3. dump the output before any post-processing;
4. only then compare end-to-end results.

In the original project this sequence found a 10-pixel row shift in a 3×3 convolution
that no amount of model-level debugging would have revealed: the network ran, the
losses were fine on the PC, and the board simply returned shifted feature maps.
