#!/usr/bin/env bash
# ally_shaders.sh - status / install / exact revert of the staged Nioh 3 shader swaps on Linux (Bazzite / SteamOS,
# Proton + vkd3d-proton). Same files and SHA-256 checks as gpu-shaders\apply_{ghdm,esm,wdo28,sweep}.ps1 on Windows.
#
#   ./ally_shaders.sh status            [ghdm|esm|wdo28|mpc|mshlod|toonedge2|gi|fog|all]
#   ./ally_shaders.sh apply             [ghdm|esm|wdo28|mpc|mshlod|toonedge2|gi|fog|all]
#   ./ally_shaders.sh revert            [ghdm|esm|wdo28|mpc|mshlod|toonedge2|gi|fog|all]
#
# mpc / toonedge2 = the 2026-10-07 shader sweep (notes/gpu-shader-sweep.md); toonedge2 covers two files.
# mpc = ONE combined 0x29ee0902.file: MPC#23 + the prefix-sum chain #28-#31 (notes/gpu-culling-rewrite.md). It
# supersedes the earlier mpc23-only file (5th column): that one is recognised and upgraded / reverted like ours.
# mshlod = 0x8d9282e6.file: MshLod#3 (meshlet LOD chain) pad loop spread over the lanes (notes/gpu-culling-rewrite.md).
# gi = 0xd070067d.file: RefProbe#121 probe-GI gather (notes/gpu-gi-rewrite.md).
# fog = 0xa9910db8.file: VFSCAT#235 volumetric-fog scattering latency rewrite (notes/gpu-fog-rewrite.md).
#
# Put this script next to the folder "replacement/" (gpu-shaders/replacement/*.file from the build PC).
# Game folder: auto-detected from the Steam library folders, or set NIOH3_DIR=/path/to/steamapps/common/Nioh3.
# Originals are backed up once to ${BACKUP_DIR:-$HOME/.local/share/nioh3-shader-backup}/ (SHA-256 checked) before the
# first overwrite; revert restores that exact file. Steam "Verify integrity of game files" also restores everything.
# Refuses while Nioh3.exe is running (the files are read at startup / PSO preload), and refuses to touch a file that is
# neither the known original nor the known replacement (= game updated: rebuild the replacement first).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPL_DIR="${REPL_DIR:-$HERE/replacement}"
BACKUP_DIR="${BACKUP_DIR:-$HOME/.local/share/nioh3-shader-backup}"

# name  file              original sha256 (Steam v2.02)                                       replacement sha256   [superseded replacement sha256s, comma-separated]
TABLE="
ghdm  0x0104840c.file  32151bd9d09f21dd995fa1ccfd23b8fd0bd7d5e02463740641b013db2675bb87  129f03a0971e0a073b346224588e2ec0dbc41a4240e1a42efd285693e5742ce9
esm   0x1ca8bb39.file  22103ae7d838771f4aa8e9506855c2dc04820feece73fe7dbcacddecfe56dd54  3c34641f1fb8d53bcac7111b66b2212f7a9ff1e7378d736626bebdf7f42bc73a
wdo28 0x1fd07cc0.file  197f0c438e05b68cf89d4f3f03e20c0a903bcffa6198a93a43c938e960d28ab3  a38f7de7522c4c28d75e458b4a8f6ba9c7015731365a313d2c745a83607536f9
mpc   0x29ee0902.file  42f045c370e99b80626c64e9d8084a4a855bb3baad3b04b8f634114d34b97b16  3947f3d70264920e8cf0514139b715ce6552c6389b0e364ca9fb443cca4f0d73  26417c0ff41b128443ce364a69e68328588abd5f044129fe1a8c2e8dea80cec7
mshlod 0x8d9282e6.file  ec0f5283660caa64565e17cfb8f2dc4d06ee498278fe1a832db16d27b916ebeb  214ac371d68ff4e258526bd6f1d7cd044347bc8c417410966ae6400a02986c24
toonedge2 0x9c481f79.file  7a8171617b2530648e12ce9dd52c091faf31cd4f1026d3b40a7fdde5788ac1ae  d672a5d530f9d1cb49f37ee3de6f626d95e7273eb897e17de3683a8dfb2c2e6d
toonedge2 0x47bc5cb4.file  07542a8322a617b7783b0c7b79580689339429c1d1e416bf70c8d0f3b0aee99a  9a37b6e45360bb8664746687757c68ebfd33cdeb711a6459bbbe29cee111ef80
gi    0xd070067d.file  d3cbb484ee3380d337641c8c893aa152f2c8d053afb37f49091609b1d87dc372  c952f39a76bc09668953b1bc483e1434f1cbe121bfe9e907d6f49fc601e0fb97
fog   0xa9910db8.file  4784aabe40fdb10646babde1df734123ce713dc56089a55b4efa0db43bf4f993  ae04ee6ac2920c62cac9d694428f8105e42c45ef9c34b13426dead6d97190e6d
"

