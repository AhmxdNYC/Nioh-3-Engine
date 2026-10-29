#!/bin/sh
# Run from the Nioh 3 folder (or pass it). Turns the mod back on. Does not touch saves or ini files.
# Same as "Put the mod back.cmd".
# A test may set NIOH3_RESTORE_SKIP_RUNNING_CHECK=1. A normal run never does.
ROOT=${1:-$(dirname "$0")}
cd "$ROOT" 2>/dev/null || { echo "Folder not found: $ROOT"; exit 3; }
if [ ! -f Nioh3.exe ]; then
  echo "Put this file in the Nioh 3 folder, next to Nioh3.exe."
  echo "Nothing was changed."
  exit 3
fi
if [ "$NIOH3_RESTORE_SKIP_RUNNING_CHECK" != "1" ] && pgrep -if 'Nioh3\.exe' >/dev/null 2>&1; then
  echo "Close Nioh 3 first. Nothing was changed. Saves were not touched."
  exit 2
fi
if [ "$2" != "--yes" ]; then
  echo "This turns the mod back on."
  echo "Saves and ini settings stay as they are."
  printf "Turn the mod on now? y or n: "
  read -r answer
  case "$answer" in y|Y|yes|YES) ;; *) echo "Cancelled. Nothing was changed."; exit 0;; esac
fi
moved=0
for f in dinput8.dll Nioh3EngineFix.asi Nioh3BossRematch.asi Nioh3Training.asi Nioh3CamKiProbe.asi; do
  [ -e "$f.mod-off" ] || continue
  if [ -e "$f" ]; then echo "Left $f.mod-off because $f is already there."; exit 1; fi
  mv "$f.mod-off" "$f" || { echo "Could not rename $f.mod-off. Close the game and try again."; exit 1; }
  moved=1
done
if [ "$moved" = 0 ]; then
  echo "No parked mod files were found. Nothing was changed."
  exit 0
fi
echo "The mod is back. Start Nioh 3 from Steam."
echo "Keep WINEDLLOVERRIDES=\"dinput8=n,b\" %command% in the Steam launch options."
echo "Saves were not touched."
exit 0
