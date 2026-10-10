r"""extraperf_ab.py - live A/B of the "Extra performance" options (ASI >= 1.7.59, notes\extra-performance-settings.md).
Cross-platform (Windows pc2 / Linux Ally under Proton): it does not launch or close the game. Run it while the game is IN THE
FIELD at a fixed spot, no camera input, with [Debug] LiveCtl = 1 and [ExtraPerf] Enabled = 1 in the ini.

    py -I tools\testmatrix\extraperf_ab.py run --game-dir <Nioh3 folder> --out <dir> [--items XCLD=0/3,XFOG=0/3,...]
          [--pairs 2] [--warm 6] [--window 20] [--settle 20] [--shot-cmd "<command with {path}>"] [--device pc2]
    py -I tools\testmatrix\fastab_report.py <dir>        (copy the launch's PresentMon v2 --date_time CSV in as presentmon*.csv first)
    py -I tools\testmatrix\extraperf_ab.py shots <dir>   (A-vs-B screenshot diff per item -> shots.md; needs Pillow)
    py -I tools\testmatrix\extraperf_ab.py selftest      (fake game answering the control file -> manifest -> fastab_report)

Per item ABBA pairs (A = first value, B = second): write <game-dir>\Nioh3EngineFix_live.ini ([Live] Gen / Tag / KEY = v),
wait for the .ack (gen echo, fail = 0), warm-up, then a measuring window whose fps comes from the mod's own frame clock (ack
frames / qpc_us, the Present hook) -> manifest.jsonl rows in fastab.ps1's format, so fastab_report.py gives the paired table
and the [RESULT] lines. Screenshot after the first A window and the first B window of each item (shots\<item>_A/B.png).
Default items: every option at its lowest level, then both presets. Ends with every key back at 0.
Windows default screenshot: the primary screen via PowerShell CopyFromScreen (the game must be the foreground, borderless).
Linux: pass --shot-cmd (e.g. "grim {path}" or the gamescope screenshot tool), or --no-shots.
"""
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_ITEMS = ("XCLD=0/3,XFOG=0/3,XGI=0/3,XSHD=0/3,XSHF=0/2,XLOD=0/3,XTER=0/3,XPRT=0/4,XAO=0/1,XPOST=0/2,"
                 "XPRE=0/1,XPRE=0/2")


def utc():
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"


class Live:
    def __init__(self, game_dir, log):
        self.ctl = os.path.join(game_dir, "Nioh3EngineFix_live.ini")
        self.ack = self.ctl + ".ack"
        self.gen = int(time.time()) % 100000  # never equal to an older control file's Gen
        self.log = log

    def read_ack(self):
        h = {}
        for _ in range(25):  # the ack is rewritten every 500 ms: retry a read that caught it mid-replace (missing / empty)
            try:
                t = open(self.ack, encoding="utf-8", errors="replace").read()
            except OSError:
                t = ""
            h = {}
            for kv in t.split():
                if "=" in kv:
                    k, v = kv.split("=", 1)
                    h[k] = v
            if "frames" in h and "qpc_us" in h:
                return h
            time.sleep(0.02)
        return h

    def set(self, kv, tag, wait=8.0):
        self.gen += 1
        lines = ["[Live]", "Gen = %d" % self.gen, "Tag = %s" % tag] + ["%s = %s" % (k, v) for k, v in kv.items()]
        tmp = self.ctl + ".w"
        with open(tmp, "w", encoding="utf-8", newline="\r\n") as f:
            f.write("\n".join(lines) + "\n")
        os.replace(tmp, self.ctl)
        t0 = time.time()
        while time.time() - t0 < wait:
            a = self.read_ack()
            if a.get("gen") == str(self.gen):
                a["good"] = a.get("fail", "1") == "0"
                return a
            time.sleep(0.1)
        raise SystemExit("no LiveCtl ack for gen %d (%s) within %.0f s - is [Debug] LiveCtl = 1 and the game running?" % (self.gen, tag, wait))


def shoot(cmd, path, log):
    try:
        if cmd:
            subprocess.run(cmd.replace("{path}", path), shell=True, timeout=30)
        elif os.name == "nt":
            ps = ("Add-Type -AssemblyName System.Windows.Forms,System.Drawing; $b=[System.Windows.Forms.Screen]::PrimaryScreen.Bounds; "
                  "$i=New-Object System.Drawing.Bitmap $b.Width,$b.Height; $g=[System.Drawing.Graphics]::FromImage($i); "
                  "$g.CopyFromScreen($b.Location,[System.Drawing.Point]::Empty,$b.Size); $i.Save('%s'); $g.Dispose(); $i.Dispose()" % path)
            subprocess.run(["powershell", "-NoProfile", "-Command", ps], timeout=30)
        else:
            return False
        return os.path.exists(path)
    except Exception as e:  # a screenshot is extra; never lose the measurement over it
        log("screenshot failed: %s" % e)
        return False


