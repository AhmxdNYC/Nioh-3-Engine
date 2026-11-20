#!/usr/bin/env python3
"""Nioh 3 standalone updater. Python 3.10+, standard library only."""
import argparse
import contextlib
import configparser
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request
import uuid
import time

# Name lists the mods read at startup. They are data, not player settings, so an update may replace them.
# .ini files stay out of that set on purpose: update() never overwrites an existing ini.
NAME_LISTS = (
    "Nioh3BossRematch_bosses.tsv",
    "Nioh3Training_actionsets.tsv",
    "Nioh3Training_action_records.tsv",
    "Nioh3Training_arenas.tsv",
    "Nioh3Training_bgm.tsv",
)
# Shipped 1.8.1 updater.exe allow-list. A manifest that names any other file makes that build
# raise "Unapproved or duplicate filename" inside validate(), before it downloads or writes anything.
SHIPPED_181_ALLOWED = {f"Nioh3{x}.{ext}" for x in
                       ("EngineFix", "BossRematch", "Training", "CamKiProbe")
                       for ext in ("asi", "ini")} | {"Nioh3 Mod Website.url", "Nioh3Training_action_records.tsv"}
ALLOWED = SHIPPED_181_ALLOWED | set(NAME_LISTS)
# A 1.8.2 package is refused unless every menu ASI and the new camera ini are named.
# The three player settings files are not required here: the update zip must not replace them.
REQUIRED_PUBLISH = (
    "Nioh3EngineFix.asi", "Nioh3Training.asi", "Nioh3BossRematch.asi", "Nioh3CamKiProbe.asi",
    "Nioh3CamKiProbe.ini",
)


# 1.8.2 and 1.8.3 shipped Boss Rematch a32e9ae4, built without soul-core forms.
BR_ASI = "Nioh3BossRematch.asi"
SOULCORE_MARKER = b"SoulCoreForms"


def boss_rematch_soul_cores(data):
    """Raise unless these Boss Rematch bytes include soul-core forms.

    data is the ASI bytes, or a path whose file is the ASI.
    """
    if isinstance(data, (bytes, bytearray, memoryview)):
        blob = bytes(data)
    else:
        blob = Path(data).read_bytes()
    if SOULCORE_MARKER not in blob:
        raise ValueError(
            "package refused: Nioh3BossRematch.asi has no SoulCoreForms (stale build, soul cores missing)"
        )


def _payload_map(names):
    if isinstance(names, dict):
        return {str(name): payload for name, payload in names.items()}
    present = {}
    for item in names:
        if isinstance(item, (bytes, bytearray)):
            raise ValueError("package refused: release gate was given bytes without a file name")
        if isinstance(item, Path):
            present[item.name] = item
        else:
            present[str(item)] = None
    return present


def publish_gate(names):
    """Refuse a package that omits a required file or ships a stale Boss Rematch.

    names is a dict of filename to bytes or path, or an iterable of paths.
    A bare name, with no bytes, is refused: that is how 1.8.2 shipped without soul cores.
    """
    present = _payload_map(names)
    missing = [name for name in REQUIRED_PUBLISH if name not in present]
    if missing:
        raise ValueError("package refused, missing: " + ", ".join(missing))
    payload = present.get(BR_ASI)
    if payload is None:
        raise ValueError(
            "package refused: Nioh3BossRematch.asi bytes were not given to the release gate"
        )
    boss_rematch_soul_cores(payload)
MANIFEST_URL = "https://nioh3-mod-feedback.vercel.app/updates/stable.json"
MAX_FILE = 128 * 1024 * 1024
# #121461 updates badge: after an update that changed files, one POST of the two version numbers
# (no install id, machine id, path or account). Only for the official manifest; --no-count or
# [Update] CountUpdate = 0 turns it off. Never blocks or fails an update.
COUNT_URL = "https://nioh3-mod-feedback.vercel.app/api/updates"


def https(url):
    p = urllib.parse.urlsplit(url)
    if p.scheme != "https" or not p.hostname or p.username or p.password or p.fragment:
        raise ValueError("Only HTTPS URLs without credentials/fragments are allowed")
    return url


class SecureRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        https(newurl)
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def count_update(old, new, url=COUNT_URL, opener=None):
    """Best effort; returns True when the counter answered 2xx."""
    try:
        body = json.dumps({"schema": 1, "from": old, "to": new}).encode("utf-8")
        req = urllib.request.Request(https(url), data=body, method="POST",
                                     headers={"User-Agent": "Nioh3Updater/1", "Content-Type": "application/json"})
        with (opener or urllib.request.build_opener(SecureRedirect())).open(req, timeout=5) as r:
            return 200 <= r.status < 300
    except Exception:
        return False


