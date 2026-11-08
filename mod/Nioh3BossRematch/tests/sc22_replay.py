#!/usr/bin/env python3
"""Offline replay of soul-core report #22 (human session 2026-11-04 20:01-20:02, public 1.8.5) against the pc/soulcore-22 rules.

Never touches the game. Reads the captured BossRematch logs, extracts the game's own events (accept = HasSkill(0xEDC9) ask, seal/throw
action edges, seal orb, spawn hits, FormSlot0 captures) and applies the decision rules of bosslog.cpp (same constants as the code
defaults) to show which path each press takes now. It also checks that the rules are really in the source.

usage: py -I tests/sc22_replay.py [repo root]   (default: the repo that contains this file; logs from captures/ and Temp/)
"""
import re
import sys
from pathlib import Path

here = Path(__file__).resolve().parent
src = (here.parent / "src" / "bosslog.cpp").read_text(encoding="utf-8")
root = Path(sys.argv[1]) if len(sys.argv) > 1 else here.parents[2]
main = Path("C:/Users/Ahmad Hamza/nioh3-engine")

CONFIRM_MS, ORB_SPAWN_MS, NOSPAWN_MS, NEED_USE_MS = 200, 1800, 400, 2200

def find(rel):
    for b in (root, main):
        if (b / rel).exists():
            return b / rel
    return None

def secs(ts):
    h, m, s = ts.split(":")
    return int(h) * 3600 + int(m) * 60 + float(s)

def events(path):
    ev = []
    prev_use = False
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = re.match(r"\[(\d\d:\d\d:\d\d\.\d+)\] (.*)", line)
        if not m:
            continue
        t, body = secs(m.group(1)), m.group(2)
        if "soul-core use accepted from act" in body:
            ev.append((t, "accept", re.search(r"act 0x([0-9A-F]+)", body).group(1)))
        elif body.startswith("OrbSkip: orb effect 0x6254D"):
            ev.append((t, "orb", ""))
        elif body.startswith("FormKey seal spawn hit"):
            ev.append((t, "spawn", re.search(r"desc\[0\]=([0-9A-F]+)", body).group(1)))
        elif body.startswith("FormSlot0 CAPTURE seal"):
            ev.append((t, "capture", re.search(r"chr=([0-9A-F]+)", body).group(1)))
        elif body.startswith("FormHide=3 player action"):
            a, b = re.search(r"0x([0-9A-F]+) -> 0x([0-9A-F]+)", body).groups()
            use_b = int(b, 16) >> 8 == 0x28
            if use_b and not prev_use:
                ev.append((t, "sealact", b))
            if prev_use and not use_b:
                ev.append((t, "throwdone", b))
            prev_use = use_b
        elif body.startswith("FormHide=4 end "):
            ev.append((t, "end", body[len("FormHide=4 end "):]))
        elif "-> [SealDur]" in body or "-> [SealHits]" in body or "-> [SealLastHit]" in body:
            ev.append((t, "learn", body))
    return ev

def after(ev, t0, kind, within):
    return [e for e in ev if e[1] == kind and t0 - 0.05 <= e[0] <= t0 + within]

fails = []
def check(ok, what):
    print(("  ok   " if ok else "  FAIL ") + what)
    if not ok:
        fails.append(what)

print("source rules present:")
for needle in ("RunThrow: closed - no seal orb / throw", "SealOrbSpawnMs", "press hide binds the first fresh seal",
               "g_fh4.keepSeal = true;\n                Log(\"FormHide=4 press hide", "!UseConfirmedSince(g_pressSnap.t)",
               "g_orbHooked.load() ? (std::max)(g_orbT.load(), g_sealEdgeT)", "!strcmp(g_fh4EndWhy, \"seal-despawn\")",
               "FormKeySwap", "form ended: %s"):
    check(needle in src, needle.split("\n")[0][:70])
check("WritePrivateProfileStringW(L\"SealDur\"" not in src and "SealLastHitFor(" not in src, "no [SealDur]/[SealLastHit] learned timing left")

sc22 = find("captures/sc22/Nioh3BossRematch.log")
if not sc22:
    print("captures/sc22/Nioh3BossRematch.log not found"); sys.exit(2)
ev = events(sc22)
print(f"\nreport #22 log {sc22} ({len(ev)} events)")
accepts = [e for e in ev if e[1] == "accept"]
print(f"  {len(accepts)} accepts (d-pad presses)")
real, refused = [], []
for t, _, act in accepts:
    conf = after(ev, t, "orb", CONFIRM_MS / 1000) or after(ev, t, "sealact", CONFIRM_MS / 1000)
    (real if conf else refused).append(t)
    tag = "used by the game (orb/throw)" if conf else f"REFUSED by the game -> RunThrow x8 closes at +{CONFIRM_MS} ms (was up to 1.2 s)"
    print(f"    {t % 60:6.3f} act 0x{act}: {tag}")
check(len(real) == 3 and len(refused) == 7, "3 real uses, 7 refused presses (Training saw 3 item uses: 37.23, 42.93, 15.39)")

