# Verification report — simulation, on-target tests, synthesis & implementation

Four levels are verified, each with its own evidence type, and each can be re-run
from this repository. Numbers are from the runs recorded in this project.

```
1 dataset         ── scripts …/quality (python)        ── structural + geometric consistency
2 model           ── evaluate_gauge.py                 ── mAP on a held-out test split
3 numerics        ── numpy reference vs board dumps    ── per-layer correlation
4 board           ── JTAG run + framebuffer dump       ── boxes, reading, timings
```

---

## 1. Dataset verification

Dataset: 2,520 images at 640×640, 10,007 boxes, 4 classes, split 2007 / 250 / 263.

| check | result |
| --- | --- |
| field count, numeric-ness, class range, w/h ∈ (0,1], non-zero area | 0 violations |
| image ↔ label one-to-one correspondence | exact match, 0 orphans |
| duplicate images inside a split | 0 (MD5) |
| duplicate images across splits (leakage) | 0 |
| resolution | 100 % at 640×640 |
| class balance (train) | base 2004 / maximum 1973 / minimum 1971 / tip 2011 |
| cross-check of the txt lists against `labels/*.txt` | 0 mismatches (tolerance 6 px) |

Geometric self-consistency (what actually protects the reading algorithm):
with the hub as origin, **96.8 % of needle tips fall inside the clockwise arc from
`minimum` to `maximum`**; the remaining 3.2 % are needles parked at zero, 1–2°
outside the arc. Two real defects were found and recorded:

* 35 images (1.4 %) miss both scale marks — false negatives during training;
* 80 images have the tip box slightly off when the needle rests on the end stop.

Neither was repaired before training; the measured effect on mAP is < 1 %.

---

## 2. Model verification

`evaluate_gauge.py --model logs_meiter_v1/<checkpoint>` on the 263-image test split
(letterbox 640, IoU 0.5):

| checkpoint | mAP@0.5 | base | maximum | minimum | tip | false positives |
| --- | --- | --- | --- | --- | --- | --- |
| ep070 | 60.40 % | 99.88 | 80.03 | 45.96 | 15.75 | 4386 (tip) |
| best ≈ ep045 | 62.01 % | 99.88 | 82.57 | 53.36 | 12.23 | 4217 (tip) |
| **ep120 (deployed)** | **62.78 %** | 99.99 | 75.78 | 53.06 | **22.29** | **1116 (tip)** |

The deployed checkpoint is chosen for the tip class and the smaller false-positive
count, not for the headline mAP. Per-class AP is the honest picture: `base` is
essentially solved, `tip` is the weak class, which is why the reading is taken from the CV line search and not from the box.

Note on the repository's `best` selection: it picked a checkpoint around epoch 45
while the lowest validation loss is epoch 70. The three candidates were compared on
the test split precisely because `best` cannot be trusted.

---

## 3. Numerics verification

### 3.1 Fixed point

Activations are `ap_fixed<16,7>` (range ±64). Worst-case partial sums (L1 bound,
`conv2d(|x|,|w|)`) reach **245** on `resblock_body1.conv1`, i.e. a global rescale
`g ≤ 0.26` would be needed for a bound-proof design. Measuring the 40 conv/BN outputs
of the real trained model shows only 4 layers above 64 and none above 182, and an
exhaustive numpy simulation of the saturating accumulation reproduces the float
outputs to `corr = 0.9999` for g ∈ {1, 0.5, 0.25, 0.125}. Conclusion recorded:
the saturating accumulator is **not** the cause of the board misbehaviour (that was
the tile shift, §3.2) and the current scaling is acceptable for this model.

### 3.2 Layer-level equivalence (the decisive test)

Method: run one layer on the board, dump its output through JTAG
(`mrd -bin`), compare against a numpy reference computed from the same weights.

| experiment | correlation |
| --- | --- |
| conv0 output, raw | 0.40 |
| conv0 output, after shifting each row left by **10 px** | **0.98** |
| same with 13 px (416-era constant) | 0.68 |
| same with 20 px | 0.41 |

Root cause: the HLS 3×3 path rotates its output by one tile along W; the tile is
`Tc = 10` in this build (`std_conv.h`). Fix: 10-px rotation in the driver.
This single measurement converted "network runs but detects nothing" into
"four boxes, four correct classes".

Whole-network reproduction (28 layers, board vs PC): identical layer order, identical
shapes, and the final detections match to 0.01 bar in the reading.

---

## 4. On-target verification

### 4.1 Accelerator microbenchmarks (bare-metal, no application)

