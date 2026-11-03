# -----------------------------------------------------------------------------
#  build_hls.tcl  --  re-synthesise the convolution accelerator and re-package it
#
#  Usage:
#     cd hardware/hls
#     vitis_hls -f build_hls.tcl
#
#  Result:  ./ip/conv640  (IP catalog entry, VLNV xilinx.com:hls:conv:0.0)
#           which is what the Vivado block design instantiates.
#
#  NOTE
#    * csim is not part of this script: the C testbench used during development
#      drove the IP with a synthetic 4x20x20 stimulus and is kept out of the
#      deliverable (see docs/DEBUG_LOG_640.md for the numbers it produced).
#    * the IP version must stay 0.0 -- the block design references conv:0.0.
# -----------------------------------------------------------------------------

open_project hls_conv640 -reset
set_top conv

add_files src/upsample.h
add_files src/upsample.cpp
add_files src/std_conv.h
add_files src/std_conv.cpp
add_files src/pwconv.h
add_files src/pwconv.cpp
add_files src/maxpool.h
add_files src/maxpool.cpp
add_files src/conv.h
add_files src/conv.cpp

open_solution "solution1" -flow_target vivado
set_part {xc7z020-clg400-2}
create_clock -period 5.5 -name default

csynth_design

# ---------------------------------------------------------------------------
# IP packaging.
#
# Known 2020.2 bug: the packer derives the core revision from YYMMDDHHMM, e.g.
# 2611030830 on 2026-11-03, which overflows a signed 32-bit int and aborts with
# "bad lexical cast: source type value could not be interpreted as target".
# There is no -revision option in this version.  The packaging therefore fails
# on any machine whose clock is in 2026 or later, so it is guarded here: the
# synthesis result stays usable and the shipped ip/conv640 (the exact package
# the bitstream was built from) is kept untouched.
# Workaround if you really must re-package: temporarily set the system date to
# 2021, run this script, then set it back.
# ---------------------------------------------------------------------------
set pkg_ok 0
set do_export 0
if {[info exists ::env(HLS_EXPORT)] && $::env(HLS_EXPORT) eq "1"} {
    set do_export 1
}

if {$do_export} {
    if {[catch {
            export_design -format ip_catalog -rtl verilog \
                          -vendor xilinx.com -library hls -version "0.0" \
                          -output ./ip/conv640
        } err]} {
        puts "WARN: export_design failed: $err"
        puts "WARN: keeping the existing ip/conv640 package (the one the shipped bitstream uses)."
    } else {
        set pkg_ok 1
    }
} else {
    puts "INFO: C synthesis done. IP packaging skipped (2020.2 packer fails on system dates >= 2026)."
    puts "INFO: the package that the shipped bitstream was built from is already in ip/conv640."
    puts "INFO: to attempt re-packaging anyway:  set HLS_EXPORT=1  before running this script."
}

if {$pkg_ok} {
    puts "INFO: packaged IP written to ./ip/conv640"
}
exit
