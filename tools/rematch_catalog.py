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
  E0  one of the 20 Picture Scroll records (mission 0xCC96). The default region is Waves and the name is
      "<area>: <n> waves, <toughest enemy>" from notes/rematch-scroll-waves.csv (the menu shows the part after
      the colon in its right column). SCROLL_REGION overrides one record index, so a row can leave Waves
      without a code change.
  X   no route: field-only (open-world region mission), unshipped record, not a boss, or no data. 'reason' says which.
      A field-only row with a picked CHxx enemy record also carries that record's spawn (summon_chr, warp, placement).
      summon_chr is the enemy record's +4 key, the same id Load::Character and the arena table use, not the param char_id.
      The pick is every bit-4 field hit of the name: a record name containing WORLD, else BOSS, else the lowest mission and placement.

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
import json
import os
import pathlib
import re
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
N = ROOT / "notes"
LFM = pathlib.Path(os.environ.get("NIOH3_LFM") or os.environ.get("NIOH3_TEST_LFM") or (ROOT / "data" / "extract" / "lfm"))
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
# wake_step (E rows): the WaveNudgeAt offset that woke each Picture Scroll record in the test runs, so the first warp under the loading
# screen lands on it. 0 = the record warp itself / not measured (the plain walk). Sources: pc2 #78351 E00-E09, Ally #78283 E10-E19, E07 Ally #78389.
WAKE_STEP = {0: 4, 1: 1, 4: 2, 7: 9, 8: 1, 9: 3, 10: 2, 17: 1, 18: 1}
# warning (human #94922): a known live problem shown on the row's menu line and footer, keyed by replay mission id. Data, not code:
# the menu shows any non-empty warning the same way. Remove the entry once the live fix is proven.
ROW_WARN = {0xB24D: "Known bug: Death Veil may not appear (bar only, wave never starts). Fix in progress."}
SEEN = {(0x34, 76781): ("2026-10-08-pc-replay", "118")}   # (mission, placement) -> (session, defeat flag); session-2026-10-08-gamedata.md
COLS = ["id", "name", "mission_name", "region", "route", "replay_mission_id", "hidden", "story_mission", "defeat_kind", "defeat_id",
        "emaki_byte", "emaki_key_u32", "placement_id", "stage_mission", "warp_x", "warp_y", "warp_z", "warp_yaw", "box_id", "hell_id",
        "region_byte", "verified", "seen", "emaki_index", "boss", "reason", "fight_kind", "fight_dist_m", "fight_conf", "arena_x",
        "arena_y", "arena_z", "arena_yaw", "arena_box", "arena_src", "arena_conf", "arena_boxgeo", "gate_id", "gate_kind", "boss_pos", "lesson", "wake_step", "warning", "mission_class", "confirmed", "summon_chr",
        "fight_signal", "wave_groups", "clear_x", "clear_y", "clear_z"]
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


