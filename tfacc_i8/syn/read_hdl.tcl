
add_files ../bd/design_1/design_1.bd

read_verilog -library xil_defaultlib -sv {
  ../hdl/acc/logic_types.svh
}

set_property file_type "Verilog Header" [get_files ../hdl/acc/logic_types.svh]

read_verilog -library xil_defaultlib -sv {
  ../hdl/acc/u8adrgen.sv
  ../hdl/acc/i8mac.sv
  ../hdl/acc/rd_cache_nk.sv
  ../hdl/acc/tfacc_core.sv
  ../hdl/acc/input_cache.sv
  ../hdl/acc/input_arb.sv
  ../hdl/acc/output_cache.sv
  ../hdl/acc/output_arb.sv
  ../hdl/acc/rv_axi_port.sv
  ../hdl/tfacc_memif.sv
}

read_verilog -library xil_defaultlib {
  ../hdl/tfacc_cpu_v1_0.v
  ../bd/design_1/hdl/design_1_wrapper.v
  ../hdl/rv32_core/dpram_h.v  
}

#set_property is_global_include true [get_files ../hdl/acc/logic_types.svh]


