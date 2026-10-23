"""Write data/boss-action-record.tsv from extracted archive_01 action files.

One row per action on every enemy, duelist, and set-piece character that has an
action file. Shared player kits (CP_), ambient animals (CL_), and townsfolk
(CM_, DB_) are left out. The archive has no boss bit. A species file is the
moveset a boss of that species uses.

  py -I tools/boss_action_record.py

Names come from data/boss-action-names.tsv (file key, character). That index is
the action-set bind: row +0xF0 hashes matched to a character setting. It is not
a display-name database. The anim hash column is that same +0xF0 word.

Hit count is the number of different hitbox-row byte +0xA5 values on rows whose
start frame (+0x08) is not FFFF. Byte FF becomes 0, or 2 when row-qword bit 18
is set. Damage for a hit is the +0x10 word times 5. zero_power is 1 when any
started row stores 0 in that word (the Otoroshi 0x4E4E case).

When two files share a character name, the larger action table is the moveset.
The self-check exits non-zero unless Yoritomo 0x4E6E is 2 hits at 250/275,
Shingen 0x4E4E is 3 hits, and Otoroshi 0x4E4E is 1 hit with zero_power set.
"""
import csv
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXTRACT = ROOT / "data" / "extract" / "lfm" / "archive_01"
NAMES = ROOT / "data" / "boss-action-names.tsv"
OUT = ROOT / "data" / "boss-action-record.tsv"
SKIP_PREFIX = ("CP_", "CL_", "CM_", "DB_")
REC = 0x150
ROW = 192

CHECKS = {
    ("CE_YORITOMO_00", 0x4E6E): (2, "250/275", "0"),
    ("CE_SHINGEN_00", 0x4E4E): (3, "85/85/240", "0"),
    ("CE_OTOROSHI_00", 0x4E4E): (1, "0", "1"),
}


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
    d = path.read_bytes()
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
        order = []
        powers = {}
        zero = 0
        for i in idx:
            b = o2 + i * ROW
            if struct.unpack_from("<H", d, b + 8)[0] == 0xFFFF:
                continue
            hid = d[b + 0xA5]
            q = struct.unpack_from("<Q", d, b)[0]
            if hid == 0xFF:
                hid = 2 if q & (1 << 18) else 0
            power = struct.unpack_from("<H", d, b + 0x10)[0]
            if power == 0:
                zero = 1
            if hid not in powers:
                order.append(hid)
                powers[hid] = []
            if power not in powers[hid]:
                powers[hid].append(power)
        parts = []
        for hid in order:
            parts.append("|".join(str(x * 5) for x in powers[hid]))
        rows.append((aid, mot, anim, len(order), "/".join(parts), zero))
    if p != o0 + s0:
        raise ValueError("packed walk left %d bytes" % (o0 + s0 - p))
    return rows


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
    for name, key, path in chosen:
        rows = parse(path)
        hits = sum(1 for r in rows if r[3])
        zeros = sum(r[5] for r in rows)
        counts.append((name, len(rows), hits, zeros, key))
        for aid, mot, anim, nhit, dmg, zero in rows:
            out_rows.append((name, aid, mot, anim, nhit, dmg, zero))
    out_rows.sort(key=lambda r: (r[0], r[2], r[1]))
    by = {(r[0], r[1]): r for r in out_rows}
    for (name, aid), (nhit, dmg, zero) in CHECKS.items():
        got = by.get((name, aid))
        if got is None or got[4] != nhit or got[5] != dmg or str(got[6]) != zero:
            raise SystemExit("self-check failed %s 0x%X got %s" % (name, aid, got))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["boss", "action_id", "motion", "anim_hash", "hit_count", "damage_per_hit", "zero_power"])
        for name, aid, mot, anim, nhit, dmg, zero in out_rows:
            w.writerow([name, "0x%X" % aid, mot, "%08x" % anim, nhit, dmg, zero])
    print("bosses %d actions %d skipped_smaller_files %d" % (len(counts), len(out_rows), len(skipped)))
    print("self-check ok")
    for name, n, hits, zeros, key in counts:
        print("%s\t%d\t%d\t%d\t%s" % (name, n, hits, zeros, key))
    return 0


if __name__ == "__main__":
    sys.exit(main())