def _script_box_calls(path):
    """Read script triggers with the validated LFM decoder; return placement -> calls and proven starts.

    A gate is a WaitWarpStart(G) whose later Create of the selected body is either in the same
    function, or in a parallel function that EndWarpRights(G) then Creates. Kind is door when any
    StartWarpByGadgetRights uses G, else box when StartWarp uses G. CALL_NATIVE ids are 1-based
    (lfm_disasm.insn_name). A later IsInteractionPlayer on that body is not a gate: it can start a
    different placement's BossBattleMode.
    """
    here = ROOT / "re" / "scripts_re"
    if str(here) not in sys.path:
        sys.path.insert(0, str(here))
    import lfm_disasm
    try:
        s = lfm_disasm.load(str(path))
    except (OSError, ValueError, IndexError, struct.error):
        return {}
    if not s.insns:
        return {}

    functions, cur = {}, None
    for ins in s.insns:
        if ins.op == 0x3B:
            cur = lfm_disasm.insn_name(s, ins)
            functions[cur] = []
        elif cur is not None:
            functions[cur].append(ins)
    names = lambda ins: lfm_disasm.insn_name(s, ins)

    def literal_args(at):
        # LFM decoder returns literal scalar arguments as decimal strings; packed WORDS arrays are hex words.
        out = []
        for token in lfm_disasm.call_args(s, at):
            try:
                out.append(int(token, 0))
            except (TypeError, ValueError):
                if isinstance(token, str) and token.startswith("[") and token.endswith("]"):
                    for word in token[1:-1].split(","):
                        try:
                            out.append(int(word, 16))
                        except ValueError:
                            pass
        return out

    out = collections.defaultdict(set)
    end_rights = collections.defaultdict(list)
    doors, boxes = set(), set()
    waits = []
    for fn, insns in functions.items():
        for k, ins in enumerate(insns):
            if ins.op not in (0x1B, 0x1F):
                continue
            nm = names(ins)
            vals = literal_args(s.insns.index(ins))
            if nm.startswith(TRIG_CALLS) and ("Box" in nm or "Invade" in nm):
                for v in vals:
                    out[v].add(nm)
            if not vals or vals[0] <= 0:
                continue
            gid = vals[0] & 0xFFFFFFFF
            if nm == "Function::WaitWarpStart":
                waits.append((fn, k, gid))
            elif nm == "Function::EndWarpRights":
                end_rights[gid].append((fn, k))
            elif nm == "Function::StartWarpByGadgetRights":
                doors.add(gid)
            elif nm == "Function::StartWarp":
                boxes.add(gid)

    def creates_after(fn, begin):
        found = set()
        insns = functions.get(fn, [])
        for ins in insns[begin:]:
            if ins.op not in (0x1B, 0x1F):
                continue
            if names(ins) in ("Character::Create", "Character::CreateAndWait"):
                found.update(v & 0xFFFFFFFF for v in literal_args(s.insns.index(ins)) if v > 0)
        return found

    gates = []
    for fn, k, gate in waits:
        created = creates_after(fn, k + 1)
        for efn, ek in end_rights.get(gate, []):
            created |= creates_after(efn, ek + 1)
        if not created:
            continue
        kind = "door" if gate in doors else "box" if gate in boxes else "other"
        gates.append((fn, gate, kind, sorted(created)))

    out[None] = gates
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
        trig = _script_box_calls(sp) if sp else {}
        for fn, g, kind, cr in trig.pop(None, []):
            rows.append({"mission_id": mh, "kind": "gate", "id": g, "flags": kind, "sub": " ".join(str(c) for c in cr[:12]), "x": "0", "y": "0",
                         "z": "0", "yaw": "0", "ex": "", "ey": "", "ez": "", "name": ascii_(fn)[:60]})
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
    fi = collections.defaultdict(lambda: {"start": [], "box": [], "trig": [], "gate": [], "boss": {}})
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


def _in_enter_box(pos, box):
    """box is centre x y z, yaw degrees, full size x y z from the script's first Player::IsEnterBox."""
    import math
    cx, cy, cz, yaw, sx, sy, sz = box
    dx, dz = pos[0] - cx, pos[2] - cz
    a = math.radians(yaw)
    lx = dx * math.cos(a) - dz * math.sin(a)
    lz = dx * math.sin(a) + dz * math.cos(a)
    return abs(lx) <= sx / 2.0 + 50 and abs(lz) <= sz / 2.0 + 50 and abs(pos[1] - cy) <= sy / 2.0 + 50


def _mission_only_start(s, facts):
    """One mission start and no boss placement. Distance is to the clear gadget, else the enter box."""
    clear = facts.get("clear")
    box = facts.get("box")
    ref = clear if clear is not None else ((box[0], box[1], box[2]) if box else None)
    dist = _dist_m(s["pos"], ref) if ref else 0.0
    if dist > 120:
        return None
    inside = _in_enter_box(s["pos"], box) if box else False
    conf = "high" if facts.get("signal") == "waves" and inside else "medium"
    return {"start": s["id"], "kind": "mission start", "dist": dist, "conf": conf, "pos": s["pos"], "yaw": s["yawf"], "box": 0}


def pick_fight_start(fi, mid, pid, facts=None):
    """-> dict(start, kind, dist, conf, pos, yaw, box) or None.  Rules (notes/boss-rematch-build.md s.E1):
    one start point = the mission already starts at its fight (high);  flags 0x200 = a post-boss / script retry point: never used;
    flags 0x40 within 40 m = the boss retry point class (Jakotsu-baba's sits 0.6 m outside BOSS_AREA, facing the boss): high;
    a plain spawn point (kind 0) or gadget point (908) 8..40 m away, or a shrine (906/907/909) 8..60 m away: medium;
    anything else within 120 m (incl. points < 8 m = probably inside the arena / a post-fight point): low;  farther: not offered.
    No placement: the single mission start, measured to the script clear gadget or enter box. Not a per-row list."""
    m = fi.get(mid)
    if not m or not m["start"]:
        return None
    if not pid or pid not in m["boss"]:
        if pid or len(m["start"]) != 1:
            return None
        return _mission_only_start(m["start"][0], facts or {})
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
    # same floor as the boss first (Takeda Shingen: a gadget point 20 m below the arena tied with a spawn point on its level;
    # self-test 2026-10-09), then the distance
    ok = sorted((s for s in st if not int(s["flags"], 16) & 0x200), key=lambda s: (abs(s["pos"][1] - bp[1]) > 500, d(s), s["id"]))
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


