"""Build and verify the fresh-install zip every public release must carry (human #60918, 2026-11-01).

The update-only zips of 1.7.87-1.7.90 had no dinput8.dll loader, so new players could not install (#60907).
Rule: every GitHub release has Nioh3-<ver>-full-install.zip with the loader, the installer and default settings.

  py -I release/full_install_zip.py build --asi-dir DIR --loader-zip ZIP --out FILE.zip
        DIR = the released Nioh3EngineFix/Training/BossRematch .asi (+ Nioh3Updater.exe if shipped);
        ZIP = any old full zip holding dinput8.dll + LICENSE-UltimateASILoader.txt (e.g. enginefix-1.7.61);
        ini/tsv/installer files come from this checkout (use the release branch's worktree).
  py -I release/full_install_zip.py check FILE.zip       exit 1 unless the zip is installable from scratch
  py -I release/full_install_zip.py check-tag v1.7.91    downloads the release's assets via gh, same check;
                                                         run it after `gh release create` and before announcing.
"""
import hashlib
import pathlib
import subprocess
import sys
import tempfile
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = "AhmxdNYC/Nioh-3-Engine"
REQUIRED = ("dinput8.dll", "LICENSE-UltimateASILoader.txt", "Nioh3EngineFix.asi", "Nioh3Training.asi",
            "Nioh3BossRematch.asi", "Nioh3EngineFix.ini", "Nioh3Training.ini", "Nioh3BossRematch.ini",
            "install_mod.py", "Install-Mod.cmd", "install-mod.sh", "INSTALL.txt", "SHA256SUMS.txt")
REPO_FILES = {
    "mod/Nioh3EngineFix/Nioh3EngineFix.ini": None, "mod/Nioh3BossRematch/Nioh3BossRematch.ini": None,
    "mod/Nioh3Training/Nioh3Training.ini": None,
}
RELEASE_FILES = ("Nioh3 Mod Website.url", "install_mod.py", "Install-Mod.cmd", "install-mod.sh", "USER_GUIDE.md",
                 "Restore Vanilla Game.cmd", "Put the mod back.cmd", "restore-vanilla.sh", "put-mod-back.sh")
INSTALL_TXT = """Nioh 3 mods - FRESH INSTALL (first time)

This zip has everything: the mods (.asi), their settings (.ini/.tsv) and dinput8.dll
(Ultimate ASI Loader by ThirteenAG, MIT - it loads the .asi files; without it nothing loads).

Windows, easiest:
  1. Close Nioh 3.
  2. Copy EVERY file from this zip into the Nioh 3 game folder, next to Nioh3.exe
     (Steam > Nioh 3 > Manage > Browse local files).
  3. Start the game. Mod menu: F10 (or Esc x3 / Start x3).
Windows, with the installer (Python 3 needed): Install-Mod.cmd --game-dir "<Nioh 3 folder>"
  (keeps existing .ini settings and makes a backup; "Restore Vanilla Game.cmd" undoes it).

Linux / Steam Deck / ROG Ally (Proton):
  sh install-mod.sh --game-dir "<Nioh 3 folder>" --proton-prefix "<prefix>"   (or copy the files as above)
  and set the Steam launch option:  WINEDLLOVERRIDES="dinput8=n,b" %command%

Already installed? You do not need this zip: update from the in-game Update page or Nioh3Updater.exe.
Do not combine with Lyall's Nioh3Fix (both patch the same code).
"""


def sha(data):
    return hashlib.sha256(data).hexdigest()


def build(asi_dir, loader_zip, out):
    files = {}
    for name in ("Nioh3EngineFix.asi", "Nioh3Training.asi", "Nioh3BossRematch.asi", "Nioh3Updater.exe"):
        p = asi_dir / name
        if p.exists():
            files[name] = p.read_bytes()
    with zipfile.ZipFile(loader_zip) as z:
        for name in ("dinput8.dll", "LICENSE-UltimateASILoader.txt"):
            files[name] = z.read(name)
    for rel in REPO_FILES:
        p = ROOT / rel
        files[p.name] = p.read_bytes()
    for mod in ("Nioh3BossRematch", "Nioh3Training"):
        for p in (ROOT / "mod" / mod).glob("*.tsv"):
            files[p.name] = p.read_bytes()
    for name in RELEASE_FILES:
        p = ROOT / "release" / name
        if p.exists():
            files[name] = p.read_bytes()
    upd = ROOT / "tools" / "updater" / "update-linux.sh"
    if upd.exists():
        files[upd.name] = upd.read_bytes()
    files["INSTALL.txt"] = INSTALL_TXT.encode()
    files["SHA256SUMS.txt"] = "".join(f"{sha(d)}  {n}\n" for n, d in sorted(files.items())).encode()
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in files.items():
            z.writestr(name, data)
    return check(out)


def check(path):
    with zipfile.ZipFile(path) as z:
        names = set(z.namelist())
        missing = [n for n in REQUIRED if n not in names]
        bad = []
        if "SHA256SUMS.txt" in names:
            for line in z.read("SHA256SUMS.txt").decode().splitlines():
                digest, name = line.split(maxsplit=1)
                if name in names and sha(z.read(name)) != digest:
                    bad.append(name)
    if missing or bad:
        print(f"FAIL {path}: missing {missing} checksum-mismatch {bad}")
        return 1
    print(f"PASS {path}: fresh-install zip complete ({len(names)} files, loader + installer + settings), "
          f"sha256 {sha(pathlib.Path(path).read_bytes())[:8]}")
    return 0


def check_tag(tag):
    with tempfile.TemporaryDirectory() as d:
        r = subprocess.run(["gh", "release", "download", tag, "-R", REPO, "-p", "*full-install*.zip", "-D", d],
                           capture_output=True, text=True)
        zips = list(pathlib.Path(d).glob("*.zip"))
        if r.returncode or not zips:
            print(f"FAIL {tag}: no *full-install*.zip asset on the GitHub release (rule #60918). "
                  f"Build one with 'full_install_zip.py build' and 'gh release upload'.")
            return 1
        return check(zips[0])


def main(argv):
    if len(argv) >= 2 and argv[0] == "check":
        return check(pathlib.Path(argv[1]))
    if len(argv) >= 2 and argv[0] == "check-tag":
        return check_tag(argv[1])
    if argv[:1] == ["build"] and "--asi-dir" in argv and "--loader-zip" in argv and "--out" in argv:
        arg = lambda k: pathlib.Path(argv[argv.index(k) + 1])
        return build(arg("--asi-dir"), arg("--loader-zip"), arg("--out"))
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
