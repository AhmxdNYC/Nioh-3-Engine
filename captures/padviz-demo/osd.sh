#!/bin/bash
# osd.sh LABEL : wait for next OSD stats line (discard: window overlaps menu), then collect next 2 windows (~60 s)
L="/c/Program Files (x86)/Steam/steamapps/common/Nioh3/Nioh3EngineFix.log"
n0=$(grep -c "OSD: stats" "$L")
while [ $(grep -c "OSD: stats" "$L") -lt $((n0+3)) ]; do sleep 2; done
echo "== $1"; grep "OSD: stats" "$L" | tail -2 | sed -E 's/^(\[[^]]*\]) OSD: stats: rendered ([0-9.]+) fps.*draws ([0-9]+).*/\1 rendered \2 fps draws \3/'
