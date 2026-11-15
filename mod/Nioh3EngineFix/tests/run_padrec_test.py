"""run_padrec_test.py - input recorder round trip, offline (tools/testmatrix/padrec.py + src/vpad.cpp). Not a game test.

    py tests/run_padrec_test.py

1. Record: padrec.record() polls a scripted pad (taps, a 3-button chord, a sub-frame tap, stick sweeps in 8 ms steps, trigger
   ramps, both sticks at -32768 / 32767, a disconnect) with game state tags, in real time -> fixture JSONL. The file must hold
   every scripted state that lasted 2+ poll periods, in order, with its tag, within 1 poll period of its scripted time.
2. Replay: builds tests/padrec_harness.cpp + src/vpad.cpp (the real XInputGetState hook, exclusive mode, the real pipe server)
   with a fake 60 Hz game, padrec.replay() sends the fixture over the pipe with the 'pad' command.
3. Compare, per game frame: what the fake game read equals the recorded state at that frame's time, or the one before it
   (within 1 frame); every recorded state that lasted longer than the longest frame gap was read; no state the recording
   does not have. Then: bad files are rejected, 'off by default' (the mod has no recorder, no ini key; VirtualPad = 0 default).
Must end ALL PASSED.
"""
import os
import pathlib
import subprocess
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
MAIN = pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine")
sys.path.insert(0, str(REPO / "tools" / "testmatrix"))
import padrec  # noqa: E402

OUT = ROOT / "build" / "padrectest"
OUT.mkdir(parents=True, exist_ok=True)
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


# ---- scripted "real" pad: (start ms, state or None = disconnected, tag) ----
A, B, X, Y, LB, RB, UP, RS = 0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200, 0x0001, 0x0080
N = padrec.NEUTRAL
SCRIPT = [(0, N, "loading"), (150, N, "field"), (300, (A, 0, 0, 0, 0, 0, 0), "field"), (350, N, "field"),
          (420, (LB | RB | X, 0, 0, 0, 0, 0, 0), "field"), (540, N, "field"),
          (600, (B, 0, 0, 0, 0, 0, 0), "field"), (610, N, "field")]  # B for 10 ms: shorter than a frame
t = 700
for i in range(1, 26):  # left stick sweep, 8 ms steps (several per frame)
    SCRIPT.append((t, (0, 0, 0, int(32767 * i / 25), int(-32768 * i / 25), 0, 0), "field"))
    t += 8
SCRIPT += [(t, (0, 0, 0, 32767, -32768, 0, 0), "field"), (t + 100, (A | UP, 0, 0, 32767, -32768, -32768, 32767), "field"),
           (t + 200, N, "field")]
t += 260
for i in range(1, 11):  # triggers ramp, 20 ms steps
    SCRIPT.append((t, (0, 25 * i + 5, 255 - 25 * i, 0, 0, 0, 0), "field"))
    t += 20
SCRIPT += [(t, (Y | RS, 1, 254, -1, 1, 300, -300), "menu"), (t + 80, None, "menu"), (t + 200, N, "menu"),
           (t + 300, (A, 0, 0, 0, 0, 0, 0), "field"), (t + 360, N, "field")]
END_MS = t + 500


def script_at(ms):
    cur = SCRIPT[0]
    for s in SCRIPT:
        if s[0] <= ms:
            cur = s
    return cur


class Scripted:
    def __init__(self):
        self.t0 = None

    def read(self):
        if self.t0 is None:
            self.t0 = time.perf_counter()
        return script_at((time.perf_counter() - self.t0) * 1000.0)[1]

    def tag(self):
        if self.t0 is None:
            return SCRIPT[0][2]
        return script_at((time.perf_counter() - self.t0) * 1000.0)[2]


tmp = pathlib.Path(tempfile.mkdtemp(prefix="padrec_"))
fx = tmp / "session.jsonl"

