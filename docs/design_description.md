# Design description — function, architecture, interfaces, partitioning, optimisations

Target: **XC7Z020 (Zynq-7020)**, Vivado/Vitis 2020.2, bare-metal (no OS).
Everything below is measured on the board unless it is explicitly marked *(estimate)*.

---

## 1. Function

The board is a self-contained analog-gauge reader.

1. Reads a 640×640 photo from the SD card (`test1.bmp … testN.bmp`, 24-bit BMP).
2. Runs a 4-class YOLOv4-tiny detector on it and locates four parts of the dial:
   `base` (needle hub), `minimum` (printed value at the low end of the scale),
   `maximum` (printed value at the high end), `tip` (needle tip).
3. Turns the geometry into a reading: the needle angle is mapped linearly onto the
   arc between the two scale marks, using the scale configured for that dial.
4. Draws the image, the boxes, a needle ray and a `READING …` banner on HDMI
   (1024×600), and prints per-stage timings and the reading on UART1 (115200 8N1).

Only the four classes above are hard-coded; the numeric scale (min, max, unit,
arc angles) lives on the SD card, so a different dial needs a text file, not a
rebuild. When the needle tip is not found confidently the position of the needle is
recovered by a pure-CV ray search through the hub (see §5.6).

Measured end-to-end: **2.52 s/inference, 2.77 s including read + preprocess + draw**,
0.36 fps, 2.77 W total on-chip power.

---

## 2. Architecture

```
        ┌──────────────── PS (ARM Cortex-A9 @667 MHz, bare metal) ─────────────┐
        │ main loop:  SD read → gray → letterbox → conv driver → decode/NMS   │
        │             → reading (geometry or CV) → OSD → HDMI                 │
        └───────┬──────────────────────────────────────────────┬──────────────┘
   AXI-Lite     │                                              │ AXI HP (DDR)
   (registers)  ▼                                              ▼
        ┌──────────────── PL (100 MHz) ───────────────────────────────────────┐
        │ conv_v0_0 : HLS conv engine, 4 operators, 16 m_axi + AXI-Lite        │
        │ axi_vdma + v_tc + v_axi4s_vid_out + DVI_TX : 1024×600 display path   │
        └─────────────────────────────────────────────────────────────────────┘
```

* **The PL contains exactly two things**: the convolution engine and the display
  pipeline. There is no CNN state machine, no scheduler and no inference FSM in
  hardware.
* **The PS owns all control**: layer order, tiling loop, pointer setup, activation,
  decode, NMS, reading, drawing. Every layer is one `program(); start(); poll done`
  sequence over AXI-Lite.

Layer sequence executed per frame (28 layers):

| stage | layers | time |
| --- | --- | --- |
| backbone (CSPDarkNet-tiny) | 0–19 | 2255 ms |
| neck (1×1 + upsample) | 20–25 | 53 ms |
| heads (two detection heads) | 26–27 | 209 ms |

---

## 3. Interfaces

### 3.1 AXI-Lite register map (verified by byte-wise probe against the driver header)

| window | address | contents |
| --- | --- | --- |
| CTRL | `0x4000_0000` | `AP_CTRL[0]=start, [1]=done, [2]=idle`; scalars at `+0x10 +8k`: `ch_in, ch_out, fsize, stride, kernel, act` (8-byte stride, wraps at 0x40) |
| DATA | `0x4001_0000` | 13 pointers, 0x0C apart, each low word + high word: `in1..in4` @0x10/0x1C/0x28/0x34, `w1..w4` @0x40/0x4C/0x58/0x64, `b` @0x70, `out1..out4` @0x7C/0x88/0x94/0xA0 |

`kernel` selects the operator: **3** = 3×3 conv, **1** = 1×1 conv, **2** = maxpool,
**0** = upsample. `act` = 1 applies leaky-ReLU inside the engine. Because the four
`in*` (and the four `out*`) pointers must point at the same buffer — the engine
walks `ch c` at `base + c*fsize²` — the driver always writes the same address into
all four.

