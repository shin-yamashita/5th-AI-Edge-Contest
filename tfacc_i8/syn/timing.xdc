
set_input_delay -clock [get_clocks clk_pl_0] -min -add_delay 5.000 [get_ports TXD_0]
set_input_delay -clock [get_clocks clk_pl_0] -max -add_delay 5.000 [get_ports TXD_0]
set_output_delay -clock [get_clocks clk_pl_0] -min -add_delay 0.000 [get_ports RXD_0]
set_output_delay -clock [get_clocks clk_pl_0] -max -add_delay 0.000 [get_ports RXD_0]
#set_max_delay -from [get_clocks clk_pl_0] -to [get_ports {{fp_0[*]} {pout_0[*]} ]

set_multicycle_path 2 -setup -from [get_pins design_1_i/tfacc_cpu_v1_0_0/inst/u_sr_core/u_sr_sio/txd_reg/C] -to [get_ports RXD_0]
set_multicycle_path 1 -hold -from [get_pins design_1_i/tfacc_cpu_v1_0_0/inst/u_sr_core/u_sr_sio/txd_reg/C] -to [get_ports RXD_0]

set_multicycle_path 2 -setup -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/u_u8adrgen/in_offs_reg[*]/C]
set_multicycle_path 1 -hold -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/u_u8adrgen/in_offs_reg[*]/C]
#set_multicycle_path 2 -setup -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/u_u8adrgen/fil_offs_reg[*]/C]
#set_multicycle_path 1 -hold -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/u_u8adrgen/fil_offs_reg[*]/C]
set_multicycle_path 2 -setup -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/u_u8adrgen/out_offs_reg[*]/C]
set_multicycle_path 1 -hold -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/u_u8adrgen/out_offs_reg[*]/C]
set_multicycle_path 2 -setup -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/quant_reg[*]/C]
set_multicycle_path 1 -hold -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/quant_reg[*]/C]
#set_multicycle_path 2 -setup -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/u_u8adrgen/out_shift_reg[*]*/C] 
#set_multicycle_path 1 -hold -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/u_u8adrgen/out_shift_reg[*]*/C]

#set_multicycle_path 2 -setup -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/u_u8adrgen/actmin_reg[*]/C]
#set_multicycle_path 1 -hold -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/u_u8adrgen/actmin_reg[*]/C]
#set_multicycle_path 2 -setup -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/u_u8adrgen/actmax_reg[*]/C]
#set_multicycle_path 1 -hold -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/u_u8adrgen/actmax_reg[*]/C]

set_multicycle_path 2 -setup -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/baseadr_reg[*]/C]
set_multicycle_path 1 -hold -from [get_pins design_1_i/tfacc_memif_0/inst/u_tfacc_core/baseadr_reg[*]/C]

set_multicycle_path -setup -from [get_clocks clk_pl_0] -through [get_nets -hierarchical {*rwdatx[*]*}] -to [get_clocks clk_pl_0] 2
set_multicycle_path -hold -from [get_clocks clk_pl_0] -through [get_nets -hierarchical {*rwdatx[*]*}] -to [get_clocks clk_pl_0] 1

