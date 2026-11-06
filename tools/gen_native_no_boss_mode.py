"""Placements whose create path never reaches Mission::BossBattleMode or a door wait.

The catalogue is walked only to name the placement each row already uses. A row is
emitted when some function both pushes that placement and creates a character, and
neither that function, the functions it calls, nor the caller continuation reaches
Mission::BossBattleMode or Function::WaitWarpStart. Door rows (gate_kind door) stay
out, so the entry cover keeps them until FIGHT start. No hand-written mission list.
"""
import argparse, csv, struct, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "re" / "scripts_re"))
sys.path.insert(0, str(ROOT / "tools"))
import lfm_disasm as dis
import rematch_catalog as cat

BOSS = "Mission::BossBattleMode"
DOOR = "Function::WaitWarpStart"
CREATE = ("Character::Create", "Character::CreateAndWait")

def strings(script):
    b = script.data
    end = script.code_start - 4
    needed = max((i.args[0] for i in script.insns if i.op == 0x2f), default=-1) + 1
    for off in range(script.after_syms, end - 2):
        count = struct.unpack_from("<H", b, off)[0]
        if count < needed or count > 4096:
            continue
        cur = off + 2
        out = []
        for _ in range(count):
            if cur + 3 > end:
                break
            n = struct.unpack_from("<H", b, cur)[0]
            if cur + 3 + n > end or b[cur + 2 + n] != 0:
                break
            try:
                value = b[cur + 2:cur + 2 + n].decode("utf8")
            except UnicodeError:
                break
            if "\0" in value:
                break
            out.append(value)
            cur += n + 3
        if len(out) == count and cur == end:
            return out
    return []

def bodies_of(script):
    bodies = {}
    cur = None
    for ins in script.insns or []:
        if ins.op == 0x3b:
            cur = dis.insn_name(script, ins)
            bodies[cur] = []
        elif cur is not None:
            bodies[cur].append(ins)
    return bodies

def call_name(script, pool, insns, k):
    if k < 0 or k >= len(insns):
        return None
    i = insns[k]
    if i.op not in (0x1b, 0x1f):
        return None
    name = dis.insn_name(script, i)
    if name == "Script::Create" and k and insns[k - 1].op == 0x2f and insns[k - 1].args[0] < len(pool):
        return pool[insns[k - 1].args[0]]
    return name

def closure(bodies, script, pool, fn, seen=None):
    seen = set() if seen is None else seen
    if fn in seen or fn not in bodies:
        return seen
    seen.add(fn)
    ins = bodies[fn]
    for k in range(len(ins)):
        name = call_name(script, pool, ins, k)
        if name in bodies:
            closure(bodies, script, pool, name, seen)
    return seen

def mentions(script, pool, insns, native):
    for k in range(len(insns)):
        if call_name(script, pool, insns, k) == native:
            return True
    return False

def path_has_boss_or_door(script, pool, bodies, fn):
    group = closure(bodies, script, pool, fn)
    for f in group:
        ins = bodies[f]
        if mentions(script, pool, ins, BOSS) or mentions(script, pool, ins, DOOR):
            return True
    for parent, ins in bodies.items():
        for k in range(len(ins)):
            if call_name(script, pool, ins, k) != fn:
                continue
            if mentions(script, pool, ins[:k + 1], DOOR):
                return True
            after = ins[k + 1:]
            if mentions(script, pool, after, BOSS) or mentions(script, pool, after, DOOR):
                return True
            extra = set()
            for j in range(len(after)):
                name = call_name(script, pool, after, j)
                if name in bodies:
                    extra |= closure(bodies, script, pool, name)
            for f in extra:
                if mentions(script, pool, bodies[f], BOSS) or mentions(script, pool, bodies[f], DOOR):
                    return True
    return False

def no_boss_mode(script, placement):
    """True when every create site for this placement has no boss mode and no door wait."""
    pool = strings(script)
    bodies = bodies_of(script)
    sites = []
    for fn, ins in bodies.items():
        pushed = any(i.op == 0x29 and i.args and i.args[0] == placement for i in ins)
        creates = any(call_name(script, pool, ins, k) in CREATE for k in range(len(ins)))
        if pushed and creates:
            sites.append(fn)
    if not sites:
        return False
    return all(not path_has_boss_or_door(script, pool, bodies, fn) for fn in sites)

def generate(lfm):
    cat.LFM = lfm
    b = cat.lfm(cat.IDTAB).read_bytes()
    ids = {}
    for i in range(struct.unpack_from("<I", b, 4)[0]):
        e = 8 + i * 8
        k, o = struct.unpack_from("<Ii", b, e)
        ids[k] = b[e + o:b.index(b"\0", e + o)].decode()
    def table(rec, off, archive):
        name = ids.get(struct.unpack_from("<I", rec, off)[0])
        if not name:
            return None
        p = lfm / archive / name.lstrip("/")
        return p if p.exists() else None
    b = cat.lfm(cat.MISSIONDB).read_bytes()
    missions = {}
    for i in range(struct.unpack_from("<I", b, 4)[0]):
        r = b[8 + i * 0x198:8 + (i + 1) * 0x198]
        missions[struct.unpack_from("<H", r, 0x158)[0]] = r
    rows = []
    cache = {}
    with (ROOT / "mod" / "Nioh3BossRematch" / "Nioh3BossRematch_bosses.tsv").open(encoding="utf-8") as f:
        catalogue = list(csv.DictReader((x for x in f if not x.startswith("#")), delimiter="\t"))
    for r in catalogue:
        try:
            mid = int(r["replay_mission_id"], 0)
            placement = int(r["placement_id"])
        except ValueError:
            continue
        if mid not in missions or r["route"] not in ("R", "H", "D"):
            continue
        if r.get("gate_kind") == "door":
            continue
        rec = missions[mid]
        p = table(rec, 0x50, "archive_09")
        if not p:
            continue
        key = str(p)
        if key not in cache:
            script = dis.load(p)
            cache[key] = script
        if not no_boss_mode(cache[key], placement):
            continue
        rows.append(dict(id=r["id"], mission=mid, placement=placement))
    rows.sort(key=lambda r: (r["mission"], r["placement"], r["id"]))
    return rows

def render(rows):
    s = [
        "// Generated by tools/gen_native_no_boss_mode.py from mission scripts.",
        "// A placement whose create path does not reach Mission::BossBattleMode or WaitWarpStart.",
        "// Door rows are absent. The runtime has no mission list of its own.",
        "typedef struct { const char* id; uint32_t mission, placement; } NativeNoBossMode;",
        "static const NativeNoBossMode g_native_no_boss_mode[] = {",
    ]
    for r in rows:
        s.append('    {"%s", %uu, %uu},' % (r["id"], r["mission"], r["placement"]))
    if not rows:
        s.append("    {0, 0u, 0u},")
    s.append("};")
    s.append("")
    return "\n".join(s)

if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--lfm", type=Path, required=True)
    p.add_argument("--check", action="store_true")
    a = p.parse_args()
    rows = generate(a.lfm)
    ids = {r["id"] for r in rows}
    if "RD617" not in ids:
        sys.exit("RD617 create path was not classified as no-boss-mode")
    if "RB3FE" in ids:
        sys.exit("door row RB3FE must keep its cover")
    text = render(rows)
    path = ROOT / "mod" / "Nioh3Training" / "src" / "native_no_boss_mode_gen.h"
    if a.check:
        assert path.read_text(encoding="utf-8") == text, path
    else:
        path.write_text(text, encoding="utf-8")
    print("no-boss-mode rows: %d" % len(rows))
    print(", ".join(r["id"] for r in rows))