def fetch(url, limit=MAX_FILE):
    # GET only; no cookies, machine identifiers or uploads (the one update count is count_update).
    req = urllib.request.Request(https(url), headers={"User-Agent": "Nioh3Updater/1"})
    with urllib.request.build_opener(SecureRedirect()).open(req, timeout=30) as r:
        https(r.geturl())
        data = r.read(limit + 1)
    if len(data) > limit:
        raise ValueError("Download exceeds size limit")
    return data


def validate(m):
    if not isinstance(m, dict) or m.get("schema") != 1:
        raise ValueError("Unsupported manifest schema")
    if not isinstance(m.get("version"), str) or not re.fullmatch(r"\d+\.\d+\.\d+", m["version"]):
        raise ValueError("version must be major.minor.patch")
    if not isinstance(m.get("news"), str) or len(m["news"]) > 240 or any(c in m["news"] for c in "\r\n"):
        raise ValueError("news must be a single line of at most 240 characters")
    files = m.get("files")
    if not isinstance(files, list) or not 1 <= len(files) <= len(ALLOWED):
        raise ValueError("Invalid file list")
    seen = set()
    for f in files:
        if not isinstance(f, dict) or f.get("name") not in ALLOWED or f["name"] in seen:
            raise ValueError("Unapproved or duplicate filename")
        seen.add(f["name"])
        if not isinstance(f.get("sha256"), str) or not re.fullmatch(r"[0-9a-f]{64}", f["sha256"]):
            raise ValueError("Invalid SHA256")
        https(f.get("url", ""))
    return m


def game_running():
    if os.name == "nt":
        result = subprocess.run(["tasklist", "/FO", "CSV", "/NH"], check=True,
                                capture_output=True, text=True)
        names = [r[0].lower() for r in csv.reader(io.StringIO(result.stdout)) if r]
        return any(n in ("nioh3.exe", "nioh3") for n in names)
    return proc_game_running(Path("/proc"))


def proc_game_running(proc):
    # Match only a process argument whose basename is Nioh3.exe (Wine/Proton
    # keeps the Windows path in argv). A bare "nioh3" word in another
    # program's arguments (shell, editor, file manager) is not the game.
    if not proc.is_dir():
        raise RuntimeError("Cannot verify game is closed on this platform")
    for entry in proc.iterdir():
        if not entry.name.isdigit():
            continue
        try:
            args = (entry / "cmdline").read_bytes().split(b"\0")
        except (FileNotFoundError, ProcessLookupError):
            continue  # process exited
        except PermissionError:
            raise RuntimeError("Cannot inspect all processes; cannot verify game is closed")
        if any(a.replace(b"\\", b"/").rsplit(b"/", 1)[-1].lower() == b"nioh3.exe" for a in args):
            return True
    return False


def ensure_stopped():
    if game_running():
        raise RuntimeError("Close Nioh 3 before updating or rolling back")


def regular(path):
    if path.is_symlink() or (path.exists() and not path.is_file()):
        raise ValueError(f"Unsafe file: {path.name}")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def write_json(path, value):
    temp = path.with_suffix(".tmp")
    with temp.open("w", encoding="utf-8") as f:
        json.dump(value, f, indent=2)
        f.flush()
        os.fsync(f.fileno())
    os.replace(temp, path)


@contextlib.contextmanager
def locked(root):
    path = root / ".nioh3-updater.lock"
    fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    try:
        os.write(fd, str(os.getpid()).encode())
        yield
    finally:
        os.close(fd)
        path.unlink()


def backup_root(root):
    folder = root / "nioh3-update-backups"
    if folder.is_symlink():
        raise ValueError("Backup directory cannot be a symlink")
    folder.mkdir(exist_ok=True)
    return folder


def restore(root, backup):
    """Verify every backup before touching installed files. Repeatable after interruption."""
    journal = json.loads((backup / "journal.json").read_text(encoding="utf-8"))
    entries = journal["entries"]
    seen = set()
    for e in entries:
        if e["name"] not in ALLOWED or e["name"] in seen:
            raise ValueError("Invalid backup filename")
        seen.add(e["name"])
        regular(root / e["name"])
        if e["existed"]:
            regular(backup / e["name"])
            if digest((backup / e["name"]).read_bytes()) != e["sha256"]:
                raise ValueError("Backup integrity check failed")
    ensure_stopped()
    with tempfile.TemporaryDirectory(prefix=".nioh3-restore-", dir=root) as td:
        for e in entries:
            ensure_stopped()
            target = root / e["name"]
            if e["existed"]:
                staged = Path(td) / e["name"]
                shutil.copy2(backup / e["name"], staged)
                os.replace(staged, target)
            else:
                target.unlink(missing_ok=True)
    journal["state"] = "rolled-back"
    write_json(backup / "journal.json", journal)