def pick_gate(fi, mid, pid):
    """-> (gate id, kind) the boss's start function waits on (WaitWarpStart), or None. One gate in the mission = that one; several =
    the one whose function creates the boss placement."""
    m = fi.get(mid)
    if not m or not m["gate"]:
        return None
    gs = m["gate"]
    if len({g["id"] for g in gs}) == 1:
        return gs[0]["id"], gs[0]["flags"]
    for g in gs:
        if str(pid) in g["sub"].split():
            return g["id"], g["flags"]
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
    hx0, hz0 = ext[0] / 2, ext[2] / 2
    bdx, bdz = bp[0] - c[0], bp[2] - c[2]
    blx, blz = bdx * math.cos(a) - bdz * math.sin(a), bdx * math.sin(a) + bdz * math.cos(a)
    # human #129352 (R1CBC Train in arena left the player behind the arena fence): when the boss stands outside the trigger box, the box is
    # a doorway trigger across the arena entrance, so its start-point half lies outside the arena. Aim at the boss side of the box instead.
    tgt = bp if abs(blx) > hx0 or abs(blz) > hz0 else sp
    dx, dz = tgt[0] - c[0], tgt[2] - c[2]
    lx, lz = dx * math.cos(a) - dz * math.sin(a), dx * math.sin(a) + dz * math.cos(a)   # into box-local axes
    hx, hz = ext[0] / 2, ext[2] / 2
    t = min(hx / abs(lx) if lx else 1e9, hz / abs(lz) if lz else 1e9, 1.0)      # ray centre -> start point leaves the box at t
    wx, wz = c[0] + dx * t * 0.6, c[2] + dz * t * 0.6
    hs = math.hypot(sp[0] - wx, sp[2] - wz)
    hb = math.hypot(bp[0] - wx, bp[2] - wz)
    wy = (sp[1] * hb + bp[1] * hs) / (hs + hb if hs + hb else 1) + 100
    if abs(sp[1] - bp[1]) > 500:  # start point on another floor: use the boss's floor (Takeda: 9443 vs 11492 -> fell below the arena)
        wy = bp[1] + 100
    lo, hi = c[1] - ext[1] / 2, c[1] + ext[1] / 2
    if not lo <= wy <= hi and lo <= bp[1] + 100 <= hi:
        wy = bp[1] + 100
    yaw = math.degrees(math.atan2(bp[0] - wx, bp[2] - wz))
    geo = "%.0f_%.0f_%.0f_%.1f_%.0f_%.0f_%.0f" % (c[0], c[1], c[2], b["yawf"], ext[0], ext[1], ext[2])
    return {"x": wx, "y": wy, "z": wz, "yaw": yaw, "box": b["id"], "src": "%s %d %s" % (src, b["id"], b["name"][-40:]), "conf": conf, "geo": geo}


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


def is_placeholder_title(title):
    """Resolved game text. The mission-name string Dummy (text id 33397680) is the placeholder. Not a list of mission ids."""
    return mission_title(title or "") in ("", "Dummy")


def display_mission_name(game_title, boss_name, other_titles):
    """Menu label only. other_titles is one resolved title per other mission that places this same boss character.
    Exactly one real name -> that name. Zero or several -> the boss's own name.
    A real game title is returned unchanged. This is a display choice, not a claim that a title field links the missions.
    """
    t = mission_title(game_title or "")
    if not is_placeholder_title(t):
        return t
    named = [mission_title(x) for x in other_titles if not is_placeholder_title(x)]
    if len(named) == 1:
        return named[0]
    return ascii_(boss_name or "") or t


