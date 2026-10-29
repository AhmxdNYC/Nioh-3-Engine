r"""DLC / patch readiness: which action files (= characters) the runtime autobind would bind, compose or leave unbound, with no hand table (pc2 #24151).

    py -I tools/dlc_bind_report.py NEW.tsv [--old OLD.tsv] [--csv out.csv]
    py -I tools/dlc_bind_report.py --selftest          current table as 'new', current minus 25 own files as 'old' (a pretend DLC)

NEW.tsv / OLD.tsv = tools/gen_actionsets.py output (file, char, rows, hex action-object hashes) built from a game's archive_01. Without --old every file is
reported; with --old only files not in OLD (the DLC additions). The matching is a port of mod/Nioh3Training/src/autobind.h AbMatch (Jaccard > 70 %, runner-up
>= 15 points lower or a same-family near twin) run over NEW, so a file's status = what the runtime would conclude when a boss loads it:
  BOUND     own file recognised (100 % self match, unambiguous)       -> boss binds directly
  BOUND-TWIN  near-identical file of the same character family       -> binds the top one
  COMPOSED  CP_HUMAN_* shared file (human duelist: common + weapon)   -> boss is composed of fragments of shared files (per-block containment)
  AMBIGUOUS two different families with near-identical sets           -> stays unbound, flagged
Exit code 1 when any file is AMBIGUOUS."""
import csv, os, random, sys


def load(p):
    t = []
    for ln in open(p, encoding='utf-8'):
        if ln.startswith('#'):
            continue
        f = ln.rstrip('\r\n').split('\t')
        if len(f) >= 4:
            t.append((f[0].upper(), f[1], int(f[2]), frozenset(int(x, 16) for x in f[3].split(',') if x)))
    return t


def family(c):
    if not c or c == '?':
        return None
    h, _, tail = c.rpartition('_')
    return h if h and tail.isdigit() else c


def classify(table, f):
    ids = f[3]
    best, second, sec = 0, 0, None
    top = None
    for g in table:
        inter = len(ids & g[3]); uni = len(ids) + len(g[3]) - inter
        p = inter * 100 // uni if uni else 0
        if p > best or (p == best and top is not None and len(g[3]) < len(top[3])):
            second, sec, best, top = best, top, p, g
        elif p > second:
            second, sec = p, g
    if f[1].startswith('CP_HUMAN_'):
        return 'COMPOSED', best, second
    if best <= 70:
        return 'AMBIGUOUS', best, second
    if best - second >= 15:
        return 'BOUND', best, second
    if sec is not None and second < best and family(top[1]) and family(top[1]) == family(sec[1]):
        return 'BOUND-TWIN', best, second
    return 'AMBIGUOUS', best, second


def report(new, old=None):
    have = {f[0] for f in old} if old else set()
    rows = []
    for f in new:
        if f[0] in have or len(f[3]) < 3:
            continue
        st, b, s = classify(new, f)
        rows.append((st, f[1], f[0][:8], f[2], b, s))
    return rows


def main(a):
    if '--selftest' in a:
        here = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'mod', 'Nioh3Training', 'Nioh3Training_actionsets.tsv')
        new = load(here); random.seed(24151)
        own = [f for f in new if not f[1].startswith('CP_HUMAN_') and f[1] != '?' and len(f[3]) >= 20]
        gone = {f[0] for f in random.sample(own, 25)}
        rows = report(new, [f for f in new if f[0] not in gone])
    else:
        new = load(a[0]); old = load(a[a.index('--old') + 1]) if '--old' in a else None
        rows = report(new, old)
    cnt = {}
    for r in rows:
        cnt[r[0]] = cnt.get(r[0], 0) + 1
    for r in sorted(rows):
        print('%-10s %-28s %s rows %-4d match %3d%% runner-up %3d%%' % r)
    print('TOTAL %d files: %s' % (len(rows), ', '.join('%s %d' % kv for kv in sorted(cnt.items()))))
    if '--csv' in a:
        csv.writer(open(a[a.index('--csv') + 1], 'w', newline='')).writerows([('status', 'char', 'file', 'rows', 'match', 'runner_up')] + rows)
    return 1 if cnt.get('AMBIGUOUS') else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
