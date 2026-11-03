set_property PACKAGE_PIN J20 [get_ports {TMDS_0_tmds_data_p[2]}]
set_property PACKAGE_PIN K19 [get_ports {TMDS_0_tmds_data_p[1]}]
set_property PACKAGE_PIN G19 [get_ports {TMDS_0_tmds_data_p[0]}]
set_property PACKAGE_PIN J18 [get_ports TMDS_0_tmds_clk_p]

set_operating_conditions -grade extended

# --- OpenMV serial link (PS UART1 via EMIO) ---
set_property -dict {PACKAGE_PIN T19 IOSTANDARD LVCMOS33} [get_ports UART1_RX_0]
set_property -dict {PACKAGE_PIN J15 IOSTANDARD LVCMOS33} [get_ports UART1_TX_0]
