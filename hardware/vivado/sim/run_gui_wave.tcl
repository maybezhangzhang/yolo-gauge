# ------------------------------------------------------------------
# Open the finished simulation WDB in Vivado GUI (wave-database mode)
# usage: vivado.bat -mode gui -source run_gui_wave.tcl -log vivado_wave.log
# ------------------------------------------------------------------
set wdb "E:/final_entry/hardware/vivado/sim/tb_sim.wdb"
puts "INFO: opening wdb = $wdb"
open_wave_database $wdb
puts "INFO: wdb opened"

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

    puts "INFO: waves added"
} werr]} {
    puts "WARN: wave setup: $werr"
}
catch {zoom_fit}
flush stdout

after 6000 {
    catch {current_wave_config tb_wave}
    catch {zoom_fit}
    catch {zoom_fit -view [current_wave_config]}
    puts "INFO: late zoom done"
    flush stdout
}
puts "INFO: GUI ready, script done"




