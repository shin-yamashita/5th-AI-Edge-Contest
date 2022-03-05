
# FPGA sources

FPGA に実装した accelerator の RTL ソースである。  

Vivado/2020.2 Webpack で論理シミュレーション、論理合成を行った。 

## simulation 実行
$ cd sim  
$ sh compile_tfacc_core.sh  
$ sh run_tfacc_core.sh 0 5   # 0 番目から 5 番目までの test vector を shimulation 実行   
結果は xsim-r.log に

## synthesis 実行
$ cd syn  
$ sh build.sh  
生成物は、./rev/design_1_wrapper.bit  
design_1.bit に rename して用いる  

## files
```
tfacc_i8/
├── README.md
├── bd                           PL block design (user clock = 125MHz)
├── doc
├── firm
│   ├── rvmon                   rv32 firmware
│   │   ├── include
│   │   ├── lib                mini stdio etc library
│   │   ├── pre_data.c         tracking algorithm
│   │   └── rvmon.c            monitor program
│   └── term/                   debug serial terminal
├── hdl                         FPGA RTL sources
│   ├── acc                     Accelerator sources (SystemVerilog)
│   │   ├── i8mac.sv            int8 MAC
│   │   ├── input_arb.sv        input access arbiter
│   │   ├── input_cache.sv
│   │   ├── logic_types.svh
│   │   ├── output_arb.sv       output access arbiter
│   │   ├── output_cache.sv
│   │   ├── rd_cache_nk.sv      filter/bias/quant buffer
│   │   ├── rv_axi_port.sv      rv32 axi access port
│   │   ├── tfacc_core.sv       Accelerator block top
│   │   └── u8adrgen.sv         Conv2d/dwConv2d address generator
│   ├── rv32_core               rv32emc  Controller sources (SystemVerilog)
│   │   ├── dpram.sv            insn/data dualport memory
│   │   ├── dpram_h.v
│   │   ├── rv_mem.sv
│   │   ├── pkg_rv_decode.sv
│   │   ├── rv_dec_insn.sv      INSN table
│   │   ├── rv_exp_cinsn.sv     C-INSN table
│   │   ├── rv32_core.sv        cpu/memory wrapper
│   │   ├── rv_core.sv          processor core
│   │   ├── rv_alu.sv           ALU
│   │   ├── rv_muldiv.sv        mul/div
│   │   ├── rv_regf.sv          register file 16x32
│   │   ├── rv_sio.sv           debug serial terminal
│   │   └── rv_types.svh
│   ├── tfacc_cpu_v1_0.v        Controller top design      
│   └── tfacc_memif.sv          Data pass top design
├── ip/                         FPGA ip (axi/bram)
├── sim/                        Vivado simulation environment
│ ├── compile_tfacc_core.sh     Elaborate testbench  
│ ├── run_tfacc_core.sh         Execute logic simulation
│ ├── xsim_tfacc_core.sh        Execute logic simulation (GUI)
│ ├── tb_tfacc_core.prj
│ ├── tb_tfacc_core.sv          Testbench
│ ├── axi_slave_bfm.sv          AXI bus functiol model with dpi-c interface
│ ├── c_main.c                  dpi-c source
│ └── tvec/                      test vectors
│       ├── tdump-0-i8.in

│       ├── tdump-70-i8.in
│       └── tdump-71-i8.in
└── syn                         Vivado synthesis environment
    ├── rev/                    report output dir
    ├── build.sh                build FPGA script
    ├── build.tcl  
    ├── design_1_bd.tcl
    ├── dont_touch.xdc
    ├── read_hdl.tcl
    ├── read_ip.tcl
    ├── tfacc_pin.xdc
    └── timing.xdc
```

