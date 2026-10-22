r"""Add the static action-table rows of every bound boss (tools/gen_boss_move_facts.py BOSSES) to notes/boss-action-ids-all.csv.

    py -I tools/add_static_boss_rows.py     then py -I tools/gen_boss_moves.py

user #22709: the Boss moves menu of every boss lists all of its game-table moves, like Yoritomo's 173. Idempotent (existing pairs are kept).
Attack rows (hit window, attack block or GreatAttack flag in the game data) are kind '' = visible, grouped by hits / Unblockable.
Everything else is kind M (hidden unless 'Show twitches'): not proven safe to force (a hit reaction can kill the boss, see N3MoveUnsafe).
"""
import csv
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gen_boss_move_facts as g  # noqa: E402
import yoritomo_move_facts as ymf  # noqa: E402

CSV = os.path.join(ROOT, 'notes', 'boss-action-ids-all.csv')


def main():
    with open(CSV, encoding='utf-8-sig', newline='') as f:
        rd = csv.DictReader(f)
        cols = rd.fieldnames
        rows = list(rd)
    have = {(int(r['chr']), int(r['action_id'], 0)) for r in rows}
    names = {int(r['chr']): r['boss'] for r in rows}
    names.update(g.NAMES)
    added = 0
    for chr_id, rel in sorted(g.BOSSES.items()):
        if chr_id == 16102:
            continue
        path = g.data_file(rel)
        if not path:
            print('action file missing for', chr_id)
            return 1
        d = open(path, 'rb').read()
        for x in ymf.facts(path):
            aid = int(x['action'], 16)
            if (chr_id, aid) in have:
                continue
            atk = bool(x['attack'] or x['hits'] or x['unblockable'] or x['n_rows'])
            label = ('static table: attack-family motion %d' % x['motion']) if atk else (
                'static table: motion %d (non-attack)' % x['motion'])
            rows.append({'boss': names[chr_id], 'row': 'R7B35', 'chr': str(chr_id), 'action_id': '0x%X' % aid, 'times_used': '',
                         'total_ms': '', 'avg_ms': '', 'label_lowconf': label, 'kind': '' if atk else 'M', 'typ_ms': ''})
            added += 1
    with open(CSV, 'w', encoding='utf-8', newline='') as f:
        w = csv.DictWriter(f, cols)
        w.writeheader()
        w.writerows(rows)
    print('added', added, 'rows')
    return 0


if __name__ == '__main__':
    sys.exit(main())
