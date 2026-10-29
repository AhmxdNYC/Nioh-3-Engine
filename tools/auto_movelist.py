r"""Static boss move list straight from the game's action files: no hand table, no observed moves (pc2 #23909/#23911).

    py -I tools/auto_movelist.py --extract                      extract every archive_01 action file (367) into data/extract (read-only game access)
    py -I tools/auto_movelist.py --file FA/5FBD...54 [--file ...] [--all-rows]
                                                                 move list of one action file, or of several layered files (first = base)
    py -I tools/auto_movelist.py --chr 16102 [--all-rows]        move list of a boss; its files come from the runtime binding logs (see BINDING)
    py -I tools/auto_movelist.py --check                         assert the known Yoritomo facts
    py -I tools/auto_movelist.py --validate [--hand-binding]     compare with mod/boss_moves.h, boss_move_facts.h, boss_sequences.h
    py -I tools/auto_movelist.py --sweep [--outdir notes/auto-movelist]
                                                                 every boss in the binding logs: summary.tsv + moves.tsv

PER FILE (the same rules as mod/Nioh3Training/src/auto_moves.h, field evidence in notes/yoritomo-action-fields.md and
notes/yoritomo-hits-and-unblockable.md; hit windows via tools/yoritomo_move_facts.py):
  block  = action row +0x13C or +0x142 set (attack block)
  great  = action row u16 +0x04 bit 0x2000 = GreatAttack (unblockable)
  nwin   = hit windows of the action's own hitbox rows (same start frame + hit group = 1 hit; touching same-group keys merge)
  hits   = nwin, but 0 (= unknown) when the action spawns a non-ambient object (event 20000+N) or owns untimed hitbox rows;
           ambient = a frame-0 spawn found in >= 80 % of the file's actions (standing body / weapon object)
  attack = block or nwin or great
  next   = +0xE0 (next action on finish)
OVER THE (LAYERED) TABLE:
  idle   = action id 0 / row 0, the action most others end in, and a self-looping non-attack state that 5+ actions end in
  kind   = I idle, A attack-like (attack or own hitbox rows), M other
  link   = next is a real move (exists, not idle, not itself); loop = the next action repeats itself
  sequence head = has a link, is no link target, not idle, and its chain (max 8) contains an attack

LAYERING (human duelists, notes/auto-movelist-human-layering.md): a boss has several action files loaded at once (its own file, or the
shared CP_HUMAN_COMMON base + one CP_HUMAN_<weapon> file + small extras). Over all 27 multi-file human sets the files' action-id spaces
are disjoint except 11 system ids (0x0, 0x2, 0x61, 0x41A, 0xC51, 0x1035, 0x6AE, 0x3EE9-0x3EEC); 7 of them carry the same action-object
hash in both files (= the same action object) and none is an attack. So the move list = the union; for the few duplicate ids the later
(more specific) file is kept: INFERRED, it changes no attack / hit / sequence fact.

BINDING (which files a chr uses) is the runtime's job (autobind_game.h finds the loaded rows in memory). Offline the same information is
read back from the autobind sweep logs (captures/pc2-autobind/run*/Nioh3Training.log): a bound block names its file; an unbound block
(human duelists: the scan stops at runs of hash-less rows, so it sees only part of a big shared file) is resolved by replaying the
scan on every file (same start action id, same distinct-hash count, same best / runner-up match %). The logs are spawn-time memory
reads, not move observations. The hand table of tools/gen_boss_move_facts.py (BOSSES / EXTRA) is used ONLY by --validate, for hand-list
chr ids that never appeared in a sweep log, and that is printed as such.
"""
import collections
import glob
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import yoritomo_move_facts as ymf  # noqa: E402

REC, HB = 0x150, 192
GAME = 'C:/Program Files (x86)/Steam/steamapps/common/Nioh3'
SIBLINGS = ('nioh3-engine', 'nioh3-autobind', 'nioh3-autoframe')


