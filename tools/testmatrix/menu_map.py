"""menu_map.py - key sequences for the live toggles in the mod's F10 menu, derived from mod\\Nioh3EngineFix\\src\\menu.cpp.

  py -I menu_map.py [--ini FILE] [--no-batch] [--wine] [--unknown-build] [--device pc] [-o menu_map.json]
  py -I menu_map.py --selftest

Rules read from the source (menu.cpp: OnPresentImpl / Move / Change / Open / Close):
  * rows = enum Id (kItems has the labels, same order). A row is selectable when Visible(id) && !ReadOnly(id); both
    predicates are PARSED from the source text here, not copied. Rows that are visible but not Available stay selectable.
  * Up/Down: Move() steps to the next selectable row with wrap-around. g_sel is NOT reset when the menu is closed and
    reopened (Open() only resets it when the row became invisible), so the cursor has to be tracked. Invariant used here:
    every sequence ends with the cursor parked on the last row "Close menu" (Enter on it closes the menu). Reopening starts
    there, so "open -> N downs" counts from Close (one Down wraps to the first selectable row). A fresh game starts on
    row 0 (VRR): `init` = F10, Up (wraps to Close), Enter parks it.
  * Enter = Change(sel, 0): flips the row's live switch and logs "MENU: <label> -> <value>" (ON/OFF; R2a/R2b/R1 print
    ON/off, Barriers prints ON / "OFF (hooked)"). A row whose patch is not installed does nothing (live rows) or flips only
    the ini copy ("* restart"): verify by log and abort that item.
  * Keys: F10 opens/closes; Up/Down/Enter via the DirectInput GetDeviceState hook (fallback GetAsyncKeyState). Arrows need
    the EXTENDED key flag (DIK_UP/DOWN = 0xC8/0xD0): ui.fsx `keyseq` sets it. Edges are OR-ed per Present, so presses are
    spaced by 200 ms waits; the menu acts on the next Present.
Row visibility depends on the ini written for the run: Barriers/TraceRecords/GPUProf != 0 -> GPU rows; Wine -> R2a/R2b/R1.
"""
import argparse, json, os, random, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
MENU_CPP = os.path.join(ROOT, "mod", "Nioh3EngineFix", "src", "menu.cpp")
DEFAULT_INI = os.path.join(ROOT, "mod", "Nioh3EngineFix", "Nioh3EngineFix.ini")
VK = {"open": "79", "down": "28", "up": "26", "enter": "0D"}      # F10, Down, Up, Return
OPEN_WAIT, GAP, TOGGLE_WAIT, CLOSE_WAIT = "w500", "w200", "w700", "w400"


def parse_ini(path):
    d = {}
    sec = ""
    for ln in open(path, encoding="utf-8-sig", errors="replace"):
        ln = ln.strip()
        if not ln or ln[0] in ";#":
            continue
        if ln.startswith("["):
            sec = ln.strip("[]").lower()
        elif "=" in ln:
            k, v = ln.split("=", 1)
            d[(sec, k.strip().lower())] = v.split(";")[0].strip()
    return d


def ini_int(ini, sec, key):
    try:
        return int(ini.get((sec.lower(), key.lower()), 0))
    except ValueError:
        return 0


class Menu:
    """Row table parsed from menu.cpp."""

    def __init__(self, src_path=MENU_CPP):
        s = open(src_path, encoding="utf-8", errors="replace").read()
        m = re.search(r"enum Id \{(.*?)\};", s, re.S)
        names = [x.strip() for x in re.sub(r"//[^\n]*", "", m.group(1)).split(",") if x.strip()]
        assert names[-1] == "kCount"
        self.rows = names[:-1]
        m = re.search(r"kItems\[kCount\] = \{(.*?)\n\};", s, re.S)
        ents = re.findall(r'\{\s*(nullptr|"(?:[^"\\]|\\.)*")\s*,\s*"((?:[^"\\]|\\.)*)"', m.group(1))
        self.labels = [e[1].replace('\\"', '"') for e in ents]
        assert len(self.labels) == len(self.rows), (len(self.labels), len(self.rows))
        m = re.search(r"kExpIniKey\[kNumExp\] = \{(.*?)\};", s, re.S)
        keys = re.findall(r'L"([^"]+)"', m.group(1))
        first = self.rows.index("kXW6")
        assert self.rows[first + len(keys) - 1] == "kXDL"
        self.exp_row = {k: self.rows[first + i] for i, k in enumerate(keys)}
        a = self.rows.index("kGR2a")
        assert self.rows[a:a + 3] == ["kGR2a", "kGR2b", "kGR1"]
        m = re.search(r"bool ReadOnly\(int id\) \{ return (.*?); \}", s)
        self.readonly = set(re.findall(r"k\w+", m.group(1)))
        m = re.search(r"bool Visible\(int id\) \{\s*switch \(id\) \{(.*?)\n    \}\n\}", s, re.S)
        self.vis = [(re.findall(r"k\w+", c), e.strip())
                    for c, e in re.findall(r"((?:case k\w+:\s*)+)return ([^;]+);", m.group(1))]
        self.vis_default = re.search(r"default: return (\w+);", m.group(1)).group(1) == "true"

    def visible(self, row, env):
        for rows, expr in self.vis:
            if row in rows:
                e = expr.replace("&&", " and ").replace("||", " or ").replace("g_host.", "")
                e = re.sub(r"gpuopt::GetState\(\)\s*!=\s*gpuopt::State::Off", "gpuopt_on", e)
                return bool(eval(e, {"__builtins__": {}}, env))
        return self.vis_default

    def selectable(self, env):
        return [r for r in self.rows if self.visible(r, env) and r not in self.readonly]


