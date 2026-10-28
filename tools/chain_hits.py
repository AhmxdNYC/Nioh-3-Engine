"""Per-move hit count including the follow-up pieces a move chains into (static, read-only).

  py -I tools/chain_hits.py CE_YORITOMO_00 [--depth 6] [--all]   -> TSV on stdout
  py -I tools/chain_hits.py --file <archive_01 action file> ...
  py -I tools/chain_hits.py --check                              -> Yoritomo self-check

Data (16-sub-table action file, see notes/chain-hits.md):
- Sub-table 0 holds one packed block per action (action order of sub-table 1): u16 n1 + n1 u16 hitbox-row indexes (sub-table 2),
  then u16 n2 + n2 u16 transition-row indexes (sub-table 3). Block start = header + align16(0x40 + 2 * n_actions).
- Sub-table 3 row (128 B): +0x00..+0x0A = 6 target action words (FFFF = none; on Yoritomo only +0x00 is ever used by a private
  row), +0x10 u16 = AI request number (FFFF = none), +0x42 s16 = blend frames (35CF28 reads it; < 1 falls back to action +0x11A).
  The rest of the condition bytes are NOT decoded.
- A row is "private" when exactly one action lists it. Shared rows (reactions, openers from idle) are listed by up to 130 actions.
- Action +0xE0 u16 = action played when this one finishes (proven; 2 = idle).

Rule (INFERRED, validated only on Yoritomo facts): a follow-up piece is a target that the game can only reach through a link from
another move: no shared row and no non-attack action points at it, and it is not a standalone opener. Links counted are
(a) the +0xE0 finish link into any attack/hit action (forced) and (b) private sub-table 3 links into follow-up pieces only. Links into standalone openers
(e.g. 0x4E4E -> 0x4E64) are the AI picking a different move and are not added.

Own hits = distinct hit ids (+0xA5) on started hitbox rows (tools/boss_action_record.py rule); groups = runs of equal
power/element/type rows (tools/chr_hit_groups.py rule). chain_min = own + the hits forced by the +0xE0 finish chain only;
chain_max = best path over all follow-up links (cycle-safe, depth limit). A private link is a choice, not a guarantee.
"""
import argparse
import csv
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXTRACT = ROOT / "data" / "extract" / "lfm" / "archive_01"
if not EXTRACT.is_dir():  # worktrees do not carry the extract; use the main checkout's copy
    EXTRACT = Path("C:/Users/Ahmad Hamza/nioh3-engine/data/extract/lfm/archive_01")
NAMES = ROOT / "data" / "boss-action-names.tsv"
REC, ROW, TR = 0x150, 192, 128
IDLE = 2
# human #41382: locomotion / walk motions are never a follow-up piece (Shingen 4E22 motion 11, Otoroshi 4E22/25/28 motion 31 carry a stray
# hit row; counting them made Shingen / Otoroshi gains too high). A link into one of these motions is dropped, so the chain stops there.
LOCOMOTION = frozenset((11, 31))


def hit_group(d, b):
    """RVA 0x2B5B98: hitbox-row byte +0xA5; 0xFF -> qword +0x00 bit 17 + 2 * bit 18 (same as AmHitGroup in auto_moves.h)."""
    g = d[b + 0xA5]
    if g == 0xFF:
        q = struct.unpack_from("<Q", d, b)[0]
        g = ((q >> 17) & 1) + 2 * ((q >> 18) & 1)
    return g


