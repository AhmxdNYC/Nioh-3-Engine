"""Write data/boss-action-record.tsv from extracted archive_01 action files.

One row per action on every enemy, duelist, and set-piece character that has an
action file. Shared player kits (CP_), ambient animals (CL_), and townsfolk
(CM_, DB_) are left out. The archive has no boss bit. A species file is the
moveset a boss of that species uses.

  py -I tools/boss_action_record.py

Names come from data/boss-action-names.tsv (file key, character). That index is
the action-set bind: row +0xF0 hashes matched to a character setting. It is not
a display-name database. The anim hash column is that same +0xF0 word.

Hit count = hits the player really takes (pc #41677 accuracy run, notes/hit-label-accuracy-yoritomo.md): the move's hit
WINDOWS, not its hitbox rows or hit ids (tools/chain_hits.hit_windows, the same rule as AmWindows in auto_moves.h). Rows that
never start (+0x08 FFFF) or deal no damage (power +0x10 and element +0x12 both 0) are dropped; rows with the same (start frame,
hit group) are one hit; same-group keys that overlap / touch are one swept swing. Damage for a hit is the +0x10 word times 5; a
window whose rows store different powers lists them as 'a|b' (alternatives of ONE hit). zero_power is 1 when any started row
stores power 0 (such a row is not counted as a hit when its element is 0 too; the Otoroshi 0x4E4E catch is 0 hits).

Chain rule (notes/chain-hits.md, pc #41677): hit_count / damage_per_hit / zero_power of an action also include the follow-up
pieces it really plays into (tools/chain_hits.plays_next): the first private sub-table 3 row with no AI request, else the +0xE0
finish link, cycle-safe, depth 6. Links that carry an AI request are the AI picking another move and are not counted. Locomotion
motions 11 / 31 are never a piece, and a zero-damage catch ends the chain. Follow-up damage is appended after the action's own hits.
Own counts and paths of every changed row go to notes/chain-hits-gains.tsv.

When two files share a character name, the larger action table is the moveset.
The self-check exits non-zero unless Yoritomo 0x4E6E is 2 hits at 250/275, the
Yoritomo drill measurements of tools/test_hit_labels.py hold for 0x4E4E / 0x4E55 / 0x4E59 /
0x4E5C, Shingen 0x4E4E is 3 hits, and Otoroshi 0x4E4E is 0 hits with zero_power set.
"""
import csv
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import chain_hits  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
EXTRACT = ROOT / "data" / "extract" / "lfm" / "archive_01"
if not EXTRACT.is_dir():  # worktrees do not carry the extract; use the main checkout's copy
    EXTRACT = Path("C:/Users/Ahmad Hamza/nioh3-engine/data/extract/lfm/archive_01")
GAINS = ROOT / "notes" / "chain-hits-gains.tsv"
NAMES = ROOT / "data" / "boss-action-names.tsv"
OUT = ROOT / "data" / "boss-action-record.tsv"
SKIP_PREFIX = ("CP_", "CL_", "CM_", "DB_")
REC = 0x150
ROW = 192

CHECKS = {
    ("CE_YORITOMO_00", 0x4E6E): (2, "250/275", "0"),
    ("CE_SHINGEN_00", 0x4E4E): (3, "85/85/240", "0"),
    ("CE_OTOROSHI_00", 0x4E4E): (0, "", "1"),
    ("CE_YORITOMO_00", 0x4E59): (2, "175/180", "0"),
}
# pc #41677 / #41579 drills (real game): 4E4E and 4E55 land 1 (their 2nd hit in a fight is an AI-chosen next move), 4E5C plays 4E60 > 4E62
CHAIN_CHECKS = {("CE_YORITOMO_00", 0x4E4E): 1, ("CE_YORITOMO_00", 0x4E55): 1, ("CE_YORITOMO_00", 0x4E5C): 3}


def load_names():
    bosses = {}
    with NAMES.open(encoding="utf-8", newline="") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            name = r["character"]
            if not name or name == "?" or name.startswith(SKIP_PREFIX):
                continue
            key = r["file"].upper()
            bosses.setdefault(name, []).append(key)
    return bosses


def parse(path):
    return own_rows(path.read_bytes())


def own_rows(d):
    if len(d) < 16 or struct.unpack_from("<I", d, 0)[0] != 16:
        raise ValueError("not an action table")
    st = [struct.unpack_from("<II", d, 4 + 8 * i) for i in range(16)]
    o0, s0 = st[0]
    o1, s1 = st[1]
    o2, s2 = st[2]
    if s1 % REC or s2 % ROW or not s1:
        raise ValueError("row size does not divide")
    na = s1 // REC
    sizes = struct.unpack_from("<%dH" % na, d, o0 + 64)
    p = o0 + s0 - sum(sizes)
    if p < o0:
        raise ValueError("packed list does not fit")
    rows = []
    for r in range(na):
        cnt = struct.unpack_from("<H", d, p)[0]
        idx = struct.unpack_from("<%dH" % cnt, d, p + 2)
        p += sizes[r]
        ro = o1 + r * REC
        aid = struct.unpack_from("<H", d, ro + 0x0C)[0]
        mot = struct.unpack_from("<H", d, ro + 0x20)[0]
        anim = struct.unpack_from("<I", d, ro + 0xF0)[0]
        win, _started = chain_hits.hit_windows(d, o2, idx)
        zero = int(any(struct.unpack_from("<H", d, o2 + i * ROW + 8)[0] != 0xFFFF
                       and struct.unpack_from("<H", d, o2 + i * ROW + 0x10)[0] == 0 for i in idx))
        parts = ["|".join(str(x * 5) for x in w["powers"]) for w in win]
        rows.append((aid, mot, anim, len(win), "/".join(parts), zero))
    if p != o0 + s0:
        raise ValueError("packed walk left %d bytes" % (o0 + s0 - p))
    return rows


