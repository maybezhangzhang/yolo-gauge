# build.ps1 -- build the 640x640 gauge-detection application for the Zynq-7020 board.
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
#
# Requirements (paths are configurable below):
#   * Xilinx Vitis 2020.2 (arm-none-eabi toolchain + xilffs sources)
#   * a standalone BSP generated for the system_wrapper platform, which provides
#     libxil.a and the BSP headers.  For this project the BSP was generated once
#     and its headers were merged into one include directory ($BSP_INC).
#   * the conv accelerator driver sources ($DRV, from the HLS package)
#
# The application is *not* built from the Vitis GUI: `app build` silently does
# nothing for this project, so the objects are compiled and linked by hand.

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------- config
$VITIS  = 'D:\Vitis\2020.2'
$BSP    = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'bsp'
$BSPINC = Join-Path $BSP 'include'                 # merged BSP headers (copied from a generated BSP)
$BSPLIB = Join-Path $BSP 'lib'                     # libxil.a
$DRV    = Join-Path $BSP 'drivers\conv_v0_0\src'   # xconv.c / xconv_hw.h
$XILFFS = Join-Path $VITIS 'data\embeddedsw\lib\sw_services\xilffs_v4_4\src'

$ROOT = Split-Path $PSScriptRoot -Parent          # .../gauge_hdmi_640
$SRC  = Join-Path $ROOT 'src'
$OUT  = Join-Path $PSScriptRoot 'out'
$BIN  = Join-Path $VITIS 'gnu\aarch32\nt\gcc-arm-none-eabi\bin'
$GCC  = Join-Path $BIN 'arm-none-eabi-gcc.exe'
$GPP  = Join-Path $BIN 'arm-none-eabi-g++.exe'

New-Item -ItemType Directory -Force -Path $OUT | Out-Null

# ffconf.h with FF_USE_LFN=1 and FF_CODE_PAGE=437 -- without long file names the
# firmware cannot open the "gauge_weights" directory on the SD card.
$INC = @(
  $PSScriptRoot,
  $BSPINC,
  $SRC,
  (Join-Path $SRC 'display_ctrl_hdmi'),
  (Join-Path $SRC 'vdma_api'),
  $XILFFS,
  $DRV
) | ForEach-Object { "-I$_" }

$ARCH = @('-mcpu=cortex-a9','-mfpu=vfpv3','-mfloat-abi=hard','-O2','-g','-Wall',
          '-fno-asynchronous-unwind-tables','-fno-unwind-tables')
$OBJS = @()

function Compile-Cxx($name, $file, $extra) {
  $o = Join-Path $OUT ($name + '.o')
  & $GPP @ARCH @extra -c $file -o $o @INC
  if ($LASTEXITCODE -ne 0) { throw "compile failed: $file" }
  return $o
}
function Compile-C($name, $file, $extra) {
  $o = Join-Path $OUT ($name + '.o')
  & $GCC @ARCH @extra -c $file -o $o @INC
  if ($LASTEXITCODE -ne 0) { throw "compile failed: $file" }
  return $o
}

foreach ($n in @('main','basic_op','img_prep','img_convert','yolo_decode','draw_box','osd','sd_io','cam_link','gauge_overlay','pointer_cv','gauge_config','syscalls_stub')) {
  $OBJS += Compile-Cxx $n (Join-Path $SRC "$n.cpp") @()
}
# C sources that must be compiled as C++ (the main program is C++ and will fail
# to link against C-mangled names)
$OBJS += Compile-Cxx 'display_ctrl_hdmi' (Join-Path $SRC 'display_ctrl_hdmi\display_ctrl_hdmi.c') @('-x','c++')
$OBJS += Compile-Cxx 'vdma_api'          (Join-Path $SRC 'vdma_api\vdma_api.c')                   @('-x','c++')

# accelerator driver (compiled here so it matches the HLS package)
$OBJS += Compile-C 'xconv'       (Join-Path $DRV 'xconv.c')       @()
$OBJS += Compile-C 'xconv_sinit' (Join-Path $DRV 'xconv_sinit.c') @()

# FatFs with long file name support
$LFN = @('-DFILE_SYSTEM_USE_LFN=1')
foreach ($n in @('ff','ffunicode','diskio','ffsystem')) {
  $OBJS += Compile-C $n (Join-Path $XILFFS "$n.c") $LFN
}

# ---------------------------------------------------------------- link
$ELF = Join-Path $OUT 'gauge_hdmi_640.elf'
$LINK = @(
  '-o', $ELF,
  '-mcpu=cortex-a9', '-mfpu=vfpv3', '-mfloat-abi=hard', '-Wl,-build-id=none',
  "-specs=$(Join-Path $SRC 'Xilinx.spec')",
  "-T$(Join-Path $SRC 'lscript.ld')",
  '-Wl,-defsym=_HEAP_SIZE=0x10000000',      # 256 MB heap: the weights are loaded with new[]
  '-Wl,-defsym=_STACK_SIZE=0x2000000',      # 32 MB stack
  "-L$BSPLIB",
  '-Wl,--start-group,-lxil,-lgcc,-lc,--end-group'
)
& $GPP @OBJS @LINK
if ($LASTEXITCODE -ne 0) { throw 'link failed' }

& (Join-Path $BIN 'arm-none-eabi-size.exe') $ELF
Write-Host "BUILD OK -> $ELF"