def u16(d, o):
    return struct.unpack_from('<H', d, o)[0]


def u32(d, o):
    return struct.unpack_from('<I', d, o)[0]


# ---------------------------------------------------------------- data location
def extract_root():
    for base in [ROOT] + [os.path.join(os.path.dirname(ROOT), s) for s in SIBLINGS]:
        p = os.path.join(base, 'data', 'extract', 'lfm', 'archive_01')
        if os.path.isdir(p) and len(os.listdir(p)) > 0:
            yield p


def file_path(key):
    """key = 'FA/5FBD...' or 'FA5FBD...' (64 hex)."""
    k = key.replace('/', '').upper()
    for r in extract_root():
        p = os.path.join(r, k[:2], k[2:])
        if os.path.exists(p):
            return p
    return None


def all_files():
    seen = {}
    for r in extract_root():
        for p in glob.glob(os.path.join(r, '*', '*')):
            k = os.path.basename(os.path.dirname(p)) + os.path.basename(p)
            seen.setdefault(k.upper(), p)
    return seen


def owners():
    """file -> character key (setting DB owner, tools/gen_actionsets.py output); '?' = no owner."""
    o = {}
    p = os.path.join(ROOT, 'mod', 'Nioh3Training', 'Nioh3Training_actionsets.tsv')
    for ln in open(p, encoding='utf-8'):
        if ln.startswith('#'):
            continue
        f = ln.rstrip('\n').split('\t')
        if len(f) >= 4:
            o[f[0].upper()] = (f[1], int(f[2]), {int(x, 16) for x in f[3].split(',') if x})
    return o


def extract(game=GAME):
    import lfm_extract as L
    paths = {int(f[1]): f[5].lstrip('/') for f in (ln.split('\t') for ln in open(os.path.join(ROOT, 'data', 'lfm_list.tsv'), encoding='utf-8'))
             if f[0] == '01'}
    d = open(os.path.join(game, 'archive', 'archive_01.bin'), 'rb').read()
    n = 0
    for i, e in enumerate(L.entries(d)):
        if not 2000 < e[1] < 40_000_000 or i not in paths:
            continue
        try:
            b = L.blob(d, e)
        except Exception:
            continue
        if len(b) < 140 or u32(b, 0) != 16 or u32(b, 4) != 132:
            continue
        s1 = u32(b, 16)
        if not s1 or s1 % REC:
            continue
        p = os.path.join(ROOT, 'data', 'extract', 'lfm', 'archive_01', *paths[i].split('/'))
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, 'wb').write(b)
        n += 1
    print('extracted %d action files' % n)


