#!/usr/bin/env python3
"""Builds mod/Nioh3Training/Nioh3Training_arenas.tsv (bridge 4506): one line per single-boss catalogue row that has a boss position and a
character id. Joins mod/Nioh3BossRematch/Nioh3BossRematch_bosses.tsv (placement_id, start point, world, arena point, boss_pos) with
notes/boss-map-all.csv (boss_placement_id -> boss_char_id). Usage: gen_arena_table.py [repo_root]"""
import csv, sys, os
root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
cat = os.path.join(root, 'mod/Nioh3BossRematch/Nioh3BossRematch_bosses.tsv')
rows = list(csv.DictReader((l for l in open(cat, encoding='utf-8') if not l.startswith('# ')), delimiter='\t'))
chars = {}
for r in csv.DictReader(open(os.path.join(root, 'notes/boss-map-all.csv'), encoding='utf-8')):
    if r['boss_char_id'].isdigit():
        chars.setdefault(r['boss_placement_id'], r['boss_char_id'])
out = ['# Nioh3Training arena table (tools/gen_arena_table.py). mission, start point, world, boss char id, boss placement id (owner fallback), player x y z yaw, boss x y z, kind (arena = inside the trigger; start = the Fight-now start point), id, name']
n = 0
for r in rows:
    if ';' in r['boss'] or r['boss_pos'] == '-' or r['placement_id'] not in chars or r['region_byte'] == '-' or r['stage_mission'] in ('-', ''):
        continue
    if r['arena_x'] != '-':
        p, kind = [r['arena_x'], r['arena_y'], r['arena_z'], r['arena_yaw']], 'arena'
    elif r['warp_x'] != '-':
        p, kind = [r['warp_x'], r['warp_y'], r['warp_z'], r['warp_yaw']], 'start'
    else:
        continue
    b = r['boss_pos'].split('_')
    if len(b) != 3:
        continue
    out.append('\t'.join([r['replay_mission_id'], r['stage_mission'], '0x%02X' % int(r['region_byte'], 16), chars[r['placement_id']], r['placement_id'], *p, *b, kind, r['id'], r['name']]))
    n += 1
dst = os.path.join(root, 'mod/Nioh3Training/Nioh3Training_arenas.tsv')
open(dst, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
print(n, 'rows ->', dst)