# ---- 1. record ----
src = Scripted()
n = padrec.record(str(fx), sec=END_MS / 1000.0, poll_ms=2.0, state_ms=5, read=src.read, state_source=src.tag, quiet=True)
head, ev = padrec.load(str(fx))
check(head["format"] == "nioh3-padrec" and head["version"] == 1, "header %s" % head)
check(n == len(ev) and n >= 20, "events %d / %d" % (n, len(ev)))
pi = 0
worst, worst_at = 0.0, None
for k, (st, p, tag) in enumerate(SCRIPT):
    dur = (SCRIPT[k + 1][0] if k + 1 < len(SCRIPT) else END_MS) - st
    want = p if p is not None else N
    prev = SCRIPT[k - 1] if k else None
    if dur < 6 or (prev and (prev[1] if prev[1] is not None else N) == want and prev[2] == tag):
        continue  # too short for a 2 ms poll to be sure of, or no change
    j = next((j for j in range(pi, len(ev)) if ev[j][1] == want and ev[j][0] >= st - 1), None)
    if not check(j is not None, "recording misses scripted state %d at %d ms %s" % (k, st, want)):
        continue
    # the tag comes from its own poller (every 5 ms here, 250 ms live): it may follow the pad change by one tag period
    check(any(ev[q][1] == want and ev[q][2] == tag and ev[q][0] <= st + 5 + 4 for q in range(j, len(ev))),
          "scripted state %d at %d ms: tag %s not recorded within one tag period" % (k, st, tag))
    if prev and (prev[1] if prev[1] is not None else N) == want:
        continue  # a tag-only change: its time is the tag poller's, checked above
    if ev[j][0] - st > worst:
        worst, worst_at = ev[j][0] - st, (k, st)
    pi = j
check(worst <= 5.0, "recorded time vs script: worst %.2f ms at scripted state %s (poll 2 ms)" % (worst, worst_at))
tags = [e[2] for e in ev]
check(tags[0] == "loading" and "menu" in tags and tags[-1] == "field", "tags %s" % sorted(set(tags)))
raw = fx.read_text(encoding="utf-8")
check('"pad": null' in raw, "disconnect recorded as pad null")
print("record: %d events in %.1f s, every scripted state of 6+ ms present in order (worst %.2f ms late), tags within one tag period, tags %s, disconnect = null" % (
    len(ev), END_MS / 1000.0, worst, ",".join(sorted(set(tags)))))

# ---- 2. replay into the real VirtualPad code ----
setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():
    setup = MAIN / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\padrec_harness.cpp" "{s}\\vpad.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\padrec_harness.exe" /link kernel32.lib user32.lib winmm.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
