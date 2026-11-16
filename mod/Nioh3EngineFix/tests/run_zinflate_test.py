"""run_zinflate_test.py - [Patches] Inflate (src/zinflate.cpp, #108787). Not a game test.
    py -I tests/run_zinflate_test.py
Writes a fixture with Python zlib in 16 KiB chunks at windowBits 15 (the game's inflateInit_ zlib-header format; levels
0/1/6/9, default / filtered / huffman-only / RLE / fixed strategies) over random, text, structured-binary, zero and mixed
buffers, plus invalid streams (bit flips, truncation, wrong adler32, raw deflate, gzip) that Python zlib refuses. Builds
tests/zinflate_test.cpp (#includes src/zinflate.cpp) with the vendored libdeflate and runs it. Must end ALL PASSED.
"""
import pathlib
import random
import struct
import subprocess
import sys
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "zitest"
OUT.mkdir(parents=True, exist_ok=True)
CH = 0x4000

rng = random.Random(108787)
WORDS = ("the of and stage load texture model shader mesh enemy yokai samurai spirit guardian katana spear odachi "
         "kusarigama tonfa switchglaive fist splitstaff dual hatchets bow rifle cannon").split()


def text(n):
    s, k = [], 0
    while k < n:
        w = rng.choice(WORDS) + ("\n" if rng.random() < 0.08 else " ")
        s.append(w)
        k += len(w)
    return "".join(s).encode()[:n]


def structured(n):  # vertex-like records: 3 floats, 2 half-ish shorts, an index
    b = bytearray()
    i = 0
    while len(b) < n:
        b += struct.pack("<fffHHI", i * 0.01, rng.uniform(-1, 1), 1.0, i & 0xFFFF, (i * 7) & 0xFFFF, i // 3)
        i += 1
    return bytes(b[:n])


def mixed(n):
    out = bytearray()
    while len(out) < n:
        k = rng.randrange(4)
        m = rng.randrange(16, 2048)
        out += [rng.randbytes(m), text(m), bytes(m), structured(m)][k]
    return bytes(out[:n])


SOURCES = [("random", rng.randbytes), ("text", text), ("struct", structured), ("zeros", bytes), ("mixed", mixed)]
SETTINGS = [(0, zlib.Z_DEFAULT_STRATEGY), (1, zlib.Z_DEFAULT_STRATEGY), (6, zlib.Z_DEFAULT_STRATEGY),
            (9, zlib.Z_DEFAULT_STRATEGY), (6, zlib.Z_FILTERED), (6, zlib.Z_HUFFMAN_ONLY), (6, zlib.Z_RLE), (6, zlib.Z_FIXED)]


def comp(raw, level, strat, wbits=15):
    c = zlib.compressobj(level, zlib.DEFLATED, wbits, 9, strat)
    return c.compress(raw) + c.flush()


def py_ok(c, raw):
    try:
        d = zlib.decompressobj(15)
        out = d.decompress(c, CH + 1)
        return d.eof and out == raw
    except zlib.error:
        return False


recs = []
valid = []
for name, gen in SOURCES:
    buf = gen(CH * 40 + 1234)  # 40 full chunks + one partial tail, like an archive entry
    chunks = [buf[i:i + CH] for i in range(0, len(buf), CH)] + [b"", b"x"]
    for level, strat in SETTINGS:
        for raw in chunks:
            c = comp(raw, level, strat)
            assert zlib.decompress(c) == raw
            recs.append((0, raw, c))
            valid.append((raw, c))
# invalid streams (kind 1): only those Python zlib refuses
bad = 0
for raw, c in rng.sample(valid, 400):
    cands = []
    b = bytearray(c)
    b[rng.randrange(len(b))] ^= 1 << rng.randrange(8)
    cands.append(bytes(b))
    cands.append(c[:max(0, len(c) - rng.randrange(1, 6))])  # truncated (often the adler32)
    b = bytearray(c)
    b[-1] ^= 0x5A
    cands.append(bytes(b))  # wrong adler32
    cands.append(comp(raw, 6, zlib.Z_DEFAULT_STRATEGY, -15))  # raw deflate, no header
    cands.append(comp(raw, 6, zlib.Z_DEFAULT_STRATEGY, 31))  # gzip
    for x in cands:
        if not py_ok(x, raw):
            recs.append((1, raw, x))
            bad += 1
with open(OUT / "chunks.bin", "wb") as f:
    for k, raw, c in recs:
        f.write(struct.pack("<III", k, len(raw), len(c)) + raw + c)
print("fixture: %d valid, %d invalid chunks" % (len(recs) - bad, bad))

setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():  # git worktree: use the main checkout's portable MSVC
    setup = pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
LD = ROOT / "third_party" / "libdeflate" / "lib"
csrc = " ".join('"%s"' % p for p in (LD / "deflate_decompress.c", LD / "zlib_decompress.c", LD / "adler32.c", LD / "utils.c",
                                     LD / "x86" / "cpu_features.c"))
cmd = pre + (
    'cl /nologo /c /O2 /MT /std:c11 /W3 /Fo"{o}\\\\" {c} && '
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\zinflate_test.cpp" '
    '"{o}\\deflate_decompress.obj" "{o}\\zlib_decompress.obj" "{o}\\adler32.obj" "{o}\\utils.obj" "{o}\\cpu_features.obj" '
    '/Fo"{o}\\\\" /Fe"{o}\\zinflate_test.exe" /link kernel32.lib'
).format(o=OUT, t=ROOT / "tests", c=csrc)
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
r = subprocess.run([str(OUT / "zinflate_test.exe"), str(OUT / "chunks.bin")], capture_output=True, text=True, timeout=300)
print(r.stdout.strip())
ok = r.returncode == 0 and "ALL PASSED" in r.stdout and not warn
if warn:
    print("compiler warnings:", warn[:5])
print("ALL PASSED" if ok else "FAILED")
sys.exit(0 if ok else 1)
