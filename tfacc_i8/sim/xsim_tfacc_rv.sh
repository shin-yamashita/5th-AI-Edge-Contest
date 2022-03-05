#!/bin/bash

#source /opt/Xilinx/Vivado/2019.2/settings64.sh
source /opt/Xilinx/Vivado/2020.2/settings64.sh

stage=0
endst=0
if [ $# -ge 1 ]; then
  stage=$1
  endst=$2
fi
#echo $stage $endst > stage.in

xelab tb_tfacc_rv glbl -timescale 1ns/1ns -prj tb_tfacc_rv.prj -L unisims_ver -s tb_tfacc_rv_g -sv_lib dpi -debug typical
xsc c_main.c

xsim -g tb_tfacc_rv_g -testplusarg b=$stage -testplusarg e=$endst --view tb_tfacc_rv_g.wcfg --log xsim-g.log
