r"""Boss-agnostic move summary: runs the Yoritomo move-facts algorithm over every character action file bound in
notes/boss-action-files-22709.csv (94 characters) and writes notes/boss-moves-all/.

    py -I tools/boss_move_summary.py [--out notes/boss-moves-all]

Per character: moves (action rows), attacks, hit-count histogram, unblockable (+0x04 bit 0x2000), attacks with no own hitbox rows
(guess-only / unknown hits), spawn-driven (projectile) attacks. Attack = attack block OR timed hit window OR unblockable (the rule
gen_boss_move_facts.py uses for every boss except Yoritomo, who uses the attack block only). Offline, read-only.
"""
import csv
import os
import sys
from collections import Counter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import yoritomo_move_facts as ymf  # noqa: E402

YORI = 'FA/5FBD421BF96B8514DA056584B9507A22DC7956B1793995A113FBA714CFFA54'
COLS = ['character', 'file', 'moves', 'attacks', 'hits_1', 'hits_2', 'hits_3', 'hits_4plus', 'hits_unknown', 'unblockable',
        'attack_no_own_rows', 'projectile_attacks']


def data_file(rel):
    for base in (ROOT, os.path.join(os.path.dirname(ROOT), 'nioh3-engine')):
        p = os.path.join(base, 'data', 'extract', 'lfm', 'archive_01', *rel.split('/'))
        if os.path.exists(p):
            return p
    return None


def summarize(path, yori=False):
    allx = list(ymf.facts(path))
    # NB facts() yields only rows of actions; count every action row
    d, st, owned = ymf.load(path)
    cnt = Counter(sp for x in allx for sp in set(x['spawns'].split()) if sp.endswith('@0'))
    ambient = {sp for sp, n in cnt.items() if n * 10 >= len(allx) * 8}
    s = Counter(); hist = Counter(); per_move = []
    for x in allx:
        proj = [sp for sp in x['spawns'].split() if sp not in ambient] or x['untimed_rows']
        hits = x['hits'] if isinstance(x['hits'], int) else 0
        if proj:
            hits = 0
        atk = bool(x['attack'] or (not yori and (x['hits'] or x['unblockable'])))
        if atk:
            s['attacks'] += 1
            hist[min(hits, 4)] += 1
            if x['how'] == 'no own hitbox rows':
                s['attack_no_own_rows'] += 1
            if proj:
                s['projectile_attacks'] += 1
        s['unblockable'] += x['unblockable']
        per_move.append((x, atk, hits, bool(proj)))
    row = {'moves': len(owned), 'attacks': s['attacks'], 'hits_1': hist[1], 'hits_2': hist[2], 'hits_3': hist[3],
           'hits_4plus': hist[4], 'hits_unknown': hist[0], 'unblockable': s['unblockable'],
           'attack_no_own_rows': s['attack_no_own_rows'], 'projectile_attacks': s['projectile_attacks']}
    return row, per_move


def main(argv):
    out = os.path.join(ROOT, 'notes', 'boss-moves-all')
    if '--out' in argv:
        out = argv[argv.index('--out') + 1]
    os.makedirs(out, exist_ok=True)
    rows = []; missing = []
    with open(os.path.join(ROOT, 'notes', 'boss-action-files-22709.csv'), newline='', encoding='utf-8') as f:
        chars = list(csv.DictReader(f))
    # catalogue bosses bound by gen_boss_move_facts.py that the setting-DB csv does not list (e.g. Ippon-Datara)
    import gen_boss_move_facts as g
    have = {c['archive_01_file'] for c in chars}
    for chr_id, rel in sorted(g.BOSSES.items()):
        if rel not in have:
            chars.append({'char_name': 'chr %d' % chr_id, 'archive_01_file': rel})
    with open(os.path.join(out, 'moves-per-character.tsv'), 'w', encoding='utf-8', newline='\n') as mv:
        mv.write('character\taction\tmotion\tattack\thits\tunblockable\twindows\tspawns\thow\n')
        for c in chars:
            p = data_file(c['archive_01_file'])
            if not p:
                missing.append(c['char_name']); continue
            yori = c['archive_01_file'] == YORI
            r, pm = summarize(p, yori)
            r.update(character=c['char_name'], file=c['archive_01_file'])
            rows.append(r)
            for x, atk, hits, proj in pm:
                if atk or x['unblockable']:
                    mv.write('\t'.join(str(v) for v in (c['char_name'], x['action'], x['motion'], int(atk), hits,
                                                        x['unblockable'], x['windows'], x['spawns'], x['how'])) + '\n')
    rows.sort(key=lambda r: -r['attacks'])
    with open(os.path.join(out, 'summary.tsv'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\t'.join(COLS) + '\n')
        for r in rows:
            f.write('\t'.join(str(r[c]) for c in COLS) + '\n')
    tot = Counter()
    for r in rows:
        for k in COLS[2:]:
            tot[k] += r[k]
    print('characters %d (missing file: %s)' % (len(rows), missing))
    print('totals', dict(tot))
    print('with >=1 attack: %d, with unblockable: %d' % (sum(1 for r in rows if r['attacks']), sum(1 for r in rows if r['unblockable'])))


if __name__ == '__main__':
    main(sys.argv[1:])