### 3.2 Memory map (PS side)

| region | address | size | use |
| --- | --- | --- | --- |
| code | `0x0010_0000` | ~0.72 MB | `.text/.data` |
| `.bss` | `0x001B_3840` | ~30.7 MB | feature maps, weights, detections |
| heap | `0x01F1_0BF0` | 256 MB | `new Yolo4_Tiny()`, per-layer scratch |
| stack | `0x11F1_0BF0` | 32 MB | |
| framebuffer | `0x2010_0000` | 1024×600×3 | HDMI |
| source image | `0x2110_0000` | 640×640×3 | decoded BMP |
| camera buffer | `0x2150_0000` | 640×640×2 | reserved |

Heap and stack sizes are not the linker defaults: `build.ps1` passes
`-Wl,-defsym=_HEAP_SIZE=0x10000000 -Wl,-defsym=_STACK_SIZE=0x2000000`, because the
stock `lscript.ld` reserves 8 KB and the network allocates its feature maps.

### 3.3 File contracts (SD card, FAT32)

| file | contract |
| --- | --- |
| `gauge_weights/conv_forP5/...` | 42 files: per layer `wXX.bin` / `bXX.bin`, little-endian **float32**, on board multiplied by 512 and cast to int16 (`Q7.9`) |
| `testN.bmp` | 24-bit BI_RGB, any size, letterboxed to 640×640; `N` must run 1,2,3,… without gaps (scanning stops at the first gap) |
| `gauge_scale.txt` | `min`, `max`, `unit`, `clockwise`, `angle_min`, `angle_max`; each optional |
| `BOOT.BIN` | FSBL + bitstream + ELF, built by `bootgen` from `boot_640.bif` |

### 3.4 UART protocol (115200 8N1)

```
[1/1] read=144ms prep=25ms infer=2517ms dec=5ms total=2767ms det=3
GAUGE_NEEDLE angle_cdeg=31800 score_milli=707 run_px=158 ok=1
GAUGE_TIP cv angle_cdeg=31840 score_milli=707 run_px=158
GAUGE_READING value_centi=1083 angle_cdeg=31840 source=cv arc=cfg
```

### 3.5 Host-side contracts (training → board)

* Input size is one parameter through the whole chain: `GAUGE_INPUT_SIZE` selects the
  input shape *and* the anchor file; `auto_anchors.py` regenerates anchors for that
  size; `export_true640.py` writes the weights; the firmware reads the same size.
  Mixing 640 training with 416 deployment silently destroys mAP, so the chain is
  deliberately single-sourced.
* The 3×3 output of the engine is rotated by exactly one tile along W. For a 640
  model the tile is **10 px** (416-era code used 13). The driver compensates with a
  per-row `memmove`; without it the network produces zero boxes.

---

## 4. Hardware / software partition

| function | where | why |
| --- | --- | --- |
| convolution (3×3, 1×1), maxpool, upsample | PL | 16.07 GFLOPs/frame cannot run on a 667 MHz A9 in real time; the engine reaches 6.4 GFLOP/s effective |
| display (framebuffer → DVI) | PL (VDMA + VTC + DVI_TX) | pixel rate and timing must be hardware-timed |
| layer scheduling, pointer setup | PS | 28 layers × 13 pointers; changing the schedule is a rebuild of 40 s, not a re-synthesis |
| decode, NMS, reading, CV needle search, OSD | PS | needs iterative tuning (thresholds, arc angles, fallback rules) |
| SD / FAT32, weights load | PS | file I/O is software work by nature |

Decision rule used throughout: **anything that changes while tuning stays in
software; only the arithmetic that is both fixed and dominant goes to hardware.**
The PL budget confirms the choice: the engine already consumes 96.8 % of the DSPs,
so a second accelerator (or an inference FSM) would not fit anyway.

---

## 5. Optimisation rationale

### 5.1 One engine, four operators, runtime parameters