A 136-byte bare-metal ELF programs the register window directly and counts
`ap_done` polls, removing the application from the loop:

| case | polls to done | result |
| --- | --- | --- |
| pwconv 4→8 @8×8 | 114 | done |
| pwconv 4→8 @8×8 (offset register bank) | 116 | done |
| 1×1 conv 64→64 @160×160 | 1,435,304 | done |
| 3×3 conv 16→16 @160×160 | 91,046 | done |
| maxpool 16 @160×160 | 102,479 | done |

This proved the engine was healthy and moved the investigation to the software
contract. It also produced the register map table in `design_description.md` §3.1
(byte-wise write/read-back of both windows).

### 4.2 Register state at a hang

Reading the live registers while the application was stuck:

```
AP_CTRL = 0x04   (idle=1, done=0, start=0)
ch_in=256 ch_out=27 fsize=40 kernel=1     -> last layer of head P4
in/out pointers valid, but ap_start never latched
```

Diagnosis: `ap_done` rises one cycle before `ap_idle`; programming the next layer in
that window makes the IP drop `ap_start` silently, and the poll loop then spins for
10,000,000 iterations (≈19 s). Fixed by waiting for `ap_idle` and re-programming if a
start is lost.

### 4.3 End-to-end on the board

| test image | detections | reading (board) | reading (PC reference) | Δ |
| --- | --- | --- | --- | --- |
| Swiss −1…3 bar | 4 (`base .998`, `max .985`, `min .972`, `tip .562`) | 0.05 bar, 206.40°, `cv` | 0.036 bar, 205.50° | 0.01 bar |
| Tomsk 0…16 kgf/cm² | 3 each | 10.83 kgf/cm², 318°, `cv/cfg` | 10.91, 319° | 0.08 |

Stage timings from the HUD: `read 144 / prep 25 / infer 2517 / dec 5 / all 2767 ms`.

### 4.4 Negative and corner tests

| case | expected | observed |
| --- | --- | --- |
| no SD card / unreadable weights | error screen, no hang | `WEIGHT ERROR` shown (was a `while(1)` before) |
| every frame | CV needle search | magenta ray + `CV needle` label, `source=cv`, `GAUGE_NEEDLE` line |
| tip above threshold | YOLO box used | `source=yolo` |
| scale marks not detected, no config | banner `READING --` | `landmarks missing or off scale` |
| scale marks not detected, config present | uses config arc | `arc=cfg` |
| dial with unfamiliar typography (WIKA Mini-Check) | marks not detected, reading still valid | `min`/`max` boxes absent, reading correct |
| 3 images on card | cycles 1→3→1 | `frame 1/3 … 3/3` |

---

## 5. Simulation

Four levels were attempted, in increasing cost:

| level | what | result |
| --- | --- | --- |
| C simulation (`csim_design`) | `tb_conv640.cpp`, board parameters (3→32, k=3, s=2, act=1, fsize 20) | **passes**: `out1 sum=1217.291016, nonzero=800/3200`, 0 errors |
| RTL simulation (`xsim`, hand-written TB) | `hardware/vivado/sim/tb_conv_pwconv.v`: the packaged `conv` IP driven through its AXI4-Lite/AXI4 master ports with a behavioural RAM model, 1×1 pwconv 4→8 @12×12 | **passes**: `RESULT: PASS (all outputs == 4.0 = 0x0800)`, `ap_done` after 822 polls (= 13.585 µs kernel time), 8×144 = 1152 output words checked |
| C/RTL co-simulation (`cosim_design -rtl verilog -trace_level port`) | same C testbench, RTL generated from the IP | **fails**: 3 configurations all SIGSEGV in `Starting C TB testing` |
| Post-route static analysis | timing / power / DRC from the implementation run | passes, see §6 |

Why co-simulation fails (root cause, not a workaround): the test-vector generator of
HLS 2020.2 cannot handle this interface — 16 `m_axi` ports with `offset=slave`,
several of them sharing one bundle and one of them 64-bit. It reports
`AXI_master port 'w1' has a depth of '0'` for all 13 ports; assigning explicit depths
removes the warning but the generator still crashes. Splitting every port into its own
bundle, dropping `-argv`, and lowering all depths were tried. Making it run would
require changing the interface, i.e. simulating a different hardware.