def make_env(ini, wine, known_build):
    """Host/gpuopt state implied by an ini (what dllmain hands to menu::Configure / gpuopt::Start)."""
    g = lambda s, k: ini_int(ini, s, k)
    rew = wine and (g("GpuOpt", "R2a") or g("GpuOpt", "R2b") or g("GpuOpt", "R1"))
    return {"wine": wine, "knownBuild": known_build, "base": known_build,
            "w3Enable": bool(g("Patches", "W3")), "w3UseSleep": bool(wine and g("Patches", "W3")),
            "gpuopt_on": bool(g("GpuOpt", "Barriers") or g("GpuOpt", "TraceRecords") or g("Profiler", "GPUProf") or rew)}


def row_for_item(menu, item):
    """Menu row for a matrix item: its first (non-needs) ini triple names the switch."""
    for sec, key, _ in item.get("ini", []):
        if sec == "GpuOpt":
            return {"Barriers": "kGpu", "R2a": "kGR2a", "R2b": "kGR2b", "R1": "kGR1"}.get(key)
        return menu.exp_row.get(key) if sec == "Patches" else None
    return None


def live_items(matrix, device, wine):
    return [i for i in matrix["items"] if i.get("mode") == "live" and device in i.get("devices", [])
            and i.get("enabled", True) is not False and (wine or not i.get("wine_only")) and not i.get("handheld_only")]


def batch_ini(matrix, live, base):
    """The ini run_pc.ps1 writes for LIVE-BATCH: alloff + every live item (needs, then ini at live_installed_value)."""
    ini = dict(base)
    for s, k, v in matrix["alloff"]:
        ini[(s.lower(), k.lower())] = v
    for it in live:
        for s, k, v in it.get("needs", []):
            ini[(s.lower(), k.lower())] = v
        for s, k, v in it.get("ini", []):
            ini[(s.lower(), k.lower())] = it.get("live_installed_value", v)
    return ini


def step_keys(sel, target, n):
    """Cheapest Up/Down presses from selectable index sel to target (wrap-around)."""
    d = (target - sel) % n
    return ["down"] * d if d <= n - d else ["up"] * (n - d)


def build_tokens(sel_rows, row):
    """F10, move from the parked Close row to `row`, Enter (toggle), move back to Close, Enter (close)."""
    n, ci, ti = len(sel_rows), sel_rows.index("kClose"), sel_rows.index(row)
    t = [VK["open"], OPEN_WAIT]
    for k in step_keys(ci, ti, n):
        t += [VK[k], GAP]
    t += [VK["enter"], TOGGLE_WAIT]
    for k in step_keys(ti, ci, n):
        t += [VK[k], GAP]
    return t + [VK["enter"], CLOSE_WAIT]


def build_map(menu, matrix, base_ini, device="pc", wine=False, known_build=True, batch=True):
    live = live_items(matrix, device, wine)
    ini = batch_ini(matrix, live, base_ini) if batch else base_ini
    env = make_env(ini, wine, known_build)
    sel = menu.selectable(env)
    out = {"rows": [menu.labels[menu.rows.index(r)] for r in sel],
           "init": [VK["open"], OPEN_WAIT, VK["up"], GAP, VK["enter"], CLOSE_WAIT], "items": {}, "skipped": {}}
    for it in live:
        row = row_for_item(menu, it)
        if not row:
            out["skipped"][it["id"]] = "no menu row for its ini switch"
        elif row not in sel:
            out["skipped"][it["id"]] = "row %s not visible with this ini/device (wine=%s)" % (row, wine)
        else:
            out["items"][it["id"]] = {"row": row, "label": menu.labels[menu.rows.index(row)], "index": sel.index(row),
                                      "start": "OFF" if it.get("live_installed_value") else "ON",
                                      "keys": build_tokens(sel, row)}
    return out, env, live


