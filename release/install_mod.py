"""Offline Windows/Proton installer. Back up current saves before mod activation.

Never restores or deletes saves. Requires Python 3.9+; uses only the standard library.
"""
import argparse
import csv
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import uuid

FILES = ("dinput8.dll", "Nioh3EngineFix.asi", "Nioh3EngineFix.ini",
         "Nioh3BossRematch.asi", "Nioh3BossRematch.ini", "Nioh3BossRematch_bosses.tsv",
         "Nioh3Training.asi", "Nioh3Training.ini", "Nioh3Training_action_records.tsv",
         "Nioh3Training_actionsets.tsv", "Nioh3Training_arenas.tsv", "Nioh3Training_bgm.tsv",
         "Nioh3 Mod Website.url", "Nioh3Updater.exe", "SaveSlots-after-exit.pyz",
         "Restore Vanilla Game.cmd", "Put the mod back.cmd", "restore-vanilla.sh", "put-mod-back.sh")


def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def plain(path):
    s = path.lstat()
    if path.is_symlink() or getattr(s, "st_file_attributes", 0) & 0x400:
        raise RuntimeError(f"Links/reparse points are not supported: {path}")
    if not (stat.S_ISREG(s.st_mode) or stat.S_ISDIR(s.st_mode)):
        raise RuntimeError(f"Not a regular file/directory: {path}")


def inventory(root):
    plain(root)
    result = {}
    def scan_error(error):
        raise error
    for folder, dirs, files in os.walk(root, followlinks=False, onerror=scan_error):
        for name in dirs + files:
            plain(Path(folder) / name)
        for name in sorted(files):
            p = Path(folder) / name
            result[p.relative_to(root).as_posix()] = {"size": p.stat().st_size, "sha256": sha(p)}
    return result


def write_receipt(path, data):
    tmp = path.with_suffix(".new")
    with tmp.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)


def exclusive_copy(src, dst):
    dst.parent.mkdir(parents=True, exist_ok=True)
    with src.open("rb") as source, dst.open("xb") as target:
        shutil.copyfileobj(source, target)
        target.flush()
        os.fsync(target.fileno())


def game_running():
    if os.name == "nt":
        r = subprocess.run(["tasklist", "/FI", "IMAGENAME eq Nioh3.exe", "/FO", "CSV", "/NH"],
                           capture_output=True, text=True, check=True)
        return any(row and row[0].lower() == "nioh3.exe" for row in csv.reader(r.stdout.splitlines()))
    proc = Path("/proc")
    if not proc.is_dir():
        raise RuntimeError("Cannot verify game is closed on this platform")
    for entry in proc.iterdir():
        if not entry.name.isdigit():
            continue
        try:
            args = (entry / "cmdline").read_bytes().split(b"\0")
            if any(arg.replace(b"\\", b"/").rsplit(b"/", 1)[-1].lower() == b"nioh3.exe" for arg in args):
                return True
        except FileNotFoundError:
            pass  # process exited
        except PermissionError:
            # Refuse rather than silently miss a game process hidden by proc policy.
            raise RuntimeError("Cannot inspect all processes; cannot verify game is closed")
    return False


def overlaps(a, b):
    return a == b or a in b.parents or b in a.parents


def current_hash(path):
    if path.exists() or path.is_symlink():
        plain(path)
        return sha(path)
    return None


