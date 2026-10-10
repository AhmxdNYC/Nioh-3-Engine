#!/usr/bin/env python3
"""fightnow_selftest.py - Fight now self-test WITHOUT the pipe or input (Linux / Proton, Steam Deck / ROG Ally, or any PC).

The add-on (Nioh3BossRematch >= 1.3.1) runs the test itself when its ini has [Rematch] SelfTest = arena (or a comma list of row
ids): after the save loads it starts Fight now on every row with an arena point / boss gate that is CLEARED on this save (uncleared
rows are logged 'SELFTEST <id> SKIP' and never started), waits for PASS / FAIL, leaves the mission, and ends with 'SELFTEST DONE'.
This script only does the safe framing around it:

  1. refuses if the game runs; backs up the save folder and the game-folder files it touches,
  2. installs the add-on build + catalogue, sets the test keys ([Rematch] Enable/FightNow/FightNowWarp = 1, SelfTest, [Menu]
     Overlay = 0; perf ini [General] AutoContinueLoad = 1 so the title -> Continue needs no input),
  3. launches the game (--launch command), follows the add-on log, prints each SELFTEST line,
  4. after 'SELFTEST DONE' (or --timeout-min) closes the game (only the pid it found), restores every file and the save folder
     exactly (byte-compared), writes <out>.csv with id, result, load seconds (request -> mission load end) and the log lines.

  python3 tools/rematch/fightnow_selftest.py --game-dir <Nioh3 folder> --save-dir <...KoeiTecmo/NIOH3> --asi <Nioh3BossRematch.asi>
      --catalog <Nioh3BossRematch_bosses.tsv> --launch "steam -applaunch 3681010" [--rows arena] [--timeout-min 120] [--out DIR]
      [--close-cmd "..."] [--dry-run]

Exit 0 = ran (see the csv: PASS / FAIL / SKIP per row), 2 = refused (game running / missing files), 3 = no SELFTEST DONE in time.
"""
import argparse
import csv
import filecmp
import hashlib
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time

ADDON_FILES = ["Nioh3BossRematch.asi", "Nioh3BossRematch.ini", "Nioh3BossRematch_bosses.tsv", "Nioh3EngineFix.ini"]


def log(msg):
    print("[%s] %s" % (time.strftime("%H:%M:%S"), msg), flush=True)


def game_pids():
    pids = []
    if os.name == "nt":
        out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq Nioh3.exe", "/FO", "CSV", "/NH"], capture_output=True, text=True).stdout
        for line in out.splitlines():
            parts = [p.strip('"') for p in line.split('","')]
            if parts and parts[0].lower().startswith("nioh3.exe"):
                pids.append(int(parts[1]))
    else:
        out = subprocess.run(["pgrep", "-f", r"Nioh3\.exe"], capture_output=True, text=True).stdout
        pids = [int(x) for x in out.split()]
        pids = [p for p in pids if "Nioh3.exe" in pathlib.Path("/proc/%d/cmdline" % p).read_bytes().replace(b"\0", b" ").decode("latin1", "ignore")
                and "fightnow_selftest" not in pathlib.Path("/proc/%d/cmdline" % p).read_bytes().decode("latin1", "ignore")]
    return pids


def set_key(text, sec, key, val):
    nl = "\r\n" if "\r\n" in text else "\n"
    m = re.search(r"(?ms)^\[%s\][^\r\n]*\r?\n(.*?)(?=^\[|\Z)" % re.escape(sec), text)
    if not m:
        return text.rstrip() + "%s%s[%s]%s%s = %s%s" % (nl, nl, sec, nl, key, val, nl)
    body = m.group(1)
    rx = re.compile(r"(?m)^%s[ \t]*=.*?(\r?)$" % re.escape(key))
    nb = rx.sub(lambda mm: "%s = %s%s" % (key, val, mm.group(1)), body, count=1) if rx.search(body) else "%s = %s%s%s" % (key, val, nl, body)
    return text[:m.start(1)] + nb + text[m.end(1):]