def hit_windows(d, o2, idx):
    """Hits a move lands = its hit windows, not its hitbox rows (pc #41677 accuracy run, notes/hit-label-accuracy-yoritomo.md).

    Same rule as AmWindows (mod/Nioh3Training/src/auto_moves.h) and tools/yoritomo_move_facts.py windows():
    rows with start frame (+0x08) FFFF never open; rows with power (+0x10) 0 and element (+0x12) 0 deal no damage and are dropped;
    1. rows with the same (start frame, hit group) are ONE hit (the game's own per-action hit key, RVA 0x78EADF..0x78EB74);
    2. keys of the same hit group that overlap or touch ([start, end] with end = +0x0A, or start + length +0x0C) merge into one swept
       swing (group 0 keys also need the same power / element). Two windows of group 0 at different frames are two hits even when
       their hit id is the same (Yoritomo 0x4E59: frames 88 and 104, measured 2 HP hits per rep).
    Returns (windows, started) where each window is dict(start, end, grp, powers=[distinct powers in row order], pow, elem) and started
    is the number of started rows (with or without damage)."""
    u16 = lambda o: struct.unpack_from("<H", d, o)[0]
    rows, started = [], 0
    for n, k in enumerate(idx):
        b = o2 + k * ROW
        st = u16(b + 8)
        if st == 0xFFFF:
            continue
        started += 1
        pw, el = u16(b + 0x10), u16(b + 0x12)
        if not pw and not el:
            continue
        ef = u16(b + 0xA)
        rows.append(dict(start=st, end=ef if ef != 0xFFFF else st + u16(b + 0xC), grp=hit_group(d, b), pow=pw, elem=el,
                         powers=[pw], n=n))
    keys = {}
    for r in sorted(rows, key=lambda r: (r["start"], r["n"])):
        k = keys.get((r["start"], r["grp"]))
        if k is None:
            keys[(r["start"], r["grp"])] = dict(r, powers=list(r["powers"]))
            continue
        k["end"] = max(k["end"], r["end"])
        k["pow"], k["elem"] = max(k["pow"], r["pow"]), max(k["elem"], r["elem"])
        k["powers"] += [x for x in r["powers"] if x not in k["powers"]]
    out = []
    for r in sorted(keys.values(), key=lambda r: (r["start"], r["n"])):
        for w in out:
            same = w["grp"] == r["grp"] and (r["grp"] or (w["pow"], w["elem"]) == (r["pow"], r["elem"]))
            if same and r["start"] <= w["end"] + 1:
                w["end"] = max(w["end"], r["end"])
                w["powers"] += [x for x in r["powers"] if x not in w["powers"]]
                break
        else:
            out.append(dict(r, powers=list(r["powers"])))
    return sorted(out, key=lambda w: (w["start"], w["n"])), started


def parse(d):
    u16 = lambda o: struct.unpack_from("<H", d, o)[0]
    if len(d) < 132 or struct.unpack_from("<I", d, 0)[0] != 16:
        raise ValueError("not an action table")
    st = [struct.unpack_from("<II", d, 4 + 8 * i) for i in range(16)]
    (h, hs), (a, asz), (o2, s2), (o3, s3) = st[0], st[1], st[2], st[3]
    if not asz or asz % REC or (s3 and s3 % TR):
        raise ValueError("row size does not divide")
    n = asz // REC
    n3 = s3 // TR
    sizes = struct.unpack_from("<%dH" % n, d, h + 0x40)
    cur = h + ((0x40 + 2 * n + 15) & ~15)
    acts = {}
    use = Counter()
    for i, size in enumerate(sizes):
        end = cur + size
        c1 = u16(cur)
        hit_idx = struct.unpack_from("<%dH" % c1, d, cur + 2)
        sec = cur + 2 + 2 * c1
        c2 = u16(sec) if sec + 2 <= end else 0
        tr_idx = struct.unpack_from("<%dH" % c2, d, sec + 2)
        if sec + 2 + 2 * c2 > end or any(k >= n3 for k in tr_idx):
            raise ValueError("packed block %d does not fit" % i)
        cur = end
        ro = a + i * REC
        aid = u16(ro + 0x0C)
        groups = []
        for k in hit_idx:
            key = struct.unpack_from("<3H", d, o2 + k * ROW + 0x10)
            if not groups or groups[-1] != key:
                groups.append(key)
        win, started = hit_windows(d, o2, hit_idx)
        acts[aid] = dict(
            row=i, motion=u16(ro + 0x20), nxt=u16(ro + 0xE0),
            attack=u16(ro + 0x13C) != 0xFFFF or u16(ro + 0x142) != 0xFFFF,
            hits=len(win), groups=len(groups), tr=tr_idx, catch=bool(started) and not win)
        use.update(tr_idx)
    if cur != h + hs:
        raise ValueError("packed blocks do not cover sub-table 0")
    rows = [struct.unpack_from("<6H", d, o3 + k * TR) + (u16(o3 + k * TR + 0x10),) for k in range(n3)]
    return acts, rows, use