Instead of four IPs, `conv` implements 3×3 conv, 1×1 conv, maxpool and upsample
behind one register interface. Cost: 35,122 LUT / 213 DSP / 45 RAMB36. Benefit: the
whole network runs on one datapath and one driver, and the partition does not need
to be fixed at synthesis time.

### 5.2 Fixed point: Q7.9 with ×512 weights

Activations are `ap_fixed<16,7>` (9 fractional bits), so weights are stored as
float32 and scaled by 512 on load. This keeps every partial sum inside ±64 for the
trained network (worst-case bound 245 would clip, so the design relies on the
distribution, verified on 40 layers — see `verification.md`). Cost: one multiply by
512 per weight at load; benefit: no runtime quantisation step, no calibration pass,
weights exported directly from PyTorch.

### 5.3 Tile size 10 (3×3) / 100 (1×1)

`std_conv.h` uses `Tr=Tc=10`, `Tm=32`, `Tn=4`; `pwconv.h` uses `Tp=100`, `Tm=8`.
The 3×3 path is compute-bound (DSP 96.8 %), the 1×1 path is memory-bound and
over-fetches on tile edges — measured effect: the four 1×1 layers at 160/80/40
resolution take 283 ms each, i.e. **45 % of the frame for 1/9 of the MACs**. That is
the top item for the next revision (§8).

### 5.4 640×640 rather than 416×416

Measured cost of going from 416 to 640 is only +36 % on the GPU-side estimate
(16.07 vs 6.79 GFLOPs) and 2.52 s on the board; the accuracy gain is decisive for
the small targets: the median object is 31×18 px, the needle tip 20×13 px.
`evaluate` on the 263-image test set: 62.78 % mAP@0.5 at 640. The 416 variant was
measured only on the PC side and is kept as a fallback *(estimate ≈1.1 s/frame)*.

### 5.5 Tile-shift compensation in the driver

The 3×3 output is rotated by one tile along W (10 px for this build). Measured
effect: correlation with the PC reference 0.40 before, **0.98** after a 10-px shift,
and 0.68/0.41 for 13/20 px. The fix is a 10-element rotation per row in
`basic_op.cpp`, i.e. 82 ms per frame on the PS — far cheaper than a re-synthesis and
it keeps the hardware contract explicit.

### 5.6 CV needle search on every frame

`tip` is the weakest class (AP 22.3 %, box 20×13 px), so the reading is **not**
taken from that box. Every frame the firmware searches 360 rays × 48 samples from
the hub for the darkest line, refines ±1° at 0.05°, and rejects lines shorter than
¼ of the radius; the found line is drawn in magenta and the value is computed from
its angle. The detector's `tip` box is still drawn (cyan) and the angle difference
between box and line is logged as a cross-check, which makes a disagreement
visible both on the HDMI picture and on the UART. Only if the ray search fails is
the box used as the reading source (`source=yolo`).

Cost: about 8 ms per frame on the A9 (0.3 % of the 2.5 s budget), so there is no
reason to skip it. Cross-checked against an independent numpy implementation:
PC 205.50°, board 206.40°, reading difference 0.01 bar.

### 5.7 Scale as data, not code

`gauge_scale.txt` on the SD card carries min/max/unit and the arc angles. Changing
the dial is a text edit + reboot. The compiled default is `0..16 MPa` (the bench
dial); while validating the mechanism the Tomsk 0…16 kgf/cm² dial read
10.83 kgf/cm² with `arc=cfg`.

---

## 6. Performance and resource comparison against baselines

### 6.1 Latency / throughput

| platform | input | per frame | note |
| --- | --- | --- | --- |
| RTX 4060 Laptop, PyTorch fp32 | 640 | 15 ms | accuracy reference, not a deployment target |
| PC CPU (single thread) | 640 | 780 ms *(estimate: 16.07 GFLOPs @ ~20 GFLOP/s)* | no accelerator |
| **this design, PS only (A9, no PL)** | 640 | **≈ 10 s** *(estimate: 16.07 GFLOPs @ ~1.6 GFLOP/s)* | software baseline |
| **this design, PS + PL accelerator** | 640 | **2.52 s** (2.77 s e2e) | measured, `infer` from the HUD |
| this design, 416 variant | 416 | ≈ 1.1 s *(estimate, ratio of 6.79/16.07)* | not built |

