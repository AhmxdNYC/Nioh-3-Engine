#!/bin/sh
# np_variant.sh TAG [mkfog flags]: unpinned build (the RADV view: NIR folds the pins) -> RGA gfx1103 line
cd "$(dirname "$0")"; TAG=$1; shift
py -I mkfog.py "$@" >/dev/null || exit 1
cd ../gi && py -c "
import mkfast; mkfast.build('vf235', precise='', tag='np_$TAG')" && py -I rga_stats.py $TAG=out/vf235_np_$TAG.dxil --chips gfx1103 | grep "| $TAG"