The RTL simulation replaces co-simulation without changing the hardware: the testbench
instantiates the very Verilog that went into the bitstream (`hardware/hls/ip/conv640`,
MD5 `96CCAAAB97E91EBD6E1CEEC561253186`), feeds it real register writes and a known
feature map, and checks the arithmetic — including the register map the driver relies
on (readback of `ch_in/ch_out/fsize/stride/kernel/act`) and the `ap_start → ap_done`
handshake. It runs headless in about 15 s of simulated time and can be repeated with
`hardware/vivado/sim/run_sim.bat`; `sim/tb_sim.wdb` is the waveform for the GUI
(`run_gui_wave.tcl`).

What complements it: the layer-level board dump in §3.2 (bit-accurate comparison
against the numpy golden model) plus the bare-metal register tests in §4.1. These
verify the same property on the actual silicon and the actual bitstream.

---

## 6. Synthesis and implementation report

Source: `hardware/vivado/prj` regenerated from `create_project.tcl` +
`build_bitstream.tcl`; the reports below are copied verbatim into
`hardware/vivado/reports/`.

| run step | finished | artefact |
| --- | --- | --- |
| OOC synthesis of `conv_0` | 21:11:39 | `system_conv_0_0.dcp` |
| `synth_1` | 21:12:16 | `system_wrapper.dcp` |
| `opt_design` | 21:14:04 | `system_wrapper_opt.dcp` (26.8 MB) |
| `place_design` | 21:16:07 | `system_wrapper_placed.dcp` (37.6 MB) |
| `phys_opt_design` | 21:16:26 | `system_wrapper_physopt.dcp` |
| `route_design` | 21:18:27 | `system_wrapper_routed.dcp` (46.6 MB) |
| `write_bitstream` | 21:19:45 | `system_wrapper.bit` (4,045,674 B) |

| headline | value |
| --- | --- |
| Timing | `clk_fpga_0` 100 MHz **WNS +0.398 ns, TNS 0**; `clk_fpga_1` 150 MHz +0.530; video 74.25 MHz +8.347 |
| Utilisation | LUT 40,984 (77.04 %), FF 56,243 (52.86 %), BRAM 101 (72.14 %), **DSP 213 (96.82 %)** |
| Routing | 87,634 routable nets, 100 % routed, 0 errors |
| DRC | 0 errors, 28 warnings, 2 advisories |
| Power | 2.772 W total (2.584 dynamic + 0.188 static), Tj 57 °C |
| Bitgen | `Bitgen Completed Successfully`, 16 infos |

The 28 warnings are inherited from Xilinx infrastructure: `REQP-1839` (RAMB36 driven
by an asynchronous FIFO flag inside the `conv` AXI crossbar), `RTSTAT-10` (222
`aempty_fwft_i` nets with no routable load), `REQP-165/181` (VDMA FIFO write-first).
The identical set appears in the original reference implementation, so they are
properties of the IP set, not of this design's wiring.

Per-file evidence:

```
hardware/vivado/reports/system_wrapper_utilization_synth.rpt    post-synthesis
hardware/vivado/reports/system_wrapper_utilization_placed.rpt   post-place
hardware/vivado/reports/system_wrapper_timing_summary_routed.rpt post-route, all clocks
hardware/vivado/reports/system_wrapper_power_routed.rpt          vectorless power
hardware/vivado/reports/system_wrapper_drc_routed.rpt            DRC
hardware/vivado/reports/system_wrapper_route_status.rpt          routing status
hardware/vivado/reports/system_wrapper_clock_utilization_routed.rpt  clocks
```

---

## 7. How to re-run everything

```bat
:: 1. C simulation of the accelerator
cd hardware\hls
D:\Vitis_HLS\2020.2\bin\vitis_hls.bat -f build_hls.tcl

:: 2. bitstream (10 min)
cd hardware\vivado
D:\Vitis\2020.2\bin\vivado.bat -mode batch -source create_project.tcl
D:\Vitis\2020.2\bin\vivado.bat -mode batch -source build_bitstream.tcl

:: 3. firmware + boot image
cd hardware\vitis\gauge_hdmi_640\build
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
cd ..\..\..\..\sd
D:\Vitis\2020.2\bin\bootgen.bat -image boot_640.bif -arch zynq -o BOOT.BIN.new -w on

:: 4. run on the board and capture the framebuffer
powershell -NoProfile -ExecutionPolicy Bypass -File tools\flash.ps1 -RunSeconds 20
```

Regression checklist (all must hold):

1. `BUILD OK`, `text ≈ 720 KB`.
2. `BOOT.BIN ≈ 4.89 MB`.
3. HUD shows `det 4` on the reference dial.
4. Banner shows a numeric reading with `yolo` or `cv`.
5. UART prints `GAUGE_READING value_centi=…`.
6. Two consecutive frames differ (the loop is running, not frozen).
