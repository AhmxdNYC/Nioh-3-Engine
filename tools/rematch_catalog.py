"""rematch_catalog.py - the all-bosses rematch catalogue (notes/rematch-all-bosses.md).

    py -I tools/rematch_catalog.py --index          # once: scan data/extract/lfm -> notes/rematch-enemy-index.csv + notes/rematch-emaki.csv
    py -I tools/rematch_catalog.py --fight-index    # once: scan data/extract/lfm -> notes/rematch-fight-index.csv (start points, bosses, boss boxes)
    py -I tools/rematch_catalog.py [--out TSV] [--check]

Outputs
  notes/rematch-all-bosses.csv   one row per boss name (every boss-master name, junk included, + the online cross-check
                                 additions): route, mission, placement, stage, arena, defeat check, variants, what is unverified,
                                 or the exact reason a rematch is not possible.
  <TSV> (default mod/Nioh3BossRematch/Nioh3BossRematch_bosses.tsv, skipped when that folder does not exist)
                                 the runtime catalogue of the Nioh3BossRematch add-on: one row per startable entry (route R / H / D /
                                 E0) plus one row per boss without a route (route X, greyed in the menu with its reason). The add-on
                                 reads columns by header name, ASCII only. --check: exit 1 if the TSV or the CSV differ from a rebuild.

Routes (notes/rematch-all-bosses.md s.1)
  R   listable Battle Scroll mission (record +0x40 has none of 1|0x40|0x4000): the game's replay (R-a menu, R-b direct).
  H   one of the 3 data-hidden duels (+0x40 bit 0x4000): R plus the reverted un-hide.
  D   a shipped mission record the replay list never shows (master-duel "hidden-wave" stages, other unlisted records with a
      boss record): R-b direct start of that record id. Refused unless the mission reads cleared on this journey.
  E0  one of the 20 Hundred Demon Realms Picture Scroll records (the Emaki table, mission 0xCC96), started with an owned
      Picture Scroll item; E1 = the same with the carrier's map entry redirected to the chosen record (reverted).
  X   no route: field-only (open-world region mission), unshipped record, not a boss, or no data. 'reason' says which.

Fight now (route E1 "straight to the boss", notes/boss-rematch-build.md s.E1): for every R / H / D row the mission start point
(mission record +0x6C table, 0xA4-byte records: +0 id, +8 flags, +0xC gadget kind 0/906-909, +0x40 spawn x y z yaw) nearest the boss
placement (enemy table +0x54, 0x138-byte records, +0x40 x y z yaw) is chosen; MissionRequestById(mission, slot, start point, params)
starts the mission there (the third argument = request +0xC = what Continue passes for the saved retry point). Columns:
stage_mission = that start point id, warp_x..warp_yaw = its spawn transform, box_id = the boss-area PlacementBox, fight_kind /
fight_dist_m / fight_conf = how it was picked and how sure (high / medium / low / - = not offered).
Fight now v2 (in the arena): arena_x..arena_yaw = a point just inside the boss's arena trigger box (the PlacementBox the mission
script tests with Player::IsEnterPlacementBox* / Wait_InvadeBox* / Loop_*EnterPlacementBox*, name with BOSS, <= 40 m from the boss;
else a BOSS area box of the table), on the side the start point approaches from; arena_box / arena_src / arena_conf say which. The
add-on warps the player there after the load (the game's own Player::SetPos path) so the game's trigger starts the fight.
"""
import argparse
import collections
import csv
import io
import os
import pathlib
import re
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
N = ROOT / "notes"
LFM = ROOT / "data" / "extract" / "lfm"
IDTAB = ("archive_08", "AE", "50B70535D0EDD19523951019C533295915AE7D4F65FE37E351E09D3858A8C5")
MISSIONDB = ("archive_03", "52", "BD29D45533CFACBFA2D6EC80A3B9F40E07CBF9183727358B0B3C3ABE3E4FC2")
EMAKI = ("archive_03", "70", "14FB3450175890EB5F7574DD50BE04684A511B7B4A5B55E00FC2EAF66849FD")
EMAKI_GADGETS = ("archive_03", "6A", "D9BEDC4195ECC462D419B594BB26EBA7D94A55EC135C7C475910F4D45E7829")
EMAKI_BOXES = ("archive_03", "87", "9AB5DD3165C199CFB6653605CE156F20F973BB0BD274604B4D9FD59B0580E8")
FIGHT_IDX = "rematch-fight-index.csv"
# arena trigger boxes confirmed by an in-game trace: (mission, box) -> source
ARENA_SEEN = {(0x1CBC, 66599): "2026-10-09 PC trace: SetMissionFlag 456 -> Create 65092 -> BossBattleMode after this box"}
TRIG_CALLS = ("Player::IsEnter", "Function::Wait_Invade", "Function::Loop_")
FCOLS = ["mission_id", "kind", "id", "flags", "sub", "x", "y", "z", "yaw", "ex", "ey", "ez", "name"]
HIDDEN = {0xD22D: "Honda Tadakatsu", 0xB3F8: "Minamoto no Yoshitsune", 0xFB96: "Yagyu Jubei & Marubashi Chuya"}
TITLE_RECORD = 0x7F16          # "The Battle of Hitokoto Slope": new-game prologue variant (+0x40 bit0)
EMAKI_TEST = 0xA19A            # "Hundred Demon Realms Picture Scroll" test record (EMAKI_TEST_ENEMY_xx, +0x40 bit1)
ERA = [("Sengoku", "Sengoku"), ("Heian", "Heian"), ("Yamatai", "Yamatai"), ("finale", "Edo finale"), ("post-game", "Edo post"),
       ("Bakumatsu", "Bakumatsu"), ("Keian", "Keian DLC"), ("Eternal Rift", "Rift")]