Speed-up of hardware over the A9 software baseline: **≈4×** on the convolution
stage; end-to-end throughput 0.36 fps.

Stage breakdown (measured):

| stage | ms | % |
| --- | --- | --- |
| SD read + gray | 133 | 4.8 |
| letterbox + /255 | 25 | 0.9 |
| YOLO inference (28 layers) | 2517 | 91.0 |
| decode + NMS | 5 | 0.2 |
| OSD draw | 82 | 3.0 |
| **total** | **2767** | 100 |

### 6.2 Resources

| | used | available | % |
| --- | --- | --- | --- |
| Slice LUT | 40,984 | 53,200 | **77.04** |
| Slice Register | 56,243 | 106,400 | 52.86 |
| Block RAM tile | 101 | 140 | 72.14 |
| DSP48E1 | 213 | 220 | **96.82** |
| BUFGCTRL | 4 | 32 | 12.5 |
| MMCM | 1 | 4 | 25.0 |
| Slice (sites) | 13,006 | 13,300 | 97.79 |

Of these, the convolution engine alone is 35,122 LUT / 213 DSP / 95.5 RAMB; the rest
is the display path (VDMA, VTC, video-out, DVI) and interconnect.

Against a "no accelerator" baseline the accelerator costs **66 % of the LUTs and
96.8 % of the DSPs** and buys the 4× speed-up above. There is deliberately no spare
DSP: a second engine would need the parallel factors (`Tm`, `Tr/Tc`, `MAX_LEN`) to be
reduced first.

### 6.3 Accuracy

| checkpoint | mAP@0.5 | base | maximum | minimum | tip |
| --- | --- | --- | --- | --- | --- |
| ep070 | 60.40 | 99.88 | 80.03 | 45.96 | 15.75 |
| best (≈ep045) | 62.01 | 99.88 | 82.57 | 53.36 | 12.23 |
| **ep120 (deployed)** | **62.78** | 99.99 | 75.78 | 53.06 | **22.29** |

Test set: 263 images, letterboxed to 640. The board reproduces the PC result — the
same image yields the same four boxes (`base 0.998`, `maximum 0.985`,
`minimum 0.972`, `tip 0.562`) and a reading within 0.01 bar of the PC reference.

### 6.4 Timing quality of the implementation

| clock | frequency | WNS | TNS | endpoints |
| --- | --- | --- | --- | --- |
| `clk_fpga_0` (PS→PL + engine) | 100 MHz | **+0.398 ns** | 0 | 163,241 |
| `clk_fpga_1` | 150 MHz | +0.530 ns | 0 | 9,976 |
| `clk_out1` (video) | 74.25 MHz | +8.347 ns | 0 | 5,187 |

All constraints met, 0 un-routed nets, 0 DRC errors, power 2.772 W
(2.584 dynamic + 0.188 static, junction 57 °C). Full reports:
`hardware/vivado/reports/`.

---

## 7. What the design does **not** do

* No video-rate processing: 0.36 fps; the reading window is a still frame by design.
* No camera requirement: the camera path exists (`cam_link`) but the shipped mode is
  static images, which is what a gauge inspection station needs.
* No on-board re-training / no runtime quantisation: the model is fixed at build time.
* The 416 fallback is an estimate, not a measured board configuration.

---

## 8. Next steps (ranked by expected gain)

1. **1×1 path tile efficiency** — 45 % of the frame for 1/9 of the MACs; `Tp=100`
   boundary handling re-fetches edges. Expected 25–35 % end-to-end gain.
2. **Double-buffer / overlap SD read + preprocess with inference** — hides 158 ms.
3. **tip accuracy** — needs a two-stage crop around `base` (est. 2–3× tip AP at
   +2.5 s/frame) or more tip annotations.
4. **Per-image scale file** (`testN.txt`) so a multi-dial demo card is correct
   without editing one global file.
