# Rebuild flow — from source to a running board

This page is the complete, reproducible path from the files in this repository to a
bitstream and firmware that run on the XC7Z020 board. Every step below was executed
end to end on the machine that produced the numbers quoted here.

```
hardware/hls/src           --(Vitis HLS)-->  hardware/hls/ip/conv640      conv accelerator IP
hardware/vivado/srcs       --(Vivado)----->  prj/.../system_wrapper.bit   PL bitstream
hardware/vitis/.../src     --(arm-none-eabi)--> build/out/gauge_hdmi_640.elf   PS firmware
sd/{fsbl,bit,elf}          --(bootgen)---->  BOOT.BIN                     SD boot image
```

Tool versions used: **Vivado 2020.2**, **Vitis HLS 2020.2**, **Vitis 2020.2** (only for
its `bootgen`, `xsct`, `arm-none-eabi-*` binaries), part **xc7z020clg400-2**.

---

## 0. Environment

| Tool | Path used here | Used for |
| --- | --- | --- |
| `vivado.bat` | `D:\Vitis\2020.2\bin\vivado.bat` | project creation, synthesis, implementation, bitstream |
| `vitis_hls.bat` | `D:\Vitis_HLS\2020.2\bin\vitis_hls.bat` | C synthesis of the conv IP |
| `xsct.bat` | `D:\Vitis\2020.2\bin\xsct.bat` | JTAG download / memory dump |
| `bootgen.bat` | `D:\Vitis\2020.2\bin\bootgen.bat` | `BOOT.BIN` |
| `arm-none-eabi-g++` | `D:\Vitis\2020.2\gnu\aarch32\nt\gcc-arm-none-eabi\bin` | firmware |
| `hw_server.exe` | `D:\Vitis\2020.2\bin\unwrapped\win64.o\hw_server.exe` | JTAG server (started automatically by xsct) |

Nothing here depends on the absolute paths of the original development machine: all
scripts resolve their inputs relative to their own location.

---

## 1. Conv accelerator IP (Vitis HLS)

```bat
cd hardware\hls
D:\Vitis_HLS\2020.2\bin\vitis_hls.bat -f build_hls.tcl
```

`build_hls.tcl` creates `hls_conv640/hls_conv640` , sets `conv` as top, adds the ten
sources, runs `csynth_design` and exports an IP catalog entry to `ip/conv640`.

Two details are contractual:

* **`-version "0.0"`** — the block design instantiates `xilinx.com:hls:conv:0.0`;
  exporting another version silently produces an IP the BD will not pick up.
* `create_clock -period 5.5` reproduces the 100 MHz + margin target used in the
  original run; the exported IP is clocked by `clk_fpga_0` in the BD.

Expected result (`solution1/syn/report/csynth.rpt`, reproduced on 2026-11-03):

| | |
| --- | --- |
| BRAM_18K | 163 (58 %) |
| DSP48E1 | 194 (88 %) |
| FF | 69,119 (64 %) |
| LUT | 77,211 (145 %) — see the note below |
| Latency of one 3×3 layer | ~7 k cycles |

> **About the 145 % LUT line.** `csynth`'s resource estimator counts every HLS
> instance without cross-instance sharing, so it over-reports by roughly 2×. The
> number that matters is what Vivado actually places: the same `conv.v` after
> out-of-context synthesis takes **35,122 LUT = 66.0 %**, and the complete design
> closes at 40,984 LUT = 77.0 %. Do not read the 145 % as "does not fit" — that
> mistake cost a full day of this project (see `docs/`).

The IP is *runtime parameterised*: `ch_in, ch_out, fsize, stride, kernel, act` are
AXI-Lite registers, so the same hardware executes 3×3 conv, 1×1 conv, maxpool and
upsample (`kernel` = 3 / 1 / 2 / 0).

---

## 2. Vivado project and bitstream

