"""Dump the Nioh 3 mission table (LFM archive_03 52/BD29...) to CSV. Read-only.
usage: py -I mcsv.py <en.tsv> <mission_table_file> <out.csv>"""
import sys, struct, csv
TXT, F, OUT = sys.argv[1:4]
txt = {}
for line in open(TXT, encoding='utf-8'):
    c = line.rstrip('\n').split('\t')
    txt[int(c[1], 16)] = c[3]
b = open(F, 'rb').read()
assert b[:4] == bytes.fromhex('00220420')
n = struct.unpack_from('<I', b, 4)[0]
RS = 0x198
assert 8 + n * RS == len(b)
U32 = lambda r, o: struct.unpack_from('<I', r, o)[0]
U16 = lambda r, o: struct.unpack_from('<H', r, o)[0]
ERA = {2: 'Sengoku 1572 (Totomi)', 3: 'Heian 1190', 4: 'Antiquity 247 (Yamatai)', 5: 'Edo 1622 (finale)',
       6: 'Bakumatsu 1864 (Kyoto)', 7: 'Edo 1622 (post-game)', 8: 'Keian / DLC Edo', 1: 'Eternal Rift', 0: 'system'}
rows = []
for k in range(n):
    off = 8 + k * RS
    r = b[off:off + RS]
    name = txt.get(U32(r, 0x70), '')
    summ = txt.get(U32(r, 0x84), '')
    flags = U32(r, 0x40)
    mid = U16(r, 0x158); kind = U16(r, 0x15a)
    era = U16(r, 0x160)
    listed = (flags & 1) == 0 and (flags & 0x40) == 0 and (flags & 0x4000) == 0 and kind in (0, 1) and name not in ('', 'Dummy', '?')
    rows.append({
        'row': k, 'file_off': '0x%x' % off, 'seq': U32(r, 0), 'key': '0x%x' % U32(r, 4),
        'mission_id': '0x%x' % mid, 'mission_id_dec': mid,
        'kind': {0: 'main', 1: 'sub', 2: 'system'}.get(kind, str(kind)),
        'flags40': '0x%x' % flags,
        'menu_cat': U16(r, 0x14c), 'menu_order': U16(r, 0x14e),
        'parent_id': '0x%x' % U16(r, 0x110), 'region_15c': '0x%x' % U16(r, 0x15c),
        'era_160': era, 'era': ERA.get(era, '?'), 'is_era_160_hi': U16(r, 0x162),
        'story_flag_164': U16(r, 0x164), 'story_prog_166': U16(r, 0x166),
        'unlock_prog_156': U16(r, 0x156), 'unlock_prog_17e': U16(r, 0x17e),
        'rec_lv_178': U16(r, 0x178), 'rec_lv_17a': U16(r, 0x17a), 'rec_lv_17c': U16(r, 0x17c),
        'name_txt': '0x%08x' % U32(r, 0x70), 'name': name,
        'summary_txt': '0x%08x' % U32(r, 0x84), 'summary': summ[:240],
        'battle_scroll_candidate': int(listed),
    })
with open(OUT, 'w', newline='', encoding='utf-8') as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
    w.writeheader(); w.writerows(rows)
print(n, 'rows;', sum(r['battle_scroll_candidate'] for r in rows), 'candidates')
