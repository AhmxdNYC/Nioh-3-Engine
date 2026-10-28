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
        hid_order, groups = [], []
        for k in hit_idx:
            b = o2 + k * ROW
            key = struct.unpack_from("<3H", d, b + 0x10)
            if groups and groups[-1] == key:
                pass
            else:
                groups.append(key)
            if u16(b + 8) == 0xFFFF:
                continue
            hid = d[b + 0xA5]
            if hid == 0xFF:
                hid = 2 if struct.unpack_from("<Q", d, b)[0] & (1 << 18) else 0
            if hid not in hid_order:
                hid_order.append(hid)
        acts[aid] = dict(
            row=i, motion=u16(ro + 0x20), nxt=u16(ro + 0xE0),
            attack=u16(ro + 0x13C) != 0xFFFF or u16(ro + 0x142) != 0xFFFF,
            hits=len(hid_order), groups=len(groups), tr=tr_idx)
        use.update(tr_idx)
    if cur != h + hs:
        raise ValueError("packed blocks do not cover sub-table 0")
    rows = [struct.unpack_from("<6H", d, o3 + k * TR) + (u16(o3 + k * TR + 0x10),) for k in range(n3)]
    return acts, rows, use


def build_links(acts, rows, use):
    """Return (links, followup set). links[src] = list of (dst, kind, ai_request)."""
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
    hitter = lambda t: acts[t]["attack"] or acts[t]["hits"] > 0
    follow = {t for t in acts if t not in entered_freely and hitter(t)}
    links = defaultdict(list)
    for aid, a in acts.items():
        n = a["nxt"]
        if n != aid and n != IDLE and n in acts and hitter(n):  # forced: counted even into a standalone move
            links[aid].append((n, "E0", None))
        for t, req in private.get(aid, ()):
            if t in follow and t != aid and (t, "S3", req) not in links[aid]:
                links[aid].append((t, "S3", req))
    return links, follow


def chain(acts, links, start, depth, forced_only):
    """Best total hits from start (own + follow-ups). Returns (total, path)."""
    def walk(node, seen, left):
        own = acts[node]["hits"]
        best = (own, [node])
        if left == 0:
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
    out = []
    for aid in sorted(acts):
        a = acts[aid]
        if not a["attack"] and not a["hits"]:
            continue
        if not show_all and aid in follow:
            continue  # pieces are counted inside their parent move
        lo, _ = chain(acts, links, aid, depth, True)
        hi, path_ = chain(acts, links, aid, depth, False)
        ln = " ".join("%X>%X%s" % (aid, d, "" if k == "E0" or q == 0xFFFF else "(r%d)" % q)
                      if k == "S3" else "%X>%X(E0)" % (aid, d) for d, k, q in links.get(aid, ()))
        out.append((aid, a["motion"], a["hits"], a["groups"], lo, hi, "-".join("%X" % x for x in path_),
                    int(aid in follow), ln))
    return out, acts, links, follow


CHECK = {  # Yoritomo: (own hits, chain_max)
    0x4E6E: (2, 2), 0x4E4E: (1, None), 0x4E55: (1, 2),
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
            if by[aid][2] != own or (hi is not None and by[aid][5] != hi):
                bad.append((aid, by[aid]))
        if by[0x4E4E][5] < 2:
            bad.append(("4E4E chain_max < 2", by[0x4E4E]))
        print("self-check %s" % ("ok" if not bad else "FAILED %s" % bad))
        return 1 if bad else 0
    w = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
    w.writerow(["action", "motion", "own_hits", "own_groups", "chain_min", "chain_max", "best_path", "is_piece", "links"])
    for r in out:
        w.writerow(["0x%X" % r[0]] + list(r[1:]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
