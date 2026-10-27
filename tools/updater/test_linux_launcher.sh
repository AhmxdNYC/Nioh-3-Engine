#!/bin/sh
# Offline fake Wine: no network, Steam, game or real prefix access.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
tmp=$(CDPATH= cd -- "$tmp" && pwd -P)
trap 'rm -rf -- "$tmp"' EXIT HUP INT TERM
game="$tmp/Steam Library/steamapps/common/Nioh3"
prefix="$tmp/Steam Library/steamapps/compatdata/3681010/pfx"
proton="$tmp/Proton 11"
mkdir -p "$game" "$prefix/drive_c" "$proton/files/bin"
touch "$game/Nioh3.exe" "$game/Nioh3Updater.exe" "$prefix/system.reg"
export FAKE_LOG="$tmp/log"
cat > "$proton/files/bin/wine" <<'EOF'
#!/bin/sh
if [ "$1" = winepath.exe ]; then
    [ "${FAKE_CONVERT_FAIL:-0}" = 0 ] || exit 1
    printf 'Z:\\Steam Games\\Nioh3\r\n'; exit 0
fi
printf '%s\n' "$WINEPREFIX" "$@" > "$FAKE_LOG"
exit "${FAKE_EXIT:-0}"
EOF
chmod +x "$proton/files/bin/wine"
unset WINEPREFIX PROTON_DIR
sh "$here/update-linux.sh" --game-dir "$game" --proton-dir "$proton" -- --no-pause
test "$(sed -n '1p' "$FAKE_LOG")" = "$prefix"
test "$(sed -n '2p' "$FAKE_LOG")" = "$game/Nioh3Updater.exe"
test "$(sed -n '3p' "$FAKE_LOG")" = --game-dir
test "$(sed -n '4p' "$FAKE_LOG")" = 'Z:\Steam Games\Nioh3'
test "$(sed -n '5p' "$FAKE_LOG")" = --no-pause
# Existing alternate prefix, older Proton layout, rollback argument boundaries.
mkdir -p "$tmp/other prefix/drive_c" "$proton/dist/bin"
touch "$tmp/other prefix/system.reg"
mv "$proton/files/bin/wine" "$proton/dist/bin/wine"
WINEPREFIX="$tmp/other prefix" PROTON_DIR="$proton" sh "$here/update-linux.sh" --game-dir "$game" -- --rollback abc123
test "$(sed -n '1p' "$FAKE_LOG")" = "$tmp/other prefix"
test "$(sed -n '5p' "$FAKE_LOG")" = --rollback
test "$(sed -n '6p' "$FAKE_LOG")" = abc123
# Preserve Wine/updater exit status.
set +e
FAKE_EXIT=7 sh "$here/update-linux.sh" --game-dir "$game" --proton-dir "$proton"
rc=$?
set -e
test "$rc" = 7
expect_failure() {
    if "$@" >"$tmp/error" 2>&1; then echo 'Expected rejection' >&2; exit 1; fi
}
expect_failure env FAKE_CONVERT_FAIL=1 sh "$here/update-linux.sh" --game-dir "$game" --proton-dir "$proton"
expect_failure sh "$here/update-linux.sh" --game-dir "$game" --proton-dir "$proton" --prefix "$tmp/missing"
expect_failure sh "$here/update-linux.sh" --game-dir "$tmp/missing" --proton-dir "$proton"
expect_failure sh "$here/update-linux.sh" --game-dir
expect_failure sh "$here/update-linux.sh" --unrecognized
mv "$game/Nioh3Updater.exe" "$tmp/updater.exe"
expect_failure sh "$here/update-linux.sh" --game-dir "$game" --proton-dir "$proton"
sh "$here/update-linux.sh" --game-dir "$game" --proton-dir "$proton" --updater "$tmp/updater.exe"
test "$(sed -n '2p' "$FAKE_LOG")" = "$tmp/updater.exe"
echo 'PASS Linux launcher: direct Wine, existing prefix, spaced paths, CRLF conversion, argument forwarding, exit status, invalid inputs and conversion failure.'
