
# enable ip container *.xci --> *.xcix

set ips [list rdbuf32k rdbuf16k rdbuf8k rdbuf4k rdbuf2k axi_ic dpram32kB axi_apb_bridge_0 rv32_core_0 ila_0 ]

foreach ip $ips {
        puts "****** read and compile ip ($ip)"
        read_ip ../ip/$ip.xcix
#       generate_target -force {Synthesis} [get_ips $ip ]
#       synth_ip -force [get_ips $ip ]
        generate_target {Synthesis simulation} [get_ips $ip ]
        export_ip_user_files -of_objects [get_ips $ip]
 }

set BD bd125M/bd125M.srcs/sources_1/bd/design_1/design_1.bd
set WRAPP bd125M/bd125M.gen/sources_1/bd/design_1/hdl/design_1_wrapper.v

read_bd $BD
#../bd/design_1/design_1.bd
set_property synth_checkpoint_mode None [get_files $BD ]
#../bd/design_1/design_1.bd]
make_wrapper -files [get_files $BD ] -top

read_verilog -library xil_defaultlib {
  bd125M/bd125M.gen/sources_1/bd/design_1/hdl/design_1_wrapper.v
}
generate_target {Synthesis simulation} [get_files $BD ]




