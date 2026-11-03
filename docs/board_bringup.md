# Board bring-up notes

Target: Zynq-7020 (xc7z020), HDMI 1024x600 output, SD card with weights and
test images, JTAG for development.

## Memory map used by the application

| address | use |
|---------|-----|
| `0x00100000` | application image (entry point) |
| `0x05000000` | scratch area used by the debug probes only |
| `0x20000000` | HDMI framebuffer, 1024x600x3 bytes |
| `0x21000000` | decoded source bitmap (`src_buf`) |
| `0x21500000` | camera frame buffer (only in camera mode) |
| `0x40000000` | conv IP control registers (`s_axi_CTRL`): `AP_CTRL 0x00`, `CH_IN 0x10`, `CH_OUT 0x18`, `FSIZE 0x20`, `STRIDE 0x28`, `KERNEL 0x30`, `ACT 0x38` |
| `0x40010000` | conv IP data registers (`s_axi_CONTROL`): `IN1..IN4 0x10/0x1C/0x28/0x34`, `W1..W4 0x40/0x4C/0x58/0x64`, `B 0x70`, `OUT1..OUT4 0x7C/0x88/0x94/0xA0` (each 64-bit pointer, low word first) |
| `0x43000000` | AXI VDMA |
| `0x43C10000` | video timing controller |

`lscript.ld` reserves a 256 MB heap (`_HEAP_SIZE`) and a 32 MB stack
(`_STACK_SIZE`), which keeps `new`-allocated buffers away from the video
frame buffers at 512 MB.

## Boot and download

* Development: `tools/flash.ps1` (bitstream + FSBL + application over JTAG).
* Production: `boot_640.bif` contains FSBL + bitstream + application;
  `bootgen -image boot_640.bif -arch zynq -o BOOT.BIN -w on`, copy to the SD
  card root and switch the boot-mode jumper from JTAG to SD.

Watch out: `rst -system` in xsct leaves the DDR controller in reset, so the
download that follows fails with `Cannot access DDR`. Always use
`rst -processor`, and set `cpsr` to `0x1d3` before `con` so that pending PL
interrupts cannot vector into the default handler before `main()` installs its
own (this application runs with interrupts disabled).

## UART

Two PS UARTs are used, for two different purposes:

| port | role | pins | baud |
| --- | --- | --- | --- |
| UART0 | console / log (on-board CH340, shows up as `USB-SERIAL CH340`, COMx) | MIO, fixed by the board | 115200 8N1 |
| UART1 | **camera link to the OpenMV**, routed through EMIO to PL pins | `UART1_RX_0` = **T19**, `UART1_TX_0` = **J15**, both LVCMOS33 | 921600 8N1 |

The console prints the boot banner, the tile timings, and one line per image:

```
IMG test1.bmp: 640x640, read 133ms
[static] read=133ms prep= 25ms infer=2516ms dec= 5ms total=2726ms det=4
```

## Camera link wiring (OpenMV → Zynq)

The camera side is `F:\main.py` on the OpenMV (H7/H7 Plus): `UART_ID = 3`, and on
those boards **UART3 is P4 = TX, P5 = RX**, 921600 8N1, `timeout_char = 3000`.
The board side is PS UART1 through EMIO, constrained in
`hardware/vivado/srcs/constrs_1/new/system_wrapper1.xdc`:

```
set_property -dict {PACKAGE_PIN T19 IOSTANDARD LVCMOS33} [get_ports UART1_RX_0]
set_property -dict {PACKAGE_PIN J15 IOSTANDARD LVCMOS33} [get_ports UART1_TX_0]
```

So the cable must **cross over**, with a common ground:

| OpenMV | board | note |
| --- | --- | --- |
| **P4** (TX) | **T19** (UART1_RX) | camera → FPGA, the only data path that matters for frames |
| **P5** (RX) | **J15** (UART1_TX) | FPGA → camera, carries the per-frame `K` acknowledge |
| **GND** | **GND** | required; a missing ground produces `ack=False` and dropped frames |

Both sides are 3.3 V — do not feed 5 V into P4/P5.  If only the frame path were
wired (P4 → T19) the board would receive images but the camera would keep printing
`ack=False` and resend; that is the signature of a missing/mis-wired P5 line, not of
a broken board.

Frame format (little endian, `cam_link.h`):

```
A5 5A | ver = 0x02 | fmt (0 = GRAY8, 1 = RGB565) | w (u16) | h (u16)
      | len (u32, = w*h*bytes_per_pixel) | payload[len] | sum (u16) | 5A A5
```

`sum` is the 16-bit arithmetic sum of the payload.  The camera sends one frame and
then waits for a single `K` byte; the firmware answers after the frame has been
decoded and drawn (`cam_link_ack()`), which is also why a hung inference shows up as
an endless stream of `ack=False` on the camera side.

## SD card layout

```
BOOT.BIN            optional (SD boot)
gauge_weights/      42 .bin files + manifest.json
gauge_model.json    classes + anchors + input size
test1.bmp ...       24-bit BMP (BI_RGB, bpp=24, comp=0), any size
```

The application enumerates `testN.bmp` starting at 1 and stops at the first
gap. If at least one image is found it runs in static-image mode (no camera, no
handshake); otherwise it falls back to the camera link loop.

BMP parsing accepts only 24-bit uncompressed bitmaps, top-down or bottom-up,
and any row stride. Everything is letterboxed to 640x640 with pad value 114 and
converted to gray inside `img_prep.cpp`, so a colour BMP and its grayscale
version give the same detections.

## What "no boxes" looked like while debugging

* screen shows the image but no boxes -> the conv IP output was wrong; the
  network ran end-to-end (28 convolutions per frame, `det=0`);
* CPU parked at `XConv_IsDone` -> the IP never signalled done, which turned out
  to be an illegal parameter combination in an early test, not the hardware;
* CPU parked in `Xil_ExceptionNullHandler` -> the CPU was started with IRQs
  unmasked (see the `cpsr` note above);
* CPU parked at `read_params` (`while(1)`) -> the weight directory could not be
  opened because FatFs had long file names disabled.

## Mode selection at boot

`main.cpp` picks the mode from the card contents, not from a switch or a rebuild:

```
SD_Init()          fails      -> error banner + idle (weights unusable)
find_img_count()   nimgs > 0  -> static image loop, camera link never initialised
                   nimgs == 0 -> camera mode (cam_link_init + per-frame K ack)
```

Static mode is deliberately silent: no `cam_link_init()`, no handshake traffic, so
a card with `testN.bmp` makes the board run stand-alone.

Camera mode is a passive receiver: it waits for 320x240 frames pushed by the
OpenMV over UART3 at 921600 and answers each one with `K`.  When the screen stays
on `waiting for OpenMV frame`, check two things:

1. Is the camera enumerated? `Get-CimInstance Win32_PnPEntity | ? { $_.Name -like
   "*COM*" }` - the OpenMV appears as USB CDC (`VID_37C5`, e.g. COM14).
2. What is the OpenMV printing? `python tools/openmv_source.py --restart --port
   COM14` dumps its console.  Healthy output is `lock gain=.. exposure=..`,
   `gray api = bytearray, 320x240, uart3 @ 921600`, then one line per frame.  If it
   instead repeats `file frame NNNNN: send 834 ms, ack=False`, the camera script is
   still in replay-from-file mode and the board is legitimately showing that file.

The HUD tells the two apart: `CAM 320x240 RGB565` is live capture, any other size
or `GRAY8` means a stored BMP is being streamed.

