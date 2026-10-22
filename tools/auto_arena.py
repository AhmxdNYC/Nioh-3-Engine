#!/usr/bin/env python3
"""auto_arena.py - builds mod/Nioh3Training/Nioh3Training_arenas.tsv for EVERY boss straight from the game's own data
(user #23436/#23447/#23451/#23479, notes/auto-arena.md). No per-boss list, no exe address: only the game folder's LFM data.

    py -I tools/auto_arena.py [--game DIR] [--out TSV] [--report CSV] [--check]

--game defaults to $NIOH3_DIR, then the usual Steam folders. --check: exit 1 when the TSV differs from a rebuild (or, without a game
folder, only validates the committed TSV). --report writes the per-boss coverage table (notes/auto-arena-bosses.csv by default).

Sources (all located by structure, the known LFM path is only the fast path; tools/n3lfm.py reads the archives):
  id table        u32 0, u32 n, n x {u32 file id, i32 rel -> 'XX/<62 hex>'}: file id -> LFM path
  mission table   0x198-byte records: +0x40 flags, +0x50 script id, +0x54 enemy table id, +0x5C box table id, +0x6C start
                  point table id, +0x70 name text id, +0x158 u16 mission id, +0x15A kind (2 = open-field region), +0x15C world byte
  enemy table     0x138-byte records: +0 placement id, +4 character id (the Load::Character id the mod spawns), +0x40 x y z yaw,
                  +0x9C flags (0x04 = boss record, rematch_catalog --index; record name *BOSS* also counts), +0x134 name
  box table       0xA0-byte PlacementBox records: +0 id, +0x10 centre, +0x20 yaw, +0x28 size, +0x9C name (BOSS_AREA..., trigger boxes)
  start points    0xA4-byte records: +0 id, +8 flags, +0xC kind, +0x40 x y z yaw
  mission script  the boxes it tests with Player::IsEnter* / Wait_Invade* / Loop_* (= the arena trigger), see rematch_catalog.py
  chara params    0x398-byte records: +0x64 character id, +0xA0 name text id;  text = {u32 id, u32 n, UTF-16 n chars} files
Arena point / start point choice = rematch_catalog.pick_fight_start / pick_arena (same rules as the Boss Rematch catalogue).

Rows: one per startable mission with a boss record (its first boss = the Fight-now boss, id R<mission> listable / hidden duel,
D<mission> unlisted stage), then one 'arena'/'start' row per further boss character of a mission that has no row of its own (id
S<chr>; Fight now still uses the mission's first row), then kind 'summon' rows (mission 0, id S<chr>) = the per-boss roster of every
boss character without a mission row of its own (field bosses keep their field position + world byte in the columns, unused by
the runtime). Column 16 (CrossWorldCellSkip whitelist, measured in game) is carried over from the existing TSV by row id.
"""
import argparse
import collections
import csv
import math
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from n3lfm import Lfm  # noqa: E402
import rematch_catalog as rc  # noqa: E402

TSV = os.path.join(ROOT, "mod", "Nioh3Training", "Nioh3Training_arenas.tsv")
REPORT = os.path.join(ROOT, "notes", "auto-arena-bosses.csv")
MAGIC = b"\x00\x22\x04\x20"
# fast paths (path hashes of the shipped files); every one is re-validated and re-discovered by structure when it fails
KNOWN = {"idtab": "/AE/50B70535D0EDD19523951019C533295915AE7D4F65FE37E351E09D3858A8C5",
         "missions": "/52/BD29D45533CFACBFA2D6EC80A3B9F40E07CBF9183727358B0B3C3ABE3E4FC2",
         "chara": "/C4/E1CBAE0841F7ECE87BD581BA0998F242FE69C4449DB57A473D072E0EB85FAC"}
MREC, EREC, BREC, SREC, CREC = 0x198, 0x138, 0xA0, 0xA4, 0x398
HEADER = ("# Nioh3Training arena table (tools/auto_arena.py, generated from the game data; do not edit). mission, start point, world, "
          "boss char id, boss placement id, player x y z yaw, boss x y z, kind (arena = inside the trigger; start = the Fight-now start "
          "point; summon = roster only), id, name[, CrossWorldCellSkip cells]")
