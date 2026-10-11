"""make_strings.py - rebuild the game string table (strings.tsv) from YOUR OWN game dump (stdlib only).

usage: py -I tools\\progress\\make_strings.py [--dump PATH]      (regions.py runs it for you when the file is missing)

The table lists every NUL-terminated ASCII (>= 4 chars) and UTF-16LE (>= 4 chars) string in .rdata/.data/_RDATA, with
the functions that reference it. It contains the game's own text, so it is NOT published: this script scans your dump
for the strings and joins them with data\\string_xrefs.tsv (our cross references: string rva, encoding, referencing
functions, pointer slots; addresses only, made with Ghidra). Output: <local dir>\\strings.tsv (data\\local by default,
gitignored), columns  rva  enc  funcs  via  string  (tab, newline, CR escaped as \\t \\n \\r).
The scan is the same as the Ghidra script that made our table (N3Strings.java), so on the same game build the file is
byte-identical to ours.
"""
import os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402

PR = rb'[\x09\x0a\x0d\x20-\x7e]'
RX_NEXT = re.compile(PR)
RX_ASCII = re.compile(PR + b'+')
RX_U16 = re.compile(b'(?:' + PR + rb'\x00)+')


def sections(b):
    e = struct.unpack_from('<I', b, 0x3c)[0]
    ns, so = struct.unpack_from('<H', b, e + 6)[0], struct.unpack_from('<H', b, e + 20)[0]
    out = {}
    for i in range(ns):
        o = e + 24 + so + 40 * i
        vs, va = struct.unpack_from('<II', b, o + 8)
        out[b[o:o + 8].rstrip(b'\0').decode()] = (va, vs)
    return out


def scan(b):
    """-> [(rva, enc, text)] in address order, same rules as N3Strings.java"""
    sec = sections(b)
    out = []
    for name in ('.rdata', '.data', '_RDATA'):
        if name not in sec:
            continue
        va, vs = sec[name]
        buf = b[va:va + vs]
        n = len(buf)
        i = 0
        while i < n:
            m = RX_NEXT.search(buf, i)        # a non-printable byte can start neither kind of string
            if not m:
                break
            i = m.start()
            if not i & 1:                      # UTF-16LE candidate (2-aligned)
                u = RX_U16.match(buf, i)
                if u:
                    j = u.end()
                    if (j - i) // 2 >= 4 and j + 1 < n and buf[j] == 0 and buf[j + 1] == 0:
                        out.append((va + i, 'u16', buf[i:j:2].decode('latin-1')))
                        i = j + 2
                        continue
            a = RX_ASCII.match(buf, i)
            j = a.end()
            if j - i >= 4 and j < n and buf[j] == 0:
                out.append((va + i, 'ascii', buf[i:j].decode('latin-1')))
                i = j + 1
                continue
            i = j
    return out


def clean(s):
    return s.replace('\t', '\\t').replace('\n', '\\n').replace('\r', '\\r')


def build(path=None):
    path = path or C.STRINGS
    b = C.load_dump()
    xr = {}
    for l in open(C.data('string_xrefs.tsv'), encoding='utf-8'):
        f = l.rstrip('\n').split('\t')
        if f[0].startswith('0x'):
            xr[f[0]] = f
    rows = scan(b)
    seen = 0
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as w:
        w.write('rva\tenc\tfuncs\tvia\tstring\n')
        for r, enc, s in rows:
            k = '0x%x' % r
            f = xr.get(k)
            if f and f[1] == enc:
                seen += 1
            w.write('%s\t%s\t%s\t%s\t%s\n' % (k, enc, f[2] if f and f[1] == enc else '', f[3] if f and f[1] == enc else '', clean(s)))
    print('strings: %d found in the dump, %d of %d cross-referenced strings matched -> %s' % (len(rows), seen, len(xr), path))
    if seen < len(xr):
        print('WARNING: %d cross-referenced strings were not found at their address; is this the same game build?'
              % (len(xr) - seen), file=sys.stderr)
    return path


if __name__ == '__main__':
    build()
