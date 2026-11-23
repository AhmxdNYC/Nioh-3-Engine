"""Nioh 3 mod one-click setup (human #60944/#60948, 2026-11-01).

Windows: double-click Nioh3-Setup.exe.   Linux/Steam Deck/Ally: python3 Nioh3-Setup-Linux.pyz
It finds Nioh 3 through Steam, waits for the game to close, backs up saves and replaced files, and installs the
whole full-install zip embedded in it (dinput8.dll loader, mods, settings, data, updater, restore scripts).
Existing .ini settings are kept. On Linux it also turns on the dinput8 loader in the game's Proton prefix,
so no Steam launch option is needed. No copying by hand.

Build: tools/setup/build-setup.ps1 -Zip FULL.zip  (exe)  /  py -I tools/setup/build_linux.py FULL.zip OUT.pyz
"""
import argparse
import io
import os
from pathlib import Path
import sys
import tempfile
import zipfile

APPID = "3681010"
FROZEN = getattr(sys, "frozen", False)
ZIPAPP = not FROZEN and zipfile.is_zipfile(sys.argv[0]) if sys.argv and sys.argv[0] else False
HERE = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))
if not FROZEN and not ZIPAPP:
    REPO = Path(__file__).resolve().parents[2]
    sys.path[:0] = [str(REPO / "release"), str(REPO / "tools" / "updater")]
import install_mod  # noqa: E402
import updater  # noqa: E402

OVERRIDE_KEY = "[Software\\\\Wine\\\\DllOverrides]"


def bundled(name):
    """Bytes of a file shipped inside the exe / pyz (None when missing)."""
    if ZIPAPP:
        with zipfile.ZipFile(sys.argv[0]) as z:
            return z.read(name) if name in z.namelist() else None
    p = HERE / name
    return p.read_bytes() if p.is_file() else None


def proton_prefix(game):
    return game.parent.parent / "compatdata" / APPID / "pfx"  # steamapps/common/Nioh3 -> steamapps/compatdata


def saves_dir(game):
    if os.name == "nt":
        local = os.environ.get("LOCALAPPDATA")
        if not local:
            raise RuntimeError("LOCALAPPDATA is not set; use --save-root")
        return Path(local) / "KoeiTecmo" / "NIOH3"
    pfx = proton_prefix(game)
    if pfx.is_dir():
        return pfx / "drive_c/users/steamuser/AppData/Local/KoeiTecmo/NIOH3"
    return Path.home() / ".local/share/nioh3-mod/no-prefix-saves"  # game never started: nothing to back up


def enable_loader(pfx):
    """Linux: dinput8=native,builtin in the prefix registry (same effect as WINEDLLOVERRIDES launch option)."""
    reg = pfx / "user.reg"
    if not reg.is_file():
        return False
    text = reg.read_text(encoding="utf-8", errors="surrogateescape")
    nl = "\r\n" if "\r\n" in text else "\n"
    lines = text.split(nl)
    start = next((i for i, l in enumerate(lines) if l.startswith(OVERRIDE_KEY)), None)
    entry = '"dinput8"="native,builtin"'
    if start is None:
        lines += ["", OVERRIDE_KEY, entry]
    else:
        end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith("[")), len(lines))
        for i in range(start + 1, end):
            if lines[i].lower().startswith('"dinput8"='):
                lines[i] = entry
                break
        else:
            lines.insert(start + 1 + (1 if start + 1 < end and lines[start + 1].startswith("#time") else 0), entry)
    new = nl.join(lines)
    if new != text:
        (pfx / "user.reg.nioh3-setup.bak").write_text(text, encoding="utf-8", errors="surrogateescape")
        tmp = reg.with_suffix(".reg.new")
        tmp.write_text(new, encoding="utf-8", errors="surrogateescape")
        os.replace(tmp, reg)
    return True


def run(game_dir=None, saves=None, payload=None):
    version = (bundled("version.txt") or b"?").decode().strip()
    payload = payload or bundled("payload.zip")
    if not payload:
        raise RuntimeError("This setup has no payload.zip inside; download it again")
    print(f"Nioh 3 Mod Setup {version}")
    game = updater.choose_game(game_dir)
    print(f"Game folder: {game}")
    updater.wait_stopped()
    saves = Path(saves) if saves else saves_dir(game)
    saves.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="nioh3-setup-") as tmp:
        with zipfile.ZipFile(io.BytesIO(payload)) as z:
            for name in z.namelist():
                if "/" in name or "\\" in name or name.startswith("."):
                    raise RuntimeError(f"Unexpected path in payload: {name}")
            z.extractall(tmp)
        backup = install_mod.install(tmp, game, saves, saves.parent / "NIOH3-mod-backups", fresh_ok=True)
    print(f"Installed {version}. Backup of saves and replaced files: {backup}")
    if os.name != "nt":
        if enable_loader(proton_prefix(game)):
            print("Loader turned on in the game's Proton prefix (no Steam launch option needed).")
        else:
            print("Start Nioh 3 once, then run this setup again to turn the loader on,")
            print('or set the Steam launch option:  WINEDLLOVERRIDES="dinput8=n,b" %command%')
    print("Existing .ini settings were kept. Start Nioh 3; the mod menu opens with F10 (or Esc x3 / Start x3).")
    print("Later updates: in-game Update page, or Nioh3Updater.exe in the game folder.")
    return backup


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--game-dir", type=Path)
    p.add_argument("--save-root", type=Path, help="Testing only: save folder to back up")
    p.add_argument("--no-pause", action="store_true", help="For scripted use")
    args = p.parse_args(argv)
    try:
        run(args.game_dir, saves=args.save_root)
        return 0
    except (Exception, KeyboardInterrupt) as e:
        print(f"Setup stopped: {e or 'Cancelled'}. Files already replaced were rolled back.", file=sys.stderr)
        return 1
    finally:
        if not args.no_pause:
            updater.pause()


if __name__ == "__main__":
    sys.exit(main())
