#!/bin/sh
# Use the game's existing Proton prefix without running Proton's prefix setup.
set -eu
fail() { printf '%s\n' "Update stopped: $*" >&2; exit 1; }
usage() {
    printf '%s\n' 'Usage: sh update-linux.sh --game-dir DIR --proton-dir DIR [--prefix PFX] [--updater EXE] [-- updater options]'
    printf '%s\n' 'Close Nioh 3 first. Prefix defaults to steamapps/compatdata/3681010/pfx in the game library.'
}
game= proton=${PROTON_DIR:-} prefix=${WINEPREFIX:-} updater=
while [ "$#" -gt 0 ]; do
    case "$1" in
        --game-dir|--proton-dir|--prefix|--updater)
            [ "$#" -ge 2 ] || fail "Missing value for $1"
            case "$1" in
                --game-dir) game=$2;; --proton-dir) proton=$2;;
                --prefix) prefix=$2;; --updater) updater=$2;;
            esac
            shift 2;;
        --) shift; break;;
        --help|-h) usage; exit 0;;
        *) fail "Unknown option: $1 (put updater options after --)";;
    esac
done
[ -n "$game" ] && [ -f "$game/Nioh3.exe" ] || fail 'Use --game-dir with the folder containing Nioh3.exe.'
[ -n "$proton" ] && [ -d "$proton" ] || fail 'Use --proton-dir with the Proton version selected for Nioh 3 in Steam.'
game=$(CDPATH= cd -- "$game" && pwd -P)
proton=$(CDPATH= cd -- "$proton" && pwd -P)
if [ -z "$prefix" ]; then
    prefix=$(dirname -- "$(dirname -- "$game")")/compatdata/3681010/pfx
fi
[ -d "$prefix/drive_c" ] && [ -f "$prefix/system.reg" ] || fail 'Existing game prefix not found. Use --prefix /path/to/compatdata/3681010/pfx; launch the game once first.'
prefix=$(CDPATH= cd -- "$prefix" && pwd -P)
wine=$proton/files/bin/wine
[ -x "$wine" ] || wine=$proton/dist/bin/wine
[ -x "$wine" ] || fail 'No files/bin/wine or dist/bin/wine in the selected Proton directory.'
[ -n "$updater" ] || updater=$game/Nioh3Updater.exe
[ -f "$updater" ] || fail 'Put Nioh3Updater.exe from the mod website in the game folder (or use --updater).'
updater_dir=$(CDPATH= cd -- "$(dirname -- "$updater")" && pwd -P)
updater=$updater_dir/$(basename -- "$updater")
command -v timeout >/dev/null 2>&1 || fail 'The timeout command is required (GNU coreutils).'
# Pass a Windows path explicitly so updater discovery cannot select a different library.
export WINEPREFIX="$prefix"
win_game=$(timeout 15 "$wine" winepath.exe -w "$game") || fail 'Wine path conversion failed or timed out.'
win_game=$(printf '%s' "$win_game" | tr -d '\r')
[ -n "$win_game" ] || fail 'Wine returned an empty game path.'
printf '%s\n' "Nioh 3 updater: $game" 'Close the game if it is running; the updater waits for it. Settings are kept.'
exec "$wine" "$updater" --game-dir "$win_game" "$@"
