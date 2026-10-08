#!/usr/bin/env python3
r"""Build notes\boss-text-index.csv from D:\nioh3-re\text_en.tsv / text_ja.tsv (run text_dump.py first).

py -I re\scripts_re\boss_text_index.py
One row per (source csv row, boss atom). Columns: source,row,boss_text,atom,name_ids,name_en,name_ja,
mission_text,mission_name_ids,objective_ids,related_counts,related (group:id:text, <=12).
name_ids = ids in group 1 (names) whose English text equals the atom; mission/objective ids = exact
English matches in group 7 (missions). related = other strings (items g3/4/5/13, scrolls/rewards g11/28,
titles g20, lore g36, ...) containing the atom name; related_counts = per-group counts of ALL hits.
"""
import csv, os, re, collections, unicodedata

OUT = r'D:\nioh3-re'
NOTES = 'notes'
BS = chr(92)
MARK = re.compile(r'\^[0-9A-F]{2}~[^~]*~|<[^>]*>')


def unesc(s):
    return s.replace(BS + 'n', '\n').replace(BS + 't', '\t').replace(BS + BS, BS)


def clean(s):
    return MARK.sub('', unesc(s)).strip()


def norm(s):
    s = unicodedata.normalize('NFKD', s.lower())
    s = ''.join(c for c in s if not unicodedata.combining(c))
    return re.sub(r'[^a-z0-9]+', ' ', s).strip()


def load(lang):
    d = collections.defaultdict(dict)
    with open(os.path.join(OUT, 'text_%s.tsv' % lang), encoding='utf-8') as f:
        next(f)
        for ln in f:
            p = ln.rstrip('\n').split('\t', 3)
            if len(p) == 4:
                d[int(p[0])][int(p[1])] = p[3]
    return d


def atoms(text):
    """boss_text -> candidate names (without form qualifiers)."""
    t = re.sub(r'\(names from[^)]*\)', '', text)
    out = []
    # "Group label (A; B; C)" -> inner names only
    t = re.sub(r"[^;,(]*\(([^()]*;[^()]*)\)", lambda m: m.group(1), t)
    for part in re.split(r';|,| & | and ', t):
        part = part.strip()
        if not part or part.lower().startswith(('none', 'unverified', 'boss waves')):
            continue
        m = re.match(r'^(.*?)\s*\((.*?)\)\s*$', part)
        if m:
            out.append((m.group(1).strip(), m.group(1).strip()))
            alt = m.group(2).strip()
            if re.match(r'^(aka\s+)?[A-Z][\w\'-]+( [A-Z][\w\'-]+)*$', alt) and not re.match(r'^(monster|human|final|2nd|upgraded|mounted|horse|yokai)', alt, re.I):
                out.append((m.group(1).strip(), alt.replace('aka ', '')))
        else:
            out.append((part, part))
    return out


def main():
    en, ja = load('en'), load('ja')
    gnames = collections.defaultdict(list)       # norm text -> [id] in group 1
    for i, s in en[1].items():
        gnames[norm(clean(s))].append(i)
    gmis = collections.defaultdict(list)         # norm text -> [id] in group 7
    for i, s in en[7].items():
        gmis[norm(clean(s))].append(i)
    inv = []                                    # all en strings (group,id,clean text lower-norm) for 'related'
    for g, d in en.items():
        if g in (1, 6, 8, 14, 18, 38, 2, 24):   # names themselves + dialogue/system noise
            continue
        for i, s in d.items():
            inv.append((g, i, norm(clean(s)), clean(s)))

    rows_out = []

    def related(name):
        n = norm(name)
        if len(n) < 4:
            return '', ''
        pat = re.compile(r'(?:^| )' + re.escape(n) + r'(?: |$)')
        hits = [(g, i, t) for g, i, nt, t in inv if pat.search(nt)]
        cnt = collections.Counter(g for g, _, _ in hits)
        pri = {3: 0, 5: 0, 4: 0, 13: 1, 11: 1, 28: 1, 20: 2, 17: 2, 31: 2, 22: 2, 7: 3, 36: 4}
        hits.sort(key=lambda h: (pri.get(h[0], 5), h[0], h[1]))
        return ';'.join('g%d=%d' % kv for kv in sorted(cnt.items())), ' || '.join(
            '%d:%d:%s' % (g, i, t.replace('\n', ' ')[:60]) for g, i, t in hits[:12])

    def ids_for(table, text):
        out = []
        for cand in re.split(r'\s*/\s*', text):
            out += table.get(norm(cand), [])
        return out

    def emit(src, rowno, boss_text, mission_text, obj_text, mission_ids, obj_ids):
        for orig, name in atoms(boss_text):
            ids = gnames.get(norm(name), []) or gnames.get(norm(orig), [])
            if not ids:  # loose: every word of the atom inside one name entry
                w = norm(name).split()
                if w:
                    ids = [i for k, v in gnames.items() if all(x in k.split() for x in w) for i in v][:3]
            nm = en[1].get(ids[0], '') if ids else ''
            nj = ja[1].get(ids[0], '') if ids else ''
            counts, rel = related(clean(nm) if nm else name)
            rows_out.append([src, rowno, boss_text, name, ';'.join(map(str, ids)), clean(nm), clean(nj),
                             mission_text, ';'.join(map(str, mission_ids)), ';'.join(map(str, obj_ids)), counts, rel])

    with open(os.path.join(NOTES, 'boss-names-online.csv'), encoding='utf-8', newline='') as f:
        for n, r in enumerate(csv.DictReader(f), 2):
            obj = r['mission']
            emit('boss-names-online', n, r['english_name'], '', obj, [], ids_for(gmis, obj))
    with open(os.path.join(NOTES, 'boss-per-mission-online.csv'), encoding='utf-8', newline='') as f:
        for n, r in enumerate(csv.DictReader(f), 2):
            mt = r['mission_name']
            m = re.match(r'^(.*?)\s*\((?:obj|objs):\s*(.*)\)\s*$', mt)
            main_t, obj_t = (m.group(1), m.group(2)) if m else (mt, '')
            mids = ids_for(gmis, main_t)
            oids = ids_for(gmis, obj_t) if obj_t else []
            emit('boss-per-mission-online', n, r['bosses'], main_t, obj_t, mids, oids)

    with open(os.path.join(NOTES, 'boss-text-index.csv'), 'w', encoding='utf-8', newline='') as f:
        w = csv.writer(f)
        w.writerow(['source', 'row', 'boss_text', 'atom', 'name_ids', 'name_en', 'name_ja', 'mission_text',
                    'mission_name_ids', 'objective_ids', 'related_counts', 'related'])
        w.writerows(rows_out)
    res = sum(1 for r in rows_out if r[4])
    print(len(rows_out), 'rows,', res, 'with a name id')


if __name__ == '__main__':
    main()
