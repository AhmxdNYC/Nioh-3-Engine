"""Per-move facts for one boss from its archive_01 action file: hit windows (hit count) and the unblockable flag.

Static, read-only. Works on any 16-sub-table character action file (all bosses), Yoritomo is the default.

  py -I tools/yoritomo_move_facts.py [<archive_01 file>] [--all] [--check]
      default file: data/extract/lfm/archive_01/FA/5FBD421BF96B8514DA056584B9507A22DC7956B1793995A113FBA714CFFA54
      --all    also print non-attack actions that own no hitbox rows
      --check  assert the known Yoritomo facts (0x4E6E 2 hits + unblockable, 0x4E68 3, 0x4E6C 2 and blockable)

Evidence: notes/yoritomo-hits-and-unblockable.md (and notes/yoritomo-hit-source-22305.md for the unblockable code chain).

Fields used (all from the file, RVAs are re/Nioh3.dump.exe):
- action row (sub-table 1, 336 B): +0x0C id, +0x20 motion, qword +0x00 bit 45 (= u16 +0x04 bit 0x2000) = "great attack"
  -> Collision::Damage::Hit::IsAttackGreatAttack (code chain in yoritomo-hit-source-22305.md). Unblockable.
- action -> owned hitbox rows: sub-table 0 packed records (u16 count + u16 indexes) in action order (proven family-wide).
- hitbox row (sub-table 2, 192 B), read by the per-tick slot walk RVA 0x35A220 (slots 0..[R+0x4A]-1):
  +0x08 u16 start frame (0xFFFF = not frame-timed; used by spawned objects via sub-table 7 +0x12/+0x15),
  +0x0A u16 end frame or 0xFFFF, +0x0C u16 length; end = +0x0A if set else +0x08 + +0x0C (RVA 0x907FC0).
  +0x10 u16 physical power, +0x12 u16 element power (GetAttackBaseDamage RVA 0x23C785C), +0xA5 u8 group id (meaning not proven).
- action events (+0x40, 16 x (id, frame, len)): 20000+N = spawn of sub-table 7 row N (projectile / spawned object).

Hit count = number of hit windows: frame-timed rows with power or element > 0, merged when they overlap or touch
(start <= previous end + 1) and have the same group id (group 0 rows must also have the same power/element).
Rows that sweep one swing (e.g. 0x4E8E at frames 52..57) merge into one.
ASSUMPTION (not proven in code): one window hits the player once.
"""
import os
import struct
import sys

REC = 0x150
HB = 192
RED_EVENTS = {10250, 10251, 10254, 10255}
DEFAULT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'data', 'extract', 'lfm', 'archive_01', 'FA',
                       '5FBD421BF96B8514DA056584B9507A22DC7956B1793995A113FBA714CFFA54')


def u16(d, o):
    return struct.unpack_from('<H', d, o)[0]


def load(path):
    d = open(path, 'rb').read()
    assert struct.unpack_from('<I', d, 0)[0] == 16, 'not a 16-sub-table archive_01 file'
    st = [struct.unpack_from('<II', d, 4 + 8 * i) for i in range(16)]
    o0, s0 = st[0]
    o1, s1 = st[1]
    assert s1 and s1 % REC == 0, 'sub-table 1 is not 336-byte action rows'
    na = s1 // REC
    sizes = struct.unpack_from('<%dH' % na, d, o0 + 64)
    p = o0 + s0 - sum(sizes)
    owned = []
    for r in range(na):
        cnt = u16(d, p)
        owned.append(list(struct.unpack_from('<%dH' % cnt, d, p + 2)))
        p += sizes[r]
    return d, st, owned


def hitbox(d, st, i):
    b = st[2][0] + i * HB
    start, endf, ln = u16(d, b + 8), u16(d, b + 0xA), u16(d, b + 0xC)
    end = endf if endf != 0xFFFF else (start + ln if start != 0xFFFF else 0xFFFF)
    return {'row': i, 'start': start, 'end': end, 'pow': u16(d, b + 0x10), 'elem': u16(d, b + 0x12), 'grp': d[b + 0xA5]}