def build_links(acts, rows, use, exclude_motions=LOCOMOTION):
    """Return (links, followup set). links[src] = list of (dst, kind, ai_request). Targets whose motion is in exclude_motions get no link."""
    entered_freely = set()  # reachable by a shared row or from a non-attack action
    private = defaultdict(list)
    for aid, a in acts.items():
        for k in a["tr"]:
            for t in rows[k][:6]:
                if t == 0xFFFF:
                    continue
                if use[k] > 1 or not a["attack"]:
                    entered_freely.add(t)
                else:
                    private[aid].append((t, rows[k][6]))
    hitter = lambda t: (acts[t]["attack"] or acts[t]["hits"] > 0) and acts[t]["motion"] not in exclude_motions
    follow = {t for t in acts if t not in entered_freely and hitter(t)}
    links = defaultdict(list)
    for aid, a in acts.items():
        n = a["nxt"]
        if n != aid and n != IDLE and n in acts and hitter(n):  # forced: counted even into a standalone move
            links[aid].append((n, "E0", None))
        for t, req in private.get(aid, ()):
            # human #41382: a combo link (an AI choice) only into a piece with an attack block. Otoroshi 4E36/4E4E/4EF2 -> 18C7 (no attack
            # block, 5 alternate damage rows at one frame = grab / throw damage, INFERRED) would make the known 1-hit zero-power 4E4E 2.
            if t in follow and t != aid and acts[t]["attack"] and (t, "S3", req) not in links[aid]:
                links[aid].append((t, "S3", req))
    return links, follow


def plays_next(acts, rows, use, exclude_motions=LOCOMOTION):
    """{action: the follow-up the game plays after it in a drill, or None}. Rule (pc #41677 accuracy run, INFERRED from the ActLog):
    - a private sub-table 3 row with no AI request (+0x10 = FFFF) is a combo continuation; the first such row in the action's own
      row order wins (Yoritomo 4E60 lists 4E62 then 4E61 and plays 4E62; 4EA0 / 4EA1 play 4EA2; 4E5C plays 4E60; 4E59 plays 4E5E;
      4E52 / 4E53 play 4E54; 4EB8 plays 4EB9 - every one seen in the ActLog of the drill run);
    - it preempts the +0xE0 finish link (4E60 E0 = 4E61 and 4EA0 E0 = 4EA1 never played);
    - with no such row, the +0xE0 finish link into a hitter plays (forced when the move ends: 4E5C -> 4E60);
    - a row with an AI request (+0x10 set) is the AI choosing another move (4E4E -> 4E52 r13, 4E8E -> 4E92 r13, 4E95 -> 4E99 r8,
      4E64 / 4E6C -> 4EB7 r6, 4E81 -> 4EA9 r53: none played in the drill run). It is not counted.
    A continuation into idle, a locomotion motion or any action without an attack block (a recovery / stance, or grab / throw damage
    such as Otoroshi 18C7) ends the chain: it is still the row that plays, so the +0xE0 link is not tried after it."""
    hitter = lambda t: (acts[t]["attack"] or acts[t]["hits"] > 0) and acts[t]["motion"] not in exclude_motions
    out = {}
    for aid, a in acts.items():
        combo = [t for k in a["tr"] if use[k] == 1 and rows[k][6] == 0xFFFF
                 for t in rows[k][:6] if t != 0xFFFF and t != aid]
        nxt = None
        if combo:
            t = combo[0]
            if t in acts and t != IDLE and acts[t]["attack"] and acts[t]["motion"] not in exclude_motions:
                nxt = t
        elif a["nxt"] != aid and a["nxt"] != IDLE and a["nxt"] in acts and hitter(a["nxt"]):
            nxt = a["nxt"]
        out[aid] = nxt
    return out


def play_path(acts, nxt, start, depth=6):
    """Hits the move lands in a drill: own windows plus each continuation that plays (plays_next), cycle-safe, depth limited.
    A move whose every started row deals no damage is a catch: what follows plays only when the catch lands, so the chain stops."""
    path = [start]
    node = start
    while len(path) <= depth and not acts[node]["catch"]:
        n = nxt.get(node)
        if n is None or n in path:
            break
        path.append(n)
        node = n
    return sum(acts[x]["hits"] for x in path), path


