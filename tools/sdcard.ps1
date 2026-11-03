# sdcard.ps1 -- rebuild sd/BOOT.BIN and write the SD payload to a card reader.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File sdcard.ps1 -Drive H
#   powershell -NoProfile -ExecutionPolicy Bypass -File sdcard.ps1 -Drive H -Images "E:\my_entry\gauge_training\test_bmps\*.bmp"
#   powershell -NoProfile -ExecutionPolicy Bypass -File sdcard.ps1 -BuildOnly
#
# It never deletes anything on the card except the files it overwrites; the
# previous BOOT.BIN / ELF are kept as *.prev.

[CmdletBinding()]
param(
    [string]$Drive = '',
    [string[]]$Images = @(),
    [string]$Scale,
    [switch]$BuildOnly,
    [string]$Vitis = 'D:\Vitis\2020.2'
)

$ErrorActionPreference = 'Stop'
$HERE = $PSScriptRoot
$ROOT = Split-Path $HERE -Parent
$SD   = Join-Path $ROOT 'sd'
$ELF  = Join-Path $ROOT 'hardware\vitis\gauge_hdmi_640\build\out\gauge_hdmi_640.elf'
$BIF  = Join-Path $SD 'boot_640.bif'
$bootgen = Join-Path $Vitis 'bin\bootgen.bat'

foreach ($p in @($BIF, $ELF, $bootgen)) {
    if (-not (Test-Path $p)) { throw "missing file: $p" }
}

if (Test-Path (Join-Path $SD 'gauge_hdmi_true640.elf')) {
    Copy-Item $ELF (Join-Path $SD 'gauge_hdmi_true640.elf') -Force
}

Push-Location $SD
if (Test-Path 'BOOT.BIN') { Move-Item 'BOOT.BIN' 'BOOT.BIN.prev' -Force }
& $bootgen -image $BIF -arch zynq -o BOOT.BIN -w on | Select-String 'Bootimage|ERROR' | ForEach-Object { "  $($_.Line.Trim())" }
Pop-Location

"  BOOT.BIN : $((Get-Item (Join-Path $SD 'BOOT.BIN')).Length) bytes"
"  ELF      : $((Get-Item $ELF).Length) bytes"
if ($BuildOnly) { return }
if (-not $Drive) { throw 'give -Drive H (or -BuildOnly)' }

$root = "$($Drive):\"
if (-not (Test-Path $root)) { throw "drive not found: $root" }

foreach ($f in @('BOOT.BIN', 'gauge_hdmi_true640.elf')) {
    $dst = Join-Path $root $f
    if (Test-Path $dst) { Copy-Item $dst "$dst.prev" -Force }
    Copy-Item (Join-Path $SD $f) $dst -Force
    "  -> $dst"
}

$scaleText = if ($Scale) {
    Get-Content $Scale -Raw
} else {
    "# Gauge scale - edit and reboot, no need to rebuild.`r`nmin = 0`r`nmax = 16`r`nunit = MPa`r`nclockwise = 1`r`n"
}
Set-Content -Path (Join-Path $root 'gauge_scale.txt') -Value $scaleText -Encoding ASCII
"  -> $($root)gauge_scale.txt"

foreach ($pat in $Images) {
    Get-ChildItem $pat -ErrorAction SilentlyContinue | ForEach-Object {
        $n = $_.BaseName
        Copy-Item $_.FullName (Join-Path $root "$n.bmp") -Force
        "  -> $n.bmp"
    }
}

$bmps = Get-ChildItem $root -Filter 'test*.bmp' -ErrorAction SilentlyContinue
"  card now has $($bmps.Count) test image(s) -> " + $(if ($bmps.Count -gt 0) { 'STATIC IMAGE mode' } else { 'CAMERA mode' })
