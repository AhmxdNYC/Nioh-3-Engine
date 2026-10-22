"""Run tools/yoritomo_move_facts.py on every bound character action file and print coverage stats (offline, read-only).
  py -I tools/allboss_coverage.py > notes/all-bosses-move-facts-coverage.tsv
"""
import csv, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import yoritomo_move_facts as ymf

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
BASES = [os.path.join(ROOT, 'data', 'extract', 'lfm', 'archive_01'),
         os.path.join(ROOT, '..', 'nioh3-engine', 'data', 'extract', 'lfm', 'archive_01')]
# boss name (notes/boss-master.csv) -> action-file character (notes/boss-action-files-22709.csv). Name matches by hand; guesses flagged.
BOSS = {
 'Minamoto no Yoritomo': 'CE_YORITOMO_00', 'Takeda Shingen': 'CE_SHINGEN_00', 'Hino-enma': 'CE_HINOENMA_00',
 'Yamagata Masakage': 'CN_MASAKAGE', 'Ippon-Datara': 'CE_IPPONDATARA_00', 'Otoroshi': 'CE_OTOROSHI_00',
 'Nuribotoke': 'CE_NURIBOTOKE_00', 'Red Kappa': 'CE_KAPPA_00', 'Takasugi Shinsaku': 'CN_SHINSAKU', 'Kamaitachi': 'CE_KAMAITACHI_00',
 'Baba Nobuharu': 'CE_NOBUHARU_00', 'Mikoshi Nyudo': 'CE_MIKOSHI_00', 'Great Tengu': 'CE_OTENGU_00',
 'Minamoto no Yoshitsune': 'CE_YOSHITSUNE_00', 'Enki': 'CE_ENKI_00', 'Tokugawa Yoshinobu': 'CE_YOSHINOBU_00',
 'Ryomen Sukuna': 'CE_RYOUMENSUKUNA_00', 'Himiko': 'CN_HIMIKO', 'Hiruko': 'CN_HIRUKO', 'Honda Tadakatsu': 'CN_TADAKATSU',
 'Hattori Hanzo': 'CN_HANZO_MASANARI', 'Okita Soji': 'CN_SOUJI', 'Yagyu Munenori': 'CN_MUNENORI', 'Tokugawa Kunimatsu': 'CN_KUNIMATU',
 'Jakotsu-baba': 'KY_JAKOTSUBA_00', 'Kasha': 'CE_KASYA_00', 'Kyokotsu': 'CE_KYOUKOTSU_00', 'Mezuki': 'CE_MEZUKI_00',
 'Nuppeppo': 'CE_NUPPEFUHOFU_00', 'Onmoraki': 'CE_ONMORAKI_00', 'Onryoki': 'CE_ONRYOKI_00', 'Tsurara-Onna': 'CE_TSURARAONNA_00',
 'Yasha': 'CE_YASHA_00', 'Gaki': 'CE_GAKI_00', 'Gaki Chief': 'CE_GAKIDAISHO_00', 'Joro-gumo': 'CE_ARACHNE_00',
 'One-eyed Oni (guess: hitotume)': 'CE_HITOTUME_00',
}
SPECIAL = {'CE_IPPONDATARA_00': 'FD/D80A7992FD2F3EB4AE582BAE0A06A071449E2F89C91978B13471449D28C062'}


def find(rel):
    for b in BASES:
        p = os.path.join(b, *rel.split('/'))
        if os.path.exists(p):
            return p


def stats(path):
    rows = list(ymf.facts(path))
    s = dict(actions=len(rows), moves=0, hits=0, unk=0, unbl=0, simul=0, zero=0, over8=0, ovl=0, bad=0, maxh=0)
    for x in rows:
        if not (x['attack'] or x['n_rows'] or x['unblockable']):
            continue
        s['moves'] += 1
        h = x['hits']
        if h == '':
            s['unk'] += 1
        else:
            if h > 0:
                s['hits'] += 1
            else:
                s['zero'] += 1  # own rows but no powered timed window (untimed / zero power)
            s['maxh'] = max(s['maxh'], h)
            if h > 8:
                s['over8'] += 1
        s['unbl'] += x['unblockable']
        prev = None
        sp = []
        for w in x['windows'].split():
            a, b = map(int, w.split('(')[0].split('-'))
            if sp and a <= sp[-1][1]:
                sp[-1][1] = max(sp[-1][1], b)
            else:
                sp.append([a, b])
        if h != '' and len(sp) < h:
            s['simul'] += 1  # windows of different groups that overlap in time: may be 1 hit by 2 hitboxes (over-count risk)
        for w in x['windows'].split():
            a, b = w.split('(')[0].split('-')
            a, b = int(a), int(b)
            if b < a:
                s['bad'] += 1
            if prev is not None and a <= prev:
                s['ovl'] += 1
            prev = b
    return s


def main():
    csvrows = {r['char_name']: r for r in csv.DictReader(open(os.path.join(ROOT, 'notes', 'boss-action-files-22709.csv'), encoding='utf8'))}
    inv = {v: k for k, v in BOSS.items()}
    names = list(BOSS.items()) + [(c, c) for c in csvrows if c not in BOSS.values()]
    print('\t'.join('boss char file moves with_hits unknown zero_hit unbl max_hits over8 overlap_moves simul_moves badwin status'.split()))
    for boss, ch in names:
        rel = SPECIAL.get(ch) or (csvrows[ch]['archive_01_file'] if ch in csvrows else None)
        p = find(rel) if rel else None
        if not p:
            print('\t'.join([boss, ch, 'n'] + ['-'] * 11 + ['no file']))
            continue
        try:
            s = stats(p)
            st = 'OK' if not (s['over8'] or s['bad']) else 'WARN(>8 hits)'
            print('\t'.join(map(str, [boss, ch, 'y', s['moves'], s['hits'], s['unk'], s['zero'], s['unbl'], s['maxh'], s['over8'], s['ovl'], s['simul'], s['bad'], st])))
        except Exception as e:
            print('\t'.join([boss, ch, 'y'] + ['-'] * 11 + ['ERROR ' + repr(e)[:60]]))


main()