REGION_BYTE_ERA = {0x96: "Sengoku", 0x4B: "Sengoku", 0xB1: "Heian", 0xE0: "Bakumatsu", 0xD8: "Yamatai", 0x35: "Keian DLC"}
JUNK = {"10 yokai)": "parse fragment of the online row 'Gaki Chief (... 10 yokai)'",
        "3 yokai waves": "parse fragment: a wave description, not a boss",
        "Gaki Chief (": "parse fragment of 'Gaki Chief (...)' (the boss itself has its own row)",
        "Hattori Hanzo met as mid-mission fights)": "parse fragment of a Hanzo note (Hattori Hanzo has its own row)",
        "Baba Nobuharu (Anayama Nobutada": "parse fragment: Baba Nobuharu and Anayama Nobutada have their own rows",
        "generic Emaki/Picture Scroll mission 0xCC96": "the generic Picture Scroll mission, not a boss: its 20 records are the E0 rows",
        "yokai": "parse fragment: generic word, not a boss",
        "Onmyo mages": "parse fragment: generic enemy group, not a boss",
        "": "boss-master script rows with no boss name: tutorials / training / unnamed or unshipped stages (missions in mission_id)"}
ALIAS = {"hino-enma": "Hino-enma"}
EXTRA = ["Marubashi Chuya"]      # second boss of the Yagyu Principles duel (0xFB96) and Keian Dreams (0xD84E)
SEEN = {(0x34, 76781): ("2026-10-08-pc-replay", "118")}   # (mission, placement) -> (session, defeat flag); session-2026-10-08-gamedata.md
COLS = ["id", "name", "mission_name", "region", "route", "replay_mission_id", "hidden", "story_mission", "defeat_kind", "defeat_id",
        "emaki_byte", "emaki_key_u32", "placement_id", "stage_mission", "warp_x", "warp_y", "warp_z", "warp_yaw", "box_id", "hell_id",
        "region_byte", "verified", "seen", "emaki_index", "boss", "reason", "fight_kind", "fight_dist_m", "fight_conf", "arena_x",
        "arena_y", "arena_z", "arena_yaw", "arena_box", "arena_src", "arena_conf"]
ACOLS = ["boss", "aliases", "type", "region", "route", "mission_id", "mission_name", "mission_class", "placement_id", "record_name",
         "stage", "warp", "arena", "defeat_check", "variants", "verified", "unverified", "reason", "sources"]
ASCII_MAP = {0x014D: "o", 0x016B: "u", 0x2019: "'", 0x2013: "-", 0x2014: "-"}
PRI = {"R": 0, "H": 1, "D": 2, "E0": 3}


def ascii_(s):
    s = s.translate(ASCII_MAP)
    return "".join(c if 32 <= ord(c) < 127 else "?" for c in s).strip()


def lfm(parts):
    return LFM.joinpath(*parts)


