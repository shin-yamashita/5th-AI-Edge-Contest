#!/bin/bash

function chkerr
{
 if [ $? != 0 ] ; then
  echo "***** error exit ******"
  exit
 fi
}

prj=rv32_core
if [ $# = 1 ] ; then
 mode=$1
fi

source xilinx_env.sh

#run 20us

echo Simulation Tool: Viavdo Simulator $prj $mode

if [ "$mode" = "nodebug" ] ;  then

xelab tb_$prj -timescale 1ns/1ns -prj tb_$prj.prj -i ../hdl -L unisims_ver -s tb_$prj
xsim tb_$prj -R --log xsim-r.log

else

xsim -wdb tb_$prj.wdb tb_$prj <<EOF
run all
exit
EOF

fi



chkerr

echo done

