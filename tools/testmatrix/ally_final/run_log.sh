#!/usr/bin/env bash
# run_log.sh - MangoHud capture driver for the Nioh 3 final benchmark (Ally). Writes ONE CSV per window with run tags.
# Game-side only: never touches TDP / governor / env vars / gamescope. MangoHud is the Steam launch option you already use
# (`mangohud %command%`); this script only installs the logging config and files the CSVs it produces.
#
#   ./run_log.sh install                       copy mangohud_nioh3.conf to ~/.config/MangoHud/ (backups kept)
#   ./run_log.sh tag RUNID [VARIANT] [PRESET]  set the current tags. VARIANT = STOCK|MOD|MOD_FG|free text; PRESET = LOW|MID|HIGH|MAX|USER
#   ./run_log.sh window KIND [LABEL]           wait for the next MangoHud CSV and file it as one window. KIND = stand|move|combat|load|lat
#   ./run_log.sh run [combat]                  stand, stand, move (+ combat) windows in a row for the current RUNID
#   ./run_log.sh list                          show filed windows
# A window: call `window`, then in game press Shift_L+F2 (or its Steam Input mapping) to START logging and again to STOP
# (log_duration=40 also stops it). The script sees the new CSV, waits until it stops growing, trims it to the KIND's nominal
# length, writes a .meta sidecar and moves it to runs/.
# Env: NIOH3_DIR=<game folder> (for the mod ini hash + mod log stats line), RUNS_DIR, MH_SPOOL.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNS="${RUNS_DIR:-$HERE/runs}"; STATE="$HERE/.state"; SPOOL="${MH_SPOOL:-$HOME/nioh3_mh_spool}"
mkdir -p "$RUNS" "$SPOOL"
declare -A DUR=([stand]=20 [move]=20 [combat]=25 [load]=240 [lat]=20)

cmd="${1:-}"; shift || true
case "$cmd" in
install)
  for n in wine-Nioh3.conf Nioh3.conf; do
    d="$HOME/.config/MangoHud/$n"; mkdir -p "$(dirname "$d")"
    if [ -f "$d" ] && [ ! -f "$d.bak_nioh3" ]; then cp "$d" "$d.bak_nioh3"; fi
    sed "s|__OUT__|$SPOOL|" "$HERE/mangohud_nioh3.conf" > "$d"; echo "installed $d"
  done
  echo "CSVs land in $SPOOL, then are filed into $RUNS. Check the overlay shows in game (else name the conf after the process: ls ~/.config/MangoHud)."
  ;;
tag)
  { echo "RUNID=${1:?runid}"; echo "VARIANT=${2:-MOD}"; echo "PRESET=${3:-USER}"; } > "$STATE"; cat "$STATE" ;;
window)
  kind="${1:?kind}"; label="${2:-}"
  [ -f "$STATE" ] || { echo "run ./run_log.sh tag RUNID first" >&2; exit 1; }
  . "$STATE"
  n=$(ls "$RUNS" 2>/dev/null | grep -c "^ally_${RUNID}__${kind}__.*\.csv$" || true); n=$((n+1))
  mark="$(mktemp "$SPOOL/.mark.XXXX")"; echo "waiting for a new MangoHud CSV in $SPOOL (start logging in game) ..."
  new=""
  for _ in $(seq 1 900); do
    new="$(find "$SPOOL" -maxdepth 1 -name '*.csv' -newer "$mark" | head -1)"
    [ -n "$new" ] && break; sleep 1
  done
  rm -f "$mark"; [ -n "$new" ] || { echo "timeout: no CSV" >&2; exit 2; }
  last=-1; while :; do s=$(stat -c %s "$new"); [ "$s" = "$last" ] && break; last=$s; sleep 3; done
  base="ally_${RUNID}__${kind}__${n}__${PRESET}__${VARIANT}"
  python3 - "$new" "$RUNS/$base.csv" "${DUR[$kind]:-20}" <<'PY'
import sys
src, dst, dur = sys.argv[1], sys.argv[2], float(sys.argv[3])
L = open(src, encoding="utf-8-sig").read().splitlines()
# MangoHud: 2 system-info lines, then the column header, then rows. 'elapsed' is ns since log start.
hi = next(i for i, l in enumerate(L) if l.startswith("fps,"))
cols = L[hi].split(","); ei = cols.index("elapsed")
out = L[:hi + 1]
for l in L[hi + 1:]:
    p = l.split(",")
    if len(p) == len(cols) and float(p[ei]) / 1e9 <= dur:
        out.append(l)
open(dst, "w").write("\n".join(out) + "\n")
print(len(out) - hi - 1, "frames kept")
PY
  rm -f "$new"
  gl=""; if [ -n "${NIOH3_DIR:-}" ] && [ -f "$NIOH3_DIR/Nioh3EngineFix.log" ]; then gl="$NIOH3_DIR/Nioh3EngineFix.log"; fi
  {
    echo "run_id=$RUNID"; echo "kind=$kind"; echo "n=$n"; echo "variant=$VARIANT"; echo "preset_tag=$PRESET"; echo "label=$label"
    echo "when=$(date -Iseconds)"
    echo "config_xml=$("$HERE/cfgswitch.sh" show 2>/dev/null | tr '\n' ' ')"
    if [ -n "${NIOH3_DIR:-}" ] && [ -f "$NIOH3_DIR/Nioh3EngineFix.ini" ]; then echo "mod_ini_sha=$(sha256sum "$NIOH3_DIR/Nioh3EngineFix.ini" | cut -c1-12)"; fi
    if [ -n "$gl" ]; then echo "mod_log_stats=$(grep -E 'stats[ /]' "$gl" | tail -1 | cut -c1-200)"; fi
  } > "$RUNS/$base.meta"
  echo "filed $RUNS/$base.csv" ;;
run)
  for k in stand stand move "$@"; do "$0" window "$k"; done ;;
list) ls -1 "$RUNS" | grep 'csv$' || true ;;
*) sed -n 2,14p "$0"; exit 1 ;;
esac
