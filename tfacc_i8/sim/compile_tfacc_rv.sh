#!/bin/bash

source xilinx_env.sh

xelab -timescale 1ns/1ns tb_tfacc_rv glbl -L unisims_ver -dpiheader dpi.h
xsc c_main.c

xelab tb_tfacc_rv glbl -timescale 1ns/1ns -prj tb_tfacc_rv.prj -L unisims_ver -sv_lib dpi -s tb_tfacc_rv -debug typical 