def parse_items(s):
    out = []
    for it in [x.strip() for x in s.split(",") if x.strip()]:
        k, ab = it.split("=", 1) if "=" in it else (it, "0/1")
        a, b = ab.split("/", 1)
        out.append(dict(id="%s=%s/%s" % (k, a, b), key=k, a=a, b=b))
    return out


def run(args):
    import argparse
    ap = argparse.ArgumentParser(prog="extraperf_ab.py run")
    ap.add_argument("--game-dir", default=r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3")
    ap.add_argument("--out", default="")
    ap.add_argument("--items", default=DEFAULT_ITEMS)
    ap.add_argument("--pairs", type=int, default=2)
    ap.add_argument("--warm", type=float, default=6.0)
    ap.add_argument("--window", type=float, default=20.0)
    ap.add_argument("--settle", type=float, default=20.0)
    ap.add_argument("--shot-cmd", default="")
    ap.add_argument("--no-shots", action="store_true")
    ap.add_argument("--device", default=os.environ.get("UO_AGENT_ID", "pc"))
    ap.add_argument("--tag", default="extraperf")
    o = ap.parse_args(args)
    out = o.out or os.path.join(os.environ.get("TEMP", "/tmp"), "extraperf_" + time.strftime("%Y%m%d-%H%M"))
    os.makedirs(os.path.join(out, "shots"), exist_ok=True)
    man = open(os.path.join(out, "manifest.jsonl"), "a", encoding="utf-8")
    logf = open(os.path.join(out, "sched.log"), "a", encoding="utf-8")

    def log(s):
        line = "[%s] %s" % (time.strftime("%H:%M:%S"), s)
        print(line, flush=True)
        logf.write(line + "\n")
        logf.flush()

    def jl(d):
        man.write(json.dumps(d) + "\n")
        man.flush()

    plan = parse_items(o.items)
    keys = sorted({p["key"] for p in plan})
    jl(dict(kind="meta", tag=o.tag, device=o.device, asi="1.7.59+", stage="fixed spot (in the field, no camera input)",
            windows_per_item=2 * o.pairs, window_s=o.window, warm_s=o.warm, config="as installed", items=[p["id"] for p in plan]))
    live = Live(o.game_dir, log)
    a = live.set({k: 0 for k in keys}, "init_all_0")
    if not a["good"]:
        raise SystemExit("LiveCtl refused the initial values (ack: %s) - [ExtraPerf] Enabled = 1? ASI >= 1.7.59?" % a)
    jl(dict(kind="event", event="field", t_utc=utc()))
    log("all %d keys at 0, settle %.0f s" % (len(keys), o.settle))
    time.sleep(o.settle)
    jl(dict(kind="event", event="first_window", t_utc=utc()))
    for p in plan:
        log("item %s: %d pairs, A = %s, B = %s" % (p["id"], o.pairs, p["a"], p["b"]))
        shot_done = set()
        for q in range(o.pairs):
            for arm in (("A", "B") if q % 2 == 0 else ("B", "A")):
                v = p["a"] if arm == "A" else p["b"]
                tag = "%s_%s_q%d_%s" % (p["key"], v, q, arm)
                ak = live.set({p["key"]: v}, tag)
                time.sleep(o.warm)
                w0, c0 = utc(), live.read_ack()
                time.sleep(o.window)
                w1, c1 = utc(), live.read_ack()
                clk = {}
                for k in ("frames", "qpc_us", "ft_sq_us", "h20", "h33", "h50"):
                    if k in c0 and k in c1:
                        clk["mod_" + k] = float(c1[k]) - float(c0[k])
                jl(dict(kind="window", item=p["id"], key=p["key"], pair=q, arm=arm, value=v, a_value=p["a"], b_value=p["b"],
                        tag=tag, gen=live.gen, t0_utc=w0, t1_utc=w1, ack_ok=bool(ak["good"]), readback=ak.get(p["key"], ""),
                        warm_s=o.warm, dwell_ended=False, clock=clk))
                fps = clk.get("mod_frames", 0) * 1e6 / clk["mod_qpc_us"] if clk.get("mod_qpc_us") else 0
                log("  %s: %s = %s (ack %s) %.1f fps" % (tag, p["key"], v, "ok" if ak["good"] else "NOT OK", fps))
                if not o.no_shots and arm not in shot_done:
                    name = "%s_%s_%s.png" % (p["key"], v, arm)
                    ok = shoot(o.shot_cmd, os.path.join(out, "shots", name), log)
                    jl(dict(kind="shot", item=p["id"], arm=arm, value=v, file="shots/" + name, ok=ok, t_utc=utc()))
                    shot_done.add(arm)
        live.set({p["key"]: 0}, "%s_back_to_0" % p["key"])
    live.set({k: 0 for k in keys}, "all_back_to_0")
    jl(dict(kind="event", event="last_window", t_utc=utc()))
    log("done -> %s (next: copy the PresentMon CSV in as presentmon_*.csv, then fastab_report.py %s and extraperf_ab.py shots %s)"
        % (out, out, out))
    return 0


def shots(d):
    try:
        from PIL import Image, ImageChops
    except ImportError:
        print("Pillow not installed - shots.md skipped")
        return 0
    rows = [json.loads(x) for x in open(os.path.join(d, "manifest.jsonl"), encoding="utf-8") if x.strip()]
    by = {}
    for r in rows:
        if r.get("kind") == "shot" and r.get("ok"):
            by.setdefault(r["item"], {})[r["arm"]] = os.path.join(d, r["file"])
    L = ["# Extra performance A-vs-B screenshots: %s" % os.path.basename(os.path.normpath(d)), "",
         "| item | A shot | B shot | mean abs diff (0-255) | pixels changed > 8/255 | worst 16x16 cell changed |", "|---|---|---|---|---|---|"]
    for it, s in by.items():
        if "A" not in s or "B" not in s:
            continue
        a, b = Image.open(s["A"]).convert("L"), Image.open(s["B"]).convert("L")
        if a.size != b.size:
            continue
        diff = ImageChops.difference(a, b)
        px = list(diff.getdata())
        mad = sum(px) / len(px)
        chg = 100.0 * sum(1 for x in px if x > 8) / len(px)
        w, h = diff.size
        worst = 0.0
        for gy in range(16):
            for gx in range(16):
                cell = diff.crop((gx * w // 16, gy * h // 16, (gx + 1) * w // 16, (gy + 1) * h // 16))
                cp = list(cell.getdata())
                worst = max(worst, 100.0 * sum(1 for x in cp if x > 8) / max(1, len(cp)))
        L.append("| %s | %s | %s | %.2f | %.1f %% | %.1f %% |" % (it, os.path.relpath(s["A"], d), os.path.relpath(s["B"], d), mad, chg, worst))
    L.append("")
    L.append("A-vs-A noise floor (TAA jitter, wind, idle animation) is not in this table: compare against a BASE pair if one was taken.")
    open(os.path.join(d, "shots.md"), "w", encoding="utf-8").write("\n".join(L) + "\n")
    print("\n".join(L))
    return 0


def selftest():
    import tempfile
    import threading
    d = tempfile.mkdtemp(prefix="epab_")
    game = os.path.join(d, "game")
    out = os.path.join(d, "out")
    os.makedirs(game)
    stop = threading.Event()

    def fake_game():  # answers the control file like the mod: ack with gen, ok/fail, readback and a frame clock
        frames, qpc, last = 0, 0, None
        cur = {}
        while not stop.is_set():
            time.sleep(0.05)
            ctl = os.path.join(game, "Nioh3EngineFix_live.ini")
            try:
                t = open(ctl, encoding="utf-8").read()
            except OSError:
                continue
            kv = dict(l.split(" = ", 1) for l in t.splitlines() if " = " in l)
            for k, v in kv.items():
                if k not in ("Gen", "Tag"):
                    cur[k] = int(v)
            fps = 60.0 * (1.0 + 0.02 * sum(cur.values()))  # every level adds 2 % fps
            frames += fps * 0.05
            qpc += 50000
            open(os.path.join(game, "Nioh3EngineFix_live.ini.ack"), "w").write(
                "gen=%s tag=%s ok=1 fail=0 frames=%d qpc_us=%d ft_sq_us=0 h20=0 h33=0 h50=0 %s" % (
                    kv.get("Gen"), kv.get("Tag"), frames, qpc, " ".join("%s=%d" % kv2 for kv2 in cur.items())))
    th = threading.Thread(target=fake_game, daemon=True)
    th.start()
    try:
        run(["--game-dir", game, "--out", out, "--items", "XCLD=0/3,XPRE=0/2", "--pairs", "2", "--warm", "0.3",
             "--window", "2.5", "--settle", "0.2", "--no-shots", "--device", "selftest"])
    finally:
        stop.set()
        th.join(2)
    sys.path.insert(0, HERE)
    import fastab_report
    res, _ = fastab_report.report(out)
    f = res["XCLD=0/3"]["fps"]
    assert f and f["n"] == 2 and f["delta"] > 0, f
    print("extraperf_ab selftest OK")
    return 0


if __name__ == "__main__":
    a = sys.argv[1:]
    if not a or a[0] not in ("run", "shots", "selftest"):
        print(__doc__)
        sys.exit(2)
    sys.exit(run(a[1:]) if a[0] == "run" else shots(a[1]) if a[0] == "shots" else selftest())