STEAM = ["~/.local/share/Steam/steamapps/common/Nioh3", "~/.steam/steam/steamapps/common/Nioh3",
         "C:/Program Files (x86)/Steam/steamapps/common/Nioh3", "C:/Program Files/Steam/steamapps/common/Nioh3",
         "D:/SteamLibrary/steamapps/common/Nioh3"]


def game_dir(arg):
    for d in [arg, os.environ.get("NIOH3_DIR")] + STEAM:
        if d and os.path.isdir(os.path.join(os.path.expanduser(d), "archive")):
            return os.path.expanduser(d)
    return None


# ---------------------------------------------------------------- table discovery (structure, not addresses / names)
def _tab(d, stride):
    if not d or len(d) < 8 or d[:4] != MAGIC:
        return 0
    n = struct.unpack_from("<I", d, 4)[0]
    return n if n and 8 + n * stride <= len(d) else 0


def parse_idtab(d):
    if not d or len(d) < 16 or struct.unpack_from("<I", d, 0)[0] != 0:
        return None
    n = struct.unpack_from("<I", d, 4)[0]
    if not 100 <= n <= 1000000 or 8 + n * 8 > len(d):
        return None
    E = {}
    try:
        for i in range(n):
            e = 8 + i * 8
            k, o = struct.unpack_from("<Ii", d, e)
            s = d[e + o:d.index(b"\0", e + o)].decode("ascii")
            E[k] = "/" + s.upper()
    except (ValueError, UnicodeDecodeError, struct.error):
        return None
    good = sum(1 for v in list(E.values())[:200] if re.fullmatch(r"/[0-9A-F]{2}/[0-9A-F]{62}", v))
    return E if good >= min(150, len(E)) else None


def heads(L, want):
    """yield (path, first bytes) of every bundled entry (cheap: only the first chunk is inflated)."""
    for p, e in L.index.items():
        if len(e) == 1:
            continue
        ap, eo, us, cs, cp = e
        if us < 16:
            continue
        a = L._blob(ap)
        try:
            h = rc_inflate_head(a, eo, cs) if cp else bytes(a[eo:eo + 16])
        except Exception:
            continue
        if want(h, us):
            yield p


def rc_inflate_head(a, eo, cs):
    import zlib
    return zlib.decompressobj().decompress(a[eo + 4:eo + min(cs, 4096)], 64)


def find_tables(L, log):
    t = {}
    E = parse_idtab(L.get(KNOWN["idtab"]))
    if E is None:
        for p in heads(L, lambda h, us: h[:4] == b"\0\0\0\0" and us > 1000):
            E = parse_idtab(L.get(p))
            if E:
                log("id table found by structure at %s (known path failed)" % p)
                break
    if E is None:
        raise SystemExit("no LFM id table found in the game data")
    t["E"] = E

    def missions_ok(d):
        n = _tab(d, MREC)
        if not 20 <= n <= 4096:
            return False
        ok = 0
        for i in range(n):
            r = d[8 + i * MREC:8 + (i + 1) * MREC]
            sid, eid = struct.unpack_from("<II", r, 0x50)
            ok += sid in E and eid in E
        return ok >= n // 3
    d = L.get(KNOWN["missions"])
    if not missions_ok(d):
        d = None
        for p in heads(L, lambda h, us: h[:4] == MAGIC and (us - 8) % MREC < 64):
            c = L.get(p)
            if missions_ok(c):
                d = c
                log("mission table found by structure at %s" % p)
                break
    if d is None:
        raise SystemExit("no mission table found in the game data")
    t["missions"] = d
    return t


def chara_names(L, chrs, log):
    """character id -> English name, via the chara param table (+0x64 id, +0xA0 name text id) and the English text file."""
    def chara_ok(d):
        n = _tab(d, CREC)
        if not n or 8 + n * CREC > len(d):
            return 0
        ks = {struct.unpack_from("<I", d, 8 + i * CREC + 0x64)[0] for i in range(n)}
        return len(ks & chrs)
    d = L.get(KNOWN["chara"])
    if chara_ok(d) < len(chrs) // 2:
        best = (0, None)
        for p in heads(L, lambda h, us: h[:4] == MAGIC and us > 100000):
            c = L.get(p)
            k = chara_ok(c)
            if k > best[0]:
                best = (k, c)
        d = best[1]
        log("chara param table found by structure (%d boss characters matched)" % best[0])
    tid = {}
    if d:
        for i in range(_tab(d, CREC)):
            r = d[8 + i * CREC:8 + (i + 1) * CREC]
            k, nid = struct.unpack_from("<I", r, 0x64)[0], struct.unpack_from("<I", r, 0xA0)[0]
            if k in chrs and nid:
                tid.setdefault(k, nid)
    return tid


