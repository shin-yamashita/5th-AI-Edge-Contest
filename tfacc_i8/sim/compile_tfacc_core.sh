#!/bin/bash

#source /opt/Xilinx/Vivado/2019.2/settings64.sh
source /opt/Xilinx/Vivado/2020.2/settings64.sh

#stage=0

#if [ $# -ge 1 ]; then
#  stage=$1
#fi

xelab -timescale 1ns/1ns tb_tfacc_core glbl -L unisims_ver -dpiheader dpi.h
xsc c_main.c

#xelab work.tb_tfacc_core -generic_top "stage=${stage}" work.glbl -timescale 1ns/1ns -prj tb_tfacc_core.prj -L unisims_ver -L secureip -s tb_tfacc_core -sv_lib dpi -debug typical
xelab tb_tfacc_core glbl -timescale 1ns/1ns -prj tb_tfacc_core.prj -L unisims_ver -sv_lib dpi -s tb_tfacc_core -debug typical 


#xelab -wto 9b7dd6a7f33846378683c7cc4c3b80e3 --incr --debug typical --relax --mt 8 -L xil_defaultlib -L uvm -L unisims_ver -L unimacro_ver -L secureip --snapshot tb_tfacc_core_behav xil_defaultlib.tb_tfacc_core xil_defaultlib.glbl -log elaborate.log