def apply_placeholder_labels(out, bs, idx):
    """Replace mission_name when the game title is placeholder text. Route, replay id, placement and the started mission stay put."""
    title_of = {mid: r["name"] for mid, r in bs.items()}
    key_at = {}
    keys_on = collections.defaultdict(set)
    missions_of = collections.defaultdict(set)
    for h in idx:
        m = int(h["mission_id"], 16)
        k = int(h["key64"])
        missions_of[k].add(m)
        if int(h["flags9c"], 16) & 4:
            key_at[(m, str(h["placement_id"]))] = k
            keys_on[m].add(k)
    for o in out:
        mid_s = str(o.get("replay_mission_id") or "")
        if not mid_s.startswith("0x"):
            continue
        mid = int(mid_s, 16)
        game = title_of.get(mid, "")
        if not is_placeholder_title(game):
            continue
        k = key_at.get((mid, str(o.get("placement_id") or "")))
        keys = [k] if k else sorted(keys_on.get(mid, ()))
        seen, other = set(), []
        for key in keys:
            for om in sorted(missions_of.get(key, ())):
                if om == mid or om in seen:
                    continue
                seen.add(om)
                other.append(title_of.get(om, ""))
        o["mission_name"] = display_mission_name(game, o.get("name") or "", other)


def _pid_int(pid):
    try:
        return int(pid)
    except (TypeError, ValueError):
        return 1 << 31


def enemy_positions():
    """(mission id, placement id) -> (x, y, z, yaw) from each mission's +0x54 enemy table, including open-field regions."""
    if getattr(enemy_positions, "cache", None) is not None:
        return enemy_positions.cache
    found = {}
    enemy_positions.cache = found
    base = _game_data_root()
    if base is None:
        return found
    lfm_root = base / "data" / "extract" / "lfm"
    d = lfm_root.joinpath(*IDTAB).read_bytes()
    E = {}
    for i in range(struct.unpack_from("<I", d, 4)[0]):
        e = 8 + i * 8
        k, o = struct.unpack_from("<Ii", d, e)
        E[k] = d[e + o:d.index(b"\0", e + o)].decode("latin1")
    files = {}
    for root, _, fs in os.walk(lfm_root):
        for f in fs:
            p = pathlib.Path(root) / f
            files["/".join(p.relative_to(lfm_root).parts[1:])] = p
    md = lfm_root.joinpath(*MISSIONDB).read_bytes()
    n = struct.unpack_from("<I", md, 4)[0]
    for i in range(n):
        rec = md[8 + i * 0x198:8 + (i + 1) * 0x198]
        mid = struct.unpack_from("<H", rec, 0x158)[0]
        nm = E.get(struct.unpack_from("<I", rec, 0x54)[0])
        p = files.get(nm) if nm else None
        if not p:
            continue
        t = p.read_bytes()
        if len(t) < 8 or t[:4] != b"\x00\x22\x04\x20":
            continue
        c = struct.unpack_from("<I", t, 4)[0]
        if c == 0 or 8 + c * 0x138 > len(t):
            continue
        for j in range(c):
            r = t[8 + j * 0x138:8 + (j + 1) * 0x138]
            pid = struct.unpack_from("<I", r, 0)[0]
            found[(mid, pid)] = struct.unpack_from("<4f", r, 0x40)
    return found