SW = set("the and to of you your a in is for with that this it be on are as at from will can his her he she by or an not have has "
         "was their into".split())


def parse_text(b):
    out, p, n_b = {}, 0, len(b)
    while p + 8 <= n_b:
        i, n = struct.unpack_from("<II", b, p)
        if n == 0 or n > 100000 or p + 8 + 2 * n > n_b:
            break
        s = b[p + 8:p + 8 + 2 * n].decode("utf-16le", "replace")
        if s[-1:] != "\0":
            break
        out[i] = s[:-1]
        p += 8 + 2 * n
    if not out or any(b[p:]):
        return None
    return out


MARKUP = re.compile(r"\^[0-9A-Fa-f]{2}~[A-Z_]+~")


def english_text(L, ids):
    """{text id: English string} for the wanted ids, with no language table or file path: every language file of a text group holds
    the same ids. English prose files are the ones with a high English stop-word share; a candidate value (ASCII-only, markup
    stripped) wins when it also occurs in that English prose (Takeda Soldier, not Soldado de Takeda), then by how many language
    files agree on it (Yamagata Masakage x6 vs Masakage Yamagata x1), then by its own file's stop-word share (mission titles)."""
    def looks(h, us):
        if len(h) < 12:
            return False
        i, n = struct.unpack_from("<II", h, 0)
        return 0 < n < 4096 and us >= 8 + 2 * n and h[9] == 0
    cand = collections.defaultdict(list)
    prose = []
    for p in heads(L, looks):
        t = parse_text(L.get(p))
        if not t:
            continue
        allt = " ".join(t.values())
        words = re.findall(r"[a-z']+", allt.lower())
        score = sum(w in SW for w in words) / (len(words) or 1)
        asc = sum(ord(c) < 0x180 for c in allt) / (len(allt) or 1)
        if score >= 0.2 and asc >= 0.97 and len(words) >= 200:
            prose.append(MARKUP.sub("", allt))
        for i in ids & set(t):
            v = MARKUP.sub("", t[i]).strip()
            if v and not v.startswith("UNUSED") and all(ord(c) < 0x180 for c in v):
                cand[i].append((v, score))
    corpus = "\n".join(prose)
    out = {}
    for i, cs in cand.items():
        cnt = collections.Counter(v for v, _ in cs)
        inp = {v: v in corpus for v in cnt}
        out[i] = max(cs, key=lambda c: (inp[c[0]], cnt[c[0]], c[1], c[0]))[0]
    return out


# ---------------------------------------------------------------- per mission data
def f0(v):
    return float("%.0f" % v)


def f1(v):
    return float("%.1f" % v)


def enemy_records(et):
    n = _tab(et, EREC)
    out = []
    if not n:
        return out
    pool = 8 + n * EREC
    for j in range(n):
        e = et[8 + j * EREC:8 + (j + 1) * EREC]
        no = struct.unpack_from("<I", e, 0x134)[0]
        nm = et[pool + no:et.index(b"\0", pool + no)].decode("latin1") if pool + no < len(et) else ""
        x, y, z, yaw = struct.unpack_from("<4f", e, 0x40)
        out.append({"pid": struct.unpack_from("<I", e, 0)[0], "chr": struct.unpack_from("<I", e, 4)[0], "fl": e[0x9C],
                    "pos": (f0(x), f0(y), f0(z)), "yawf": f1(yaw), "name": nm})
    return out