def installed_version(root):
    """Read the build banner from the installed binary, never a stale log/INI."""
    binary = root / "Nioh3EngineFix.asi"
    regular(binary)
    if not binary.exists():
        return None
    data = binary.read_bytes()
    versions = set(re.findall(rb"Nioh3EngineFix (\d+\.\d+\.\d+)(?:-[^\x00 ]+)? \(built ", data))
    if len(versions) > 1:
        raise ValueError("Ambiguous installed EngineFix version; update stopped")
    return next(iter(versions)).decode("ascii") if versions else None


def version_tuple(value):
    return tuple(int(part) for part in value.split("."))


def update(root, manifest_url=MANIFEST_URL, progress=print):
    root = Path(root).resolve(strict=True)
    with locked(root):
        ensure_stopped()
        backups = backup_root(root)
        for j in backups.glob("*/journal.json"):
            if json.loads(j.read_text(encoding="utf-8"))["state"] == "prepared":
                raise RuntimeError(f"Interrupted update: roll back {j.parent.name} first")
        progress("Checking for updates...")
        m = validate(json.loads(fetch(manifest_url, 65536)))
        current = installed_version(root)
        if current and version_tuple(current) > version_tuple(m["version"]):
            progress(f"Already newer: installed {current}; public stable {m['version']}. No files changed.")
            return {"version": current, "changed": 0, "backup": None, "already_newer": True}
        progress(f"Version {m['version']}: {m['news']}")
        with tempfile.TemporaryDirectory(prefix=".nioh3-update-", dir=root) as td:
            stage = Path(td)
            changed = []
            for f in m["files"]:
                target = root / f["name"]
                regular(target)
                # Existing player settings are never replaced by release defaults.
                if target.exists() and target.suffix == ".ini":
                    continue
                if target.exists() and digest(target.read_bytes()) == f["sha256"]:
                    continue
                progress(f"Downloading and checking {f['name']}...")
                data = fetch(f["url"])
                if digest(data) != f["sha256"]:
                    raise ValueError(f"SHA256 mismatch: {f['name']}")
                (stage / f["name"]).write_bytes(data)
                changed.append(f)
            if not changed:
                return {"version": m["version"], "changed": 0, "backup": None}
            ensure_stopped()
            progress("Backing up your current files and settings...")
            backup = backups / uuid.uuid4().hex
            backup.mkdir()
            entries = []
            # Also snapshot existing INIs, even though we preserve them on update.
            names = sorted({f["name"] for f in changed} |
                           {n for n in ALLOWED if n.endswith(".ini") and (root / n).exists()})
            for name in names:
                target = root / name
                regular(target)
                existed = target.exists()
                if existed:
                    shutil.copy2(target, backup / name)
                entries.append({"name": name, "existed": existed,
                                "sha256": digest((backup / name).read_bytes()) if existed else None})
            journal = {"state": "prepared", "version": m["version"], "entries": entries}
            write_json(backup / "journal.json", journal)
            try:
                progress("Installing verified files...")
                for f in changed:
                    ensure_stopped()
                    os.replace(stage / f["name"], root / f["name"])
                journal["state"] = "installed"
                write_json(backup / "journal.json", journal)
            except Exception:
                restore(root, backup)
                raise
            return {"version": m["version"], "changed": len(changed), "backup": backup.name, "from": current}


def rollback(root, backup_id):
    root = Path(root).resolve(strict=True)
    if not re.fullmatch(r"[a-f0-9]{32}", backup_id):
        raise ValueError("Invalid backup ID")
    with locked(root):
        ensure_stopped()
        backup = backup_root(root) / backup_id
        if backup.is_symlink():
            raise ValueError("Unsafe backup directory")
        restore(root, backup)


