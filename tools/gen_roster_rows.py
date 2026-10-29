"""Add summon-only roster rows (kind summon, mission 0, id S<chr>) to mod/Nioh3Training/Nioh3Training_arenas.tsv for every boss of the game's
enemy parameter table that has no row yet (pc #23911: Mikoshi Nyudo 257162 and Himiko 835773 gave 'SUMMON: unknown boss' on train S<chr>).

    py -I tools/gen_roster_rows.py           rewrite the generated block of the TSV
    py -I tools/gen_roster_rows.py --check   exit 1 when the TSV block is out of date

Data, no hand list: notes/boss-enemy-params.csv = the decoded chara parameter table (archive_03 C4/E1CBAE08..., 920-byte records:
+0x64 runtime character id = the id Training spawns, +0xA0 name text id, +0x18 base HP; notes/boss-enemy-params.md).
Rule: a named record (name text id resolves) with boss-scale base HP (>= 6000) that is the top-HP variant of its name group
(variant_rank 1) and whose name and character id have no row in the TSV yet -> one roster row. Mikoshi Nyudo (6500) and Himiko (7000)
are in no shipped mission enemy table (tools/auto_arena.py on ally/autoarena finds no placement), so they can only be roster rows.
"""
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TSV = os.path.join(ROOT, 'mod', 'Nioh3Training', 'Nioh3Training_arenas.tsv')
PARAMS = os.path.join(ROOT, 'notes', 'boss-enemy-params.csv')
BEGIN = '# roster rows from the enemy parameter table (tools/gen_roster_rows.py, pc #23911; do not edit by hand)'
MIN_HP = 6000


def rows(base_lines):
    have_chr, have_name = set(), set()
    for ln in base_lines:
        if ln.startswith('#'):
            continue
        f = ln.rstrip('\n').split('\t')
        if len(f) >= 15:
            have_chr.add(int(f[3], 0))
            have_name.add(f[14].split(' (')[0])
    out = []
    for r in csv.DictReader(open(PARAMS, encoding='utf-8')):
        name, chr_ = r['name'], int(r['key64'])
        if not name or int(r['hp_0x18']) < MIN_HP or r['variant_rank'] != '1' or chr_ in have_chr or name in have_name:
            continue
        have_name.add(name)
        out.append('0\t0\t0x00\t%d\t0\t0\t0\t0\t0.0\t0\t0\t0\tsummon\tS%d\t%s\n' % (chr_, chr_, name))
    return out


def build():
    lines = open(TSV, encoding='utf-8').readlines()
    if BEGIN + '\n' in lines:
        lines = lines[:lines.index(BEGIN + '\n')]
    return ''.join(lines) + BEGIN + '\n' + ''.join(rows(lines))


def main(argv):
    text = build()
    old = open(TSV, encoding='utf-8').read()
    if '--check' in argv:
        print('arenas.tsv roster rows ' + ('up to date' if old == text else 'OUT OF DATE'))
        return 0 if old == text else 1
    open(TSV, 'w', encoding='utf-8', newline='\n').write(text)
    print('wrote %s (%d generated roster rows)' % (TSV, text.count('\n') - text[:text.index(BEGIN)].count('\n') - 1))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