# ---------------------------------------------------------------- --index (needs data/extract/lfm)
def build_index():
    d = lfm(IDTAB).read_bytes()
    n = struct.unpack_from("<I", d, 4)[0]
    E = {}
    for i in range(n):
        e = 8 + i * 8
        k, o = struct.unpack_from("<Ii", d, e)
        E[k] = d[e + o:d.index(b"\0", e + o)].decode("latin1")
    md = lfm(MISSIONDB).read_bytes()
    use = collections.defaultdict(list)
    for i in range(struct.unpack_from("<I", md, 4)[0]):
        b = 8 + i * 0x198
        ids = struct.unpack_from("<8I", md, b + 0x50)
        key = struct.unpack_from("<H", md, b + 0x158)[0]
        for slot, x in enumerate(ids):
            if x in E:
                use[E[x]].append((key, slot))
    named = {int(r["char_id"]) for r in csv.DictReader(open(N / "boss-names.csv", encoding="utf-8")) if r["english_name"].strip()}
    k64 = {}
    for r in csv.DictReader(open(N / "boss-enemy-params.csv", encoding="utf-8")):
        k64[int(r["key64"])] = int(r["char_id"])
    rows = []
    for root, _, fs in os.walk(LFM):
        for f in fs:
            p = pathlib.Path(root) / f
            t = p.read_bytes()
            if len(t) < 8 or t[:4] != b"\x00\x22\x04\x20":
                continue
            c = struct.unpack_from("<I", t, 4)[0]
            if c == 0 or 8 + c * 0x138 > len(t):
                continue
            name = "/".join(p.relative_to(LFM).parts[1:])
            if name not in use:
                continue
            pool = 8 + c * 0x138
            recs = []
            for j in range(c):
                r = t[8 + j * 0x138:8 + (j + 1) * 0x138]
                no = struct.unpack_from("<I", r, 0x134)[0]
                if pool + no >= len(t):
                    recs = None
                    break
                recs.append((struct.unpack_from("<I", r, 0)[0], struct.unpack_from("<I", r, 4)[0], r[0x9C],
                             t[pool + no:t.index(b"\0", pool + no)].decode("latin1")))
            if not recs:
                continue
            for pid, key, fl, nm in recs:
                cid = k64.get(key, 0)
                if not (fl & 4 or "BOSS" in nm.upper() or cid in named):
                    continue
                for m, slot in use[name]:
                    rows.append({"mission_id": "0x%04X" % m, "set_slot": slot, "table": name[:12], "placement_id": pid,
                                 "key64": key, "char_id": cid, "flags9c": "0x%02X" % fl, "record_name": nm})
    rows.sort(key=lambda r: (r["mission_id"], r["placement_id"]))
    with open(N / "rematch-enemy-index.csv", "w", newline="", encoding="utf-8") as o:
        w = csv.DictWriter(o, fieldnames=list(rows[0].keys()), lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    # Emaki table (20 x 0x34) + the 0xCC96 gadget table (40 x 0xA4) + the HYAKKI wave boxes (50 x 0xA0 + names)
    e = lfm(EMAKI).read_bytes()
    g = lfm(EMAKI_GADGETS).read_bytes()
    bx = lfm(EMAKI_BOXES).read_bytes()
    gad = {struct.unpack_from("<I", g, 8 + i * 0xA4)[0]: (i, struct.unpack_from("<I", g, 8 + i * 0xA4 + 0xC)[0])
           for i in range(struct.unpack_from("<I", g, 4)[0])}
    nb = struct.unpack_from("<I", bx, 4)[0]
    pool = 8 + nb * 0xA0
    boxes = {}
    for i in range(nb):
        r = bx[8 + i * 0xA0:8 + (i + 1) * 0xA0]
        no = struct.unpack_from("<I", r, 0x9C)[0]
        boxes[struct.unpack_from("<I", r, 0)[0]] = bx[pool + no:bx.index(b"\0", pool + no)].decode("latin1")
    er = []
    for i in range(struct.unpack_from("<I", e, 4)[0]):
        r = e[8 + i * 0x34:8 + (i + 1) * 0x34]
        u = struct.unpack_from("<6I", r, 0)
        x, y, z, yaw = struct.unpack_from("<4f", r, 0x1C)
        er.append({"index": i, "emaki_byte": "0x%02X" % r[0x30], "region_byte": "0x%02X" % r[0x31],
                   "hell_id": struct.unpack_from("<H", r, 0x2C)[0], "w2e": struct.unpack_from("<H", r, 0x2E)[0],
                   "u00": u[0], "u04": u[1], "u08": u[2], "start_point_0c": u[3], "gadget_10": u[4], "wave_box_14": u[5],
                   "gadget_row": gad.get(u[4], ("-", 0))[0], "gadget_kind": gad.get(u[4], ("-", 0))[1],
                   "box_name": boxes.get(u[5], "?"), "warp": "%.0f,%.0f,%.0f,%.1f" % (x, y, z, yaw),
                   "record_hex": r.hex()})
    with open(N / "rematch-emaki.csv", "w", newline="", encoding="utf-8") as o:
        w = csv.DictWriter(o, fieldnames=list(er[0].keys()), lineterminator="\n")
        w.writeheader()
        w.writerows(er)
    print("wrote notes/rematch-enemy-index.csv (%d rows), notes/rematch-emaki.csv (%d records)" % (len(rows), len(er)))


def _script_box_calls(d):
    """mission script bytes -> {int constant: set(native names)} for constants pushed (op 0x29) right before a native call."""
    try:
        ln = d[0]
        q = 1 + ln
        cnt = struct.unpack_from("<H", d, q)[0]
        q += 2
        names = []
        for _ in range(cnt):
            n = struct.unpack_from("<H", d, q)[0]
            names.append(d[q + 6:q + 6 + n].decode("latin1"))
            q += 6 + n + 1
        n = struct.unpack_from("<H", d, q)[0]
        q += 2
        syms = {struct.unpack_from("<HHH", d, q + 6 * i)[1]: struct.unpack_from("<HHH", d, q + 6 * i)[0] for i in range(n)}
        q += 6 * n
        ms = list(re.finditer(rb"[\x20-\x7e\x80-\xff]{3,}\x00", d[q:]))
        pe = q + ms[0].end() if ms else q
        for m in ms[1:]:
            if q + m.start() - pe > 5:
                break
            pe = q + m.end()
        i = d.index(b"\x35\x00", pe)
    except (ValueError, IndexError, struct.error):
        return {}
    ops = []
    while i + 2 <= len(d):
        o = struct.unpack_from("<H", d, i)[0]
        if o in (0x1b, 0x1f, 0x3b, 0x3a, 0x01, 0x2f, 0x50, 0x51):
            ln = 2
        elif o in (0x29, 0x2d, 0x22, 0x23, 0x24, 0x30, 0x31, 0x05, 0x11, 0x3e, 0x3d):
            ln = 4
        elif o in (0x35, 0x2c, 0x2e):
            if i + 4 > len(d):
                break
            ln = 2 + (8 if o == 0x35 else 4) * struct.unpack_from("<H", d, i + 2)[0]
        else:
            ln = 0
        if o > 0x5f or i + 2 + ln > len(d):
            break
        ops.append((o, d[i + 2:i + 2 + ln]))
        i += 2 + ln
    out = collections.defaultdict(set)
    for k, (o, x) in enumerate(ops):
        if o in (0x1b, 0x1f) and len(x) == 2:
            sid = struct.unpack("<H", x)[0]
            nm = names[syms[sid]] if sid in syms and syms[sid] < len(names) else ""
            if not nm.startswith(TRIG_CALLS) or not ("Box" in nm or "Invade" in nm):
                continue
            for j in range(max(0, k - 5), k):
                if ops[j][0] == 0x29:
                    out[struct.unpack("<i", ops[j][1])[0]].add(nm)
    return out


def _idtab_and_files():
    d = lfm(IDTAB).read_bytes()
    E = {}
    for i in range(struct.unpack_from("<I", d, 4)[0]):
        e = 8 + i * 8
        k, o = struct.unpack_from("<Ii", d, e)
        E[k] = d[e + o:d.index(b"\0", e + o)].decode("latin1")
    files = {}
    for root, _, fs in os.walk(LFM):
        for f in fs:
            p = pathlib.Path(root) / f
            files["/".join(p.relative_to(LFM).parts[1:])] = p
    return E, files


def build_fight_index():
    """--fight-index: per mission record, its start points (+0x6C table), boss-area boxes (+0x5C PlacementBox table, names with
    BOSS), the script's player-enter trigger boxes (kind trig: boxes the mission script (+0x50) tests with Player::IsEnter* /
    Wait_Invade* / Loop_*) and the boss-like enemy records listed in rematch-enemy-index.csv (positions from its +0x54 table)."""
    E, files = _idtab_and_files()
    md = lfm(MISSIONDB).read_bytes()
    want = collections.defaultdict(set)
    for r in rd("rematch-enemy-index.csv"):
        want[int(r["mission_id"], 16)].add(int(r["placement_id"]))
    internal = {int(r["mission_key_u16(+158)"]) for r in rd("missions.csv") if r["class"] == "internal-stage"}

    def tab(rec, off):
        nm = E.get(struct.unpack_from("<I", rec, off)[0])
        p = files.get(nm) if nm else None
        t = p.read_bytes() if p else b""
        return t if len(t) >= 8 and t[:4] == b"\x00\x22\x04\x20" else b""

    rows = []
    for i in range(struct.unpack_from("<I", md, 4)[0]):
        rec = md[8 + i * 0x198:8 + (i + 1) * 0x198]
        mid = struct.unpack_from("<H", rec, 0x158)[0]
        if mid in internal:
            continue
        mh = "0x%04X" % mid
        t = tab(rec, 0x6C)
        c = struct.unpack_from("<I", t, 4)[0] if t else 0
        if t and 8 + c * 0xA4 <= len(t):
            for j in range(c):
                r = t[8 + j * 0xA4:8 + (j + 1) * 0xA4]
                x, y, z, yaw = struct.unpack_from("<4f", r, 0x40)
                rows.append({"mission_id": mh, "kind": "start", "id": struct.unpack_from("<I", r, 0)[0], "flags": "0x%X" % struct.unpack_from("<I", r, 8)[0],
                             "sub": struct.unpack_from("<I", r, 0xC)[0], "x": "%.0f" % x, "y": "%.0f" % y, "z": "%.0f" % z, "yaw": "%.1f" % yaw,
                             "ex": "", "ey": "", "ez": "", "name": "#%d" % j})
        snm = E.get(struct.unpack_from("<I", rec, 0x50)[0])
        sp = files.get(snm) if snm else None
        trig = _script_box_calls(sp.read_bytes()) if sp else {}
        t = tab(rec, 0x5C)
        c = struct.unpack_from("<I", t, 4)[0] if t else 0
        if t and 8 + c * 0xA0 <= len(t):
            pool = 8 + c * 0xA0
            for j in range(c):
                r = t[8 + j * 0xA0:8 + (j + 1) * 0xA0]
                no = struct.unpack_from("<I", r, 0x9C)[0]
                if pool + no >= len(t):
                    break
                nm = t[pool + no:t.index(b"\0", pool + no)].decode("latin1")
                u = nm.upper()
                bid = struct.unpack_from("<I", r, 0)[0]
                if bid in trig:
                    cx, cy, cz, _w, yaw = struct.unpack_from("<5f", r, 0x10)
                    ex, ey, ez = struct.unpack_from("<3f", r, 0x28)
                    rows.append({"mission_id": mh, "kind": "trig", "id": bid, "flags": "", "sub": ascii_(" ".join(sorted(trig[bid])))[:80],
                                 "x": "%.0f" % cx, "y": "%.0f" % cy, "z": "%.0f" % cz, "yaw": "%.1f" % yaw, "ex": "%.0f" % ex,
                                 "ey": "%.0f" % ey, "ez": "%.0f" % ez, "name": ascii_(nm)[:60]})
                if "BOSS" not in u or not any(k in u for k in ("AREA", "BOX", "BARRIER")) or any(k in u for k in ("BGM", "JUKE", "ITEM")):
                    continue
                cx, cy, cz, _w, yaw = struct.unpack_from("<5f", r, 0x10)
                ex, ey, ez = struct.unpack_from("<3f", r, 0x28)
                rows.append({"mission_id": mh, "kind": "box", "id": struct.unpack_from("<I", r, 0)[0], "flags": "", "sub": "",
                             "x": "%.0f" % cx, "y": "%.0f" % cy, "z": "%.0f" % cz, "yaw": "%.1f" % yaw, "ex": "%.0f" % ex,
                             "ey": "%.0f" % ey, "ez": "%.0f" % ez, "name": ascii_(nm)[:60]})
        t = tab(rec, 0x54)
        c = struct.unpack_from("<I", t, 4)[0] if t else 0
        if t and want.get(mid) and 8 + c * 0x138 <= len(t):
            for j in range(c):
                r = t[8 + j * 0x138:8 + (j + 1) * 0x138]
                pid = struct.unpack_from("<I", r, 0)[0]
                if pid not in want[mid]:
                    continue
                x, y, z, yaw = struct.unpack_from("<4f", r, 0x40)
                rows.append({"mission_id": mh, "kind": "boss", "id": pid, "flags": "0x%02X" % r[0x9C], "sub": "", "x": "%.0f" % x,
                             "y": "%.0f" % y, "z": "%.0f" % z, "yaw": "%.1f" % yaw, "ex": "", "ey": "", "ez": "", "name": ""})
    rows.sort(key=lambda r: (r["mission_id"], r["kind"], int(r["id"])))
    with open(N / FIGHT_IDX, "w", newline="", encoding="utf-8") as o:
        w = csv.DictWriter(o, fieldnames=FCOLS, lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    c = collections.Counter(r["kind"] for r in rows)
    print("wrote notes/%s (%s)" % (FIGHT_IDX, ", ".join("%s %d" % kv for kv in sorted(c.items()))))


# ---------------------------------------------------------------- fight now (E1 straight to the boss): start point choice
def load_fight_index():
    fi = collections.defaultdict(lambda: {"start": [], "box": [], "trig": [], "boss": {}})
    p = N / FIGHT_IDX
    if not p.exists():
        return fi
    for r in csv.DictReader(open(p, encoding="utf-8")):
        m = int(r["mission_id"], 16)
        v = dict(r, id=int(r["id"]), pos=(float(r["x"]), float(r["y"]), float(r["z"])), yawf=float(r["yaw"]))
        if r["ex"]:
            v["ext"] = (float(r["ex"]), float(r["ey"]), float(r["ez"]))
        if r["kind"] == "boss":
            fi[m]["boss"][v["id"]] = v
        else:
            fi[m][r["kind"]].append(v)
    return fi


def _dist_m(a, b):
    return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2) ** 0.5 / 100.0


def pick_fight_start(fi, mid, pid):
    """-> dict(start, kind, dist, conf, pos, yaw, box) or None.  Rules (notes/boss-rematch-build.md s.E1):
    one start point = the mission already starts at its fight (high);  flags 0x200 = a post-boss / script retry point: never used;
    flags 0x40 within 40 m = the boss retry point class (Jakotsu-baba's sits 0.6 m outside BOSS_AREA, facing the boss): high;
    a plain spawn point (kind 0) or gadget point (908) 8..40 m away, or a shrine (906/907/909) 8..60 m away: medium;
    anything else within 120 m (incl. points < 8 m = probably inside the arena / a post-fight point): low;  farther: not offered."""
    m = fi.get(mid)
    if not m or not pid or pid not in m["boss"] or not m["start"]:
        return None
    bp = m["boss"][pid]["pos"]
    st = m["start"]
    bx = sorted(m["box"], key=lambda b: _dist_m(b["pos"], bp))
    box = bx[0]["id"] if bx and _dist_m(bx[0]["pos"], bp) < 60 else 0
    d = lambda s: _dist_m(s["pos"], bp)

    def out(s, kind, conf):
        return {"start": s["id"], "kind": kind, "dist": d(s), "conf": conf, "pos": s["pos"], "yaw": s["yawf"], "box": box}

    if len(st) == 1:
        s = st[0]
        return out(s, "mission start", "high" if d(s) <= 30 else "medium" if d(s) <= 80 else "low")
    ok = sorted((s for s in st if not int(s["flags"], 16) & 0x200), key=lambda s: (d(s), s["id"]))
    for s in ok:
        if int(s["flags"], 16) & 0x40 and d(s) <= 40:
            return out(s, "boss retry point", "high")
    for s in ok:
        k = int(s["sub"])
        if d(s) >= 8 and ((k in (0, 908) and d(s) <= 40) or (k in (906, 907, 909) and d(s) <= 60)):
            return out(s, "shrine" if k in (906, 907, 909) else "spawn point" if k == 0 else "gadget point", "medium")
    for s in ok:
        if d(s) <= 120:
            k = int(s["sub"])
            return out(s, "shrine" if k in (906, 907, 909) else "spawn point" if k == 0 else "gadget point", "low")
    return None


BAD_TRIG = ("BGM", "JUKE", "ITEM", "GUEST", "INV", "SILENCE", "CLEAR", "VOICE", "TALK", "NOKATANA", "NO_KATANA")


def pick_arena(fi, mid, pid, f):
    """-> dict(x, y, z, yaw, box, src, conf) = a point just inside the arena trigger, or None.  f = pick_fight_start() result.
    Box: a script trigger box (kind trig) with BOSS in its name <= 40 m from the boss (conf medium; high when ARENA_SEEN), else a
    BOSS area box of the table <= 40 m (low, not seen as a trigger in the script).  Point: from the box centre towards the start
    point, 60 % of the way to the box edge (box +0x28 = full size, rotated by +0x20 yaw about y), so it is inside the box under
    either size convention; y = the start / boss heights interpolated + 1 m (the player drops onto the floor); yaw faces the boss
    (yaw 0 = +z, as Jakotsu-baba's boss retry point (yaw -180, north of the boss) shows)."""
    import math
    m = fi.get(mid)
    if not m or not f or pid not in m["boss"]:
        return None
    bp = m["boss"][pid]["pos"]
    cands, src, conf = [], "", ""
    for b in m["trig"]:
        u = b["name"].upper()
        if "BOSS" in u and not any(k in u for k in BAD_TRIG) and _dist_m(b["pos"], bp) <= 40:
            cands.append(b)
    if cands:
        src, conf = "script trigger", "medium"
    else:
        cands = [b for b in m["box"] if _dist_m(b["pos"], bp) <= 40 and not any(k in b["name"].upper() for k in BAD_TRIG)]
        src, conf = "boss area box", "low"
    if not cands:
        return None
    b = sorted(cands, key=lambda b: (_dist_m(b["pos"], bp), b["id"]))[0]
    if (mid, b["id"]) in ARENA_SEEN:
        conf = "high"
    c, ext, a = b["pos"], b["ext"], math.radians(b["yawf"])
    sp = f["pos"]
    dx, dz = sp[0] - c[0], sp[2] - c[2]
    lx, lz = dx * math.cos(a) - dz * math.sin(a), dx * math.sin(a) + dz * math.cos(a)   # into box-local axes
    hx, hz = ext[0] / 2, ext[2] / 2
    t = min(hx / abs(lx) if lx else 1e9, hz / abs(lz) if lz else 1e9, 1.0)      # ray centre -> start point leaves the box at t
    wx, wz = c[0] + dx * t * 0.6, c[2] + dz * t * 0.6
    hs = math.hypot(sp[0] - wx, sp[2] - wz)
    hb = math.hypot(bp[0] - wx, bp[2] - wz)
    wy = (sp[1] * hb + bp[1] * hs) / (hs + hb if hs + hb else 1) + 100
    yaw = math.degrees(math.atan2(bp[0] - wx, bp[2] - wz))
    return {"x": wx, "y": wy, "z": wz, "yaw": yaw, "box": b["id"], "src": "%s %d %s" % (src, b["id"], b["name"][-40:]), "conf": conf}


# ---------------------------------------------------------------- catalogue
def rd(name):
    return list(csv.DictReader(open(N / name, encoding="utf-8")))


def region_of(era):
    for k, v in ERA:
        if k in era:
            return v
    return ascii_(era)[:20]


def mission_title(name):
    return ascii_(re.sub(r"\^\d+~[A-Z_]+~", "", name)).strip()


def mtitle(b, rn=""):
    t = mission_title(b["name"])
    if t in ("", "Dummy") and rn:
        return "unlisted stage %s" % ascii_(rn)[:34]
    return t


def classify(mid, bs, mi, ma):
    """-> (route letter or None, reason) for one mission id."""
    b = bs.get(mid)
    m = mi.get(mid, {})
    if b is None:
        return None, "mission id 0x%04X is not one of the 130 mission records (web guess)" % mid
    cls = m.get("class", "")
    shipped = bool(ma.get(mid, {}).get("script"))
    if mid in HIDDEN:
        return "H", ""
    if b["battle_scroll_candidate"] == "1":
        return "R", ""
    if mid == TITLE_RECORD:
        return None, "0x7F16 is the new-game prologue record (+0x40 bit0); the mod never starts it"
    if mid == EMAKI_TEST:
        return None, "0xA19A is the Picture Scroll test record (EMAKI_TEST_ENEMY_xx, +0x40 bit1)"
    if not shipped:
        return None, "record 0x%04X has no shipped script/placement set (set never built)" % mid
    if cls == "internal-stage":
        return None, "field: placed only in open-field region mission 0x%04X (internal-stage = a whole region)" % mid
    if cls == "tutorial":
        return None, "tutorial record"
    return "D", ""


def build():
    bm = rd("boss-master.csv")
    bs = {int(r["mission_id"], 16): r for r in rd("battle-scrolls.csv")}
    mi = {int(r["mission_key_u16(+158)"]): r for r in rd("missions.csv")}
    ma = {int(r["mission_id"]): r for r in rd("boss-map-all.csv")}
    cc = rd("boss-online-crosscheck.csv")
    idx = rd("rematch-enemy-index.csv")
    em = rd("rematch-emaki.csv")
    fi = load_fight_index()
    cnames = collections.defaultdict(set)
    for r in rd("boss-names.csv"):
        cnames[r["english_name"].strip().lower()].add(int(r["char_id"]))
    byc = collections.defaultdict(list)
    for r in idx:
        byc[int(r["char_id"])].append(r)

    groups = collections.OrderedDict()
    for r in bm:
        n = r["english_name"].strip()
        n = ALIAS.get(n.lower(), n)
        groups.setdefault(n, []).append(r)
    ccmap = {ALIAS.get(r["name"].lower(), r["name"]): r for r in cc}
    for n in ccmap:
        groups.setdefault(n, [])
    for n in EXTRA:
        groups.setdefault(n, [])

    allrows, xtsv = [], []
    used = set()
    for name, rows in groups.items():
        c = ccmap.get(name, {})
        if name in JUNK:
            ms = sorted({int(x["mission_id"]) for x in rows if x["mission_id"]})
            allrows.append({"boss": name or "(no name)", "type": "not a boss", "route": "X", "reason": JUNK[name],
                            "mission_id": " ".join("0x%04X" % m for m in ms), "verified": "-", "sources": "boss-master.csv"})
            continue
        cands = {}
        for x in rows:
            if x["mission_id"] and int(x["mission_id"]) in bs:
                m = int(x["mission_id"])
                pid = x["placement_id"] or ""
                rn = x["all_placements"].split(";")[0].split(":", 1)[1] if ":" in x["all_placements"] else ""
                cands.setdefault((m, pid), (rn, "boss-master/" + x["row_kind"], x))
        cids = set(cnames.get(name.lower(), set()))
        for x in rows:
            if x["param_char_id"]:
                cids.add(int(x["param_char_id"]))
        for cid in sorted(cids):
            for h in byc.get(cid, []):
                if not int(h["flags9c"], 16) & 4:
                    continue
                m = int(h["mission_id"], 16)
                if (m, h["placement_id"]) not in cands:
                    cands[(m, h["placement_id"])] = (h["record_name"], "enemy-index", None)
        routes, field_hits, why_x = [], [], []
        for (m, pid), (rn, src, x) in cands.items():
            rt, why = classify(m, bs, mi, ma)
            if rt is None:
                why_x.append(why)
                field_hits.append((m, pid, rn))
                continue
            if not pid and x is not None and x["row_kind"] == "script" and ma.get(m, {}).get("boss_placement_id"):
                pid = ma[m]["boss_placement_id"]
                rn = rn or ma[m]["boss_name"]
            q = 0 if (x is not None and x["row_kind"] == "script" and pid) else 1 if (x is None and pid) else 2
            routes.append((PRI[rt], rt, m, pid, rn, src, x, q))
        withpid = {r[2] for r in routes if r[3]}
        routes = [r for r in routes if r[3] or r[2] not in withpid]
        dedup = {}
        for r in routes:
            k = (r[2], r[3])
            if k not in dedup or r[7] < dedup[k][7]:
                dedup[k] = r
        routes = sorted(dedup.values(), key=lambda r: (r[0], r[7], r[2]))
        btype = c.get("type") or (rows[0]["mission_type"] if rows else "")
        if routes:
            region = region_of(bs[routes[0][2]]["era"])
        else:
            region = (rows[0]["region"] if rows else c.get("region", ""))[:30]
        if not routes:
            if field_hits:
                fm = sorted({m for m, _, _ in field_hits})
                if all(w.startswith("field:") for w in why_x):
                    reason = ("field only: every boss record of this name is in open-field region mission(s) %s (internal-stage = the "
                              "whole open-field region); no standalone record to replay, re-placing a beaten field boss needs its field "
                              "defeat flag cleared (a story-flag write, forbidden by s.6) and E1 cannot host it (0xCC96 spawns only its own "
                              "Picture Scroll gadgets, s.3)" % " ".join("0x%04X" % m for m in fm))
                else:
                    reason = "; ".join(sorted(set(why_x)))
                rec = "; ".join("0x%04X %s %s" % (m, p, n) for m, p, n in sorted(field_hits)[:4])
            elif cids:
                anyw = sorted({(h["mission_id"], h["record_name"]) for cid in cids for h in byc.get(cid, [])})
                reason = ("no boss record (+0x9C bit 4) of char %s in any shipped mission enemy table: %s, so there is no boss fight "
                          "to restart" % ("/".join(str(x) for x in sorted(cids)[:4]),
                                          ("only regular/wave records (%s)" % ", ".join("%s %s" % (a, b2[:30]) for a, b2 in anyw[:3]))
                                          if anyw else "no record at all in the 120 mission tables (cut, scripted-only or a different char id)"))
                rec = ""
            else:
                reason = "no data: the name matches no char id (boss-names.csv) and no boss-master row (online-only name, mission unconfirmed)"
                rec = ""
            allrows.append({"boss": name, "aliases": c.get("alt_names", "")[:60], "type": btype, "region": region, "route": "X",
                            "record_name": rec, "verified": "-", "reason": reason,
                            "unverified": ("web: %s" % c.get("mission", ""))[:80] if c else "",
                            "sources": "boss-master;enemy-index" + (";crosscheck" if c else "")})
            xtsv.append(xrow(name, region, reason))
            continue
        _, rt, m, pid, rn, src, x, _q = routes[0]
        b = bs[m]
        seen = SEEN.get((m, int(pid))) if pid else None
        variants, vm = [], {m}
        for r2 in routes[1:]:
            if r2[2] not in vm:
                vm.add(r2[2])
                variants.append("%s 0x%04X %s" % (r2[1], r2[2], mtitle(bs[r2[2]], r2[4] or ma.get(r2[2], {}).get("boss_name", ""))[:28]))
        variants += ["field 0x%04X %s" % (fm, fn[:28]) for fm, _, fn in sorted(field_hits)[:3]]
        unv = []
        if rt == "R" and not seen:
            unv.append("R-b direct start not yet run for this mission (R-a = the game's own list)")
        if rt == "H":
            unv.append("un-hide + replay of the hidden duel (R-12)")
        if rt == "D":
            unv.append("R-b start of an unlisted record: script, cleared flag, return path (R-15)")
        if not pid:
            unv.append("boss placement unknown (no flag-4 record): BossLog fills it")
        unv.append("NG+: the current journey slot only")
        dk = (x or {}).get("defeat_check_kind", "") if x else ""
        allrows.append({
            "boss": name, "aliases": c.get("alt_names", "")[:60], "type": btype, "region": region, "route": rt,
            "mission_id": "0x%04X" % m, "mission_name": mtitle(b, rn or ma.get(m, {}).get("boss_name", "")), "mission_class": mi.get(m, {}).get("class", ""),
            "placement_id": pid, "record_name": rn[:40], "stage": (x or {}).get("stage_id", ""),
            "warp": fight_text(pick_fight_start(fi, m, int(pid)) if pid else None), "arena": (x or {}).get("arena_box_ids", ""),
            "defeat_check": "clear 0x%04X (702B64)%s%s" % (m, "; " + dk[:50] if dk else "", "; flag %s seen" % seen[1] if seen else ""),
            "variants": "; ".join(variants), "verified": "1" if seen else "0",
            "unverified": "; ".join(unv), "reason": "", "sources": src + (";crosscheck" if c else "")})
        for r2 in routes:
            used.add((r2[1], r2[2], name, r2[3]))
    # runtime rows
    byM = collections.defaultdict(list)
    for rt, m, name, pid in used:
        byM[(rt, m)].append((name, pid))
    out = []
    for r in rd("battle-scrolls.csv"):
        mid = int(r["mission_id"], 16)
        if r["battle_scroll_candidate"] != "1" and mid not in HIDDEN:
            continue
        if "(no boss)" in r["boss_or_content"]:
            continue
        rt = "H" if mid in HIDDEN else "R"
        out.append(mrow(rt, mid, r, byM.get((rt, mid), []), ma, fi))
    for (rt, mid) in sorted(byM):
        if rt == "D":
            out.append(mrow("D", mid, bs[mid], byM[(rt, mid)], ma, fi))
    for e in em:
        out.append(erow(e))
    out.extend(xtsv)
    cnt = collections.Counter(o["name"] for o in out)
    for o in out:
        if cnt[o["name"]] > 1 and o["route"] != "X":
            mt = o["mission_name"]
            o["name"] = "%s (%s)" % (o["name"], mt.split(": ", 1)[1] if ": " in mt else mt)
    order = {v: i for i, (_, v) in enumerate(ERA)}
    rorder = {"R": 0, "H": 1, "D": 2, "E0": 3, "X": 4}
    out.sort(key=lambda o: (rorder[o["route"]], order.get(o["region"], 99)))
    return allrows, out


def blank():
    return {k: "-" for k in COLS}


def fight_text(f):
    if not f:
        return "mission start (game); fight now: not offered (no start point within 120 m of the boss)"
    return "fight now: start point %d (%s, %.0f m from the boss, %s confidence)" % (f["start"], f["kind"], f["dist"], f["conf"])


def mrow(rt, mid, b, lst, ma, fi):
    names = sorted({n for n, _ in lst})
    mb = ma.get(mid, {}).get("boss_placement_id", "")
    pids = [p for _, p in sorted(lst) if p]
    pid = mb if (mb and (mb in pids or not pids)) else (pids[0] if pids else "")
    o = blank()
    mt = mtitle(b, ma.get(mid, {}).get("boss_name", ""))
    nm = ascii_(" & ".join(names)) if names else mt
    o.update({"id": "%s%04X" % ("D" if rt == "D" else "R", mid), "name": nm if len(nm) <= 60 else nm[:57] + "...",
              "mission_name": mt, "region": region_of(b["era"]), "route": rt, "replay_mission_id": "0x%04X" % mid,
              "hidden": "1" if rt == "H" else "0", "story_mission": "0x%04X" % mid, "defeat_kind": "clear",
              "defeat_id": "0x%04X" % mid, "placement_id": str(pid) if pid else "-", "region_byte": b["region_15c"],
              "verified": "1" if rt == "R" else "0", "boss": ascii_(";".join(names)) or "-"})
    if pid and (mid, int(pid)) in SEEN:
        o["seen"] = SEEN[(mid, int(pid))][0]
    f = pick_fight_start(fi, mid, int(pid)) if pid else None
    if f:
        o.update({"stage_mission": str(f["start"]), "warp_x": "%.0f" % f["pos"][0], "warp_y": "%.0f" % f["pos"][1], "warp_z": "%.0f" % f["pos"][2],
                  "warp_yaw": "%.1f" % f["yaw"], "box_id": str(f["box"]) if f["box"] else "-", "fight_kind": f["kind"],
                  "fight_dist_m": "%.0f" % f["dist"], "fight_conf": f["conf"]})
        if f["kind"] == "mission start" and f["dist"] <= 30:
            o.update({"arena_src": "mission start (the mission already starts at the fight)", "arena_conf": f["conf"]})
        else:
            ar = pick_arena(fi, mid, int(pid), f)
            if ar:
                o.update({"arena_x": "%.0f" % ar["x"], "arena_y": "%.0f" % ar["y"], "arena_z": "%.0f" % ar["z"], "arena_yaw": "%.1f" % ar["yaw"],
                          "arena_box": str(ar["box"]), "arena_src": ascii_(ar["src"]).replace(" ", "_"), "arena_conf": ar["conf"]})
    return o


def erow(e):
    o = blank()
    x, y, z, yaw = e["warp"].split(",")
    i = int(e["index"])
    area = e["box_name"].replace("HYAKKI_WB_START_", "").replace("_", " ").title()
    o.update({"id": "E%02d" % i, "name": "Hundred Demon Realms: %s" % area, "mission_name": "Picture Scroll 0xCC96 record %d" % i,
              "region": REGION_BYTE_ERA.get(int(e["region_byte"], 16), "?"), "route": "E0", "hidden": "0", "story_mission": "0xCC96",
              "defeat_kind": "scroll", "defeat_id": "0xFFFF", "emaki_byte": e["emaki_byte"], "placement_id": e["gadget_10"],
              "stage_mission": e["start_point_0c"], "warp_x": x, "warp_y": y, "warp_z": z, "warp_yaw": yaw, "box_id": e["wave_box_14"],
              "hell_id": str(e["hell_id"]), "region_byte": e["region_byte"], "verified": "0", "emaki_index": str(i)})
    return o


def xrow(name, region, reason):
    o = blank()
    o.update({"id": "X" + re.sub(r"[^A-Za-z0-9]", "", ascii_(name))[:14], "name": ascii_(name),
              "region": region_of(region) if region else "-", "route": "X", "hidden": "0", "verified": "0", "boss": ascii_(name),
              "reason": ascii_(reason)[:240]})
    return o


def render_tsv(out):
    lines = ["# Nioh3BossRematch catalogue (tools/rematch_catalog.py; notes/rematch-all-bosses.md). Routes R/H/D/E0/X; "
             "'seen' = confirmed by an in-game log; Nioh3BossRematch_verified.tsv (verify_all.ps1) overrides 'verified' at run time.",
             "\t".join(COLS)]
    for o in out:
        lines.append("\t".join((str(o[c]).replace("\t", " ") or "-") for c in COLS))
    return "\r\n".join(lines) + "\r\n"


def render_csv(allrows):
    s = io.StringIO()
    w = csv.DictWriter(s, fieldnames=ACOLS, lineterminator="\n")
    w.writeheader()
    for r in allrows:
        w.writerow({k: ascii_(str(r.get(k, ""))) for k in ACOLS})
    return s.getvalue()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--index", action="store_true")
    ap.add_argument("--fight-index", action="store_true")
    ap.add_argument("--out", default=str(ROOT / "mod" / "Nioh3BossRematch" / "Nioh3BossRematch_bosses.tsv"))
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    if a.index:
        build_index()
        return 0
    if a.fight_index:
        build_fight_index()
        return 0
    allrows, out = build()
    csvtext = render_csv(allrows)
    tsvtext = render_tsv(out)
    cp = N / "rematch-all-bosses.csv"
    tp = pathlib.Path(a.out)
    if a.check:
        ok1 = cp.exists() and cp.read_bytes().replace(b"\r\n", b"\n") == csvtext.encode("ascii")
        ok2 = (not tp.parent.exists()) or (tp.exists() and tp.read_bytes() == tsvtext.encode("ascii"))
        print("rematch-all-bosses.csv: %s; %s: %s" % ("in sync" if ok1 else "OUT OF DATE", tp.name, "in sync" if ok2 else "OUT OF DATE"))
        return 0 if ok1 and ok2 else 1
    cp.write_bytes(csvtext.encode("ascii"))
    c = collections.Counter(r["route"] for r in allrows if r.get("type") != "not a boss")
    print("wrote %s: %d names (%d not a boss); bosses by route: %s" % (cp, len(allrows), sum(r.get("type") == "not a boss" for r in allrows),
                                                                      ", ".join("%s %d" % kv for kv in sorted(c.items()))))
    if tp.parent.exists():
        tp.write_bytes(tsvtext.encode("ascii"))
        c2 = collections.Counter(o["route"] for o in out)
        print("wrote %s: %d rows (%s)" % (tp, len(out), ", ".join("%s %d" % kv for kv in sorted(c2.items()))))
    else:
        print("runtime TSV skipped: %s does not exist on this checkout (branch rematch3 has it)" % tp.parent)
    return 0


if __name__ == "__main__":
    sys.exit(main())
