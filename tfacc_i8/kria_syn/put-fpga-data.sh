#!/bin/sh

MOT=../firm/rvmon/rvmon.mot
HWH=bd125M/bd125M.gen/sources_1/bd/design_1/hw_handoff/design_1.hwh
BIT=rev/design_1_wrapper.bit

echo $MOT
echo $HWH
echo $BIT

scp $MOT kria.local:fpga-data/
#scp $HWH kria.local:fpga-data/
scp $BIT kria.local:fpga-data/design_1.bit

