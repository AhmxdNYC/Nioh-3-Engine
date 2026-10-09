#!/bin/sh
# isastat.sh FILE.isa : scalar-reload and wait counts
f=$1
echo "$(basename $f): sbufload $(grep -c s_buffer_load $f) sload $(grep -c 's_load_b' $f) lgkm0 $(grep -c -E 'lgkmcnt\(0\)|s_wait_kmcnt 0x0$|s_wait_kmcnt 0$|s_wait_kmcnt +0x0 ' $f) samples $(grep -c image_sample $f) vmwaits $(grep -c -E 'vmcnt\(|s_wait_samplecnt|s_wait_loadcnt' $f) valu $(grep -c -E '^\s+v_' $f) vgpr $(grep -o -m1 'vgpr_count([0-9]*)' $f)"
