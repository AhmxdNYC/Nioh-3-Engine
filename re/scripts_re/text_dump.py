#!/usr/bin/env python3
r"""Dump every LFM archive_17 text string for English + Japanese (stdlib only, read-only).

py -I re\scripts_re\text_dump.py [--lfm data\extract\lfm\archive_17] [--out D:\nioh3-re]
Writes text_en.tsv / text_ja.tsv: group<TAB>id<TAB>lang<TAB>text (backslash, TAB, CR, LF escaped as
\\ \t \r \n; ^13~UNUSED~ and empty slots skipped). group = id>>22 (1..42); one group = the same id set
in 23 language files. Record format: repeated {u32 id, u32 nchars_incl_NUL, UTF-16LE chars + NUL}.
Language is not in the file: en = English-stopword ratio >0.1 and most non-empty strings; ja = most kana.
Importable: load_groups(root) -> {group: {'en': {id: text}, 'ja': {id: text}, ...}}.
"""
import os, re, struct, sys, collections

SW = set('the and to of you your a in is for with that this it be on are as at from will can his her he she by or an not have has was their into'.split())
BS = chr(92)


def parse(b):
    out = []
    p = 0
    n_b = len(b)
    while p + 8 <= n_b:
        i, n = struct.unpack_from('<II', b, p)
        if n == 0 or n > 100000 or p + 8 + 2 * n > n_b:
            break
        s = b[p + 8:p + 8 + 2 * n].decode('utf-16le', 'replace')
        if s[-1:] != chr(0):
            break
        out.append((i, s[:-1]))
        p += 8 + 2 * n
    if not out or any(b[p:]):  # trailing non-zero junk => not a text file
        return None
    return out


def en_ratio(r):
    sc = tot = 0
    for _, s in r:
        for w in re.findall(r"[a-z']+", s.lower()):
            tot += 1
            sc += w in SW
    return sc / (tot or 1)


def kana(r):
    return sum('\u3040' <= ch <= '\u30ff' for _, s in r for ch in s)


def used(s):
    return bool(s) and not s.startswith('^13~UNUSED')


def nonempty(r):
    return sum(1 for _, s in r if used(s))


def cjk_heavy(r):
    t = ''.join(s for _, s in r)
    return sum(0x370 <= ord(c) < 0x2000 and not 0xc0 <= ord(c) < 0x250 or ord(c) >= 0x2e80 for c in t) > 0.02 * (len(t) or 1)


def words(r):
    return [w for _, s in r for w in re.findall(r"[a-z']+", s.lower())]


def load_groups(root):
    g = collections.defaultdict(list)
    for d, _, fs in os.walk(root):
        for x in fs:
            fp = os.path.join(d, x)
            r = parse(open(fp, 'rb').read())
            if r:
                g[min(i for i, _ in r) >> 22].append((fp, r))
    # pass 1: confident English (stopword ratio) in text-heavy groups -> vocabulary
    vocab = collections.Counter()
    for files in g.values():
        for fp, r in files:
            if en_ratio(r) > 0.2:
                vocab.update(words(r))
    vocab = {w for w, c in vocab.items() if c >= 2 and len(w) > 2}
    # pass 2: per group English = best vocab hit-rate (within 85% of the best), then most non-empty strings
    res = {}
    for grp, files in sorted(g.items()):
        lat = [f for f in files if not cjk_heavy(f[1])]
        cand = [f for f in lat if en_ratio(f[1]) > 0.1]  # stopword ratio is authoritative
        if not cand:  # name-only groups: fall back to vocabulary hit-rate
            sc = []
            for f in lat:
                w = words(f[1])
                sc.append((sum(x in vocab for x in w) / len(w) if len(w) >= 20 else 0, f))
            best = max([s for s, _ in sc] or [0])
            cand = [f for s, f in sc if best > 0.3 and s >= 0.85 * best]
        en = max(cand, key=lambda f: nonempty(f[1])) if cand else None
        ja = max(files, key=lambda f: kana(f[1]))
        has_ja = kana(ja[1]) > 0
        res[grp] = {'en': dict(en[1]) if en else {}, 'ja': dict(ja[1]) if has_ja else {},
                    'en_file': en[0] if en else '', 'ja_file': ja[0] if has_ja else '',
                    'nfiles': len(files), 'nrec': len(files[0][1])}
    return res


def esc(s):
    return s.replace(BS, BS + BS).replace('\t', BS + 't').replace('\r', BS + 'r').replace('\n', BS + 'n')


def main():
    a = sys.argv[1:]
    root = a[a.index('--lfm') + 1] if '--lfm' in a else os.path.join('data', 'extract', 'lfm', 'archive_17')
    out = a[a.index('--out') + 1] if '--out' in a else r'D:\nioh3-re'
    G = load_groups(root)
    os.makedirs(out, exist_ok=True)
    for lang in ('en', 'ja'):
        n = 0
        with open(os.path.join(out, 'text_%s.tsv' % lang), 'w', encoding='utf-8', newline='\n') as f:
            f.write('group\tid\tlang\ttext\n')
            for grp, d in G.items():
                for i, s in sorted(d[lang].items()):
                    if used(s):
                        f.write('%d\t%d\t%s\t%s\n' % (grp, i, lang, esc(s)))
                        n += 1
        print(lang, n, 'strings')
    for grp, d in G.items():
        print('group', grp, 'records', d['nrec'], 'en', sum(map(used, d['en'].values())),
              'ja', sum(map(used, d['ja'].values())), 'files', d['nfiles'])


if __name__ == '__main__':
    main()