die() { echo "error: $*" >&2; exit 1; }
sha() { sha256sum "$1" | cut -d' ' -f1; }

find_game() {
  if [ -n "${NIOH3_DIR:-}" ]; then echo "$NIOH3_DIR"; return; fi
  local libs=() vdf
  for vdf in "$HOME/.local/share/Steam/steamapps/libraryfolders.vdf" "$HOME/.steam/steam/steamapps/libraryfolders.vdf" \
             "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/libraryfolders.vdf"; do
    [ -f "$vdf" ] || continue
    while IFS= read -r p; do libs+=("$p"); done < <(sed -n 's/^[[:space:]]*"path"[[:space:]]*"\(.*\)"/\1/p' "$vdf")
  done
  libs+=("$HOME/.local/share/Steam" "$HOME/.steam/steam")
  local l
  for l in "${libs[@]}"; do
    if [ -d "$l/steamapps/common/Nioh3/shader_dx12" ]; then echo "$l/steamapps/common/Nioh3"; return; fi
  done
  die "Nioh3 not found in the Steam libraries; set NIOH3_DIR=/path/to/steamapps/common/Nioh3"
}

[ $# -ge 1 ] || { sed -n '2,17p' "$0"; exit 1; }
CMD="$1"; WHICH="${2:-all}"
case "$CMD" in status|apply|revert) ;; *) die "unknown command '$CMD' (status|apply|revert)";; esac
GAME="$(find_game)"; SD="$GAME/shader_dx12"
[ -d "$SD" ] || die "no shader_dx12 in $GAME"
if [ "$CMD" != status ] && pgrep -fi 'nioh3\.exe' >/dev/null 2>&1; then die "Nioh3 is running - close the game first"; fi
mkdir -p "$BACKUP_DIR"

# pass 1 checks every selected file (nothing is touched if any check fails); pass 2 acts
for PASS in check act; do
any=0
while read -r name file orig repl old; do
  [ -n "$name" ] || continue
  [ "$WHICH" = all ] || [ "$WHICH" = "$name" ] || continue
  any=1
  g="$SD/$file"; [ -f "$g" ] || die "$name: missing $g"
  hg="$(sha "$g")"
  case "$hg" in
    "$orig") state=ORIGINAL;; "$repl") state=REPLACEMENT;; *) state="UNKNOWN($hg)";;
  esac
  case ",${old:-}," in *",$hg,"*) state=SUPERSEDED;; esac   # an older build of our replacement for this file
  b="$BACKUP_DIR/$file"; r="$REPL_DIR/$file"
  if [ "$CMD" = status ]; then [ "$PASS" = act ] && printf '%-10s %-16s %s\n' "$name" "$file" "$state"; continue; fi
  case "$state" in UNKNOWN*) die "$name: $file is neither the v2.02 original nor our replacement - game updated? not touching anything";; esac
  if [ "$PASS" = check ]; then
    if [ "$CMD" = apply ]; then
      [ "$state" = REPLACEMENT ] && continue
      [ -f "$r" ] || die "$name: replacement file missing: $r"
      [ "$(sha "$r")" = "$repl" ] || die "$name: $r has the wrong sha256 (corrupt copy?)"
      if [ "$state" = SUPERSEDED ] && ! { [ -f "$b" ] && [ "$(sha "$b")" = "$orig" ]; }; then die "$name: older replacement installed but no valid backup in $BACKUP_DIR - use Steam 'Verify integrity of game files' first"; fi
      if [ -f "$b" ] && [ "$(sha "$b")" != "$orig" ]; then die "$name: backup $b is not the original - remove it and retry"; fi
    else
      [ "$state" = ORIGINAL ] && continue
      [ -f "$b" ] && [ "$(sha "$b")" = "$orig" ] || die "$name: no valid backup of $file in $BACKUP_DIR - use Steam 'Verify integrity of game files'"
    fi
    continue
  fi
  if [ "$CMD" = apply ]; then
    if [ "$state" = REPLACEMENT ]; then echo "$name: $file already installed"; continue; fi
    if [ ! -f "$b" ]; then cp -p "$g" "$b"; fi
    [ "$(sha "$b")" = "$orig" ] || die "$name: backup $b is not the original - remove it and retry"
    cp "$r" "$g"; [ "$(sha "$g")" = "$repl" ] || die "$name: copy failed"
    echo "$name: $file installed (backup: $b)"
  else
    if [ "$state" = ORIGINAL ]; then echo "$name: $file already original"; continue; fi
    cp "$b" "$g"; [ "$(sha "$g")" = "$orig" ] || die "$name: restore failed"
    echo "$name: $file reverted to the original"
  fi
done <<< "$TABLE"
done
[ "$any" = 1 ] || die "unknown shader '$WHICH' (ghdm|esm|wdo28|mpc|mshlod|toonedge2|gi|fog|all)"
