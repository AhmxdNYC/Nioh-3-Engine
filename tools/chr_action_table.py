"""Dump a per-character action table from LFM archive_01 (static, read-only).

Format (notes/yoritomo-move-table-search.md, fields: notes/yoritomo-action-fields.md):
archive_01 file = u32 n=16, then 16 (offset,size) sub-tables. Sub-table 0 is a header whose words 2.. are
(count, record_size) per sub-table (173x336, 182x192, 503x128, 81x48, 93x208, 2x16, 51x336, 30x80, 5x128, ...).
Sub-table 1 = action records, 336 (0x150) bytes each:
  +0x0C u16 action id                           PROVEN (all 44 logged Yoritomo ids)
  +0x20 u16 motion number                       PROVEN-ish (families line up with move kinds)
  +0x40 16 x (u16 id, u16 frame, u16 len) event slots, empty = (FFFF,0,FFFF). id ranges: <1000 local
        (hitbox / effect?), 10000+ common (SE/effect?), 20000+ = an ACTION id (same space as +0x0C)  GUESSED meaning
  +0xA0 5 x (u16 value, u16 startFrame, u16 endFrame) schedule A (values 20-400, rising frames; speed %?)  GUESSED
  +0xBE 5 x (u16 value, u16 startFrame, u16 endFrame) schedule B                                       GUESSED
  +0xE0 u16 next action when this one finishes  PROVEN (0x4E21->0x4E22, 0x4E5C->0x4E60, 0x4E30->0x4E31,
        0x4E7B->0x4EA7 = logged chains; 2 = idle; self = looping state)
  +0xF0 u32 action object hash (MotorCharacterSetting.CharacterActionObjectNameHashArray entry)  PROVEN
Sub-table 3 = 503 x 128-byte records keyed by action id at +0x00 (133 distinct keys, all action ids):
  per-action rule/branch entries (count exported as sub3_rules; content not decoded).

  py -I tools/chr_action_table.py <archive_01 file>            -> TSV: row, action_id, motion, flags06, flags10, obj_hash
  py -I tools/chr_action_table.py <archive_01 file> --csv      -> full CSV (all named fields)
"""
import collections
import struct
import sys

REC = 0x150
EMPTY = (0xFFFF, 0, 0xFFFF)


def subtables(d):
    n = struct.unpack_from('<I', d, 0)[0]
    assert n == 16, 'not a 16-sub-table archive_01 file'
    return [struct.unpack_from('<II', d, 4 + 8 * i) for i in range(n)]