def pick_field_spawn(hits, positions, summon_keys):
    """One table-derived spawn. WORLD in the record name, else BOSS, else the lowest mission and placement.
    summon_keys maps (mission, placement) to the enemy record +4 key (Load::Character id)."""
    def rank(h):
        m, pid, rn = h
        u = (rn or "").upper()
        return (0 if "WORLD" in u else 1, 0 if "BOSS" in u else 1, m, _pid_int(pid))

    for m, pid, rn in sorted(hits, key=rank):
        ip = _pid_int(pid)
        if ip >= 1 << 31:
            continue
        pos = positions.get((m, ip))
        summon = summon_keys.get((m, ip), 0)
        if pos and summon:
            return m, ip, pos, summon, rn
    return None


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
    summon_keys, recname = {}, {}
    for r in idx:
        byc[int(r["char_id"])].append(r)
        try:
            key = (int(r["mission_id"], 16), int(r["placement_id"]))
            summon_keys[key] = int(r["key64"] or 0)
            recname[key] = r["record_name"]
        except (TypeError, ValueError):
            continue
    positions = enemy_positions()

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
            spawn = None
            if field_hits and all(w.startswith("field:") for w in why_x):
                spawn = pick_field_spawn(field_hits, positions, summon_keys)
                if spawn:
                    sm, spid, _pos, schr, srn = spawn
                    rb = bs[sm]["region_15c"] if sm in bs else "-"
                    spawn = (sm, spid, _pos, schr, rb, recname.get((sm, spid), srn))
                else:
                    print("open-world: %s has field records but no table position and char id" % name)
            allrows.append({"boss": name, "aliases": c.get("alt_names", "")[:60], "type": btype, "region": region, "route": "X",
                            "record_name": rec, "verified": "-", "reason": reason,
                            "unverified": ("web: %s" % c.get("mission", ""))[:80] if c else "",
                            "sources": "boss-master;enemy-index" + (";crosscheck" if c else "")})
            xtsv.append(xrow(name, region, reason, spawn))
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
    # mission_class (human #95155): the game's mission class from missions.csv (e.g. dojo-way), so menu filters group rows from data.
    for o in out:
        m = o.get("replay_mission_id") or "-"
        cls = mi.get(int(m, 16), {}).get("class", "") if m.startswith("0x") else ""
        o["mission_class"] = cls or "-"
    apply_confirmed(out)
    # f2e80b34 refused this row by editing the TSV only; keep the refusal when the catalogue is regenerated.
    for o in out:
        if o["id"] == "RBAB8":
            o.update({"route": "X", "replay_mission_id": "-", "verified": "0", "reason": 'script leaves Mori Gentatsu and Yamada Shinryuken frozen (AI off, invincible); only Anazawa Morihide can be hit, and his death does not end the mission'})
    apply_placeholder_labels(out, bs, idx)
    cnt = collections.Counter(o["name"] for o in out)
    for o in out:
        if cnt[o["name"]] > 1 and o["route"] != "X":
            mt = o["mission_name"]
            o["name"] = "%s (%s)" % (o["name"], mt.split(": ", 1)[1] if ": " in mt else mt)
    order = {v: i for i, (_, v) in enumerate(ERA)}
    rorder = {"R": 0, "H": 1, "D": 2, "E0": 3, "X": 4}

    def sort_key(o):
        idx = int(o["emaki_index"]) if o.get("route") == "E0" and str(o.get("emaki_index", "")).isdigit() else 0
        era = 0 if o.get("route") == "E0" else order.get(o["region"], 99)
        return (rorder[o["route"]], era, idx)

    out.sort(key=sort_key)
    return allrows, out


CONFIRMED_JSON = ROOT / "tools" / "boss_confirmed.json"


def apply_confirmed(out, path=CONFIRMED_JSON):
    """confirmed (human #84670, #99050): the date of a recorded working live entry for this row, else '-'. Evidence lives in
    tools/boss_confirmed.json with the row's fight-path fields at test time; any change to those fields (route, mission, warp,
    arena, gate, wake step...) invalidates it, so new or changed bosses land in Not recently confirmed with no per-row rule."""
    reg = json.loads(path.read_text(encoding="utf-8")) if path.is_file() else {"rows": [], "path_fields": []}
    fields = reg.get("path_fields", [])
    ev = {r["id"]: r for r in reg.get("rows", [])}
    for o in out:
        e = ev.get(o["id"])
        ok = e is not None and o.get("route") != "X" and all((str(o.get(f, "")).replace("\t", " ") or "-") == e["path"].get(f, "-") for f in fields)
        o["confirmed"] = e["date"] if ok else "-"
    return out


_LESSON_IDS = None
TSV_DEFAULT = ROOT / "mod" / "Nioh3BossRematch" / "Nioh3BossRematch_bosses.tsv"


