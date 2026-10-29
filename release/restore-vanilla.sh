#!/bin/sh
# Run from the Nioh 3 folder (or pass it). Turns the mod off. Does not touch saves or ini files.
# Same as "Restore Vanilla Game.cmd". "put-mod-back.sh" turns the mod back on.
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
  echo "This turns the mod off so Nioh 3 starts as the normal game."
  echo "Saves, ini settings, the updater, and the website shortcut stay."
  printf "Turn the mod off now? y or n: "
  read -r answer
  case "$answer" in y|Y|yes|YES) ;; *) echo "Cancelled. Nothing was changed."; exit 0;; esac
fi
moved=0
for f in dinput8.dll Nioh3EngineFix.asi Nioh3BossRematch.asi Nioh3Training.asi Nioh3CamKiProbe.asi; do
  [ -e "$f" ] || continue
  if [ -e "$f.mod-off" ]; then echo "Left $f in place because $f.mod-off is already there."; exit 1; fi
  mv "$f" "$f.mod-off" || { echo "Could not rename $f. Close the game and try again."; exit 1; }
  moved=1
done
if [ "$moved" = 0 ]; then
  echo "The mod is already off. Run put-mod-back.sh to turn it on."
  exit 0
fi
echo "The mod is off. Start Nioh 3 from Steam for the normal game."
echo "You can also remove WINEDLLOVERRIDES=\"dinput8=n,b\" from the Steam launch options."
echo "Saves were not touched. Run put-mod-back.sh to turn the mod on again."
exit 0
