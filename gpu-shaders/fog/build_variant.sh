#!/bin/sh
# usage: build_variant.sh TAG [mkfog flags...]  -> gi/out/vf235_TAG.{hlsl,dxil,ll} + RGA line
cd "$(dirname "$0")"
TAG=$1; shift
py -I mkfog.py "$@" >/dev/null || exit 1
cd ../gi && py -c "
import mkfast
p=open('../fog/out/pins_plain.txt').read().strip()
mkfast.build('vf235', precise=p, tag='$TAG')
" && py -I rga_stats.py $TAG=out/vf235_$TAG.dxil --out ../fog/out/rga 2>&1 | grep "| $TAG"
