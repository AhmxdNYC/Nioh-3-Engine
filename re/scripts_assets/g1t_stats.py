"""G1T (v0066) texture header census: format code, size, mip count, full-chain check, exact duplicates.
usage: py -I g1t_stats.py <dir_of_.g1t> [tex.tsv]   (tex.tsv = stage_textures.py output, col2 = references)
Header: 'GT1G' ver size tableOff count platform ...; tex header at tableOff+table[0]: u8 (mips<<4|z), u8 fmt, u8 (dy<<4|dx), u8, u32 flags."""
import os, sys, struct, hashlib, collections
FMT = {0x00: 'RGBA8', 0x01: 'BGRA8', 0x02: 'R32F?', 0x06: 'DXT1', 0x08: 'DXT5', 0x59: 'BC1', 0x5a: 'BC2', 0x5b: 'BC3',
       0x5c: 'BC4', 0x5d: 'BC5', 0x5e: 'BC6H', 0x5f: 'BC7'}
d = sys.argv[1]; refs = {}
if len(sys.argv) > 2:
    for ln in open(sys.argv[2]):
        c = ln.split('\t'); refs[c[0]] = int(c[1])
fmt = collections.Counter(); fmtb = collections.Counter(); short = []; hashes = collections.defaultdict(list); dims = collections.Counter()
n = 0
for fn in sorted(os.listdir(d)):
    b = open(os.path.join(d, fn), 'rb').read()
    if b[:4] != b'GT1G': continue
    n += 1
    toff, cnt = struct.unpack_from('<II', b, 12)
    o = toff + struct.unpack_from('<I', b, toff)[0]
    m, f, dxy = b[o], b[o + 1], b[o + 2]
    mips = m >> 4; w, h = 1 << (dxy & 15), 1 << (dxy >> 4)
    full = max(min(w, h).bit_length() - 2, 1)   # BCn chains stop when the short side reaches 4
    name = FMT.get(f, 'fmt%02x' % f)
    fmt[name] += 1; fmtb[name] += len(b); dims[(w, h)] += 1
    if mips < full and max(w, h) > 4: short.append((fn, name, w, h, mips, full))
    hashes[hashlib.sha1(b[o:]).hexdigest()].append(fn)
print(f'textures={n}  formats:', ', '.join(f'{k} {v} ({fmtb[k]/1e6:.0f} MB)' for k, v in fmt.most_common()))
print('sizes:', dims.most_common(8))
print(f'incomplete mip chains (missing levels above 4x4): {len(short)}', short[:6])
dup = [v for v in hashes.values() if len(v) > 1]
print(f'exact-duplicate texture payloads: {len(dup)} groups, {sum(len(v) - 1 for v in dup)} redundant files', dup[:3])