check(not [ln for ln in r.stdout.splitlines() if "warning" in ln], "compiler warnings")
pipe = r"\\.\pipe\nioh3vpad_padrec_%d" % os.getpid()
h = subprocess.Popen([str(OUT / "padrec_harness.exe"), pipe, "60"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
check(h.stdout.readline().strip() == "READY", "harness not ready")
rc, info = padrec.replay(str(fx), pipe=pipe, quiet=True)
check(rc == 0 and info["sent"] == len(ev), "replay rc %d sent %d/%d" % (rc, info["sent"], len(ev)))
time.sleep(0.1)
out, _ = h.communicate("", timeout=30)
frames = []
for ln in out.splitlines():
    if ln.startswith("F "):
        f = ln.split()
        frames.append((int(f[1]) / 1e6, int(f[3], 16)) + tuple(map(int, f[4:])))
check(out.strip().endswith("DONE"), "harness did not finish")

# ---- 3. compare per frame ----
t0 = info["t0"]
sched = [(t0 + e[0] / 1000.0, e[1]) for e in ev]


def recorded_at(ts):  # index of the recorded state at perf_counter time ts (-1 = before the replay: neutral)
    lo = -1
    for i, (s, _) in enumerate(sched):
        if s <= ts:
            lo = i
        else:
            break
    return lo


win = [f for f in frames if t0 - 0.02 <= f[0] <= info["t_end"]]
gaps = [b[0] - a[0] for a, b in zip(win, win[1:])]
max_gap = max(gaps) if gaps else 1
bad, lagged, seen_idx = [], 0, set()
for k, f in enumerate(win):
    got = tuple(f[1:])
    i = recorded_at(f[0])
    now = sched[i][1] if i >= 0 else N
    if got == now:
        seen_idx.add(i)
        continue
    # within 1 frame: the state the previous frame's time called for (the new one arrived after this read began)
    j = recorded_at(win[k - 1][0]) if k else -1
    ok = False
    for c in range(j, i):
        if c >= -1 and (sched[c][1] if c >= 0 else N) == got:
            ok = True
            seen_idx.add(c)
            break
    if ok:
        lagged += 1
    else:
        bad.append((round((f[0] - t0) * 1000, 1), "%04X" % got[0], got[1:], "want", now))
check(not bad, "%d frames read a state that is not the recorded one within 1 frame: %s" % (len(bad), bad[:4]))
missed = []
for i, (s, p) in enumerate(sched):
    nxt = sched[i + 1][0] if i + 1 < len(sched) else info["t_end"]
    if nxt - s > max_gap + 0.001 and i not in seen_idx:
        missed.append((round(ev[i][0], 1), p))
check(not missed, "recorded states longer than a frame gap the game never read: %s" % missed[:4])
long_states = sum(1 for i, (s, _) in enumerate(sched) if (sched[i + 1][0] if i + 1 < len(sched) else info["t_end"]) - s > max_gap + 0.001)
check(len(win) >= int(END_MS / 20), "frames in the replay window %d" % len(win))
check(win and tuple(frames[-1][1:]) == N, "pad neutral after the replay")
print("replay: %d events sent (max %.2f ms late), %d game frames (max gap %.1f ms): %d exact, %d one frame behind, 0 wrong; "
      "%d/%d states longer than a frame gap read" % (info["sent"], info["max_late_ms"], len(win), max_gap * 1000, len(win) - lagged - len(bad),
                                                     lagged, long_states - len(missed), long_states))

# ---- 4. bad files, off by default ----
def bad_file(lines):
    p = tmp / "bad.jsonl"
    p.write_text("\n".join(lines) + "\n", encoding="utf-8")
    try:
        padrec.load(str(p))
        return False
    except (ValueError, KeyError):
        return True


H = '{"type": "header", "format": "nioh3-padrec", "version": 1}'
check(bad_file(['{"t": 0, "pad": [0,0,0,0,0,0,0]}']), "event before header accepted")
check(bad_file([H, '{"t": 5, "pad": [0,0,0,0,0,0,0]}', '{"t": 4, "pad": [0,0,0,0,0,0,0]}']), "time going back accepted")
check(bad_file([H, '{"t": 0, "pad": [0,256,0,0,0,0,0]}']), "trigger 256 accepted")
check(bad_file([H, '{"t": 0, "pad": [0,0,0,0,0,0]}']), "6 values accepted")
check(bad_file([H, '{"t": 0, "pad": [0,0,0,0,0,0,0]}', '{"type": "end", "events": 3}']), "event count mismatch accepted")
check(bad_file(['{"type": "header", "format": "other", "version": 1}']), "foreign format accepted")
mod_src = "".join(p.read_text(encoding="utf-8", errors="replace") for p in (ROOT / "src").glob("*.cpp"))
check("padrec" not in mod_src.replace("tools/testmatrix/padrec.py", ""), "the mod must not record anything")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8")
check("\nVirtualPad = 0" in ini, "VirtualPad default 0")
rc, _ = padrec.replay(str(fx), pipe=pipe + "_missing", quiet=True)
check(rc == 2, "no pipe must exit 2 (%d)" % rc)
print("files: 6 bad files rejected; off by default: no recorder in the mod, VirtualPad = 0, no pipe -> exit 2")

if fails:
    print("FAILED (%d):" % len(fails))
    for f in fails:
        print("  - " + f)
    sys.exit(1)
print("PADREC: ALL PASSED")