def install(package, game, saves, backups, running=game_running):
    package, game, saves, backups = map(lambda p: Path(p).absolute(), (package, game, saves, backups))
    for root in (package, game, saves):
        plain(root)
        if not root.is_dir():
            raise RuntimeError(f"Directory required: {root}")
    if backups.exists() or backups.is_symlink():
        plain(backups)
    package, game, saves, backups = (p.resolve() for p in (package, game, saves, backups))
    if any(overlaps(a, b) for a, b in ((game, saves), (backups, saves), (backups, game), (package, game), (package, backups), (package, saves))):
        raise RuntimeError("Package, game, save and backup directories must not overlap")
    if not (game / "Nioh3.exe").is_file():
        raise RuntimeError("Select the folder containing Nioh3.exe")
    if running():
        raise RuntimeError("Close Nioh 3 before installing/updating")
    sums = {}
    for line in (package / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines():
        digest, name = line.split(maxsplit=1)
        name = name.lstrip("*")
        if name in sums:
            raise RuntimeError("Duplicate checksum entry")
        sums[name] = digest.lower()
    selected = []
    for name in FILES:
        source, target = package / name, game / name
        if not source.exists():
            continue
        plain(source)
        if not source.is_file() or sums.get(name) != sha(source):
            raise RuntimeError(f"Package checksum missing/mismatched: {name}")
        if target.exists() or target.is_symlink():
            plain(target)
            if not target.is_file():
                raise RuntimeError(f"Not a file: {target}")
            if name.endswith(".ini"):
                continue  # preserve user settings on update
        selected.append(name)
    if not any(name.endswith(".asi") for name in selected):
        raise RuntimeError("No supported, checksum-verified ASI in this package")
    backups.mkdir(parents=True, exist_ok=True)
    plain(backups)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    backup = backups / (stamp + "-" + uuid.uuid4().hex)
    backup.mkdir()  # never reuse an earlier backup
    receipt = {"version": 1, "status": "preparing", "created_utc": stamp,
               "game": str(game), "save_source": str(saves), "save_backup": str(backup / "saves"),
               "restore": "Manual only: close game; preserve current saves separately before copying this saves directory. No automatic save restore.",
               "files": {}, "installed": [], "rollback": []}
    receipt_path = backup / "receipt.json"
    write_receipt(receipt_path, receipt)
    print(f"Backup/receipt: {backup}", flush=True)
    lock = game / ".nioh3-mod-install.lock"
    locked = False
    release_lock = False
    try:
        with lock.open("x", encoding="utf-8") as f:
            f.write(str(receipt_path)); f.flush(); os.fsync(f.fileno())
        locked = True
        before = inventory(saves)
        if not before:
            raise RuntimeError("Save folder is empty; select the existing save root (no silent skip)")
        for name in before:
            exclusive_copy(saves / name, backup / "saves" / name)
        if inventory(saves) != before or inventory(backup / "saves") != before:
            raise RuntimeError("Saves changed during backup or backup verification failed")
        receipt["saves"] = before
        # Stage and verify all mod payloads and originals before the first activation.
        for name in selected:
            source, target = package / name, game / name
            exclusive_copy(source, backup / "payload" / name)
            if sha(backup / "payload" / name) != sums[name]:
                raise RuntimeError(f"Package changed during staging: {name}")
            old = None
            if target.exists():
                plain(target)
                exclusive_copy(target, backup / "mods-before" / name)
                old = sha(backup / "mods-before" / name)
                if sha(target) != old:
                    raise RuntimeError("Installed mod changed during backup")
            receipt["files"][name] = {"before": old, "after": sums[name]}
        receipt["status"] = "backup_verified"
        write_receipt(receipt_path, receipt)
        if running() or inventory(saves) != before:
            raise RuntimeError("Game started or saves changed before activation")
        for name in selected:
            target = game / name
            info = receipt["files"][name]
            if target.exists():
                plain(target)
            if (sha(target) if target.exists() else None) != info["before"]:
                raise RuntimeError("Installed file changed before activation")
            receipt["pending"] = name
            write_receipt(receipt_path, receipt)  # recoverable intent before replace
            staged = game / (".nioh3-install-" + uuid.uuid4().hex + ".tmp")
            try:
                exclusive_copy(backup / "payload" / name, staged)
                if sha(staged) != info["after"]:
                    raise RuntimeError("Staged payload changed; activation refused")
                if name.endswith(".sh"):
                    staged.chmod(staged.stat().st_mode | 0o755)  # Linux: restore-vanilla.sh / put-mod-back.sh run directly
                if current_hash(target) != info["before"]:
                    raise RuntimeError("Installed target changed during payload copy")
                if running():
                    raise RuntimeError("Game started during activation; remaining files not installed")
                os.replace(staged, target)
            finally:
                if staged.exists():
                    staged.unlink()
            receipt["installed"].append(name)
            receipt.pop("pending", None)
            write_receipt(receipt_path, receipt)
        receipt["status"] = "installed"
        write_receipt(receipt_path, receipt)
        release_lock = True
        return backup
    except Exception as error:
        receipt["error"] = str(error)
        # Never restore saves. Mod rollback requires ownership of the exact new bytes.
        names = list(receipt["installed"])
        if receipt.get("pending") and receipt["pending"] not in names:
            names.append(receipt["pending"])
        for name in reversed(names):
            target, info = game / name, receipt["files"][name]
            try:
                if not target.exists() and not target.is_symlink() and info["before"] is None:
                    continue
                plain(target)
                current = sha(target)
                if current == info["before"]:
                    continue
                if current != info["after"]:
                    raise RuntimeError("ownership changed; left untouched")
                if info["before"] is None:
                    if current_hash(target) != info["after"] or running():
                        raise RuntimeError("ownership changed or game running; rollback deferred")
                    target.unlink()
                else:
                    original = backup / "mods-before" / name
                    if sha(original) != info["before"]:
                        raise RuntimeError("original backup corrupt; left untouched")
                    staged = game / (".nioh3-rollback-" + uuid.uuid4().hex + ".tmp")
                    try:
                        exclusive_copy(original, staged)
                        if sha(staged) != info["before"]:
                            raise RuntimeError("Rollback copy failed verification; target untouched")
                        if current_hash(target) != info["after"] or running():
                            raise RuntimeError("ownership changed or game running; rollback deferred")
                        os.replace(staged, target)
                    finally:
                        if staged.exists(): staged.unlink()
                receipt["rollback"].append({"file": name, "status": "restored"})
            except Exception as rollback_error:
                receipt["rollback"].append({"file": name, "status": "needs_review", "error": str(rollback_error)})
        receipt["status"] = "failed_needs_review" if any(x["status"] == "needs_review" for x in receipt["rollback"]) else "failed"
        try:
            write_receipt(receipt_path, receipt)
            release_lock = receipt["status"] == "failed"
        except Exception as journal_error:
            print(f"Receipt update failed: {journal_error}; retain {backup}", file=sys.stderr)
        raise
    finally:
        if locked and release_lock:
            # Never remove another owner's replacement lock. Interrupted process
            # leaves its own lock and pending receipt for explicit recovery.
            if lock.is_file() and not lock.is_symlink() and lock.read_text(encoding="utf-8") == str(receipt_path):
                lock.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", required=True, type=Path)
    parser.add_argument("--save-root", type=Path)
    parser.add_argument("--proton-prefix", type=Path, help="The game's compatdata/<appid>/pfx directory")
    parser.add_argument("--backup-root", type=Path)
    parser.add_argument("--package", type=Path, default=Path(__file__).resolve().parent)
    args = parser.parse_args()
    saves = args.save_root
    if saves is None and args.proton_prefix:
        saves = args.proton_prefix / "drive_c/users/steamuser/AppData/Local/KoeiTecmo/NIOH3"
    if saves is None and os.name == "nt" and os.environ.get("LOCALAPPDATA"):
        saves = Path(os.environ["LOCALAPPDATA"]) / "KoeiTecmo/NIOH3"
    if saves is None:
        parser.error("Provide --save-root or the game's --proton-prefix; no guessed Steam account/prefix")
    try:
        backup = install(args.package, args.game_dir, saves, args.backup_root or saves.parent / "NIOH3-mod-backups")
        print(f"Installed. Current saves preserved at {backup / 'saves'}; existing INIs kept.")
        return 0
    except Exception as error:
        print(f"Install refused/failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
