# ------------------------------------------------------------------
# Drive Vivado GUI: open project, run behavioral sim, show key waves
# usage: vivado.bat -mode gui -source run_gui_sim.tcl -log vivado_gui.log
# ------------------------------------------------------------------
set sim_dir [file normalize [file dirname [info script]]]
set hdl_dir [file normalize [file join $sim_dir ".." ".." "hls" "ip" "conv640" "hdl" "verilog"]]
set prj_dir [file join $sim_dir prj_gui]
set xpr     [file join $prj_dir sim_prj.xpr]

if {[file exists $xpr]} {
    puts "INFO: opening existing project $xpr"
    open_project $xpr
} else {
    puts "INFO: creating project $xpr"
    create_project -force sim_prj $prj_dir -part xc7z020clg400-2
    set_property target_language Verilog [current_project]
    add_files -norecurse -fileset sources_1 [glob -nocomplain [file join $hdl_dir *.v]]
    set_property file_type Verilog [get_files -of_objects [get_filesets sources_1]]
    set_property top conv [get_filesets sources_1]
    update_compile_order -fileset sources_1
    add_files -norecurse -fileset sim_1 [list [file join $sim_dir axi4_slave_ram.v] [file join $sim_dir tb_conv_pwconv.v]]
    set_property file_type Verilog [get_files -of_objects [get_filesets sim_1]]
    set_property top tb_conv_pwconv [get_filesets sim_1]
    update_compile_order -fileset sim_1
}
set_property top tb_conv_pwconv [get_filesets sim_1]
set_property -name {xsim.elaborate.xelab.more_options} -value {-debug typical} -objects [get_filesets sim_1]
set_property -name {xsim.simulate.runtime} -value {all} -objects [get_filesets sim_1]

puts "INFO: sources = [llength [get_files -of_objects [get_filesets sources_1]]]  sim files = [llength [get_files -of_objects [get_filesets sim_1]]]"
puts "INFO: launching behavioral simulation ..."
flush stdout
if {[catch {launch_simulation} err]} {
    puts "ERROR: launch_simulation failed: $err"
    flush stdout
} else {
    puts "INFO: simulation session = [current_sim]"
    flush stdout
}

# ---- curated waveform ----
if {[catch {
    create_wave_config -name tb_wave
    add_wave [list \
        /tb_conv_pwconv/ap_clk \
        /tb_conv_pwconv/ap_rst_n \
        /tb_conv_pwconv/ap_start \
        /tb_conv_pwconv/ap_done \
        /tb_conv_pwconv/ap_idle \
        /tb_conv_pwconv/ap_ready ]

    add_wave [list \
        /tb_conv_pwconv/s_axi_CTRL_AWVALID \
        /tb_conv_pwconv/s_axi_CTRL_AWREADY \
        /tb_conv_pwconv/s_axi_CTRL_AWADDR \
        /tb_conv_pwconv/s_axi_CTRL_WVALID \
        /tb_conv_pwconv/s_axi_CTRL_WDATA \
        /tb_conv_pwconv/s_axi_CTRL_BVALID ]

    add_wave [list \
        /tb_conv_pwconv/s_axi_control_AWVALID \
        /tb_conv_pwconv/s_axi_control_AWADDR \
        /tb_conv_pwconv/s_axi_control_WDATA ]

    add_wave [list \
        /tb_conv_pwconv/w_FM1_ARVALID \
        /tb_conv_pwconv/w_FM1_ARREADY \
        /tb_conv_pwconv/w_FM1_ARADDR \
        /tb_conv_pwconv/w_FM1_RVALID \
        /tb_conv_pwconv/w_FM1_RREADY \
        /tb_conv_pwconv/w_FM1_RDATA ]

    add_wave [list \
        /tb_conv_pwconv/w_FM1_AWVALID \
        /tb_conv_pwconv/w_FM1_AWADDR \
        /tb_conv_pwconv/w_FM1_WVALID \
        /tb_conv_pwconv/w_FM1_WDATA \
        /tb_conv_pwconv/w_FM1_BVALID ]

    puts "INFO: wave config tb_wave ready"
} werr]} {
    puts "WARN: wave setup skipped: $werr"
}
catch {zoom_fit}
flush stdout
puts "INFO: GUI ready, script done"
