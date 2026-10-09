#!/bin/sh
# cc.sh NAME SRC.hlsl [extra dxc args] : compile like mkfast (cs_6_0 -O3 val1.5 -Zs) -> build/NAME.dxil + .ll, then RGA gfx1102,gfx1201
D="$(dirname "$0")"; N=$1; S=$2; shift 2
DXC="$D/../../tools/rdts/RadeonDeveloperToolSuite-2026-05-28-1806/utils/dx12/dxc/dxc.exe"
mkdir -p "$D/build"
"$DXC" -T cs_6_0 -E main -O3 -validator-version 1.5 -Zs -Fd "$D/build/" -Fo "$D/build/$N.dxil" "$S" "$@" || exit 1
"$DXC" -dumpbin "$D/build/$N.dxil" > "$D/build/$N.ll"
py -I "$D/../gi/rga_stats.py" "$N=$D/build/$N.dxil" --out "$D/rga" --chips ${CHIPS:-gfx1102,gfx1201} < /dev/null | tail -n +3
