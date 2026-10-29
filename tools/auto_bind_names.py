"""Boss name -> action file with no hand dict: takes notes/boss-action-bind-auto.tsv (file <-> setting DB <-> character) and matches the
boss english name / enemy record key tokens (notes/boss-master.csv) to the character names, picking the character's main file (most rows).
  py -I tools/auto_bind_names.py [--eval]"""
import csv, difflib, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')


def norm(s):
    return re.sub(r'[^a-z0-9]', '', s.lower())


def chars():
    best = {}
    for r in csv.DictReader(open(os.path.join(ROOT, 'notes', 'boss-action-bind-auto.tsv'), encoding='utf-8'), delimiter='\t'):
        for c in filter(None, r['char_name'].split(',')):
            if c not in best or int(r['rows']) > best[c][0]:
                best[c] = (int(r['rows']), r['file'], r['db_ktid'], float(r['score']))
    return best


def stem(c):
    c = re.sub(r'^(CE|CN|CS|CP|KY|CL|CM|DB|ME)_', '', c)
    return norm(re.sub(r'_\d\d$|_00$|_01$|_2ND$', '', c))


def codex_keys():
    """english name -> codex keys (the game's own character keys, notes/boss-codex.csv)"""
    out = {}
    for r in csv.DictReader(open(os.path.join(ROOT, 'notes', 'boss-codex.csv'), encoding='utf-8')):
        if r['english_name']:
            out.setdefault(norm(r['english_name']), set()).add(r['key'])
    return out


def match(boss, keys, ch, cx):
    """(score, char): codex keys of the boss name vs the characters that have an action file; exact stem 1.0, spelling variants by similarity"""
    best = (0.0, '')
    for k in cx.get(norm(boss), ()):
        ks = stem(k)
        if re.search(r'TEXT|FIRST|KAGEBOSHI|OLD', k):
            continue
        for c in ch:
            s = stem(c)
            r = 1.0 if ks == s else difflib.SequenceMatcher(None, ks, s).ratio()
            if r > best[0]:
                best = (r, c)
    return best


def token_match(boss, keys, ch):
    """exact stem equality between a boss-name word / enemy record key token and a character (no fuzzy)"""
    toks = {norm(w) for w in re.split(r'[ ()-]+', boss) if len(w) > 3} | {norm(boss)}
    toks |= {norm(t) for k in keys for t in k.split('_') if len(t) > 3 and not t.isdigit()}
    return [c for c in ch if stem(c) in toks]


def resolve(boss, keys, ch, cx):
    """-> (char, how). codex-key exact/near (>=0.9) wins, then exact token equality; ambiguity (several chars) -> main one = shortest name"""
    sc, c = match(boss, keys, ch, cx)
    t = token_match(boss, keys, ch)
    if sc >= 0.9 and (not t or c in t):
        return c, 'codex'
    if t:
        return sorted(t, key=lambda x: (-len(stem(x)), len(x), x))[0], 'token'
    if sc >= 0.9:
        return c, 'codex'
    # spelling variants of the whole name (Kasha/KASYA, Kyokotsu/KYOUKOTSU, Jakotsu-baba/JAKOTSUBA): unique best >= 0.78 and a 0.08 lead
    r = sorted(((difflib.SequenceMatcher(None, norm(boss), stem(x)).ratio(), x) for x in ch), reverse=True)
    if r[0][0] >= 0.78 and r[0][0] - r[1][0] >= 0.08:
        return r[0][1], 'fuzzy'
    return None, ''


def main():
    ch = chars()
    cx = codex_keys()
    names = {}
    for r in csv.DictReader(open(os.path.join(ROOT, 'notes', 'boss-master.csv'), encoding='utf-8')):
        n = r['english_name']
        if n and len(n) > 3 and not n.endswith(('(', ')')) and not n.startswith(('generic', 'yokai', '3 yokai', '10 yokai', 'Onmyo')) and 'met as' not in n:
            names.setdefault(n, set()).add(r['enemy_record_key'])
    out = {}
    for b, keys in sorted(names.items()):
        c, how = resolve(b, keys, ch, cx)
        out[b] = (c, how)
        print('%-30s %-26s %-6s %s' % (b[:30], c or '-', how, ch[c][1] if c else ''))
    n = sum(1 for v in out.values() if v[0])
    print('bound %d of %d boss names' % (n, len(out)), file=sys.stderr)
    if '--eval' in sys.argv:  # ground truth = the old hand dict (allboss_coverage.BOSS), used only to score this resolver
        import allboss_coverage as ab
        ok = bad = miss = 0
        for b, c in ab.BOSS.items():
            got = out.get(b, (None,))[0]
            ok, bad, miss = ok + (got == c), bad + bool(got and got != c), miss + (not got)
            if got != c:
                print('EVAL', 'WRONG' if got else 'MISS ', b, '->', got, 'hand:', c, file=sys.stderr)
        print('eval vs hand dict: %d same, %d wrong, %d missed' % (ok, bad, miss), file=sys.stderr)
    return out


if __name__ == '__main__':
    main()