def mission_data(L, t, log):
    E, md = t["E"], t["missions"]

    def tab(fid):
        p = E.get(fid)
        return L.get(p) if p else None
    out = []
    for i in range(_tab(md, MREC)):
        r = md[8 + i * MREC:8 + (i + 1) * MREC]
        m = {"idx": i, "mid": struct.unpack_from("<H", r, 0x158)[0], "kind": r[0x15A], "world": r[0x15C],
             "flags": struct.unpack_from("<I", r, 0x40)[0], "name_id": struct.unpack_from("<I", r, 0x70)[0],
             "script": E.get(struct.unpack_from("<I", r, 0x50)[0]), "enemies": [], "start": [], "box": [], "trig": [], "gate": []}
        ids = struct.unpack_from("<8I", r, 0x50)
        m["shipped"] = bool(m["script"]) and all((not x) or x in E for x in ids[1:7])
        for et in (tab(ids[1]), tab(ids[2])):   # +0x54 enemy table, +0x58 second enemy set (36 boss-like records in 1.0)
            m["enemies"] += enemy_records(et)
        st = tab(ids[7])
        for j in range(_tab(st, SREC)):
            s = st[8 + j * SREC:8 + (j + 1) * SREC]
            x, y, z, yaw = struct.unpack_from("<4f", s, 0x40)
            m["start"].append({"id": struct.unpack_from("<I", s, 0)[0], "flags": "0x%X" % struct.unpack_from("<I", s, 8)[0],
                               "sub": str(struct.unpack_from("<I", s, 0xC)[0]), "pos": (f0(x), f0(y), f0(z)), "yawf": f1(yaw), "name": "#%d" % j})
        sc = L.get(m["script"]) if m["script"] else None
        trig = rc._script_box_calls(sc) if sc else {}
        for fn, g, kind, cr in trig.pop(None, []):
            m["gate"].append({"id": g, "flags": kind, "sub": " ".join(str(c) for c in cr[:12]), "name": rc.ascii_(fn)[:60]})
        bt = tab(ids[3])
        n = _tab(bt, BREC)
        if n:
            pool = 8 + n * BREC
            for j in range(n):
                b = bt[8 + j * BREC:8 + (j + 1) * BREC]
                no = struct.unpack_from("<I", b, 0x9C)[0]
                if pool + no >= len(bt):
                    break
                nm = bt[pool + no:bt.index(b"\0", pool + no)].decode("latin1")
                u = nm.upper()
                bid = struct.unpack_from("<I", b, 0)[0]
                cx, cy, cz, _w, yaw = struct.unpack_from("<5f", b, 0x10)
                ex, ey, ez = struct.unpack_from("<3f", b, 0x28)
                v = {"id": bid, "pos": (f0(cx), f0(cy), f0(cz)), "yawf": f1(yaw), "ext": (f0(ex), f0(ey), f0(ez)), "name": rc.ascii_(nm)[:60]}
                if bid in trig:
                    m["trig"].append(dict(v, sub=rc.ascii_(" ".join(sorted(trig[bid])))[:80]))
                if "BOSS" in u and any(k in u for k in ("AREA", "BOX", "BARRIER")) and not any(k in u for k in ("BGM", "JUKE", "ITEM")):
                    m["box"].append(v)
        out.append(m)
    return out


# ---------------------------------------------------------------- rows
def is_boss(e):
    return bool(e["fl"] & 0x04) or "BOSS" in e["name"].upper()


def old_table(path):
    """the committed TSV: {id: cols} (names, CrossWorldCellSkip cells and the primary boss of a mission are carried over by id)."""
    out = collections.OrderedDict()
    if not os.path.exists(path):
        return out
    for ln in open(path, encoding="utf-8"):
        if ln.startswith("#") or not ln.strip():
            continue
        c = ln.rstrip("\r\n").split("\t")
        if len(c) >= 15:
            out[(c[13], c[12] == "summon")] = c
    return out


def field_point(b):
    """player point for a boss with no arena trigger (open-field placement): 8 m in front of the boss along its own facing (yaw 0 = +z),
    1 m up (the player drops onto the floor), facing the boss."""
    a = math.radians(b["yawf"])
    x, z = b["pos"][0] + 800 * math.sin(a), b["pos"][2] + 800 * math.cos(a)
    return x, b["pos"][1] + 100, z, math.degrees(math.atan2(b["pos"][0] - x, b["pos"][2] - z))


def nearest_start(m, pos):
    ok = [s for s in m["start"] if not int(s["flags"], 16) & 0x200] or m["start"]
    return min(ok, key=lambda s: (rc._dist_m(s["pos"], pos), s["id"]))["id"] if ok else 0


