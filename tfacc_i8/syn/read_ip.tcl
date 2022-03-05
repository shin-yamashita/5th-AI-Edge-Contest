
# enable ip container *.xci --> *.xcix

set ips [list rdbuf32k rdbuf16k rdbuf8k rdbuf4k rdbuf2k axi_ic dpram32kB axi_apb_bridge_0 rv32_core_0 ila_0 ]

foreach ip $ips {
        puts "****** read and compile ip ($ip)"
        read_ip ../ip/$ip.xcix
#       generate_target -force {Synthesis} [get_ips $ip ]
#       synth_ip -force [get_ips $ip ]
        generate_target {Synthesis} [get_ips $ip ]
#        synth_ip [get_ips $ip ]
 }


read_ip -quiet ../bd/design_1/ip/design_1_auto_ds_0/design_1_auto_ds_0.xci
read_ip -quiet ../bd/design_1/ip/design_1_auto_pc_0/design_1_auto_pc_0.xci
read_ip -quiet ../bd/design_1/ip/design_1_ps8_0_axi_periph_1/design_1_ps8_0_axi_periph_1.xci
read_ip -quiet ../bd/design_1/ip/design_1_rst_ps8_0_100M_0/design_1_rst_ps8_0_100M_0.xci
read_ip -quiet ../bd/design_1/ip/design_1_smartconnect_0_0/design_1_smartconnect_0_0.xci
read_ip -quiet ../bd/design_1/ip/design_1_tfacc_cpu_v1_0_0_0/design_1_tfacc_cpu_v1_0_0_0.xci
read_ip -quiet ../bd/design_1/ip/design_1_tfacc_memif_0_0/design_1_tfacc_memif_0_0.xci
read_ip -quiet ../bd/design_1/ip/design_1_zynq_ultra_ps_e_0_0/design_1_zynq_ultra_ps_e_0_0.xci



