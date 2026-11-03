# flash.ps1 -- one-click download of bitstream + FSBL + application to the
# Zynq-7020 board over JTAG.  No Vitis GUI, no Vivado GUI: it drives xsct.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File flash.ps1
#   powershell -NoProfile -ExecutionPolicy Bypass -File flash.ps1 -RunSeconds 25 -Capture
#   powershell -NoProfile -ExecutionPolicy Bypass -File flash.ps1 -Source build
#   powershell -NoProfile -ExecutionPolicy Bypass -File flash.ps1 -DownloadOnly
#   powershell -NoProfile -ExecutionPolicy Bypass -File flash.ps1 -List
#
# The two lines that cost hours when missing:
#   * "rst -processor" (NOT "rst -system") before downloading the application:
#     -system leaves the DDR controller in reset and the downloader then cannot
#     write past the first megabyte.
#   * "rwr cpsr 0x1d3" before "con": after "dow" the CPU starts with IRQs
#     unmasked and a pending PL interrupt jumps into the empty default vector
#     before main() ever runs.

[CmdletBinding()]
param(
    # bitstream + fsbl source: sd (shipped image) or build (fresh Vivado output)
    [ValidateSet('sd', 'build')]
    [string]$Source = 'sd',

    # application ELF: build (freshly compiled) or sd (shipped)
    [ValidateSet('sd', 'build')]
    [string]$App = 'build',

    # seconds to let the application run before halting (0 = leave it running)
    [int]$RunSeconds = 20,

    # dump the 1024x600 framebuffer and save it as PNG next to this script
    [switch]$Capture,

    # download and start, but do not halt afterwards
    [switch]$DownloadOnly,

    # only resolve and print the files that would be used
    [switch]$List,

    [string]$Vitis = 'D:\Vitis\2020.2'
)

$ErrorActionPreference = 'Stop'

$HERE  = $PSScriptRoot
$ROOT  = Split-Path $HERE -Parent
$SD    = Join-Path $ROOT 'sd'
$BUILD = Join-Path $ROOT 'hardware\vitis\gauge_hdmi_640\build\out'

function Resolve-Artifact([string]$dir, [string[]]$names) {
    foreach ($n in $names) {
        $p = Join-Path $dir $n
        if (Test-Path $p) { return (Resolve-Path $p).Path }
    }
    return $null
}

$BIT = $null
if ($Source -eq 'build') {
    $BIT = Resolve-Artifact (Join-Path $ROOT 'hardware\vivado\prj\yolo_gauge.runs\impl_1') @('system_wrapper.bit')
}
if (-not $BIT) { $BIT = Resolve-Artifact $SD @('system_wrapper_640.bit', 'system_wrapper.bit') }

$FSBL = Resolve-Artifact $SD @('fsbl.elf')

$ELF = $null
if ($App -eq 'build') { $ELF = Resolve-Artifact $BUILD @('gauge_hdmi_640.elf') }
if (-not $ELF) { $ELF = Resolve-Artifact $SD @('gauge_hdmi_true640.elf') }
if (-not $ELF) { $ELF = Resolve-Artifact $BUILD @('gauge_hdmi_640.elf') }

$xsct = Join-Path $Vitis 'bin\xsct.bat'

""
"  bitstream : $BIT"
"  fsbl      : $FSBL"
"  app       : $ELF"
"  xsct      : $xsct"
""

foreach ($p in @($BIT, $FSBL, $ELF, $xsct)) {
    if (-not $p -or -not (Test-Path $p)) { throw "missing file: $p" }
}
if ($List) { return }

$fb  = Join-Path $HERE 'fb.bin'
$png = Join-Path $HERE 'fb.png'
$tcl = Join-Path $env:TEMP ('flash_' + [guid]::NewGuid().ToString('N') + '.tcl')

$tail = if ($DownloadOnly -or $RunSeconds -le 0) {
@'
puts "STATUS: running (not halted)"
'@
} else {
@"
after $($RunSeconds * 1000)
catch { stop }
catch { puts "PC=[rrd pc]" }
mrd -bin -file {$fb} 0x20100000 460800
puts "STATUS: halted, framebuffer saved"
"@
}

$script = @"
# --- connect -------------------------------------------------------------
connect
if { [catch { targets 1 } err] } {
    puts "STATUS: FAIL cannot select DAP: `$err"
    puts "HINT: if the DAP reports 'AP transaction error', power-cycle the board."
    exit 1
}
# --- reset + program the PL ---------------------------------------------
catch { rst -system }
after 1000
fpga -file {$BIT}
after 800

# --- FSBL: initialises DDR, clocks, MIO ----------------------------------
targets -set -filter {name =~ "ARM*#0"}
dow {$FSBL}
con
after 3000
catch { stop }

# --- CPU reset so the downloader sees 1:1 VA/PA (MMU off) ----------------
rst -processor
after 300
targets -set -filter {name =~ "ARM*#0"}

# --- application ---------------------------------------------------------
dow {$ELF}
rwr cpsr 0x1d3
con
$tail
disconnect
exit 0
"@

Set-Content -Path $tcl -Value $script -Encoding ASCII

$out = & $xsct $tcl 2>&1
Remove-Item $tcl -Force -ErrorAction SilentlyContinue

$bad = $out | Where-Object { $_ -match 'ERROR|error:|FAIL' }
if ($bad) {
    $bad | ForEach-Object { "  ! $_" }
    ""
    "  Flashing failed.  Check, in this order:"
    "    1. board powered?             -> jtag targets lists Digilent JTAG-HS1"
    "    2. 14-pin JTAG cable seated?  -> otherwise no ARM core shows up"
    "    3. DAP says 'AP transaction error' -> power-cycle the board"
    ""
    exit 2
}

$out | Where-Object { $_ -match 'STATUS:' } | ForEach-Object { "  $_" }

if (-not $DownloadOnly -and $RunSeconds -gt 0 -and (Test-Path $fb)) {
    "  framebuffer: $fb ($((Get-Item $fb).Length) bytes)"
    if ($Capture) {
        $py = Get-Command python.exe -ErrorAction SilentlyContinue
        if (-not $py) { $py = Get-Command py -ErrorAction SilentlyContinue }
        if ($py) {
            & $py.Source -c "from PIL import Image;d=open(r'$fb','rb').read();Image.frombytes('RGB',(1024,600),d[:1024*600*3]).save(r'$png')" 2>$null
            if (Test-Path $png) { "  screenshot : $png" }
        } else {
            "  (no python on PATH: fb.bin is 1024x600x3 bytes, BGR order)"
        }
    }
}

"  done."