class Sim:
    """The C++ menu state machine (Open / Move / Change / Close) driven by the same parsed tables."""

    def __init__(self, menu, env, installed, start_on):
        self.m, self.env, self.installed = menu, env, set(installed)
        self.sel, self.open, self.log = 0, False, []
        self.state = {r: start_on[r] for r in installed}

    def move(self, d):
        n = len(self.m.rows)
        for _ in range(n):
            self.sel = (self.sel + d) % n
            r = self.m.rows[self.sel]
            if self.m.visible(r, self.env) and r not in self.m.readonly:
                return

    def press(self, vk):
        if vk == VK["open"]:
            self.open = not self.open
            if self.open and not self.m.visible(self.m.rows[self.sel], self.env):
                self.sel = 0
        elif not self.open:
            pass
        elif vk == VK["down"]:
            self.move(+1)
        elif vk == VK["up"]:
            self.move(-1)
        elif vk == VK["enter"]:
            r = self.m.rows[self.sel]
            if r == "kClose":
                self.open = False
            elif r in self.installed:
                self.state[r] = not self.state[r]
                self.log.append("MENU: %s -> %s" % (self.m.labels[self.sel], "ON" if self.state[r] else "OFF"))

    def run(self, toks):
        for t in toks:
            if not t.startswith("w"):
                self.press(t)


def selftest():
    menu = Menu()
    matrix = json.load(open(os.path.join(HERE, "matrix.json"), encoding="utf-8"))
    base = parse_ini(DEFAULT_INI)
    bad = 0
    for device, wine in (("pc", False), ("ally", True)):
        mp, env, live = build_map(menu, matrix, base, device, wine)
        rows = {v["row"] for v in mp["items"].values()}
        sim = Sim(menu, env, rows, {v["row"]: v["start"] == "ON" for v in mp["items"].values()})
        sim.run(mp["init"])
        if sim.open or menu.rows[sim.sel] != "kClose":
            bad += 1
            print("FAIL init does not park the cursor on Close")
        ids = list(mp["items"])
        random.Random(1).shuffle(ids)
        order = ids + ids[::-1] + ids
        for iid in order:
            e = mp["items"][iid]
            before = dict(sim.state)
            sim.log.clear()
            sim.run(e["keys"])
            exp = "MENU: %s -> %s" % (e["label"], "ON" if sim.state[e["row"]] else "OFF")
            if not (sim.log == [exp] and sim.state[e["row"]] != before[e["row"]]
                    and all(sim.state[r] == before[r] for r in rows if r != e["row"])
                    and not sim.open and menu.rows[sim.sel] == "kClose"):
                bad += 1
                print("FAIL %s %s: log=%s" % (device, iid, sim.log))
        print("%s wine=%s: %d items x3 toggled in sim, %d selectable rows, skipped=%s" % (device, wine, len(ids), len(mp["rows"]), list(mp["skipped"])))
    assert menu.rows[0] == "kVrr" and menu.rows[-1] == "kClose" and {"kFgState", "kUpscaler"} <= menu.readonly
    print("SELFTEST", "FAILED" if bad else "OK")
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ini", default=DEFAULT_INI)
    ap.add_argument("--matrix", default=os.path.join(HERE, "matrix.json"))
    ap.add_argument("--device", default="pc")
    ap.add_argument("--wine", action="store_true")
    ap.add_argument("--unknown-build", action="store_true")
    ap.add_argument("--no-batch", action="store_true", help="use the ini as is (default: alloff + live items at installed values)")
    ap.add_argument("-o", "--out")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    menu = Menu()
    matrix = json.load(open(a.matrix, encoding="utf-8"))
    mp, _, _ = build_map(menu, matrix, parse_ini(a.ini), a.device, a.wine, not a.unknown_build, not a.no_batch)
    mp = {"_doc": "generated by menu_map.py from menu.cpp. keys = ui.fsx `keyseq` tokens (hex VK; wNNN = wait ms). Each item: "
                  "F10, move from the parked 'Close menu' row, Enter (toggle), move back, Enter (close). Run `init` once after "
                  "the game reaches play (cursor starts on row 0). start = state right after launch with the batch ini.",
          "device": a.device, "wine": a.wine, "ini": os.path.basename(a.ini), **mp}
    if a.out:
        open(a.out, "w", encoding="utf-8").write(json.dumps(mp, indent=1) + "\n")
    for k, v in mp["items"].items():
        print("%-10s #%-2d %-26s %s" % (k, v["index"], v["label"], " ".join(v["keys"])))
    for k, v in mp["skipped"].items():
        print("%-10s SKIPPED: %s" % (k, v))
    return 0


if __name__ == "__main__":
    sys.exit(main())
