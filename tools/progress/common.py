"""common.py - paths and the game image shared by every script in tools/progress (stdlib only).

Paths (all relative to the repository root, ROOT):
  data/                  published inputs (addresses, sizes, our own names, our own profile measurements)
  data/local/            NOT published (gitignored): your game dump and tables generated from it
  docs/progress/         chart outputs
  symbols/symbols.tsv    the symbol database (+ symbols/incoming/*.tsv if you have pending rows)

The game image (a memory-layout dump of Nioh3.exe, file offset == RVA, see dump_nioh3.py) is found as:
  --dump PATH  >  environment variable NIOH3_DUMP  >  <local dir>/Nioh3.dump.exe
The local dir is data/local unless NIOH3_LOCAL points elsewhere. Chart outputs go to docs/progress unless
--out DIR or NIOH3_PROGRESS_OUT says otherwise.
"""
import hashlib, os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
DATA = os.path.join(ROOT, 'data')
PROFILES = os.path.join(DATA, 'profiles')
SYMS = os.path.join(ROOT, 'symbols', 'symbols.tsv')
INCOMING_DIR = os.path.join(ROOT, 'symbols', 'incoming')


def _arg(flag):
    a = sys.argv
    return a[a.index(flag) + 1] if flag in a and a.index(flag) + 1 < len(a) else None


LOCAL = os.path.abspath(os.environ.get('NIOH3_LOCAL') or os.path.join(DATA, 'local'))
DUMP = os.path.abspath(_arg('--dump') or os.environ.get('NIOH3_DUMP') or os.path.join(LOCAL, 'Nioh3.dump.exe'))
OUT = os.path.abspath(_arg('--out') or os.environ.get('NIOH3_PROGRESS_OUT') or os.path.join(ROOT, 'docs', 'progress'))
STRINGS = os.path.join(LOCAL, 'strings.tsv')


def data(*p):
    return os.path.join(DATA, *p)


def meta():
    """data/progress_meta.tsv -> {key: value}"""
    out = {}
    for l in open(data('progress_meta.tsv'), encoding='utf-8'):
        f = l.rstrip('\n').split('\t')
        if len(f) >= 2 and not f[0].startswith('#') and f[0] != 'key':
            out[f[0]] = f[1]
    return out


def pe_identity(b):
    """(TimeDateStamp, SizeOfImage, sha256 of .pdata) of a memory-layout dump; .pdata holds only RVAs, so it is
    identical in every dump of the same game build whatever the load address."""
    e = struct.unpack_from('<I', b, 0x3c)[0]
    ts = struct.unpack_from('<I', b, e + 8)[0]
    soi = struct.unpack_from('<I', b, e + 24 + 56)[0]
    r, sz = struct.unpack_from('<II', b, e + 24 + 112 + 3 * 8)
    return '0x%08x' % ts, '0x%x' % soi, hashlib.sha256(b[r:r + sz]).hexdigest()


_B = None


def load_dump():
    """the dump's bytes (read once per process); exits with a hint when it is missing, warns on another game build"""
    global _B
    if _B is not None:
        return _B
    if not os.path.exists(DUMP):
        sys.exit('game dump not found: %s\nMake one with  py -I tools\\progress\\dump_nioh3.py  while the game runs '
                 '(see tools\\progress\\README.md), or pass --dump PATH / set NIOH3_DUMP.' % DUMP)
    _B = open(DUMP, 'rb').read()
    try:
        m = meta()
        got = pe_identity(_B)
        want = (m.get('dump_timestamp'), m.get('dump_size_of_image'), m.get('dump_pdata_sha256'))
        if got != want:
            print('WARNING: %s is not the game build the published tables were made from\n'
                  '  dump: timestamp %s, SizeOfImage %s, .pdata sha256 %s\n'
                  '  data: timestamp %s, SizeOfImage %s, .pdata sha256 %s\n'
                  '  Every RVA in data/ and symbols/ is for that build; the charts will be wrong.'
                  % ((DUMP,) + got + want), file=sys.stderr)
    except (OSError, struct.error):
        pass
    return _B


def image_base(b):
    """ImageBase from the dump header; dump_nioh3.py writes the runtime load address there, so absolute pointers in
    .data/.rdata minus this value are RVAs"""
    e = struct.unpack_from('<I', b, 0x3c)[0]
    return struct.unpack_from('<Q', b, e + 24 + 24)[0]


def incoming_files():
    """pending symbols/incoming/*.tsv; '*-renames.tsv' last (their names replace earlier ones)"""
    if not os.path.isdir(INCOMING_DIR):
        return []
    fs = sorted(f for f in os.listdir(INCOMING_DIR) if f.endswith('.tsv'))
    return [os.path.join(INCOMING_DIR, f) for f in sorted(fs, key=lambda f: f.endswith('-renames.tsv'))]