def _game_data_root():
    """Worktree first, then the main checkout. A sparse worktree has no LFM extract."""
    for base in (ROOT, pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine")):
        if (base / "data" / "extract" / "lfm").joinpath(*IDTAB).is_file() and (base / "data" / "lfm_list.tsv").is_file():
            return base
    return None


def _lfm_paths(base):
    """file id -> script path, same id table boss_map_all.py uses."""
    lfm_root = base / "data" / "extract" / "lfm"
    d = lfm_root.joinpath(*IDTAB).read_bytes()
    n = struct.unpack_from("<I", d, 4)[0]
    rel = {}
    for i in range(n):
        e = 8 + i * 8
        k, o = struct.unpack_from("<Ii", d, e)
        rel[k] = d[e + o:d.index(b"\0", e + o)].decode("latin1")
    arch = {}
    for line in (base / "data" / "lfm_list.tsv").read_text(encoding="utf-8", errors="replace").splitlines():
        p = line.split("\t")
        if len(p) > 5:
            arch[p[5].strip("/")] = p[0]
    out = {}
    for k, v in rel.items():
        a = arch.get(v)
        if a is None:
            continue
        path = lfm_root / ("archive_" + a) / pathlib.Path(*v.split("/"))
        if path.is_file():
            out[k] = path
    return out


def _disasm():
    here = ROOT / "re" / "scripts_re"
    if not (here / "lfm_disasm.py").is_file():
        here = pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_re")
    if str(here) not in sys.path:
        sys.path.insert(0, str(here))
    import lfm_disasm
    return lfm_disasm


def _script_walk(s, lfm, fn_at, entry):
    """Functions the entry actually reaches. Library copies do not count until called."""
    seen, stack = set(), [entry]
    while stack:
        sym = stack.pop()
        if sym in seen or sym not in fn_at:
            continue
        seen.add(sym)
        for ins in s.insns[fn_at[sym] + 1:]:
            if ins.op == 0x3B:
                break
            if ins.op == 0x1F:
                stack.append(ins.args[0])
    return seen


def _slot_words(insns):
    """Flatten the int and float arrays pushed as one call's arguments. Slot 1 is the first word."""
    items = []
    for ins in insns:
        if ins.op in (0x1B, 0x1F):
            break
        if ins.op == 0x2C:
            for w in ins.args[1:]:
                items.append(("i", w if w < 0x80000000 else w - (1 << 32)))
        elif ins.op == 0x2E:
            for w in ins.args[1:]:
                items.append(("f", struct.unpack("<f", struct.pack("<I", w & 0xFFFFFFFF))[0]))
    return items


def _v1_slot(token):
    m = re.fullmatch(r"v1\.(\d+)", token)
    return int(m.group(1)) if m else 0


def _bracket_floats(token):
    if len(token) < 2 or token[0] != "[" or token[-1] != "]":
        return None
    out = []
    for part in token[1:-1].split(","):
        part = part.strip()
        if not part:
            continue
        w = int(part, 16)
        out.append(struct.unpack("<f", struct.pack("<I", w))[0])
    return out


def _body_calls(s, lfm, fn_at, sym):
    start = fn_at[sym] + 1
    out = []
    for k in range(start, len(s.insns)):
        ins = s.insns[k]
        if ins.op == 0x3B:
            break
        if ins.op in (0x1B, 0x1F):
            out.append((lfm.insn_name(s, ins), lfm.call_args(s, k)))
    return out


def _script_record(path):
    """fight_signal boss|waves|-, wave group ids, clear-gadget xyz, first IsEnterBox.

    boss wins when the entry reaches Mission::BossBattleMode. Otherwise IsDeathGroupEnemy is waves.
    BossBattleModeEnd alone is not a boss. Wave ids and the clear point are the entry call's
    flattened argument slots (v1.N) that those natives load. No mission id is named here.
    """
    empty = {"signal": "-", "waves": [], "clear": None, "box": None}
    lfm = _disasm()
    s = lfm.load(str(path))
    if not s.insns:
        return empty
    fn_at = {}
    entry = None
    for i, ins in enumerate(s.insns):
        if ins.op == 0x3B:
            fn_at[ins.args[0]] = i
            entry = ins.args[0]
    if entry is None:
        return empty
    reached = _script_walk(s, lfm, fn_at, entry)
    death = boss = False
    for sym in reached:
        for name, _args in _body_calls(s, lfm, fn_at, sym):
            if name == "Character::IsDeathGroupEnemy":
                death = True
            elif name == "Mission::BossBattleMode":
                boss = True
    signal = "boss" if boss else "waves" if death else "-"
    waves, clear, box = [], None, None
    entry_name = lfm.symname(s, entry)
    for _off, fn, ins in lfm.statements(s):
        if fn != entry_name:
            continue
        call = next((i for i in ins if i.op == 0x1F), None)
        if call is None or call.args[0] not in reached:
            continue
        slots = _slot_words(ins)
        groups = []
        floats = []
        seen_g, seen_f = set(), set()
        for name, args in _body_calls(s, lfm, fn_at, call.args[0]):
            if name in ("Character::IsDeathGroupEnemy", "Character::CreateGroupEnemy"):
                for token in args:
                    slot = _v1_slot(token)
                    if slot in seen_g or slot < 1 or slot > len(slots):
                        continue
                    kind, val = slots[slot - 1]
                    if kind == "i" and 0 < val < 0x10000:
                        seen_g.add(slot)
                        groups.append((slot, val))
            elif name in ("Gadget::CreatePlacementGadgetPosition", "UI::SetCompassDestinationIndex"):
                for token in args:
                    slot = _v1_slot(token)
                    if slot in seen_f or slot < 1 or slot > len(slots):
                        continue
                    kind, val = slots[slot - 1]
                    if kind == "f":
                        seen_f.add(slot)
                        floats.append((slot, val))
            elif name == "Player::IsEnterBox" and box is None:
                for token in args:
                    got = _bracket_floats(token)
                    if got and len(got) >= 9:
                        box = (got[0], got[1], got[2], got[4], got[6], got[7], got[8])
                        break
        if groups and not waves:
            waves = [val for _slot, val in sorted(groups)]
        if len(floats) >= 3 and clear is None:
            floats.sort()
            clear = (floats[0][1], floats[1][1], floats[2][1])
    return {"signal": signal, "waves": waves, "clear": clear, "box": box}


def _script_is_lesson(path):
    """Entry reaches IsDeathGroupEnemy and never Mission::BossBattleMode."""
    return _script_record(path)["signal"] == "waves"


_MISSION_FACTS = None


def mission_script_facts():
    """mission id -> script record. None when this machine has no LFM extract."""
    global _MISSION_FACTS, _LESSON_IDS
    if _MISSION_FACTS is not None:
        return _MISSION_FACTS
    base = _game_data_root()
    if base is None:
        return None
    files = _lfm_paths(base)
    md = (base / "data" / "extract" / "lfm").joinpath(*MISSIONDB).read_bytes()
    n = struct.unpack_from("<I", md, 4)[0]
    judged = {}
    facts = {}
    lessons = set()
    for i in range(n):
        b = 8 + i * 0x198
        if b + 0x15A > len(md):
            break
        fid = struct.unpack_from("<I", md, b + 0x50)[0]
        mid = struct.unpack_from("<H", md, b + 0x158)[0]
        path = files.get(fid)
        if path is None:
            continue
        key = str(path)
        if key not in judged:
            judged[key] = _script_record(path)
        facts[mid] = judged[key]
        if judged[key]["signal"] == "waves":
            lessons.add(mid)
    _MISSION_FACTS = facts
    _LESSON_IDS = lessons
    return _MISSION_FACTS


def lesson_mission_ids():
    """Mission ids the script scan calls a lesson, or None when the LFM extract is not on this machine.

    The Boss Match list is drawn before the mission script runs, and the ASI does not contain the
    archive, so the result is stored in the lesson column. None (not an empty set) means "could not
    scan"; the caller keeps the column already in the catalogue.
    """
    if mission_script_facts() is None:
        return None
    return _LESSON_IDS


def _preserved_lesson_ids(path=TSV_DEFAULT):
    """Lesson mission ids already in the catalogue. Used only when this machine has no LFM extract."""
    if not path.is_file():
        return set()
    header = None
    found = set()
    for line in path.read_text(encoding="ascii", errors="replace").splitlines():
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        if header is None:
            header = parts
            continue
        if "lesson" not in header or "replay_mission_id" not in header:
            return set()
        j, k = header.index("lesson"), header.index("replay_mission_id")
        if j < len(parts) and parts[j] == "1" and k < len(parts) and parts[k].startswith("0x"):
            found.add(int(parts[k], 16))
    return found


def _lesson_flag(mid):
    ids = lesson_mission_ids()
    if ids is None:
        ids = _preserved_lesson_ids()
    return "1" if mid in ids else "0"


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
    facts = {}
    scanned = mission_script_facts()
    if scanned is not None:
        facts = scanned.get(mid) or {}
    f = pick_fight_start(fi, mid, int(pid) if pid else 0, facts)
    gt = pick_gate(fi, mid, int(pid)) if pid else None
    if f and pid and fi.get(mid) and int(pid) in fi[mid]["boss"]:
        bp = fi[mid]["boss"][int(pid)]["pos"]
        o["boss_pos"] = "%.0f_%.0f_%.0f" % bp
    if f and gt:
        o.update({"gate_id": str(gt[0]), "gate_kind": gt[1]})
    if f:
        o.update({"stage_mission": str(f["start"]), "warp_x": "%.0f" % f["pos"][0], "warp_y": "%.0f" % f["pos"][1], "warp_z": "%.0f" % f["pos"][2],
                  "warp_yaw": "%.1f" % f["yaw"], "box_id": str(f["box"]) if f["box"] else "-", "fight_kind": f["kind"],
                  "fight_dist_m": "%.0f" % f["dist"], "fight_conf": f["conf"]})
        if f["kind"] == "mission start" and f["dist"] <= 30:
            o.update({"arena_src": "mission start (the mission already starts at the fight)", "arena_conf": f["conf"]})
        elif pid:
            ar = pick_arena(fi, mid, int(pid), f)
            if ar:
                o.update({"arena_x": "%.0f" % ar["x"], "arena_y": "%.0f" % ar["y"], "arena_z": "%.0f" % ar["z"], "arena_yaw": "%.1f" % ar["yaw"],
                          "arena_box": str(ar["box"]), "arena_src": ascii_(ar["src"]).replace(" ", "_"), "arena_conf": ar["conf"],
                          "arena_boxgeo": ar["geo"]})
    if scanned is None:
        o["lesson"] = _lesson_flag(mid)
    else:
        o["lesson"] = "1" if facts.get("signal") == "waves" else "0"
        o["fight_signal"] = facts.get("signal") or "-"
        groups = facts.get("waves") or []
        o["wave_groups"] = ",".join(str(g) for g in groups) if groups else "-"
        clear = facts.get("clear")
        if clear:
            o["clear_x"], o["clear_y"], o["clear_z"] = ("%.0f" % clear[0], "%.0f" % clear[1], "%.0f" % clear[2])
    if mid in ROW_WARN:
        o["warning"] = ascii_(ROW_WARN[mid])[:120]
    return o


# Record index -> region. Missing means Waves. A boss-era value drops that one row out of Waves.
SCROLL_REGION = {}
# Area names the map box id spells differently from the place. Every other area is the box id in title case.
SCROLL_AREA = {"EDOJO": "Edo Castle"}


def scroll_area(box):
    key = box.replace("HYAKKI_WB_START_", "")
    if key.endswith("_SMALL"):  # the map's box size, not part of the place name
        key = key[:-len("_SMALL")]
    return SCROLL_AREA.get(key, key.replace("_", " ").title())


def scroll_waves():
    return {int(r["index"]): r for r in rd("rematch-scroll-waves.csv")}


def erow(e):
    o = blank()
    x, y, z, yaw = e["warp"].split(",")
    i = int(e["index"])
    area = scroll_area(e["box_name"])
    w = scroll_waves()[i]
    region = SCROLL_REGION.get(i, "Waves")
    name = ("Picture Scroll - %s: %s waves, %s" % (area, w["waves"], w["toughest"])) if region == "Waves" else area
    o.update({"id": "E%02d" % i, "name": name, "mission_name": "Picture Scroll 0xCC96 record %d" % i,
              "region": region, "route": "E0", "hidden": "0", "story_mission": "0xCC96",
              "defeat_kind": "scroll", "defeat_id": "0xFFFF", "emaki_byte": e["emaki_byte"], "placement_id": e["gadget_10"],
              "stage_mission": e["start_point_0c"], "warp_x": x, "warp_y": y, "warp_z": z, "warp_yaw": yaw, "box_id": e["wave_box_14"],
              "hell_id": str(e["hell_id"]), "region_byte": e["region_byte"], "verified": "0", "emaki_index": str(i),
              "lesson": "0", "wake_step": str(WAKE_STEP.get(i, 0))})
    return o


# Open-world field spawns are parked (human #92460, 2026-11-12): X rows carry no spawn data until the human resumes that work.
OPEN_WORLD = False


def xrow(name, region, reason, spawn=None):
    o = blank()
    o.update({"id": "X" + re.sub(r"[^A-Za-z0-9]", "", ascii_(name))[:14], "name": ascii_(name),
              "region": region_of(region) if region else "-", "route": "X", "hidden": "0", "verified": "0", "boss": ascii_(name),
              "reason": ascii_(reason)[:240], "lesson": "0"})
    if spawn and OPEN_WORLD:
        m, pid, pos, chr_id, region_byte, rn = spawn
        o.update({"replay_mission_id": "0x%04X" % m, "placement_id": str(pid),
                  "warp_x": "%.0f" % pos[0], "warp_y": "%.0f" % pos[1], "warp_z": "%.0f" % pos[2], "warp_yaw": "%.1f" % pos[3],
                  "summon_chr": str(chr_id), "region_byte": region_byte, "fight_kind": "field spawn",
                  "boss_pos": "%.0f_%.0f_%.0f" % (pos[0], pos[1], pos[2]),
                  "mission_name": ascii_(rn)[:60]})
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
        ow = sum(1 for o in out if o["route"] == "X" and o.get("summon_chr") not in ("-", "", None))
        print("wrote %s: %d rows (%s); open world spawns %d" % (tp, len(out), ", ".join("%s %d" % kv for kv in sorted(c2.items())), ow))
    else:
        print("runtime TSV skipped: %s does not exist on this checkout (branch rematch3 has it)" % tp.parent)
    return 0


if __name__ == "__main__":
    sys.exit(main())
