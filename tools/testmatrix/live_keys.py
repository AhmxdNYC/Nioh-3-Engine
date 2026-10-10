r"""live_keys.py - classify every Nioh3EngineFix key as LIVE (one-launch A/B through LiveCtl) or RELAUNCH (boot-time only).

    py -I tools\testmatrix\live_keys.py            -> tools\testmatrix\live_keys.json (+ a summary)
    py -I tools\testmatrix\live_keys.py --md       -> also print the markdown table (notes\fast-testing.md)
    py -I tools\testmatrix\live_keys.py selftest

Sources (re-run after every ASI change; fastab.ps1 refuses items that are not 'live'):
  * mod\Nioh3EngineFix\src\menu.cpp: kItems (effect text "Applies now" vs "Applies at the next launch") + kExpIniKey (row <-> ini key)
  * mod\Nioh3EngineFix\Nioh3EngineFix.ini: every key of every section
  * mod\Nioh3EngineFix\patches.json: every patch group
  * the fast-testing additions: live [Tuning] values (LiveTuning in dllmain / samelook::SetTuning) and ShaderHot sets
Classes: live = LiveCtl 'KEY = v' (menu row), live-tuning = 'Tuning.KEY = v', live-shader = 'Hot.<set> = 0/1' (ShaderHot,
diagnostic twin PSOs), live-diag = SkipFx control file, relaunch = read once at boot (patch bytes / hooks / threads / boot lists).
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
MOD = os.path.join(ROOT, "mod", "Nioh3EngineFix")

# non-experimental menu rows, enum order before kXW6 (menu.cpp kItems), and their LiveCtl key / ini key
BASE_ROWS = [("VRR", "VRR.Enabled"), ("W3", "Patches.W3"), ("W3MODE", "Tuning.W3_UseSleep"), ("R1", "Patches.R1"), ("L1", "Patches.L1"),
             ("F1", "Patches.FG"), ("WK", "Patches.WK"), ("MAXLATENCY", "Latency.MaxFrameLatency")]
OTHER_LIVE = {   # rows after the experimental block (menu.cpp kItems), LiveCtl key -> ini key
    "GPU": "GpuOpt.Barriers", "R2A": "GpuOpt.R2a", "R2B": "GpuOpt.R2b", "R1GPU": "GpuOpt.R1", "FPS": "Overlay.FpsCounter",
    "FRAMETIME": "Overlay.FrameTime", "SPEED": "Overlay.Speed", "AUTOCONTINUELOAD": "General.AutoContinueLoad", "PAUSEHINT": "Menu.PauseHint",
}
NEXT_LAUNCH_ROWS = {"WORKERCAP": "Patches.WorkerCap", "P12": "Patches.P12"}
LIVE_TUNING = {   # read at use time by the installed code (dllmain LiveTuning / samelook::SetTuning)
    "PP_Period": "PP cave mask (pp_mask), read on every sweep decision", "VSMA_K": "VsmaBindC, per clipmap visit",
    "VSMA_Front": "VsmaBindC, per clipmap visit", "LPVR_N": "RcoApplyC, per frame", "SKYR_V": "RcoApplyC, per frame",
    "FOGT_V": "RcoApplyC, per frame", "CLDR_V": "RcoApplyC, per frame",
    "KL_Div": "bkstats KL: CmdFlushLimit data write, compared per draw by the engine (1.7.58-bk1)",
    "FO": "frameoverlap switch, sampled at the next frame end (1.7.59-fo1; needs [GpuOpt] FrameOverlap = 1 or 2 at boot)",
    "FO_Pct": "frameoverlap barrier position, read at every frame end (1.7.59-fo1)",
    "RPF": "replayprof: removes / re-installs the replay-profiler hooks (1.7.59-rpf1; needs [Profiler] ReplayProf = 1 or 2 at boot)",
    "CBS": "cbvupload: streaming CB copy switch, read per copy (1.7.60-cbu1; needs [Patches] CBS = 1 or 2 at boot)",
    "CBN": "cbvupload: no-op CB upload skip switch, read per table flush (1.7.60-cbu1; needs [Patches] CBN = 1 or 2 at boot)",
    "CBT": "cbvupload: CBV table reuse switch, read per table flush (1.7.60-cbu1; needs [Patches] CBT = 1 or 2 at boot)",
    "CBSTATS": "cbvupload: upload census switch (1.7.60-cbu1; needs a CBU key or [Profiler] CbvStats at boot)",
}
HOT_SETS = ["ghdm", "esm", "wdo28", "mpc", "mshlod", "gi", "fogpar", "vcu", "vcp", "vcpfu", "vcpfp"]


def parse_items(src):
    blk = src[src.index("const ItemDef kItems[kCount] = {"):]
    blk = blk[:blk.index("\n};")]
    s = r'"((?:[^"\\]|\\.)*)"'
    eff = r'((?:\s*"(?:[^"\\]|\\.)*")+|\s*E_CPU_NEXT|\s*E_CPU)'
    out = []
    for m in re.finditer(r"\{\s*" + s + r"\s*,\s*" + s + r"\s*,\s*" + s + r"\s*," + eff + r"\s*\}", blk, re.S):
        e = m.group(4).strip()
        if e == "E_CPU_NEXT":
            e = "Picture unchanged. Applies at the next launch (save to ini)."
        elif e == "E_CPU":
            e = "Picture unchanged. Applies now."
        else:
            e = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', e))
        out.append(dict(plain=m.group(1), tech=m.group(2), effect=e))
    return out


def parse_exp_keys(src):
    m = re.search(r"kExpIniKey\[kNumExp\]\s*=\s*\{(.*?)\};", src, re.S)
    return re.findall(r'L"([^"]+)"', m.group(1))


def parse_ini(txt):
    sec, out = "", []
    for ln in txt.splitlines():
        ln = ln.strip()
        if ln.startswith("[") and ln.endswith("]"):
            sec = ln[1:-1]
        elif ln and not ln.startswith(";") and "=" in ln:
            out.append((sec, ln.split("=", 1)[0].strip()))
    return out


def classify(menu_src, ini_txt, patches):
    items = parse_items(menu_src)
    exp = parse_exp_keys(menu_src)
    first = len(BASE_ROWS)
    if len(items) < first + len(exp):
        raise SystemExit("menu.cpp kItems parse: %d rows, need >= %d" % (len(items), first + len(exp)))
    keys = {}

    def put(ini_key, cls, via, why):
        keys.setdefault(ini_key, dict(key=ini_key.split(".", 1)[1], section=ini_key.split(".", 1)[0], cls=cls, via=via, why=why))

    for i, (lk, ik) in enumerate(BASE_ROWS):
        put(ik, "live", "%s = 0/1" % lk if lk != "MAXLATENCY" else "MaxLatency = 0..3", "menu row: " + items[i]["effect"][:90])
    put("Tuning.R1_TimeoutMs", "live", "R1 = 0..16", "menu row R1 sets the live timeout (ms)")
    for j, k in enumerate(exp):
        it = items[first + j]
        nl = "next launch" in it["effect"].lower()
        put("Patches." + k, "relaunch" if nl else "live", "abba2 -Arms (launch per arm)" if nl else "%s = 0/1 (install with %s = 2 or 1)" % (k, k),
            ("code bytes patched at boot: " if nl else "menu row: ") + it["effect"][:90])
    for lk, ik in OTHER_LIVE.items():
        put(ik, "live", "%s = v" % lk, "menu row (applies now)")
    for lk, ik in NEXT_LAUNCH_ROWS.items():
        put(ik, "relaunch", "abba2 -Arms", "menu row says: applies at the next launch")
    for k, why in LIVE_TUNING.items():
        put("Tuning." + k, "live-tuning", "Tuning.%s = v" % k, why)
    put("Debug.SkipPSO", "live-diag", "SkipFx control file [SkipFx] PSO", "SkipFx (diagnostic): re-read every 100 ms")
    put("Debug.SkipMarker", "live-diag", "SkipFx control file [SkipFx] Marker", "SkipFx (diagnostic): re-read every 100 ms")
    for (sec, k) in ini_txt:
        put("%s.%s" % (sec, k), "relaunch", "abba2 -Arms / one launch per arm", "read once at boot (install / hook / thread / boot list)")
    for g in sorted({s.get("group") for s in patches.get("sites", []) if s.get("group")}):
        put("Patches." + g, "relaunch", "abba2 -Arms", "patch group without a live switch")
    rows = list(keys.values())
    for s in HOT_SETS:
        rows.append(dict(key="Hot." + s, section="shader_dx12", cls="live-shader", via="Hot.%s = 0/1 (fastab.ps1 builds the set)" % s,
                         why="ShaderHot twin PSOs (diagnostic; the .file itself still needs a relaunch to ship)"))
    for r in rows:
        r["class"] = "live" if r["cls"].startswith("live") else "relaunch"
    return rows


def main(argv):
    if argv[1:2] == ["selftest"]:
        return selftest()
    menu_src = open(os.path.join(MOD, "src", "menu.cpp"), encoding="utf-8", errors="replace").read()
    ini = parse_ini(open(os.path.join(MOD, "Nioh3EngineFix.ini"), encoding="utf-8", errors="replace").read())
    patches = json.load(open(os.path.join(MOD, "patches.json"), encoding="utf-8"))
    rows = classify(menu_src, ini, patches)
    out = os.path.join(HERE, "live_keys.json")
    json.dump(dict(_doc=__doc__.strip().splitlines()[0], keys=rows), open(out, "w", encoding="utf-8"), indent=1)
    by = {}
    for r in rows:
        by[r["cls"]] = by.get(r["cls"], 0) + 1
    print("%d keys -> %s: %s" % (len(rows), out, ", ".join("%s %d" % kv for kv in sorted(by.items()))))
    if "--md" in argv:
        print("| class | keys (LiveCtl syntax) |\n|---|---|")
        for c in ("live", "live-tuning", "live-shader", "live-diag"):
            print("| %s | %s |" % (c, ", ".join(sorted(r["via"].split(" ")[0] for r in rows if r["cls"] == c))))
        rl = sorted("%s.%s" % (r["section"], r["key"]) for r in rows if r["cls"] == "relaunch" and r["section"] == "Patches")
        print("| relaunch ([Patches]) | %s |" % ", ".join(k.split(".", 1)[1] for k in rl))
    return 0


def selftest():
    src = ('const wchar_t* const kExpIniKey[kNumExp] = {L"W6", L"M2"};\nconst ItemDef kItems[kCount] = {\n'
           + "".join('    {"r%d", "(t)", "w", "Applies now."},\n' % i for i in range(len(BASE_ROWS)))
           + '    {"W6 row", "(W6)", "w", E_CPU},\n    {"M2 row", "(M2)", "w", E_CPU_NEXT},\n'
           + '    {"x", "", "multi", "a "\n     "b"},\n};\n')
    rows = classify(src, [("Patches", "W6"), ("Patches", "M2"), ("Tuning", "PP_Period"), ("Debug", "Tour")],
                    {"sites": [{"group": "W6"}, {"group": "ZZ"}]})
    c = {"%s.%s" % (r["section"], r["key"]): r["cls"] for r in rows}
    assert c["Patches.W6"] == "live" and c["Patches.M2"] == "relaunch" and c["Tuning.PP_Period"] == "live-tuning", c
    assert c["Debug.Tour"] == "relaunch" and c["Patches.ZZ"] == "relaunch" and c["shader_dx12.Hot.ghdm"] == "live-shader", c
    print("live_keys selftest OK")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
