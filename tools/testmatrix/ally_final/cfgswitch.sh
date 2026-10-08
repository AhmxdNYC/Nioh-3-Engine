#!/usr/bin/env bash
# cfgswitch.sh - config.xml preset switcher for Nioh 3 under Proton (backup / restore / presets / single keys).
# Game-side only (edits the game's own settings file). Refuses while Nioh3.exe runs (the game rewrites config.xml on exit).
#   ./cfgswitch.sh path | show | backup [name] | restore [name] | list | preset LOW|MID|HIGH|MAX | set KEY VALUE [KEY VALUE ...] | levels KEY
# Never touched by `preset`: DISP_MODE, SCR_REZ, REN_FPS, VSYNC, DYNAMIC_*, FRAME_GENERATION, RESOLUTION_SCALING, HDR*.
# Fix those once per session by hand (record them in the checklist header); `set` is the only way this script changes them.
# Presets set PRESET=CUSTOM plus every graphics-quality key below. REN_RAT is part of the preset (60 / 75 / 85 / 100).
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; BK="${CFG_BACKUPS:-$HERE/cfg_backups}"; mkdir -p "$BK"
REL="steamapps/compatdata/3681010/pfx/drive_c/users/steamuser/AppData/Local/KoeiTecmo/NIOH3/config.xml"
find_cfg() {
  if [ -n "${NIOH3_CONFIG:-}" ]; then echo "$NIOH3_CONFIG"; return; fi
  local r c
  for r in "$HOME/.local/share/Steam" "$HOME/.steam/steam" "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam"; do
    c="$r/$REL"; if [ -f "$c" ]; then echo "$c"; return; fi
  done
  c="$(find "$HOME" /run/media /var/mnt -maxdepth 12 -path '*compatdata/3681010*' -name config.xml 2>/dev/null | head -1 || true)"
  if [ -n "$c" ]; then echo "$c"; else echo "config.xml not found; set NIOH3_CONFIG=/full/path" >&2; exit 1; fi
}
CFG="$(find_cfg)"
guard() { if pgrep -f 'Nioh3\.exe' >/dev/null 2>&1; then echo "Nioh3.exe is running: close the game first" >&2; exit 1; fi; }
setkey() {
  grep -q "<$1>" "$CFG" || { echo "key $1 missing in config.xml (start the game once so it writes all keys)" >&2; exit 1; }
  sed -i "s|<$1>[^<]*</$1>|<$1>$2</$1>|" "$CFG"
}
# preset table: KEY=LOW,MID,HIGH,MAX
P="REN_RAT=60,75,85,100
SHADOW_QUAL=LIGHT,STANDARD,HIGH,HIGHEST
ANISOTROPIC_FILTER=4,8,16,16
AMBIENT_OCCLUSION=LIGHT,STANDARD,HIGH,HIGHEST
GI=LIGHT,STANDARD,HIGH,HIGHEST
SSR=LIGHT,STANDARD,HIGH,HIGHEST
MODEL_QUAL=LIGHT,STANDARD,HIGH,HIGH
MODEL_TEX_QUAL=STANDARD,HIGH,HIGHEST,HIGHEST
MODEL_DRAW=NORMAL,NORMAL,MANY,MANY
VOLUMETRIC=LIGHT,STANDARD,HIGH,HIGH
WIND_SENSITIVITY=LOW,NORMAL,NORMAL,NORMAL
VISUAL_EFFECT=LIMITED,NORMAL,NORMAL,NORMAL
MOTION_QUALITY=STANDARD,STANDARD,HIGH,HIGH
LANDSCAPE_QUALITY=LIGHT,NORMAL,HIGH,ULTRA
MESHLET_QUALITY=LIGHT,STANDARD,HIGH,HIGH
GRASS_DENSITY=LOW,NORMAL,HIGH,HIGH
POSTEFFECT_RESOLUTION=STANDARD,STANDARD,HIGH,HIGH
MOTION_BLUR=OFF,LIGHT,STANDARD,STANDARD
DEPTH_OF_FIELD=OFF,ON,ON,ON
BLOOM=ON,ON,ON,ON
COLOR_PRECISION=MIDDLE,HIGH,HIGH,HIGH
SSS=OFF,ON,ON,ON"
# value lists for the per-setting sweep (notes/settings.md section 2)
levels() {
  case "$1" in
    GI) echo LIGHTEST LIGHT STANDARD HIGH VERY_HIGH HIGHEST;;
    SHADOW_QUAL) echo LIGHTEST LIGHT STANDARD HIGH HIGHEST;;
    VOLUMETRIC) echo LIGHTEST LIGHT STANDARD HIGH;;
    AMBIENT_OCCLUSION|SSR|MOTION_BLUR) echo OFF LIGHTEST LIGHT STANDARD HIGH HIGHEST;;
    LANDSCAPE_QUALITY) echo LIGHTEST LIGHT NORMAL HIGH ULTRA;;
    GRASS_DENSITY) echo LOWEST LOW NORMAL HIGH;;
    MODEL_DRAW) echo FEW NORMAL MANY;;
    MOTION_QUALITY|MESHLET_QUALITY|MODEL_QUAL) echo LIGHT STANDARD HIGH;;
    VISUAL_EFFECT) echo LIMITED NORMAL;;
    POSTEFFECT_RESOLUTION) echo STANDARD HIGH;;
    WIND_SENSITIVITY) echo OFF LOW NORMAL;;
    ANISOTROPIC_FILTER) echo OFF 2 4 8 16;;
    MODEL_TEX_QUAL) echo LIGHTEST LIGHT STANDARD HIGH HIGHEST;;
    COLOR_PRECISION) echo MIDDLE HIGH;;
    SSS|DEPTH_OF_FIELD|BLOOM|LENS_FLARE|CHROMATIC_ABERRATION|NOISE_FILTER|VIGNETTING|MACH_BAND_NOISE) echo OFF ON;;
    REN_RAT) echo 50 60 65 70 75 80 85 90 100;;
    *) echo "unknown key" >&2; return 1;;
  esac
}
cmd="${1:-}"; shift || true
case "$cmd" in
path) echo "$CFG" ;;
show) grep -oE '<(PRESET|REN_RAT|REN_FPS|SCR_REZ|RESOLUTION_SCALING|FRAME_GENERATION|VSYNC|DYNAMIC_RESOLUTION|SHADOW_QUAL|GI|AMBIENT_OCCLUSION|SSR|VOLUMETRIC|MODEL_DRAW|GRASS_DENSITY|LANDSCAPE_QUALITY|MOTION_BLUR)>[^<]*' "$CFG" | sed 's/^<//;s/>/=/' ;;
backup) n="${1:-$(date +%Y%m%d-%H%M%S)}"; cp -p "$CFG" "$BK/config.$n.xml"; echo "saved $BK/config.$n.xml" ;;
restore)
  guard; n="${1:-}"; f="$BK/config.$n.xml"
  if [ -z "$n" ]; then f="$(ls -1t "$BK"/config.*.xml | head -1)"; fi
  [ -f "$f" ] || { echo "no such backup"; exit 1; }
  cp -p "$CFG" "$BK/config.pre-restore.xml"; cp -p "$f" "$CFG"; echo "restored $f" ;;
list) ls -1t "$BK" ;;
preset)
  guard
  case "${1:?LOW|MID|HIGH|MAX}" in LOW) i=1;; MID) i=2;; HIGH) i=3;; MAX) i=4;; *) echo "bad preset"; exit 1;; esac
  if [ ! -f "$BK/config.original.xml" ]; then cp -p "$CFG" "$BK/config.original.xml"; echo "first use: original saved"; fi
  setkey PRESET CUSTOM
  while IFS='=' read -r k v; do [ -n "$k" ] || continue; setkey "$k" "$(echo "$v" | cut -d, -f$i)"; done <<<"$P"
  echo "preset $1 applied"; "$0" show ;;
set) guard; while [ $# -ge 2 ]; do setkey "$1" "$2"; shift 2; done; setkey PRESET CUSTOM; "$0" show ;;
levels) levels "${1:?KEY}" ;;
*) sed -n 2,6p "$0"; exit 1 ;;
esac
