# build_bitstream.tcl -- synth / impl / bitstream for the gauge reader
#
# Usage:
#   cd hardware/vivado
#   vivado -mode batch -source create_project.tcl      # once, creates prj/
#   vivado -mode batch -source build_bitstream.tcl
#
# Produces ../sd/system_wrapper_640.bit (and the .xsa used by Vitis).

set script_dir [file normalize [file dirname [info script]]]
set prj_file   [file join $script_dir prj yolo_gauge.xpr]
set out_dir    [file normalize [file join $script_dir .. sd]]

if {![file exists $prj_file]} {
    error "project not found: $prj_file  (run create_project.tcl first)"
}

open_project $prj_file

# top level is the generated wrapper of the block design
set top [get_property top [current_fileset]]
puts "INFO: top = $top"

reset_run synth_1
launch_runs synth_1 -jobs 8
wait_on_run synth_1
if {[get_property PROGRESS [get_runs synth_1]] != "100%"} {
    error "synthesis failed, see prj/yolo_gauge.runs/synth_1/runme.log"
}

launch_runs impl_1 -to_step write_bitstream -jobs 8
wait_on_run impl_1
if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {
    error "implementation failed, see prj/yolo_gauge.runs/impl_1/runme.log"
}

set bit [glob -nocomplain [file join $script_dir prj yolo_gauge.runs impl_1 *.bit]]
if {[llength $bit] == 0} { error "no .bit produced" }
file copy -force [lindex $bit 0] [file join $out_dir system_wrapper_640.bit]
puts "INFO: bitstream -> [file join $out_dir system_wrapper_640.bit]"

# hardware hand-off file for Vitis (platform create -hw <xsa>)
write_hw_platform -fixed -force -file [file join $out_dir system_wrapper_640.xsa]
puts "INFO: xsa -> [file join $out_dir system_wrapper_640.xsa]"

close_project
