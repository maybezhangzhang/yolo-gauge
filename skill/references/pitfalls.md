# Pitfalls — long form, with the evidence that identified each one

Everything here happened in this project; each entry lists the symptom that made it
visible and the measurement that settled it.

---

## 1. `ap_done` rises before `ap_idle` → `ap_start` silently dropped

**Symptom.** The application stalls for ~19 s once every few frames. The HUD freezes
with the last frame on screen; no exception, no box.

**Evidence.** While the application was suspended, a bare-metal probe read the live
registers:

```
AP_CTRL   = 0x04      idle=1 done=0 start=0
ch_in=256 ch_out=27 fsize=40 kernel=1     <- last layer of head P4
in/out pointers valid
```

The configuration was there, but the engine had never been started. `ap_done` and
`ap_idle` are not simultaneous in this IP; programming the next layer while `done`
is high and `idle` low makes the AXI-Lite write sequence end with a `start` that is
ignored.

**Fix.** Program, start, then poll for `0x02` (done) followed by `0x04` (idle). If
`0x04` is seen without a done, re-program and re-start (bounded retries), and only
then declare a timeout. The 19 s came from `10,000,000` poll iterations, so also make
the timeout loud (print the layer parameters) instead of silent.

---

## 2. 3×3 output rotated by one tile → "runs, but detects nothing"

**Symptom.** All 28 layers complete, no exception, `ap_done` for every layer, and the
detector returns zero boxes at any threshold.

**Evidence.** Dump one layer (`conv0`, 3→32, 320×320 output) and correlate against a
numpy reference built from the same weights:

| shift applied to each row | correlation |
| --- | --- |
| 0 px | 0.4034 |
| **10 px** | **0.9848** |
| 13 px | 0.6836 |
| 20 px | 0.4097 |

The HLS 3×3 path walks a tile and writes it back at a one-tile offset along W; the
tile is `Tc = 10` in this build. The older 416 build had `Tc = 13`, and the reference
implementation carries a hard-coded 13, which is why copying that code produced a
rotation of the wrong size.

**Fix.** After each 3×3 layer, rotate every row left by `Tc` elements and flush the
cache. Cost: 82 ms per frame on the PS, versus a re-synthesis.

**Lesson.** Any hard-coded constant copied from a sibling project must be re-derived
from that project's own header (`#define Tc`). Correlating against a golden model is
the only reliable way to find this class of bug: the network does not crash, it just
produces nonsense.

---

## 3. FatFs without LFN → hang inside `read_params`

**Symptom.** Firmware stops right after the SD mount; the framebuffer is black; the PC
is inside a `while(1)` in `read_params`.

**Evidence.** `f_open("gauge_weights\\conv_forP5\\w1.bin")` fails; `test1.bmp` opens
fine. `ffconf.h`:

```c
#ifdef FILE_SYSTEM_USE_LFN
#define FF_USE_LFN   FILE_SYSTEM_USE_LFN
#else
#define FF_USE_LFN   0          <-- default
#endif
```

**Fix.** Compile FatFs with `-DFILE_SYSTEM_USE_LFN=1` **and** a code page that matches
the strings (`FF_CODE_PAGE 437` here; the stock 932 failed to initialise the DBCS
tables on this card). Also replace the `while(1)` with an error screen — a silent hang
looks exactly like a dead board.

---

## 4. IRQs unmasked at `con` → crash before `main()`

**Symptom.** After downloading, the CPU is in an abort/null vector; nothing on screen,
no trace of the application's first line.

**Evidence.** `CPSR = 0x200f00df` (IRQ not masked), `PC = Xil_ExceptionNullHandler`,
`SP` inside the exception stack. Pending PL interrupts (VDMA/VTC from the previous run
or from the bitstream) fire immediately after `con`, before the application installs
its handlers.

**Fix.** `rwr cpsr 0x1d3` (SVC, I and F masked) between `dow` and `con`. Registering
abort handlers in `main()` does not help — the fault happens before them.

---

## 5. Download fails at one address because of the MMU table image

**Symptom.** `Memory write error at 0x1AD000. MMU page translation fault`, and later
`Blocked address, VA 0x001AD000, PA 0x5A5AD000`.

**Evidence.** `mwr` to the same address from xsct works. A 4 KB `dow -data` succeeds at
`0x1AC000` and fails from `0x1AD000` on. The error's PA is `VA | 0x5A5A0000`: during
earlier debugging that region had been filled with `0x5A5A5A5A` (poison), and the
loader translates addresses through the `.mmu_tbl` image that lives there.

**Fix.** `rst -processor` before `dow` (MMU off → VA == PA), or clear the region.
Never poison memory that the linker script places a translation table in.

---