# (b) stale press snapshot: v2 kept the snapshot of a refused press for 2.5 s
snap_v2, snap_new = [], []
last_v2 = last_new = -100
sigs = sorted([e[0] for e in ev if e[1] in ("accept", "sealact")])
for s in sigs:
    if s - last_v2 > 2.5: last_v2 = s; snap_v2.append(s)
    unused = s - last_new > 0.3 and not (after(ev, last_new, "orb", s - last_new) or after(ev, last_new, "sealact", s - last_new - 0.06))
    if s - last_new > 2.5 or (last_new > 0 and unused): last_new = s; snap_new.append(s)
t_real2 = real[1]
v2_age = t_real2 - max(x for x in snap_v2 if x <= t_real2 + 1e-6)
new_age = t_real2 - max(x for x in snap_new if x <= t_real2 + 1e-6)
print(f"  press {t_real2 % 60:.3f}: v2 snapshot from {v2_age:.2f} s before (a refused press), new {new_age:.2f} s")
check(v2_age > 2.0 and new_age < 0.05, "real press after a refused one gets its own actor snapshot (pooled 214C6A523F0 not rejected)")

# (b)/(c) 20:02:02.35: 16127 with no orb, no throw, no item use
cap = [e for e in ev if e[1] == "capture"][0]
orbs = [e[0] for e in ev if e[1] in ("orb", "sealact") and e[0] <= cap[0]]
acc_before = [t for t in [a[0] for a in accepts] if t <= cap[0]]
v2_gap, new_gap = cap[0] - max(acc_before + orbs), cap[0] - max(orbs)
print(f"  capture {cap[0] % 60:.3f}: last accept {v2_gap:.2f} s before (v2 margin {NEED_USE_MS/1000 - v2_gap:+.2f} s), last real use {new_gap:.2f} s before")
check(new_gap > NEED_USE_MS / 1000 and v2_gap > NEED_USE_MS / 1000, "20:02:02.35 16127 (no orb/throw/item) begins no form")

# press-hide release: wait for the yokai after the orb
def release(t_press):
    orb = after(ev, t_press, "orb", 0.3)
    done = after(ev, t_press, "throwdone", 1.0)
    spawn = [e for e in ev if e[1] == "spawn" and t_press < e[0] < t_press + 2.5]
    v2_at = done[0][0] + NOSPAWN_MS / 1000 if done else None
    use = after(ev, t_press, "sealact", 1.0)
    use_t = max(orb[0][0], use[-1][0] if use else 0) if orb else 0
    new_at = max(orb[0][0] + ORB_SPAWN_MS / 1000, use_t + NEED_USE_MS / 1000) if orb else v2_at
    return orb, spawn, v2_at, new_at
for t in real[1:]:
    orb, spawn, v2_at, new_at = release(t)
    sp = spawn[0][0] if spawn else None
    print(f"  press {t % 60:.3f}: orb +{(orb[0][0]-t)*1000:.0f} ms, spawn {'+%.2f s' % (sp - t) if sp else 'none'}, "
          f"v2 releases +{v2_at - t:.2f} s, new +{new_at - t:.2f} s")
    if sp:
        check(v2_at < sp <= new_at, f"{t % 60:.3f}: v2 popped the player back before the yokai; now the yokai binds inside the press hide")
        later = [e for e in ev if e[1] == "capture" and sp + 0.05 < e[0] < sp + 1.0]
        check(bool(later), f"{t % 60:.3f}: a later capture ({later[0][2] if later else '-'}) comes in -> keepSeal keeps 16127 (was capture-change)")
    else:
        check(new_at - t < 2.5, f"{t % 60:.3f}: orb but no yokai: released +{new_at - t:.2f} s (1.8.5: 2.5 s press-no-seal)")

# (c) learning only from a complete form
for t, _, why in [e for e in ev if e[1] == "end"]:
    learned = [e for e in ev if e[1] == "learn" and t <= e[0] <= t + 0.1]
    if learned:
        ok_now = why == "seal-despawn"
        print(f"  end {t % 60:.3f} '{why}': 1.8.5 learned {learned[0][2][:60]}... -> now {'learned' if ok_now else 'nothing learned'}")
        check(why != "seal-despawn", f"end '{why}' is not a complete form: no [SealDur]/[SealHits] write now")

# other sessions: orb -> spawn latency of every use must fit SealOrbSpawnMs
for rel in ("Temp/t66107/logs/Nioh3BossRematch.log", "Temp/t66029/logsC/Nioh3BossRematch.log"):
    p = find(rel)
    if not p:
        print(f"\n{rel}: not found, skipped"); continue
    e2 = events(p)
    lat = []
    for t, _, _ in [e for e in e2 if e[1] == "orb"]:
        sp = [e for e in e2 if e[1] == "spawn" and t <= e[0] <= t + 2.5]
        if sp: lat.append(sp[0][0] - t)
    print(f"\n{rel}: {len(lat)} orb->spawn latencies, max {max(lat):.2f} s")
    check(max(lat) < ORB_SPAWN_MS / 1000, f"every yokai came within SealOrbSpawnMs {ORB_SPAWN_MS} ms of its orb")

print("\nFAIL: %d" % len(fails) if fails else "\nALL OK")
sys.exit(1 if fails else 0)
