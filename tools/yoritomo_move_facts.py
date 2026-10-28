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

Hit count = number of hit windows: frame-timed rows with power or element > 0.
1. Rows with the same (start frame +0x08, hit group) are one hit, power = max: the game's own key (RVA 0x78EADF..0x78EB74 sums power over
   these keys; hit group = getter RVA 0x2B5B98 = byte +0xA5, 0xFF -> qword +0x00 bit 17 + 2 * bit 18). Proven in code.
2. Keys that overlap or touch (start <= previous end + 1) and have the same hit group merge (group 0 keys must also have the same
   power/element): rows that sweep one swing (e.g. 0x4E8E at frames 52..57) merge into one. Data rule.
hits_upper_bound = 1 when two counted windows are live at the same frame (different keys): maybe 1 hit with 2 hitboxes.
ASSUMPTION (not proven in code): one window hits the player once. Player observations contradict this for two moves.
Exact archive/action observations take priority; static_hits and hit_source retain provenance. See notes/yoritomo-hitcount-40826.md.
"""
import os
import hashlib
import struct
import sys

REC = 0x150
HB = 192
RED_EVENTS = {10250, 10251, 10254, 10255}
DEFAULT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'data', 'extract', 'lfm', 'archive_01', 'FA',
                       '5FBD421BF96B8514DA056584B9507A22DC7956B1793995A113FBA714CFFA54')


# Human #40826, action mapping #40838. Observations, not a decoded rehit field.
# Exact archive digest prevents applying an action-id override to other bosses/builds.
OBSERVED_HITS = {} # Superseded by measured action isolation: 1c272daf, #41579.


def u16(d, o):
    return struct.unpack_from('<H', d, o)[0]


def load(path):
    with open(path, 'rb') as source:
        d = source.read()
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


def hit_group(d, b):
    """The game's hit group of a hitbox row = RVA 0x2B5B98: byte +0xA5; 0xFF -> (qword +0x00 bit 17 ? 1 : 0) + (bit 18 ? 2 : 0)."""
    g = d[b + 0xA5]
    if g == 0xFF:
        q = struct.unpack_from('<Q', d, b)[0]
        g = (q >> 17 & 1) + 2 * (q >> 18 & 1)
    return g


def hitbox(d, st, i):
    b = st[2][0] + i * HB
    start, endf, ln = u16(d, b + 8), u16(d, b + 0xA), u16(d, b + 0xC)
    end = endf if endf != 0xFFFF else (start + ln if start != 0xFFFF else 0xFFFF)
    return {'row': i, 'start': start, 'end': end, 'pow': u16(d, b + 0x10), 'elem': u16(d, b + 0x12), 'grp': hit_group(d, b)}


def windows(rows):
    timed = [h for h in rows if h['start'] != 0xFFFF and (h['pow'] or h['elem'])]
    # 1. the game's own hit key (RVA 0x78EADF..0x78EB74, the action's power total): rows with the same (start frame +0x08, hit group 0x2B5B98)
    #    are ONE hit whose power / element = the max over those rows (a swing with 2+ capsules / body spheres). Distinct keys are summed.
    keys = {}
    for h in sorted(timed, key=lambda h: (h['start'], h['row'])):
        k = keys.get((h['start'], h['grp']))
        if k:
            k['end'] = max(k['end'], h['end'])
            k['pow'], k['elem'], k['rows'] = max(k['pow'], h['pow']), max(k['elem'], h['elem']), k['rows'] + 1
        else:
            keys[(h['start'], h['grp'])] = dict(h, rows=1)
    # 2. one capsule sweeping a swing (start frames 52, 53, 54 ... each a few frames long, 0x4E8E): keys of the same hit group that overlap or
    #    touch merge into one window (group 0 keys also need the same power / element). Data rule, not in the code above.
    out = []
    for h in sorted(keys.values(), key=lambda h: (h['start'], h['row'])):
        for w in out:
            same = w['grp'] == h['grp'] and (h['grp'] or (w['pow'], w['elem']) == (h['pow'], h['elem']))
            if same and h['start'] <= w['end'] + 1:
                w['end'] = max(w['end'], h['end'])
                w['rows'] += h['rows']
                break
        else:
            out.append(dict(h))
    return sorted(out, key=lambda w: w['start'])


def uncertain(win):
    """1 = two counted windows are live at the same frame ([start, end) overlap). The game counts them as separate hits (different hit key),
    but whether both connect on the player is spatial, so the count is an upper bound for such a move."""
    s = sorted(win, key=lambda w: w['start'])
    return int(any(b['start'] < a['end'] for i, a in enumerate(s) for b in s[i + 1:]))


def facts(path):
    d, st, owned = load(path)
    o1 = st[1][0]
    archive_sha = hashlib.sha256(d).hexdigest()
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
        static_hits = len(win) if win else ('' if how == 'no own hitbox rows' else 0)
        observed = OBSERVED_HITS.get((archive_sha, aid))
        yield {
            'action': '0x%X' % aid, 'motion': motion, 'attack': int(attack),
            'unblockable': q0 >> 45 & 1, 'power_attack': q0 >> 41 & 1,
            'red_events': int(bool(RED_EVENTS & {t[0] for t in ev})),
            'hits': observed if observed is not None else static_hits,
            'static_hits': static_hits,
            'hit_source': 'player observation #40826' if observed is not None else 'static window estimate',
            'windows': ' '.join('%d-%d(%d/%d)' % (w['start'], w['end'], w['pow'], w['elem']) for w in win),
            'spawns': ' '.join('%d@%d' % s for s in spawns), 'untimed_rows': len(untimed), 'zero_power_rows': len(zero),
            'n_rows': len(rows), 'how': how, 'hits_upper_bound': uncertain(win),
        }


COLS = ['action', 'motion', 'attack', 'unblockable', 'power_attack', 'red_events', 'hits', 'windows', 'spawns',
        'untimed_rows', 'zero_power_rows', 'n_rows', 'how', 'hits_upper_bound', 'static_hits', 'hit_source']


def main(argv):
    args = [a for a in argv if not a.startswith('--')]
    path = args[0] if args else DEFAULT
    out = list(facts(path))
    if '--check' in argv:
        f = {x['action']: x for x in out}
        assert all(f[a]['hits'] == 2 and f[a]['static_hits'] == 1 for a in ('0x4E4E', '0x4E55'))
        assert f['0x4E52']['hits'] == 2
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