## 6. `csynth`'s LUT estimate is not the design

**Symptom.** `csynth.rpt` says `LUT 77,211 = 145 %` on a 53,200-LUT device, which
suggests the IP cannot fit.

**Evidence.** Out-of-context synthesis of the same `conv.v` inside the Vivado project:
`35,122 LUT = 66.0 %`. Placed and routed: 40,984 LUT = 77.04 % for the whole design,
WNS +0.398 ns.

**Lesson.** Use `csynth` for latency and DSP counts; use the implementation reports for
whether it fits. Believing the 145 % line cost a day and a detour into "we must shrink
the IP".

---

## 7. HLS 2020.2 IP packaging fails from 2026 on

**Symptom.**

```
bad lexical cast: source type value could not be interpreted as target
    while executing "rdi::set_property core_revision 2611030830 {component component_1}"
ERROR: [IMPL 213-28] Failed to generate IP.
```

**Cause.** The packer builds the core revision from `YYMMDDHHMM`; on 2026-11-03 that is
`2611030830`, above `INT32_MAX`. There is no `-revision` option on `export_design` in
this version.

**Workaround.** Keep the previously packaged IP (it is the netlist the bitstream was
built from — verify by MD5), or set the system clock to 2021 for the export. The
`build_hls.tcl` here makes packaging opt-in (`set HLS_EXPORT=1`) and leaves C synthesis
running by default.

---

## 8. C/RTL co-simulation cannot handle this interface

**Symptom.** Every `cosim_design` run aborts with `SIGSEGV` right after
`Starting C TB testing`, although `csim_design` passes.

**Attempts.** Original project; without `-argv`; ports split into separate bundles with
reduced `depth` values (450,000/65,536 → 20,000). The tool also warns
`AXI_master port 'w1' has a depth of '0'` for all 13 ports, which is itself a sign that
it does not understand `offset=slave` with 16 `m_axi` ports.

**Conclusion.** Making it run requires changing the interface, i.e. simulating different
hardware. Replaced by: bare-metal register tests (does the IP complete?) plus layer
dumps correlated against a numpy golden model (does it compute the right numbers?).
Those two cover what co-simulation would have covered.

---

## 9. Measuring on the board: the four traps

1. `mrd -bin -file <path>` **does not overwrite** an existing file — delete it first,
   or you read yesterday's data and believe the program is broken.
2. `mrd`'s count argument is **words**, not bytes (1024×600×3 bytes = 460,800 words).
3. The framebuffer is at `DDR_BASE + 0x20000000`; `DDR_BASE` is `0x00100000`, so the
   real address is `0x20100000`. Dumping at `0x20000000` gives an image shifted by
   341 rows plus noise on top — which looks like "the banner is never drawn".
4. `stop` on an already-stopped core raises `Already stopped` **and aborts the tcl**, so
   a sweep silently produces empty files. Wrap it: `catch { stop }`. Similarly
   `targets`/`jtag targets` print to the console handle that PowerShell does not
   capture — write results to a file from inside the tcl.

---

## 10. Removing the SD card while the application runs wedges the JTAG DAP

**Symptom.** After swapping cards with the application running, `targets` shows only
`1 DAP (AP transaction error, DAP status 0x30000021)` and the ARM core disappears; no
download works.

**Cause.** The SD controller is being polled by the application through AHB; pulling
the card stalls those transactions until the AP times out and the DAP locks up.

**Recovery.** Power-cycle. `rst -system`, hw_server restarts and `FT_CyclePort` do not
clear it. **Prevention**: power down, or stop the application, before touching the card.

## 11. The screen shows a photo that is not what the lens sees

**Symptom.** HDMI displays a perfectly sharp gauge picture while the camera is
pointed at the ceiling. Detections may even look plausible, so it reads like the
framebuffer is showing a stale DDR image.

**Cause.** The OpenMV script on the camera has `SOURCE = 'file'`, so it streams a
BMP stored on its own disk (`FILE_PATH`) instead of the sensor. The board is not at
fault - it is faithfully displaying the frames it receives.

**Evidence that separates it from a stale framebuffer.**

* HUD frame size: `CAM 320x248 GRAY8` (a replayed QVGA BMP) versus `CAM 320x240
  RGB565` (live sensor).
* Camera console: endless `file frame NNNNN: send 834 ms` lines instead of
  `lock gain=.. exposure=..` followed by per-frame progress.
* PC sampling: the CPU is inside `cam_link_get_frame()`, i.e. it is actively
  receiving frames, not stalled in `XConv_IsDone`.

**Fix.** `python tools/openmv_source.py --set sensor --restart --port COM14` (the
file is GBK-encoded, so patch it as bytes, and verify the new value before writing).