def tree_digest(root):
    out = {}
    for p in sorted(pathlib.Path(root).rglob("*")):
        if p.is_file():
            out[str(p.relative_to(root))] = hashlib.sha1(p.read_bytes()).hexdigest()
    return out


def mirror(src, dst):
    """make dst an exact copy of src (files only)"""
    src, dst = pathlib.Path(src), pathlib.Path(dst)
    for p in sorted(dst.rglob("*"), reverse=True):
        rel = p.relative_to(dst)
        if not (src / rel).exists():
            p.unlink() if p.is_file() else p.rmdir()
    for p in src.rglob("*"):
        q = dst / p.relative_to(src)
        if p.is_dir():
            q.mkdir(parents=True, exist_ok=True)
        elif not q.exists() or not filecmp.cmp(p, q, shallow=False):
            q.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(p, q)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--game-dir", required=True)
    ap.add_argument("--save-dir", required=True)
    ap.add_argument("--asi", required=True)
    ap.add_argument("--catalog", required=True)
    ap.add_argument("--launch", required=True, help="command that starts the game, e.g. 'steam -applaunch 3681010'")
    ap.add_argument("--close-cmd", default="", help="optional graceful close command; default: terminate the game pid")
    ap.add_argument("--rows", default="arena")
    ap.add_argument("--timeout-min", type=float, default=120)
    ap.add_argument("--out", default="")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    gd, sd = pathlib.Path(a.game_dir), pathlib.Path(a.save_dir)
    stamp = time.strftime("%Y%m%d-%H%M%S")
    out = pathlib.Path(a.out or ("fightnow-selftest-" + stamp))
    for f in (gd, sd, pathlib.Path(a.asi), pathlib.Path(a.catalog)):
        if not f.exists():
            log("missing: %s" % f)
            return 2
    if game_pids():
        log("Nioh3 is running - refusing (nothing touched)")
        return 2
    if a.dry_run:
        log("[dry] would back up %s and %s, install %s + %s, set SelfTest = %s, launch '%s', follow %s" % (
            sd, ", ".join(ADDON_FILES), a.asi, a.catalog, a.rows, a.launch, gd / "Nioh3BossRematch.log"))
        return 0
    (out / "orig").mkdir(parents=True, exist_ok=True)
    save_bak = out / "save-backup"
    shutil.copytree(sd, save_bak)
    save_digest = tree_digest(sd)
    had = {}
    for n in ADDON_FILES:
        p = gd / n
        had[n] = p.exists()
        if had[n]:
            shutil.copy2(p, out / "orig" / n)
    log("backed up the save folder (%d files) and %s" % (len(save_digest), ", ".join(n for n in ADDON_FILES if had[n])))
    rc = 3
    pid = None
    try:
        shutil.copy2(a.asi, gd / "Nioh3BossRematch.asi")
        shutil.copy2(a.catalog, gd / "Nioh3BossRematch_bosses.tsv")
        ini = (out / "orig" / "Nioh3BossRematch.ini").read_text(encoding="utf-8", errors="replace") if had["Nioh3BossRematch.ini"] else ""
        for sec, k, v in (("Rematch", "Enable", "1"), ("Rematch", "FightNow", "1"), ("Rematch", "FightNowWarp", "1"), ("Rematch", "ShowAll", "1"),
                          ("Rematch", "SelfTest", a.rows), ("Menu", "Overlay", "0"), ("Debug", "BossLog", "1")):
            ini = set_key(ini, sec, k, v)
        (gd / "Nioh3BossRematch.ini").write_text(ini, encoding="utf-8")
        if had["Nioh3EngineFix.ini"]:
            pi = (out / "orig" / "Nioh3EngineFix.ini").read_text(encoding="utf-8", errors="replace")
            if re.search(r"(?m)^AutoContinueLoad *=", pi):  # the perf ASI's own key: [General] AutoContinueLoad (wherever it already is)
                pi = re.sub(r"(?m)^AutoContinueLoad *=[^\r\n]*", "AutoContinueLoad = 1", pi)
            else:
                pi = set_key(pi, "General", "AutoContinueLoad", "1")
            (gd / "Nioh3EngineFix.ini").write_text(pi, encoding="utf-8")
        logf = gd / "Nioh3BossRematch.log"
        t_launch = time.time()
        log("launching: %s" % a.launch)
        subprocess.Popen(a.launch, shell=True)
        deadline = t_launch + a.timeout_min * 60
        pos, lines, done = 0, [], False
        while time.time() < deadline:
            time.sleep(5)
            if pid is None:
                ps = game_pids()
                pid = ps[0] if ps else None
                if pid:
                    log("game pid %d" % pid)
            elif pid not in game_pids():
                log("!! the game exited by itself (crash?)")
                break
            if not logf.exists() or logf.stat().st_mtime < t_launch:
                continue
            text = logf.read_text(encoding="utf-8", errors="replace")
            if len(text) < pos:
                pos = 0
            new = text[pos:]
            pos = len(text)
            for ln in new.splitlines():
                if "SELFTEST" in ln or "IN replay mission" in ln or "F FAIL" in ln or "F PASS" in ln:
                    lines.append(ln)
                    log("  " + ln.split("] ", 1)[-1])
                if "SELFTEST DONE" in ln:
                    done = True
            if done:
                rc = 0
                break
        if not done:
            log("no SELFTEST DONE within %.0f min" % a.timeout_min)
    finally:
        if pid and pid in game_pids():
            if a.close_cmd:
                subprocess.run(a.close_cmd, shell=True)
            else:
                try:
                    if os.name == "nt":
                        subprocess.run(["taskkill", "/PID", str(pid)])
                    else:
                        os.kill(pid, 15)
                except Exception as e:
                    log("close: %s" % e)
            for _ in range(60):
                if pid not in game_pids():
                    break
                time.sleep(2)
            if pid in game_pids():
                log("!! game pid %d still running after 120 s - kill it yourself before restoring" % pid)
        time.sleep(5)
        if not game_pids():
            if (gd / "Nioh3BossRematch.log").exists():
                shutil.copy2(gd / "Nioh3BossRematch.log", out / "Nioh3BossRematch.log")
            for n in ADDON_FILES:
                if n in ("Nioh3BossRematch.asi", "Nioh3BossRematch_bosses.tsv"):
                    continue  # the build under test stays installed (its ini is restored, so SelfTest is off again)
                if had[n]:
                    shutil.copy2(out / "orig" / n, gd / n)
                elif (gd / n).exists():
                    (gd / n).unlink()
            mirror(save_bak, sd)
            same = tree_digest(sd) == save_digest
            log("restored the inis and the save folder; save byte-identical to the backup: %s" % same)
            if not same:
                log("!! SAVE DIFFERS - the backup is kept at %s" % save_bak)
        else:
            log("!! the game is still running: NOT restoring (backup at %s)" % save_bak)
    rows, loads, cur = [], {}, None
    for ln in lines:
        m = re.search(r"SELFTEST (\S+) \(\d+/\d+\): Fight now", ln)
        if m:
            cur = m.group(1)
        m = re.search(r"IN replay mission .* the load itself ended ([\d.]+) s", ln)
        if m and cur:
            loads[cur] = m.group(1)
        m = re.search(r"SELFTEST (\S+) (PASS|FAIL|SKIP)(.*)$", ln)
        if m:
            rows.append({"id": m.group(1), "result": m.group(2), "load_s": loads.get(m.group(1), ""), "detail": m.group(3).strip()})
    with open(str(out) + ".csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["id", "result", "load_s", "detail"])
        w.writeheader()
        w.writerows(rows)
    log("csv: %s.csv (%d rows: %d PASS, %d FAIL, %d SKIP)" % (out, len(rows), sum(r["result"] == "PASS" for r in rows),
                                                            sum(r["result"] == "FAIL" for r in rows), sum(r["result"] == "SKIP" for r in rows)))
    return rc


if __name__ == "__main__":
    sys.exit(main())
