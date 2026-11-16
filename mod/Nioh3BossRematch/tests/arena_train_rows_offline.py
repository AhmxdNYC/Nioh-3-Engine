r"""#107705: 'Train in this arena' (native route) warps to Training's arena-table spot, but BR offers it from its own catalogue
(arena_x..). The rule is only safe while the two agree: every non-scroll catalogue row with arena coords must be an 'arena'
(inside the trigger) row in Nioh3Training_arenas.tsv, and every row without them must not be. No game.
    py -I tests/arena_train_rows_offline.py
"""
from pathlib import Path
import sys
mod = Path(__file__).resolve().parents[2]
lines = [l.rstrip('\r\n').split('\t') for l in (mod / 'Nioh3BossRematch/Nioh3BossRematch_bosses.tsv').read_text(encoding='utf-8').splitlines() if l and not l.startswith('#')]
head, rows = lines[0], [dict(zip(lines[0], r)) for r in lines[1:] if len(r) == len(lines[0])]
kinds = {}
for l in (mod / 'Nioh3Training/Nioh3Training_arenas.tsv').read_text(encoding='utf-8').splitlines():
    f = l.split('\t')
    if l.startswith('#') or len(f) < 14: continue
    kinds.setdefault(f[13], set()).add(f[12])
bad, inside = [], 0
for r in rows:
    if r['route'] not in ('R', 'H', 'D'): continue
    cat = r['arena_x'] not in ('', '-')
    tr = 'arena' in kinds.get(r['id'], set())
    inside += cat
    if cat != tr: bad.append(f"{r['id']}: catalogue arena {'yes' if cat else 'no'}, Training arena row {'yes' if tr else 'no'}")
for b in bad: print('MISMATCH', b)
assert inside > 0, 'no catalogue row with an in-arena spot'
if bad: sys.exit(1)
print(f'PASS arena-train rows: {inside} catalogue in-arena spots, all match Training arena rows')