def chain(acts, links, start, depth, forced_only):
    """Best total hits from start (own + follow-ups). Returns (total, path)."""
    def walk(node, seen, left):
        own = acts[node]["hits"]
        best = (own, [node])
        # human #41382: a move whose every hit has power 0 is a catch check (Otoroshi 4E4E, the known 1-hit zero-power case); what it
        # leads to (4E6D throw, 18C7) plays only when the catch lands, so the chain stops there (INFERRED).
        if left == 0 or acts[node]["catch"]:
            return best
        for dst, kind, _req in links.get(node, ()):
            if forced_only and kind != "E0":
                continue
            if dst in seen:
                continue
            sub = walk(dst, seen | {dst}, left - 1)
            if own + sub[0] > best[0]:
                best = (own + sub[0], [node] + sub[1])
        return best
    return walk(start, {start}, depth)


def chain_paths(data, depth=6, exclude_motions=LOCOMOTION):
    """{action id: path the move plays (list of action ids, starting with the action)}: plays_next / play_path."""
    acts, rows, use = parse(data)
    nxt = plays_next(acts, rows, use, exclude_motions)
    return {aid: play_path(acts, nxt, aid, depth)[1] for aid in acts}


def find_file(name):
    with NAMES.open(encoding="utf-8", newline="") as f:
        keys = [r["file"].upper() for r in csv.DictReader(f, delimiter="\t") if r["character"] == name]
    paths = [EXTRACT / k[:2] / k[2:] for k in keys]
    paths = [p for p in paths if p.is_file()]
    if not paths:
        raise SystemExit("no action file for %s" % name)
    return max(paths, key=lambda p: p.stat().st_size)  # same rule as boss_action_record.py


def table(path, depth, show_all):
    acts, rows, use = parse(path.read_bytes())
    links, follow = build_links(acts, rows, use)
    nxt = plays_next(acts, rows, use)
    out = []
    for aid in sorted(acts):
        a = acts[aid]
        if not a["attack"] and not a["hits"]:
            continue
        if not show_all and aid in follow:
            continue  # pieces are counted inside their parent move
        lo, play = play_path(acts, nxt, aid, depth)
        hi, path_ = chain(acts, links, aid, depth, False)
        ln = " ".join("%X>%X%s" % (aid, d, "" if k == "E0" or q == 0xFFFF else "(r%d)" % q)
                      if k == "S3" else "%X>%X(E0)" % (aid, d) for d, k, q in links.get(aid, ()))
        out.append((aid, a["motion"], a["hits"], a["groups"], lo, "-".join("%X" % x for x in play), hi,
                    "-".join("%X" % x for x in path_), int(aid in follow), ln))
    return out, acts, links, follow


CHECK = {  # Yoritomo: (own hit windows, hits the move plays incl. its combo continuation) - pc #41677 drill measurements
    0x4E6E: (2, 2), 0x4E4E: (1, 1), 0x4E55: (1, 1), 0x4E59: (2, 2), 0x4E5C: (1, 3), 0x4E7C: (0, 0), 0x4E8E: (1, 1),
}
LINKS = ((0x4E4E, 0x4E52), (0x4E60, 0x4E62), (0x4E53, 0x4E54), (0x4E5C, 0x4E60))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("boss", nargs="?", default="CE_YORITOMO_00")
    ap.add_argument("--file", type=Path)
    ap.add_argument("--depth", type=int, default=6)
    ap.add_argument("--all", action="store_true", help="also list follow-up pieces as rows")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    path = args.file or find_file(args.boss)
    out, acts, links, follow = table(path, args.depth, args.all or args.check)
    if args.check:
        by = {r[0]: r for r in out}
        bad = [p for p in LINKS if p[1] not in [d for d, _k, _q in links.get(p[0], ())]]
        for aid, (own, hi) in CHECK.items():
            if by[aid][2] != own or by[aid][4] != hi:
                bad.append((aid, by[aid]))
        print("self-check %s" % ("ok" if not bad else "FAILED %s" % bad))
        return 1 if bad else 0
    w = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
    w.writerow(["action", "motion", "own_hits", "own_groups", "plays_hits", "play_path", "ai_max", "ai_best_path", "is_piece", "links"])
    for r in out:
        w.writerow(["0x%X" % r[0]] + list(r[1:]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