def steam_roots():
    roots = []
    if os.name == "nt":
        import winreg
        for hive, key, value in ((winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam", "SteamPath"),
                                 (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath")):
            try:
                with winreg.OpenKey(hive, key) as k:
                    roots.append(Path(winreg.QueryValueEx(k, value)[0]))
            except OSError:
                pass
        roots.append(Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Steam")
    else:
        roots.extend((Path.home()/".steam/steam", Path.home()/".local/share/Steam"))
    return roots


def detect_game(roots=None):
    libraries = list(steam_roots() if roots is None else roots)
    for root in list(libraries):
        vdf = root / "steamapps/libraryfolders.vdf"
        if vdf.is_file():
            for value in re.findall(r'"path"\s*"([^"\r\n]+)"', vdf.read_text(encoding="utf-8", errors="replace")):
                libraries.append(Path(value.replace("\\\\", "\\")))
    found = []
    for library in libraries:
        game = library / "steamapps/common/Nioh3"
        if (game / "Nioh3.exe").is_file() and game.resolve() not in found:
            found.append(game.resolve())
    return found[0] if len(found) == 1 else None


def choose_game(explicit=None):
    # A packaged updater shipped beside the game must target that installation first.
    adjacent = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else Path(__file__).resolve().parent
    game = explicit or (adjacent if (adjacent / "Nioh3.exe").is_file() else detect_game())
    while game is None:
        answer = input("Paste the Nioh3 folder (Steam > Manage > Browse local files): ").strip().strip('"')
        if not answer:
            raise ValueError("No game folder selected")
        game = Path(answer)
        if not (game / "Nioh3.exe").is_file():
            print("That folder does not contain Nioh3.exe. Please try again.")
            game = None
    if not (game / "Nioh3.exe").is_file():
        raise ValueError("Selected folder does not contain Nioh3.exe")
    return game.resolve(strict=True)


def wait_stopped():
    announced = False
    while game_running():
        if not announced:
            print("Please close Nioh 3. Waiting here; the updater will continue automatically...", flush=True)
            announced = True
        time.sleep(2)


def pause():
    print("Press a key to close.", flush=True)
    if os.name == "nt" and sys.stdin.isatty():
        import msvcrt
        msvcrt.getwch()
    elif sys.stdin.isatty():
        input()


def count_enabled(no_count=False, config=None, dirs=()):
    """Off with --no-count or [Update] CountUpdate = 0 in the given config, else in a
    Nioh3Updater.ini next to the updater or in the game folder (any file saying 0 wins).
    Never raises: an unreadable file or a bad value means do not count."""
    if no_count:
        return False
    try:
        if config is not None:
            files = [Path(config)]
        else:
            files = [Path(d) / "Nioh3Updater.ini" for d in dirs]
        for f in files:
            if f.is_file():
                settings = configparser.ConfigParser(interpolation=None)
                with f.open(encoding="utf-8-sig") as fh:
                    settings.read_file(fh)
                if not settings.getboolean("Update", "CountUpdate", fallback=True):
                    return False
        return True
    except Exception:
        return False


def manifest_url(explicit=None, config=None):
    """Explicit URL wins; an explicitly requested config must exist and be valid."""
    if explicit:
        return https(explicit)
    if config is not None:
        settings = configparser.ConfigParser(interpolation=None)
        with Path(config).open(encoding="utf-8-sig") as f:
            settings.read_file(f)
        return https(settings.get("Update", "ManifestUrl").strip())
    return MANIFEST_URL


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--game-dir", type=Path)
    p.add_argument("--rollback", metavar="BACKUP_ID")
    p.add_argument("--manifest", metavar="HTTPS_URL", help="Use this HTTPS release manifest")
    p.add_argument("--config", type=Path, help="Read [Update] ManifestUrl from this INI")
    p.add_argument("--no-pause", action="store_true", help="For scripted use")
    p.add_argument("--no-count", action="store_true", help="Do not add this update to the public update count")
    p.add_argument("--self-test", action="store_true", help="Test update/rollback on generated temporary files only")
    args = p.parse_args(argv)
    try:
        if args.self_test:
            from updater_selftest import run
            run(sys.modules[__name__])
            return 0
        print("Nioh 3 Mod Updater")
        endpoint = manifest_url(args.manifest, args.config) if not args.rollback else None
        game = choose_game(args.game_dir)
        print(f"Game folder: {game}")
        wait_stopped()
        if args.rollback:
            rollback(game, args.rollback)
            print("Rollback complete. You can launch Nioh 3.")
        else:
            result = update(game, endpoint)
            print(f"Ready: version {result['version']}; {result['changed']} files updated. You can launch Nioh 3.")
            if result['backup']:
                print(f"Backup ID: {result['backup']}")
                adjacent = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else Path(__file__).resolve().parent
                if endpoint == MANIFEST_URL and count_enabled(args.no_count, args.config, (adjacent, game)):
                    count_update(result.get("from"), result["version"])
        return 0
    except (Exception, KeyboardInterrupt) as e:
        print(f"Update stopped: {e or 'Cancelled'}", file=sys.stderr)
        return 1
    finally:
        if not args.no_pause and not args.self_test:
            pause()


if __name__ == "__main__":
    sys.exit(main())