# ---------------------------------------------------------------- one file
def parse_file(path):
    """Per-action facts of one action file (list in row order)."""
    d, st, owned = ymf.load(path)
    o1 = st[1][0]
    na = len(owned)
    out, sp0 = [], []
    for r, idx in enumerate(owned):
        o = o1 + r * REC
        ev = [struct.unpack_from('<3H', d, o + 0x40 + 6 * k) for k in range(16)]
        spawn = [(e, fr) for e, fr, ln in ev if 20000 <= e < 0xFFFF]
        sp0.append({e for e, fr in spawn if fr == 0})
        rows = [ymf.hitbox(d, st, i) for i in idx if i < st[2][1] // HB]
        win = ymf.windows(rows)
        out.append({
            'row': r, 'id': u16(d, o + 0xC), 'motion': u16(d, o + 0x20), 'next': u16(d, o + 0xE0), 'hash': u32(d, o + 0xF0),
            'block': int(u16(d, o + 0x13C) != 0xFFFF or u16(d, o + 0x142) != 0xFFFF),
            'great': int(bool(u16(d, o + 0x04) & 0x2000)),
            'nwin': len(win), 'nrows': len(rows), 'untimed': sum(h['start'] == 0xFFFF for h in rows),
            'windows': ' '.join('%d-%d' % (w['start'], w['end']) for w in win), '_spawn': spawn,
        })
    cnt = collections.Counter(e for s in sp0 for e in s)
    ambient = {e for e, c in cnt.items() if c * 10 >= na * 8}
    for m in out:
        m['spawns'] = sum(1 for e, fr in m.pop('_spawn') if not (fr == 0 and e in ambient))
        m['hits'] = 0 if (m['spawns'] or m['untimed']) else m['nwin']
        m['attack'] = int(bool(m['block'] or m['nwin'] or m['great']))
        m['fact'] = int(bool(m['block'] or m['nrows'] or m['great']))
    return out


# ---------------------------------------------------------------- layered table
def build(files):
    """files = [(key, path)] base first. Returns (moves in id order, {id: move}). Later files override an id of earlier ones."""
    by_id = collections.OrderedDict()
    for key, path in files:
        for m in parse_file(path):
            m['file'] = key[:8]
            by_id[m['id']] = m           # the more specific (later) file wins
    t = list(by_id.values())
    incoming = collections.Counter(by_id[m['next']]['id'] for m in t if m['next'] in by_id and m['next'] != m['id'])
    top = max(t, key=lambda m: incoming[m['id']])['id'] if t else None
    first = files[0]
    for m in t:
        idle = m['id'] == 0 or (m['row'] == 0 and m['file'] == first[0][:8]) or m['id'] == top or \
            (m['next'] == m['id'] and not m['attack'] and not m['nrows'] and incoming[m['id']] >= 5)
        m['kind'] = 'I' if idle else ('A' if (m['attack'] or m['nrows']) else 'M')
    for m in t:
        n = by_id.get(m['next'])
        m['link'] = int(m['next'] != 0xFFFF and m['next'] != m['id'] and not (n is not None and n['kind'] == 'I') and n is not None)
        m['loop'] = int(bool(m['link'] and n['next'] == n['id']))
    targets = {m['next'] for m in t if m['link']}
    for m in t:
        ch, lp = chain(by_id, m['id'])
        m['chain'] = '>'.join('%X' % c for c in ch) + (' loop' if lp else '')
        m['head'] = int(bool(m['link'] and m['kind'] != 'I' and m['id'] not in targets and any(by_id[c]['attack'] for c in ch)))
    return t, by_id


def chain(by_id, head, cap=8):
    out, c = [], head
    while c in by_id and len(out) < cap:
        out.append(c)
        m = by_id[c]
        if not m['link']:
            break
        if m['next'] in out:
            return out, True
        if m['loop'] and len(out) < cap:
            out.append(m['next'])
            return out, True
        c = m['next']
    return out, False


COLS = ['id', 'file', 'row', 'motion', 'kind', 'attack', 'block', 'great', 'hits', 'nwin', 'spawns', 'untimed', 'nrows', 'next', 'link', 'loop', 'head',
        'chain', 'windows']


def fmt(m, c):
    v = m[c]
    return '0x%X' % v if c in ('id', 'next') else str(v)


def summary(t):
    return {'actions': len(t), 'attack_block': sum(m['block'] for m in t), 'attacks': sum(m['attack'] for m in t),
            'great': sum(m['great'] for m in t), 'hits_known': sum(1 for m in t if m['attack'] and m['hits'] > 0),
            'links': sum(m['link'] for m in t), 'sequences': sum(m['head'] for m in t),
            'kinds': '%d/%d/%d' % tuple(sum(m['kind'] == k for m in t) for k in 'AMI')}


# ---------------------------------------------------------------- binding from the autobind sweep logs
def log_paths():
    out = []
    for base in (ROOT, os.path.join(os.path.dirname(ROOT), 'nioh3-engine')):
        out += sorted(glob.glob(os.path.join(base, 'captures', 'pc2-autobind', 'run*', 'Nioh3Training.log')))
        if out:
            break
    return out


def roster_names():
    names = {}
    for base in (ROOT, os.path.join(os.path.dirname(ROOT), 'nioh3-engine')):
        for f in ('roster.tsv', 'roster13.tsv'):
            p = os.path.join(base, 'captures', 'pc2-autobind', f)
            if os.path.exists(p):
                for ln in open(p, encoding='utf-8'):
                    x = ln.rstrip('\n').split('\t')
                    if len(x) >= 3 and x[1].isdigit():
                        names.setdefault(int(x[1]), x[2])
    return names


class Rows:
    """Row (id, hash) lists of every extracted action file, for replaying the runtime block scan."""

    def __init__(self):
        self.own = owners()
        self.known = set().union(*[h for _, _, h in self.own.values()])
        self.files = {}
        for k, p in all_files().items():
            d = open(p, 'rb').read()
            o, s = u32(d, 12), u32(d, 16)
            self.files[k] = [(u16(d, o + r * REC + 0xC), u32(d, o + r * REC + 0xF0)) for r in range(s // REC)]

    def scan(self, rows, r0):
        """autobind_game.h AbScan inside one file: walk both ways, a row counts when its hash is a known action-file hash, 6 misses stop."""
        hs = set()
        for dr in (-1, 1):
            bad, k = 0, (1 if dr < 0 else 0)
            while bad < 6:
                r = r0 + dr * k
                k += 1
                if r < 0 or r >= len(rows):
                    break
                h = rows[r][1]
                if h not in (0, 0xFFFFFFFF) and h in self.known:
                    bad = 0
                    hs.add(h)
                else:
                    bad += 1
        return hs

    def match(self, hs):
        """autobind.h AbMatch: best / runner-up Jaccard % over all files."""
        best, second = 0, 0
        for c, n, ids in self.own.values():
            inter = len(hs & ids)
            uni = len(hs) + len(ids) - inter
            p = inter * 100 // uni if uni else 0
            if p > best:
                best, second = p, best
            elif p > second:
                second = p
        return best, second

    def ident(self, aid, n, pct, pct2):
        res = []
        for k, rows in self.files.items():
            for r, (i, h) in enumerate(rows):
                if i == aid:
                    hs = self.scan(rows, r)
                    if len(hs) == n and self.match(hs) == (pct, pct2):
                        res.append(k)
                        break
        return res


BLK = re.compile(r'chr (\d+): block \d+ at \S+ \(row of action 0x([0-9A-F]+)\): (\d+) rows -> (\S+) match (\d+)% \(runner-up (\d+)%\)(?: file ([0-9A-F]{64}))?')


def log_binding(rows=None):
    """chr -> ordered list of files (base / shared first, own file last) from the sweep logs."""
    blocks = collections.defaultdict(list)
    for p in log_paths():
        for ln in open(p, encoding='utf-8', errors='replace'):
            m = BLK.search(ln)
            if m:
                b = (int(m.group(2), 16), int(m.group(3)), m.group(4), int(m.group(5)), int(m.group(6)), m.group(7))
                if b not in blocks[int(m.group(1))]:
                    blocks[int(m.group(1))].append(b)
    rows = rows or Rows()
    out, notes = {}, {}
    for chr_, bl in blocks.items():
        files, why = [], []
        for aid, n, name, pct, pct2, f in bl:
            if f and pct == 100:
                c = [f.upper()]           # exact hash-set match: the named file (older sweeps also named < 100 % matches: replayed instead)
            elif n == 0:
                continue
            else:
                c = rows.ident(aid, n, pct, pct2)
            if len(c) == 1:
                if c[0] not in files:
                    files.append(c[0])
            else:
                why.append('block 0x%X/%d: %d candidate files' % (aid, n, len(c)))
        # order: CP_HUMAN_COMMON (shared base) first, then the rest (weapon / own files); later wins on a duplicate id
        files.sort(key=lambda k: (0 if rows.own.get(k, ('',))[0] == 'CP_HUMAN_COMMON' else 1, -len(rows.files.get(k, []))))
        if files:
            out[chr_] = files
        notes[chr_] = '; '.join(why)
    return out, notes


def table_for(files):
    return build([(k, file_path(k)) for k in files])


# ---------------------------------------------------------------- hand headers (validation only)
def hand_headers():
    mv = collections.defaultdict(dict)
    for m in re.finditer(r'\{(\d+)u, 0x([0-9A-F]+)u, "((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)", (\'.\'|0), (\d+)\}',
                         open(os.path.join(ROOT, 'mod', 'boss_moves.h'), encoding='utf-8').read()):
        mv[int(m.group(1))][int(m.group(2), 16)] = {'boss': m.group(3), 'label': m.group(4), 'kind': m.group(5).strip("'") if m.group(5) != '0' else ''}
    facts = collections.defaultdict(dict)
    for m in re.finditer(r'\{(\d+)u, 0x([0-9A-F]+)u, (\d+), (\d+), (\d+)\}', open(os.path.join(ROOT, 'mod', 'boss_move_facts.h'), encoding='utf-8').read()):
        facts[int(m.group(1))][int(m.group(2), 16)] = {'hits': int(m.group(3)), 'great': int(m.group(4)), 'attack': int(m.group(5))}
    nxt = collections.defaultdict(dict)
    for m in re.finditer(r'\{(\d+)u, 0x([0-9A-F]+)u, 0x([0-9A-F]+)u, (\d)\}', open(os.path.join(ROOT, 'mod', 'boss_sequences.h'), encoding='utf-8').read()):
        nxt[int(m.group(1))][int(m.group(2), 16)] = (int(m.group(3), 16), int(m.group(4)))
    return mv, facts, nxt


def hand_seq_heads(chr_, mv, nxt):
    """Mod/Nioh3BossRematch training_bosses_menu.h SeqHead / SeqCount over the hand headers (incl. its Yoritomo hidden heads)."""
    links = nxt.get(chr_, {})
    unsafe = {0x4E3B, 0x4E3C} if chr_ == 16102 else set()

    def is_atk(m):
        return m['kind'] in ('A', 'S') or 'attack-family' in m['label'] or 'chain start' in m['label']

    heads = []
    for i, m in mv.get(chr_, {}).items():
        if i not in links or i in (0, 2) or any(n == i for n, _ in links.values()):
            continue
        if chr_ == 16102 and i in (0x4E67, 0x4E70, 0x4E8C, 0x4E76, 0x4E7D):
            continue
        ids, c = [], i
        while len(ids) < 8:
            ids.append(c)
            if c not in links:
                break
            n, lp = links[c]
            if n in ids:
                break
            c = n
            if lp:
                ids.append(c)
                break
        if any(x in unsafe for x in ids):
            continue
        if any(is_atk(mv[chr_][x]) for x in ids if x in mv[chr_]):
            heads.append(i)
    return sorted(heads)


def compare(chr_, t, by_id, mv, facts, nxt):
    c = collections.Counter()
    bad = []
    hm = mv.get(chr_, {})
    for i in hm:
        c['ids_agree' if i in by_id else 'ids_hand_only'] += 1
    c['ids_auto_only'] = sum(1 for m in t if hm and m['id'] not in hm)
    hf = facts.get(chr_, {})
    for i, f in hf.items():
        m = by_id.get(i)
        if not m or not m['fact']:
            c['fact_hand_only'] += 1
            bad.append('fact:%X' % i)
            continue
        for k, mk in (('attack', 'attack'), ('great', 'great'), ('hits', 'hits')):
            if m[mk] == f[k]:
                c[k + '_ok'] += 1
            else:
                c[k + '_bad'] += 1
                bad.append('%s:%X %d/%d' % (k, i, f[k], m[mk]))
    if hf:
        c['fact_auto_only'] = sum(1 for m in t if m['fact'] and m['id'] not in hf)
    hn = nxt.get(chr_, {})
    for i, (n, lp) in hn.items():
        m = by_id.get(i)
        if m and m['link'] and m['next'] == n and m['loop'] == lp:
            c['next_ok'] += 1
        else:
            c['next_bad'] += 1
            bad.append('next:%X %X/%s' % (i, n, ('%X' % m['next']) if m else '-'))
    if hn:
        c['next_auto_only'] = sum(1 for m in t if m['link'] and m['id'] not in hn)
    # attack vs non-attack against the hand kinds (A / S / static attack-family row = attack, M / I = not; T / L / unclassified: skipped)
    for i, h in hm.items():
        m = by_id.get(i)
        if not m:
            continue
        ha = h['kind'] in ('A', 'S') or (not h['kind'] and h['label'].startswith('static table: attack-family'))
        hn_ = h['kind'] in ('M', 'I')
        if not ha and not hn_:
            c['kind_skip'] += 1
        elif ha == (m['kind'] == 'A'):
            c['kind_ok'] += 1
        else:
            c['kind_bad'] += 1
            bad.append('kind:%X %s/%s' % (i, h['kind'] or '0', m['kind']))
    return c, bad


def validate(args):
    import gen_boss_move_facts as gbf
    hand_bind = '--hand-binding' in args  # bind every hand-listed chr with the hand table: checks the derivation alone, not the binding
    mv, facts, nxt = hand_headers()
    rows = Rows()
    bind, _ = log_binding(rows)
    chrs = sorted(set(mv) | set(facts) | set(nxt))
    print('chr\tboss\tbinding\tfiles\tactions\tattack_block\tattacks\tsequences(auto/hand menu)\tids agree/hand-only/auto-only\t'
          'facts attack ok/bad\tgreat ok/bad\thits ok/bad\tfacts hand-only/auto-only\tnext ok/bad/auto-only\tkind ok/bad/skip\tfirst diffs')
    tot = collections.Counter()
    for chr_ in chrs:
        name = next(iter(mv[chr_].values()))['boss'] if mv.get(chr_) else gbf.NAMES.get(chr_, '?')
        if chr_ in bind and not (hand_bind and chr_ in gbf.BOSSES):
            files, how = bind[chr_], 'runtime log'
        elif chr_ in gbf.BOSSES:
            files, how = [gbf.BOSSES[chr_].replace('/', '').upper()], 'hand table (validation only)'
        else:
            print('%d\t%s\tnone\t-\t(no action file: not in any sweep log, not in the hand table)' % (chr_, name))
            continue
        if not all(file_path(k) for k in files):
            print('%d\t%s\t%s\t%s\t(action file not extracted)' % (chr_, name, how, files))
            continue
        t, by_id = table_for(files)
        s = summary(t)
        c, bad = compare(chr_, t, by_id, mv, facts, nxt)
        tot.update(c)
        hs = hand_seq_heads(chr_, mv, nxt) if nxt.get(chr_) else []
        print('\t'.join(map(str, [chr_, name, how, '+'.join(k[:8] for k in files), s['actions'], s['attack_block'], s['attacks'],
                                  '%d/%s' % (s['sequences'], len(hs) if nxt.get(chr_) else '-'),
                                  '%d/%d/%d' % (c['ids_agree'], c['ids_hand_only'], c['ids_auto_only']),
                                  '%d/%d' % (c['attack_ok'], c['attack_bad']), '%d/%d' % (c['great_ok'], c['great_bad']),
                                  '%d/%d' % (c['hits_ok'], c['hits_bad']), '%d/%d' % (c['fact_hand_only'], c['fact_auto_only']),
                                  '%d/%d/%d' % (c['next_ok'], c['next_bad'], c['next_auto_only']),
                                  '%d/%d/%d' % (c['kind_ok'], c['kind_bad'], c['kind_skip']), ' '.join(bad[:8]) or '-'])))
    print('TOTAL\t' + ' '.join('%s=%d' % kv for kv in sorted(tot.items())))
    return 0


def sweep(outdir):
    rows = Rows()
    bind, why = log_binding(rows)
    names = roster_names()
    os.makedirs(outdir, exist_ok=True)
    S = ['chr', 'boss', 'files', 'owners', 'actions', 'attack_block', 'attacks', 'great', 'hits_known', 'links', 'sequences', 'kinds A/M/I', 'note']
    with open(os.path.join(outdir, 'summary.tsv'), 'w', encoding='utf-8', newline='\n') as fs, \
            open(os.path.join(outdir, 'moves.tsv'), 'w', encoding='utf-8', newline='\n') as fm:
        fs.write('\t'.join(S) + '\n')
        fm.write('\t'.join(['chr', 'boss'] + COLS) + '\n')
        for chr_ in sorted(set(bind) | set(why), key=lambda c: names.get(c, str(c))):
            nm = names.get(chr_, '?')
            files = bind.get(chr_, [])
            if not files:
                fs.write('%d\t%s\t-\t-\t0\t\t\t\t\t\t\t\t%s\n' % (chr_, nm, why.get(chr_) or 'no block resolved'))
                continue
            t, by_id = table_for(files)
            s = summary(t)
            fs.write('\t'.join(map(str, [chr_, nm, '+'.join(k[:8] for k in files), '+'.join(rows.own.get(k, ('?',))[0] for k in files),
                                         s['actions'], s['attack_block'], s['attacks'], s['great'], s['hits_known'], s['links'], s['sequences'],
                                         s['kinds'], why.get(chr_, '')])) + '\n')
            for m in sorted(t, key=lambda m: m['id']):
                if m['kind'] != 'I' and (m['attack'] or m['link'] or m['head']):
                    fm.write('\t'.join([str(chr_), nm] + [fmt(m, c) for c in COLS]) + '\n')
    print('wrote', os.path.join(outdir, 'summary.tsv'), os.path.join(outdir, 'moves.tsv'))


def main(argv):
    if '--check' in argv:  # known Yoritomo facts (notes/yoritomo-hits-and-unblockable.md), from the file alone
        t, by_id = table_for(['FA5FBD421BF96B8514DA056584B9507A22DC7956B1793995A113FBA714CFFA54'])
        s = summary(t)
        assert s['actions'] == 173 and s['attack_block'] == 88, s
        assert by_id[0x4E6E]['hits'] == 2 and by_id[0x4E6E]['great'] and by_id[0x4E68]['hits'] == 3 and not by_id[0x4E68]['great']
        assert by_id[0x4E5C]['chain'] == '4E5C>4E60>4E61 loop'
        print('check ok:', s)
        return 0
    if '--extract' in argv:
        extract()
        return 0
    if '--validate' in argv:
        return validate(argv)
    if '--sweep' in argv:
        od = argv[argv.index('--outdir') + 1] if '--outdir' in argv else os.path.join(ROOT, 'notes', 'auto-movelist')
        sweep(od)
        return 0
    files = [argv[i + 1].replace('/', '').upper() for i, a in enumerate(argv) if a == '--file']
    if '--chr' in argv:
        chr_ = int(argv[argv.index('--chr') + 1])
        bind, why = log_binding()
        if chr_ not in bind:
            print('chr %d: no resolved action file in the sweep logs (%s)' % (chr_, why.get(chr_, 'never spawned')), file=sys.stderr)
            return 1
        files = bind[chr_]
    if not files:
        print(__doc__)
        return 1
    t, by_id = table_for(files)
    print('# files: %s  summary: %s' % ('+'.join(k[:8] for k in files), summary(t)))
    print('\t'.join(COLS))
    for m in t:
        if '--all-rows' in argv or m['kind'] == 'A' or m['link']:
            print('\t'.join(fmt(m, c) for c in COLS))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