```bat
cd hardware\vivado
D:\Vitis\2020.2\bin\vivado.bat -mode batch -source create_project.tcl
D:\Vitis\2020.2\bin\vivado.bat -mode batch -source build_bitstream.tcl
```

`create_project.tcl` builds `prj/yolo_gauge.xpr` from scratch:

* part `xc7z020clg400-2`
* IP repositories: `../hls/ip` (conv) and `./ip_repo` (DVI_TX)
* constraints: `srcs/constrs_1/new/system_wrapper.xdc`, `system_wrapper1.xdc`
* block design: `srcs/sources_1/bd/system/system.bd` → validate → generate targets → wrapper

Two traps that cost time; both are already handled in the script:

* `open_bd_design` fails with `[BD 41-1333]` unless the `.bd` was added with
  `add_files` first.
* `set_property file_type {Block Designs}` is rejected
  (`[Vivado 12-563] The file type 'Block Designs' is not user settable.`).

`build_bitstream.tcl` resets `synth_1`, runs synthesis and implementation to
`write_bitstream`, copies the result to `../sd/system_wrapper_640.bit` and writes a
fixed XSA to `../sd/system_wrapper_640.xsa`.

Expected result (the run that produced the shipped bitstream):

| | |
| --- | --- |
| Bitstream | `system_wrapper.bit`, 4,045,674 B |
| Slice LUT | 40,984 / 53,200 = **77.04 %** |
| Slice Register | 56,243 / 106,400 = 52.86 % |
| Block RAM | 101 / 140 = 72.14 % |
| DSP48E1 | 213 / 220 = **96.82 %** |
| WNS (clk_fpga_0 @100 MHz) | **+0.398 ns**, TNS = 0 |
| WNS (clk_fpga_1 @150 MHz) | +0.530 ns |
| Total on-chip power | 2.772 W (2.584 dynamic + 0.188 static) |
| DRC | 0 errors, 28 warnings, 2 advisories |

The 28 warnings are all from Xilinx infrastructure (async FIFO `REQP-1839`,
222 unrouted `aempty_fwft_i` nets from `RTSTAT-10`, VDMA `writefirst`). None of them
affects the accelerator path; they are identical to the warnings of the original
implementation run.

---

## 3. PS firmware

```bat
cd hardware\vitis\gauge_hdmi_640\build
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
```

`build.ps1` compiles the 12 C++ units, the two C units that must be compiled as C++
(`display_ctrl_hdmi.c`, `vdma_api.c`) and the Xilinx driver / FatFs objects, then
links with the BSP library shipped in `bsp/lib/libxil.a`.

Result: `out/gauge_hdmi_640.elf`

```
text    data      bss       dec
720818    3872  332783632  333508322
```

Requirements that are easy to miss:

* `-Wl,-defsym=_HEAP_SIZE=0x10000000 -Wl,-defsym=_STACK_SIZE=0x2000000` — the default
  `lscript.ld` reserves only 8 KB, and the network allocates its feature maps on the heap.
* FatFs must be compiled with `-DFILE_SYSTEM_USE_LFN=1` and the CP437 `ffconf.h` from
  `bsp/ffcfg` — otherwise the `gauge_weights` directory name (13 characters) cannot be
  opened and the firmware hangs in `read_params`.
* `XConv` driver objects are compiled from `bsp/drivers/conv_v0_0/src` and placed *before*
  the archive on the link line, so the new driver wins over the copy inside `libxil.a`.

---

## 4. Boot image

```bat
cd sd
D:\Vitis\2020.2\bin\bootgen.bat -image boot_640.bif -arch zynq -o BOOT.BIN.new -w on
move /Y BOOT.BIN.new BOOT.BIN
```

`boot_640.bif` uses **relative** file names (`fsbl.elf`, `system_wrapper_640.bit`,
`gauge_hdmi_true640.elf`), so the image is rebuilt from whatever is in `sd/`.
Result: `BOOT.BIN`, 4,885,104 B.