def build_rows(ms, old, log):
    fi = {}
    for m in ms:
        fi[m["mid"]] = {"start": m["start"], "box": m["box"], "trig": m["trig"], "gate": m["gate"],
                        "boss": {e["pid"]: e for e in m["enemies"]}}
    oldp = {}   # mission -> (id, placement, chr) of the committed primary row
    for (rid, summ), c in old.items():
        if c[0] not in ("0", "") and not rid.startswith("S"):
            oldp[int(c[0], 16)] = (rid, int(c[4] or 0), int(c[3] or 0))
    rows, have, src = [], set(), {}

    def emit(m, e, rid, how):
        mid = m["mid"]
        f = rc.pick_fight_start(fi, mid, e["pid"])
        ar = rc.pick_arena(fi, mid, e["pid"], f) if f and f["kind"] != "mission start" else None
        if m["kind"] == 2 or not f:          # open-field region (or no usable start point): stand in front of the boss
            x, y, z, yaw = field_point(e)
            kind, start, conf = "field", nearest_start(m, e["pos"]), "field placement"
        elif ar:
            x, y, z, yaw, kind, start, conf = ar["x"], ar["y"], ar["z"], ar["yaw"], "arena", f["start"], "%s (%s)" % (ar["conf"], ar["src"])
        else:
            (x, y, z), yaw, kind, start, conf = f["pos"], f["yaw"], "start", f["start"], "%s %s %.0fm" % (f["conf"], f["kind"], f["dist"])
        rows.append({"mission": mid, "start": start, "world": m["world"], "chr": e["chr"], "pid": e["pid"], "p": (x, y, z, yaw),
                     "b": e["pos"], "kind": kind, "id": rid, "how": how, "conf": conf, "mname": m["name_id"]})
        have.add(e["chr"])

    usable = [m for m in ms if m["shipped"] and m["world"] and m["enemies"]]
    # 1. one primary row per startable (non-region) mission that has a boss record
    for m in usable:
        if m["kind"] == 2:
            continue
        bs = [e for e in m["enemies"] if is_boss(e)]
        op = oldp.get(m["mid"])
        if op:   # carried: the committed row's boss (it may be a non-boss record, e.g. the dojo duels)
            e = next((e for e in m["enemies"] if e["pid"] == op[1]), None) or next((e for e in m["enemies"] if e["chr"] == op[2]), None)
            if e:
                emit(m, e, op[0], "mission (boss kept from the committed row)")
                continue
        if not bs:
            continue
        e = next((e for e in bs if e["fl"] & 0x04 and "BOSS" in e["name"].upper()), None) or next((e for e in bs if e["fl"] & 0x04), bs[0])
        listed = not m["flags"] & 0x41    # +0x40 bit0 / 0x40 = not in the replay list (0x4000 = hidden duel, listed by Boss Rematch H)
        emit(m, e, "%s%04X" % ("R" if listed else "D", m["mid"]), "mission (first boss record)")
    # 2. every further boss character of a startable mission, then of an open-field region (one row per character, id S<chr>)
    for want_kind in ("mission", "region"):
        for m in usable:
            if (m["kind"] == 2) != (want_kind == "region"):
                continue
            for e in m["enemies"]:
                if is_boss(e) and e["fl"] & 0x04 and e["chr"] not in have:
                    emit(m, e, "S%d" % e["chr"], "%s boss record" % want_kind)
    # 3. roster characters of the committed table with no boss record: any placement of that character (region first)
    for (rid, summ), c in old.items():
        if not summ or c[0] not in ("0", ""):
            continue
        chr_ = int(c[3])
        if chr_ in have:
            continue
        cand = [(m["kind"] != 2, m["mid"], m, e) for m in usable for e in m["enemies"] if e["chr"] == chr_]
        if cand:
            _, _, m, e = min(cand, key=lambda t: t[:2])
            emit(m, e, "S%d" % chr_, "regular placement (no boss record)")
    return rows


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--game")
    ap.add_argument("--out", default=TSV)
    ap.add_argument("--report", default=REPORT)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args(argv)
    log = (lambda s: None) if a.quiet else (lambda s: print(s, file=sys.stderr))
    g = game_dir(a.game)
    if not g:
        if a.check:
            n = sum(1 for l in open(a.out, encoding="utf-8") if l.strip() and not l.startswith("#"))
            print("no game folder: committed table has %d rows (not rebuilt)" % n)
            return 0
        raise SystemExit("game folder not found: pass --game or set NIOH3_DIR")
    L = Lfm(g)
    t = find_tables(L, log)
    ms = mission_data(L, t, log)
    old = old_table(a.out)
    rows = build_rows(ms, old, log)
    chrs = {r["chr"] for r in rows} | {int(c[3]) for (rid, s), c in old.items() if s and c[3].isdigit()}
    tid = chara_names(L, chrs, log)
    text = english_text(L, set(tid.values()) | {r["mname"] for r in rows if r["mname"]})
    cname = {c: rc.ascii_(text[i]) for c, i in tid.items() if i in text}
    oldn = {rid: c[14] for (rid, s), c in old.items()}
    oldcells = {rid: c[15] for (rid, s), c in old.items() if len(c) > 15 and c[15]}
    out = [HEADER, "# game data fingerprint %s (archive sizes + times): rerun tools/auto_arena.py when it changes" % L.fingerprint()]
    rep = []
    arena_chr = set()
    for r in rows:
        nm = oldn.get(r["id"]) or cname.get(r["chr"]) or "chr %d" % r["chr"]
        if r["id"] not in oldn and r["mname"] in text and r["kind"] != "field" and not r["id"].startswith("S"):
            nm = "%s (%s)" % (nm, rc.ascii_(text[r["mname"]]))
        c = ["0x%04X" % r["mission"], str(r["start"]), "0x%02X" % r["world"], str(r["chr"]), str(r["pid"]),
             "%.0f" % r["p"][0], "%.0f" % r["p"][1], "%.0f" % r["p"][2], "%.1f" % r["p"][3], "%.0f" % r["b"][0], "%.0f" % r["b"][1],
             "%.0f" % r["b"][2], r["kind"], r["id"], nm.replace("\t", " ")[:63]]
        if r["id"] in oldcells:
            c.append(oldcells[r["id"]])
        out.append("\t".join(c))
        arena_chr.add(r["chr"])
        rep.append({"id": r["id"], "name": nm, "chr": r["chr"], "kind": r["kind"], "mission": "0x%04X" % r["mission"],
                    "world": "0x%02X" % r["world"], "placement": r["pid"], "source": r["how"], "confidence": r["conf"]})
    # roster: every boss character without a mission (R/D) row of its own -> kind summon (mission 0), its arena/field row (if any) has id S<chr>
    prim = {r["chr"] for r in rows if not r["id"].startswith("S")}
    out.append("# roster (kind summon, mission 0, id S<chr>): every boss character without a mission row of its own; 'Fight in arena' uses its "
               "S<chr> arena/field row when there is one (Nioh3Training_FightNowChr)")
    roster = collections.OrderedDict()
    for (rid, s), c in old.items():     # committed summon rows first (catalogue rows with mission != 0 too), in their order
        if s and c[3].isdigit():
            roster[(c[0], int(c[3]))] = (c[13], c[14])
    for r in rows:
        if r["chr"] not in prim:
            roster.setdefault(("0", r["chr"]), ("S%d" % r["chr"], oldn.get("S%d" % r["chr"]) or cname.get(r["chr"]) or "chr %d" % r["chr"]))
    for (mis, chr_), (rid, nm) in roster.items():
        if mis not in ("0", "") and int(mis, 16) in {r["mission"] for r in rows}:
            continue     # this catalogue mission has an arena row now
        out.append("\t".join([mis if mis else "0", "0", "0x00", str(chr_), "0", "0", "0", "0", "0.0", "0", "0", "0", "summon", rid, nm]))
        if chr_ not in arena_chr:
            rep.append({"id": rid, "name": nm, "chr": chr_, "kind": "summon", "mission": mis, "world": "-", "placement": "-",
                        "source": "no placement of this character in any shipped mission/region table", "confidence": "-"})
    text_out = "\n".join(out) + "\n"
    if a.check:
        cur = open(a.out, encoding="utf-8").read() if os.path.exists(a.out) else ""
        same = [l for l in cur.splitlines() if not l.startswith("#")] == [l for l in text_out.splitlines() if not l.startswith("#")]
        print("table %s" % ("up to date" if same else "DIFFERS from a rebuild"))
        return 0 if same else 1
    open(a.out, "w", encoding="utf-8", newline="\n").write(text_out)
    if a.report:
        with open(a.report, "w", encoding="utf-8", newline="") as o:
            w = csv.DictWriter(o, fieldnames=list(rep[0].keys()), lineterminator="\n")
            w.writeheader()
            w.writerows(rep)
    c = collections.Counter(r["kind"] for r in rows)
    print("%d arena rows (%s) + %d roster rows -> %s" % (len(rows), ", ".join("%s %d" % kv for kv in sorted(c.items())),
                                                        sum(1 for l in out if "\tsummon\t" in l), a.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
