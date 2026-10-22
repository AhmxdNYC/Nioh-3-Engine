"""Bind every archive_01 action file to the character setting DB(s) whose action-hash array contains its action object hashes (offline, read-only).
No hand-written boss list: action file rows +0xF0 hold u32 action object hashes (MotorCharacterSetting.CharacterActionObjectNameHashArray entries);
a setting DB is the owner of a file when (almost) all those hashes occur as u32 words in the DB bytes.
  py -I tools/auto_bind.py [game dir] > notes/boss-action-bind-auto.tsv     cols: file, rows, hashes, db_ktid, hit, score, char_name(s)
Needs data/rdb_list.tsv (RDB index) and the game's package/*.fdata; action files via tools/lfm_extract.py from archive_01.bin."""
import csv, os, struct, sys, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lfm_extract as L

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
GAME = sys.argv[1] if len(sys.argv) > 1 else 'C:/Program Files (x86)/Steam/steamapps/common/Nioh3'
REC = 0x150


def hashes(b):
    o, s = struct.unpack_from('<II', b, 12)
    return {h for h in (struct.unpack_from('<I', b, o + r * REC + 0xF0)[0] for r in range(s // REC)) if h not in (0, 0xFFFFFFFF)}


def read_entry(fh, off, csize):
    fh.seek(off)
    raw = fh.read(csize + 128)
    out, p = [], 0
    q = 0x30
    while True:
        q = raw.find(b'\x78', q)
        if q < 0:
            break
        try:
            o = zlib.decompressobj()
            out.append(o.decompress(raw[q:]))
            q = len(raw) - len(o.unused_data)
        except zlib.error:
            q += 1
    return b''.join(out)


def main():
    d = open(os.path.join(GAME, 'archive', 'archive_01.bin'), 'rb').read()
    files = {}
    for i, e in enumerate(L.entries(d)):
        if 2000 < e[1] < 40_000_000:
            try:
                b = L.blob(d, e)
                if struct.unpack_from('<I', b, 0)[0] != 16:
                    continue
                o, s = struct.unpack_from('<II', b, 12)
                if s and s % REC == 0 and s // REC > 3:
                    files[i] = (len(b) and s // REC, hashes(b))
            except Exception:
                pass
    # path names from lfm_list.tsv
    path = {int(f[1]): f[5].lstrip('/') for f in (ln.split('\t') for ln in open(os.path.join(ROOT, 'data', 'lfm_list.tsv'), encoding='utf-8')) if f[0] == '01'}
    names = {}
    for ln in open(os.path.join(ROOT, 'notes', 'boss-char-assets.csv'), encoding='utf-8'):
        pass
    for r in csv.DictReader(open(os.path.join(ROOT, 'notes', 'boss-char-assets.csv'), encoding='utf-8')):
        for part in r['asset_ids_files'].split(';'):
            if part.startswith('setting_db='):
                names.setdefault(part[11:], []).append(r['name'])
    pk = os.path.join(GAME, 'package')
    best = {i: [] for i in files}
    fhs = {}
    for ln in open(os.path.join(ROOT, 'data', 'rdb_list.tsv'), encoding='utf-8'):
        f = ln.rstrip('\n').split('\t')
        if f[0] != 'root' or f[2] != 'ObjectDatabaseFile' or not (2000 < int(f[3]) < 400000):
            continue
        fh = fhs.setdefault(f[5], open(os.path.join(pk, f[5]), 'rb'))
        try:
            b = read_entry(fh, int(f[6], 16), int(f[4]))
        except Exception:
            continue
        b = b[:int(f[3])]
        if len(b) != int(f[3]):
            continue
        words = {struct.unpack_from('<I', b, k)[0] for k in range(0, len(b) - 3, 4)}
        for i, (n, h) in files.items():
            hit = len(h & words)
            if h and hit * 2 >= len(h):
                best[i].append((hit / len(h), hit, f[1]))
    print('file\trows\thashes\tdb_ktid\thit\tscore\tchar_name')
    for i, (n, h) in sorted(files.items()):
        for sc, hit, k in sorted(best[i], reverse=True)[:3]:
            print('\t'.join(map(str, (path.get(i, i), n, len(h), k, hit, '%.2f' % sc, ','.join(names.get(k, []))))))


main()
