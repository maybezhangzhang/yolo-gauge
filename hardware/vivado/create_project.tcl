# -----------------------------------------------------------------------------
#  create_project.tcl  --  rebuild the Vivado project for the gauge reader
#
#  Usage (Windows):
#     vivado -mode batch -source create_project.tcl
#  or from the Vivado GUI:  source create_project.tcl
#
#  What it does
#    1. creates ./prj/yolo_gauge.xpr  (xc7z020clg400-2)
#    2. registers the IP repositories:  ../../hls/ip  (conv accelerator)
#                                       ./ip_repo     (DVI_TX)
#    3. opens the block design  srcs/sources_1/bd/system/system.bd
#    4. validates it, regenerates the wrappers and the IP outputs
#
#  Then:
#     launch_runs impl_1 -to_step write_bitstream ; wait_on_run impl_1
# -----------------------------------------------------------------------------

set script_dir [file normalize [file dirname [info script]]]
set prj_dir    [file join $script_dir prj]
set bd_file    [file join $script_dir srcs sources_1 bd system system.bd]

create_project yolo_gauge $prj_dir -part xc7z020clg400-2 -force

# ---- IP repositories --------------------------------------------------------
set_property ip_repo_paths [list \
    [file normalize [file join $script_dir .. hls ip]] \
    [file normalize [file join $script_dir ip_repo]] ] [current_project]
update_ip_catalog -rebuild

# ---- constraints ------------------------------------------------------------
add_files -fileset constrs_1 -norecurse [file join $script_dir srcs constrs_1 new system_wrapper.xdc]
add_files -fileset constrs_1 -norecurse [file join $script_dir srcs constrs_1 new system_wrapper1.xdc]

# ---- block design -----------------------------------------------------------
add_files -norecurse $bd_file
open_bd_design [get_files *.bd]
validate_bd_design
save_bd_design
generate_target all [get_files *.bd]
make_wrapper -files [get_files *.bd] -top -import

puts "INFO: project created at $prj_dir"
puts "INFO: next -> launch_runs impl_1 -to_step write_bitstream ; wait_on_run impl_1"