def records(path):
    d = open(path, 'rb').read()
    off, size = subtables(d)[1]
    assert size and size % REC == 0, 'sub-table 1 is not 336-byte records'
    for r in range(size // REC):
        o = off + r * REC
        yield (r, struct.unpack_from('<H', d, o + 0x0C)[0], struct.unpack_from('<H', d, o + 0x20)[0],
               struct.unpack_from('<H', d, o + 0x06)[0], struct.unpack_from('<H', d, o + 0x10)[0],
               struct.unpack_from('<I', d, o + 0xF0)[0])


def _h(v):
    return '' if v == 0xFFFF else str(v)


def _triples(b, base, n):
    out = []
    for k in range(n):
        t = struct.unpack_from('<3H', b, base + 6 * k)
        if t != EMPTY:
            out.append((k, t))
    return out


def full(path):
    d = open(path, 'rb').read()
    st = subtables(d)
    off, size = st[1]
    o3, s3 = st[3]
    sub3 = collections.Counter(struct.unpack_from('<H', d, o3 + r * 128)[0] for r in range(s3 // 128))
    for r in range(size // REC):
        b = d[off + r * REC: off + (r + 1) * REC]
        u16 = lambda o: struct.unpack_from('<H', b, o)[0]
        aid, motion = u16(0x0C), u16(0x20)
        ev = _triples(b, 0x40, 16)
        sa = _triples(b, 0xA0, 5)
        sb = _triples(b, 0xBE, 5)
        local = [t for k, t in ev if t[0] < 1000]
        common = [t for k, t in ev if 10000 <= t[0] < 20000]
        acts = [t for k, t in ev if 20000 <= t[0] < 0xFFFF]
        kind = ('attack' if 1000 <= motion < 1200 else 'hit_reaction' if 30000 <= motion < 65535 else
                'movement' if 0 < motion < 1000 else 'other')
        yield {
            'row': r, 'action_id': '0x%X' % aid, 'motion': _h(motion), 'kind_by_motion': kind,
            'next_action': '' if u16(0xE0) == 0xFFFF else '0x%X' % u16(0xE0),
            'loops': int(u16(0xE0) == aid),
            'obj_hash': '%08x' % struct.unpack_from('<I', b, 0xF0)[0],
            'hash_11c': '%08x' % struct.unpack_from('<I', b, 0x11C)[0],
            'flags00': '0x%04X' % u16(0x00), 'flags02': '0x%04X' % u16(0x02), 'flags04': '0x%04X' % u16(0x04),
            'flags06': '0x%04X' % u16(0x06), 'flags10': '0x%04X' % u16(0x10), 'flags18': '0x%04X' % u16(0x18),
            'flags1a': '0x%04X' % u16(0x1A), 'flags1c': '0x%04X' % u16(0x1C), 'flags110': '0x%04X' % u16(0x110),
            'flags112': '0x%04X' % u16(0x112), 'flags114': '0x%04X' % u16(0x114),
            'atk_param24': _h(u16(0x24)), 'alt_motion34': _h(u16(0x34)),
            'atk_paramEA': _h(u16(0xEA)), 'atk_paramEC': _h(u16(0xEC)),
            'n_events': len(ev), 'n_local_events': len(local), 'n_common_events': len(common),
            'events': ' '.join('%d:%s@%d+%s' % (k, t[0], t[1], _h(t[2])) for k, t in ev),
            'event_action_links': ' '.join('0x%X@%d' % (t[0], t[1]) for t in acts),
            'sched_a': ' '.join('%s@%d-%s' % (_h(t[0]), t[1], _h(t[2])) for k, t in sa),
            'sched_b': ' '.join('%s@%d-%s' % (_h(t[0]), t[1], _h(t[2])) for k, t in sb),
            'tail130': ' '.join(_h(u16(o)) for o in range(0x130, 0x150, 2)),
            'sub3_rules': sub3.get(aid, 0),
        }


if __name__ == '__main__':
    if '--csv' in sys.argv:
        import csv
        rows = list(full(sys.argv[1]))
        if '--log' in sys.argv:  # --log <Nioh3Training.log> <chr>: add ACTLOG median ran-ms and observed successors
            import re
            import statistics
            lp, chr_ = sys.argv[sys.argv.index('--log') + 1], sys.argv[sys.argv.index('--log') + 2]
            dur, nxt = collections.defaultdict(list), collections.defaultdict(collections.Counter)
            pat = re.compile(r'ACTLOG boss \S+ chr %s: action 0x([0-9A-F]+) \(prev 0x([0-9A-F]+), ran (\d+) ms' % chr_)
            for ln in open(lp, encoding='utf-8', errors='replace'):
                m = pat.search(ln)
                if m and m[2] != 'FFFFFFFF':
                    p = '0x' + m[2]
                    dur[p].append(int(m[3]))
                    nxt[p]['0x' + m[1]] += 1
            for row in rows:
                a = row['action_id']
                row['log_n'] = len(dur[a])
                row['log_ms_median'] = int(statistics.median(dur[a])) if dur[a] else ''
                row['log_next'] = ' '.join('%s x%d' % kv for kv in nxt[a].most_common())
                row['next_action_matches_log'] = ('' if not nxt[a] else
                                                  int(row['next_action'] in nxt[a]))
        w = csv.DictWriter(sys.stdout, fieldnames=list(rows[0]), lineterminator='\n')
        w.writeheader()
        w.writerows(rows)
    else:
        print('row\taction_id\tmotion\tflags06\tflags10\tobj_hash')
        for r, a, m, f6, f10, h in records(sys.argv[1]):
            print('%d\t0x%X\t%d\t0x%04X\t0x%04X\t%08x' % (r, a, m, f6, f10, h))