---

## 5. Run over JTAG (development loop)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\flash.ps1 -RunSeconds 20
```

The script does, in this order:

1. `connect`, `targets 1`, `rst -system`
2. `fpga -file sd\system_wrapper_640.bit`
3. `dow sd\fsbl.elf`, `con`, wait, `stop`  — the FSBL initialises DDR
4. **`rst -processor`** — with the CPU reset the MMU is off, so the downloader writes
   physical addresses directly
5. `dow ...\out\gauge_hdmi_640.elf`
6. **`rwr cpsr 0x1d3`** before `con`
7. `con`, wait `-RunSeconds`, `stop`, then dump the framebuffer
   (`0x20100000`, 460800 words) to `tools\fb.bin`

Both bold steps are hard requirements:

* `rst -system` after the FSBL leaves the DDR controller in reset → every later download
  fails with `Cannot access DDR: the controller is held in reset`.
* without `rwr cpsr 0x1d3` the CPU starts with IRQs unmasked and the pending PL
  interrupts jump into the default vector before `main()` runs (`PC = Xil_ExceptionNullHandler`).

Convert the dump to a picture:

```powershell
python -c "from PIL import Image; d=open('tools/fb.bin','rb').read(); Image.frombytes('RGB',(1024,600),d[:1843200]).save('fb.png')"
```

Expected overlay: `YOLOv4-tiny 640x640`, `infer 2517 ms | read 144 | prep 25 | dec 5 | all 2767`,
`frame 1/2 det 3 static image`, the four boxes, and the bottom banner
`READING 10.83 kgf/cm2 | 318 deg | cv/cfg`.

---

## 6. Run from SD card

1. Copy the contents of `sd/` to the root of a FAT32 card.
2. Put the pictures as `test1.bmp … testN.bmp` (24-bit BI_RGB, any size, letterboxed to
   640×640 by the firmware). `testN` must be **consecutive from 1**; scanning stops at the
   first gap.
3. Edit `gauge_scale.txt` for the dial: `min`, `max`, `unit`, `clockwise`, `angle_min`,
   `angle_max`.
4. Set the boot-mode switch to SD and power-cycle.

The firmware picks its mode from the card, so step 2 is also the mode switch:

| card | mode | notes |
| --- | --- | --- |
| has `test1.bmp` | static image | silent, camera link never initialised, loops the images |
| no `testN.bmp` | camera | initialises the OpenMV link and reads live frames; needs the camera streaming |

For the camera branch the OpenMV must run its live variant; if the HDMI shows a photo
that the lens cannot see, switch it back with
`python tools\openmv_source.py --set sensor --restart --port COM14` (it prints
`CAM 320x240 RGB565` once live).

To convert arbitrary images:

```bat
python software\training\make_test_bmp.py photo.jpg --out-dir sd --start 1
```

---

## 7. Verification checklist

| Check | Expected |
| --- | --- |
| `prj/*.runs/impl_1/system_wrapper.bit` exists | ~4.05 MB, `Bitgen Completed Successfully` |
| DRC | 0 errors |
| firmware `text` size | ~720 KB |
| `BOOT.BIN` | ~4.89 MB |
| boot mode | with `testN.bmp`: HUD `frame 1/N ... static image`; without: `CAMERA AUTO MODE` / `CAM <w>x<h>` |
| board HUD | `infer` ≈ 2.5 s, `det` = 4 on the reference dial |
| banner | `READING <value> <unit> | <deg> deg | yolo` or `cv` |
| UART @115200 | `GAUGE_READING value_centi=... angle_cdeg=... source=...` |

If the banner shows `READING --`:

* `hub missing` → the `base` box was not detected; nothing else can be computed.
* `scale marks missing` → neither `minimum`/`maximum` boxes nor a configured arc is available.
* `needle not found` → the needle search could not find a dark line through the hub; check
  that the needle is dark and that the image is not washed out.
