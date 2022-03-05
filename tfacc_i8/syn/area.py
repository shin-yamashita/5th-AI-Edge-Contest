#!/usr/bin/env python3

insts = []

#      3            4           5       6       7       8        9       10      11
#| Total LUTs | Logic LUTs | LUTRAMs | SRLs |  FFs  | RAMB36 | RAMB18 | URAM | DSP48 Blocks |
#   70560                                     141120   216                       360

with open('rev/post_route_area.rpt') as f:
  for line in f:
    cols = line.split('|')
    if(len(cols) > 3):
      inst = cols[1]
      ik = inst.lstrip().strip()
      if(not 'Instance' in inst):
        mods = cols[2]
        luts = int(cols[3])	#LUTs
#        luts = int(cols[8])	#BRAM36
        if(luts > 0):
          id = (len(inst) - len(inst.lstrip())) // 2
          if(len(insts) < id):
            insts.append(ik)
          else:
            del insts[id:]
            insts.append(ik)
#          print(id,cols[1],cols[3])
          ci = ''
          for i in range(id+1):
            ci += '/'+insts[i]
          print(luts, ci)