def windows(rows):
    timed = sorted((h for h in rows if h['start'] != 0xFFFF and (h['pow'] or h['elem'])), key=lambda h: (h['start'], h['row']))
    out = []
    for h in timed:
        for w in out:
            same = w['grp'] == h['grp'] and (h['grp'] or (w['pow'], w['elem']) == (h['pow'], h['elem']))
            if same and h['start'] <= w['end'] + 1:
                w['end'] = max(w['end'], h['end'])
                w['rows'] += 1
                break
        else:
            out.append(dict(h, rows=1))
    return sorted(out, key=lambda w: w['start'])


def facts(path):
    d, st, owned = load(path)
    o1 = st[1][0]
    for r, idx in enumerate(owned):
        o = o1 + r * REC
        aid, motion = u16(d, o + 0xC), u16(d, o + 0x20)
        q0 = struct.unpack_from('<Q', d, o)[0]
        attack = any(u16(d, o + x) != 0xFFFF for x in (0x13C, 0x142))
        ev = [struct.unpack_from('<3H', d, o + 0x40 + 6 * k) for k in range(16)]
        ev = [t for t in ev if t != (0xFFFF, 0, 0xFFFF)]
        rows = [hitbox(d, st, i) for i in idx]
        win = windows(rows)
        spawns = [(t[0] - 20000, t[1]) for t in ev if 20000 <= t[0] < 0xFFFF]
        untimed = [h for h in rows if h['start'] == 0xFFFF]
        zero = [h for h in rows if h['start'] != 0xFFFF and not (h['pow'] or h['elem'])]
        parts = (['hitbox windows'] if win else []) + (['spawned objects'] if spawns or untimed else [])
        how = ' + '.join(parts) or ('zero-power rows only' if rows else 'no own hitbox rows')
        yield {
            'action': '0x%X' % aid, 'motion': motion, 'attack': int(attack),
            'unblockable': q0 >> 45 & 1, 'power_attack': q0 >> 41 & 1,
            'red_events': int(bool(RED_EVENTS & {t[0] for t in ev})),
            'hits': len(win) if win else ('' if how == 'no own hitbox rows' else 0),
            'windows': ' '.join('%d-%d(%d/%d)' % (w['start'], w['end'], w['pow'], w['elem']) for w in win),
            'spawns': ' '.join('%d@%d' % s for s in spawns), 'untimed_rows': len(untimed), 'zero_power_rows': len(zero),
            'n_rows': len(rows), 'how': how,
        }


COLS = ['action', 'motion', 'attack', 'unblockable', 'power_attack', 'red_events', 'hits', 'windows', 'spawns',
        'untimed_rows', 'zero_power_rows', 'n_rows', 'how']


def main(argv):
    args = [a for a in argv if not a.startswith('--')]
    path = args[0] if args else DEFAULT
    out = list(facts(path))
    if '--check' in argv:
        f = {x['action']: x for x in out}
        assert f['0x4E6E']['hits'] == 2 and f['0x4E6E']['unblockable'] == 1, f['0x4E6E']
        assert f['0x4E6C']['hits'] == 2 and f['0x4E6C']['unblockable'] == 0, f['0x4E6C']
        assert f['0x4E68']['hits'] == 3 and f['0x4E68']['unblockable'] == 0, f['0x4E68']
        assert f['0x4E74']['hits'] == 1 and f['0x4E74']['spawns'] == '1@226 2@226', f['0x4E74']
        print('check ok', file=sys.stderr)
    print('\t'.join(COLS))
    for x in out:
        if x['attack'] or x['n_rows'] or x['unblockable'] or '--all' in argv:
            print('\t'.join(str(x[c]) for c in COLS))


if __name__ == '__main__':
    main(sys.argv[1:])