def chained(d, rows, depth=6):
    """rows with follow-up pieces folded in. Returns (rows, [(aid, mot, own hits, chain hits, path)] for changed rows)."""
    paths = chain_hits.chain_paths(d, depth)
    by = {r[0]: r for r in rows}  # same last-wins id map as chain_hits.parse
    out, gains = [], []
    for aid, mot, anim, nhit, dmg, zero in rows:
        tail = [by[t] for t in paths.get(aid, [aid])[1:] if t in by and by[t][3]]
        if tail:
            total = nhit + sum(t[3] for t in tail)
            dmg = "/".join(x for x in [dmg] + [t[4] for t in tail] if x)
            zero = max([zero] + [t[5] for t in tail])
            gains.append((aid, mot, nhit, total, "-".join("%X" % x for x in paths[aid])))
            nhit = total
        out.append((aid, mot, anim, nhit, dmg, zero))
    return out, gains


def choose_files(bosses):
    chosen = []
    skipped = []
    for name in sorted(bosses):
        found = []
        for key in bosses[name]:
            path = EXTRACT / key[:2] / key[2:]
            if not path.is_file():
                raise SystemExit("missing extract %s for %s" % (path, name))
            found.append((path.stat().st_size, key, path))
        found.sort(reverse=True)
        chosen.append((name, found[0][1], found[0][2]))
        for _size, key, _path in found[1:]:
            skipped.append((name, key))
    return chosen, skipped


def main():
    bosses = load_names()
    chosen, skipped = choose_files(bosses)
    out_rows = []
    counts = []
    all_gains = []
    nochain = []
    for name, key, path in chosen:
        d = path.read_bytes()
        rows = own_rows(d)
        own_total = sum(r[3] for r in rows)
        try:
            rows, gains = chained(d, rows)
        except ValueError as e:  # sub-table 0 layout chain_hits cannot bind: own hits only
            gains = []
            nochain.append((name, str(e)))
        all_gains += [(name,) + g for g in gains]
        chain_total = sum(r[3] for r in rows)
        hits = sum(1 for r in rows if r[3])
        zeros = sum(r[5] for r in rows)
        counts.append((name, len(rows), hits, zeros, key, own_total, chain_total, len(gains)))
        for aid, mot, anim, nhit, dmg, zero in rows:
            out_rows.append((name, aid, mot, anim, nhit, dmg, zero))
    out_rows.sort(key=lambda r: (r[0], r[2], r[1]))
    by = {(r[0], r[1]): r for r in out_rows}
    for (name, aid), (nhit, dmg, zero) in CHECKS.items():
        got = by.get((name, aid))
        if got is None or got[4] != nhit or got[5] != dmg or str(got[6]) != zero:
            raise SystemExit("self-check failed %s 0x%X got %s" % (name, aid, got))
    for (name, aid), nhit in CHAIN_CHECKS.items():
        got = by.get((name, aid))
        if got is None or got[4] != nhit:
            raise SystemExit("chain self-check failed %s 0x%X got %s" % (name, aid, got))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["boss", "action_id", "motion", "anim_hash", "hit_count", "damage_per_hit", "zero_power"])
        for name, aid, mot, anim, nhit, dmg, zero in out_rows:
            w.writerow([name, "0x%X" % aid, mot, "%08x" % anim, nhit, dmg, zero])
    with GAINS.open("w", encoding="utf-8", newline="\n") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["boss", "action_id", "motion", "own_hits", "chain_hits", "path"])
        for name, aid, mot, own, total, pth in sorted(all_gains):
            w.writerow([name, "0x%X" % aid, mot, own, total, pth])
    print("bosses %d actions %d skipped_smaller_files %d chained_rows %d no_chain %d" % (
        len(counts), len(out_rows), len(skipped), len(all_gains), len(nochain)))
    print("self-check ok")
    for name, err in nochain:
        print("no chain decode: %s (%s)" % (name, err))
    print("boss\tactions\tactions_with_hits\tzero_power\tfile\town_hits_total\tchain_hits_total\tchained_rows")
    for name, n, hits, zeros, key, own, tot, ng in counts:
        print("%s\t%d\t%d\t%d\t%s\t%d\t%d\t%d" % (name, n, hits, zeros, key, own, tot, ng))
    return 0


if __name__ == "__main__":
    sys.exit(main())
