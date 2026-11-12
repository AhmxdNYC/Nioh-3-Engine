"""openworld_boss_spawns.py - spawn points of the field-only bosses (route X 'field only' rows of the rematch catalogue).

    python3 -I tools/openworld_boss_spawns.py [game dir] [--out data/openworld_boss_spawns.tsv]

Generated, no hand-set rows (#87625). Boss set = every Nioh3BossRematch_bosses.tsv row with route X whose reason starts with
'field only' (rematch_catalog.py decides that). Char ids per boss = the same set rematch_catalog.build() uses (boss-names.csv
english name + boss-master param_char_id). Records = notes/rematch-enemy-index.csv rows of those char ids with the boss bit
(+0x9C bit 4) in an internal-stage mission (missions.csv class, the CHxx_WORLD_* open-field regions). Spawn = that placement's
record in the mission's enemy table (mission DB record +0x54 -> id table -> LFM file, 0x138-byte records, +0 id, +0x40 x y z yaw,
+0x134 name offset), read straight from the game archives (n3lfm). One row per (boss, mission, placement)."""
import argparse
import collections
import csv
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from n3lfm import Lfm  # noqa: E402
import rematch_catalog as rc  # noqa: E402

MAGIC = b"\x00\x22\x04\x20"
GAME = os.path.expanduser("~/.local/share/Steam/steamapps/common/Nioh3")
OUT = os.path.join(ROOT, "data", "openworld_boss_spawns.tsv")
BOSSES = os.path.join(ROOT, "mod", "Nioh3BossRematch", "Nioh3BossRematch_bosses.tsv")
COLS = ["boss", "catalog_id", "region", "char_id", "mission_id", "placement_id", "record_name", "flags9c",
        "x", "y", "z", "yaw", "enemy_table", "row"]


def path_of(rel):
    return "/" + rel.split("/", 1)[0] + "/" + rel.split("/", 1)[1] if not rel.startswith("/") else rel


def field_bosses():
    rows = csv.DictReader((ln for ln in open(BOSSES, encoding="utf-8") if not ln.startswith("#")), delimiter="\t")
    return [r for r in rows if r["route"] == "X" and r["reason"].startswith("field only")]


def char_ids():
    cn = collections.defaultdict(set)
    for r in rc.rd("boss-names.csv"):
        cn[r["english_name"].strip().lower()].add(int(r["char_id"]))
    for r in rc.rd("boss-master.csv"):
        n = rc.ALIAS.get(r["english_name"].strip().lower(), r["english_name"].strip()).lower()
        if r["param_char_id"]:
            cn[n].add(int(r["param_char_id"]))
    return cn


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("game", nargs="?", default=GAME)
    ap.add_argument("--out", default=OUT)
    a = ap.parse_args()
    L = Lfm(a.game)

    def get(t):
        return L.get("/%s/%s" % (t[1], t[2]))

    d = get(rc.IDTAB)
    ids = {}
    for i in range(struct.unpack_from("<I", d, 4)[0]):
        e = 8 + i * 8
        k, o = struct.unpack_from("<Ii", d, e)
        ids[k] = d[e + o:d.index(b"\0", e + o)].decode("latin1")
    mi = {int(r["mission_key_u16(+158)"]): r for r in rc.rd("missions.csv")}
    internal = {m for m, r in mi.items() if r["class"] == "internal-stage"}
    md = get(rc.MISSIONDB)
    tables = {}
    for i in range(struct.unpack_from("<I", md, 4)[0]):
        rec = md[8 + i * 0x198:8 + (i + 1) * 0x198]
        mid = struct.unpack_from("<H", rec, 0x158)[0]
        nm = ids.get(struct.unpack_from("<I", rec, 0x54)[0])
        if mid in internal and nm:
            tables[mid] = nm
    byc = collections.defaultdict(list)
    for r in rc.rd("rematch-enemy-index.csv"):
        if int(r["flags9c"], 16) & 4 and int(r["mission_id"], 16) in internal:
            byc[int(r["char_id"])].append(r)
    cn = char_ids()
    cache = {}

    def table(mid):
        if mid not in cache:
            t = L.get(path_of(tables[mid])) if mid in tables else None
            recs = {}
            if t and t[:4] == MAGIC:
                c = struct.unpack_from("<I", t, 4)[0]
                pool = 8 + c * 0x138
                for j in range(c if pool <= len(t) else 0):
                    r = t[8 + j * 0x138:8 + (j + 1) * 0x138]
                    no = struct.unpack_from("<I", r, 0x134)[0]
                    nm = t[pool + no:t.index(b"\0", pool + no)].decode("latin1") if pool + no < len(t) else ""
                    recs[struct.unpack_from("<I", r, 0)[0]] = (j, struct.unpack_from("<4f", r, 0x40), r[0x9C], nm)
            cache[mid] = recs
        return cache[mid]

    out, missing = [], []
    for b in field_bosses():
        got = 0
        for cid in sorted(cn.get(b["name"].strip().lower(), ())):
            for h in byc.get(cid, []):
                mid, pid = int(h["mission_id"], 16), int(h["placement_id"])
                rec = table(mid).get(pid)
                if rec is None:
                    continue
                j, (x, y, z, yaw), fl, nm = rec
                key = (b["name"], mid, pid)
                if any((o["boss"], int(o["mission_id"], 16), int(o["placement_id"])) == key for o in out):
                    continue
                out.append({"boss": b["name"], "catalog_id": b["id"], "region": b["region"], "char_id": cid,
                            "mission_id": "0x%04X" % mid,
                            "placement_id": pid, "record_name": rc.ascii_(nm), "flags9c": "0x%02X" % fl, "x": "%.1f" % x,
                            "y": "%.1f" % y, "z": "%.1f" % z, "yaw": "%.1f" % yaw, "enemy_table": tables[mid], "row": j})
                got += 1
        if not got:
            missing.append(b["name"])
    out.sort(key=lambda r: (r["catalog_id"], r["mission_id"], r["placement_id"]))
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, "w", newline="", encoding="utf-8") as o:
        o.write("# generated by tools/openworld_boss_spawns.py from the game archives (n3lfm) + notes/rematch-enemy-index.csv; "
                "do not edit. x y z = game units (cm), yaw degrees; one row per boss-bit placement in an open-field region.\n")
        w = csv.DictWriter(o, fieldnames=COLS, delimiter="\t", lineterminator="\n")
        w.writeheader()
        w.writerows(out)
    print("wrote %s: %d rows, %d/%d bosses%s" % (os.path.relpath(a.out, ROOT), len(out), len({r["boss"] for r in out}),
                                                  len(field_bosses()), ("; no spawn: " + ", ".join(missing)) if missing else ""))
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
