r"""run_pj_harness.py - PJ (PSO journal + prewarm, src/psojournal.cpp) on a real D3D12 WARP device. Not a game test.

    py -I tests/run_pj_harness.py

Builds tests/pj_harness.cpp with the real src/psojournal.cpp + src/psoswap.cpp (the device reaches PJ through
psoswap's observe-only D3D12CreateDevice IAT hook, as in the mod; same cl flags as build.bat; MSVC on PATH or
..\..\tools\msvc\setup_x64.bat) into build/pjtest and runs six "launches" against one journal folder:
rec (journal), warm (prewarm; descs below PJ equal the engine's), again (no growth), then on copies: trunc (cut tail),
badsh (corrupt shader chunk), build (other exe stamp -> new journal), hand / handoff (PJH = 1 / 2: prewarmed PSOs
handed to the engine's creates, or kept but not handed). Must end "ALL PJ CHECKS OK".
"""
import pathlib
import shutil
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "pjtest"
OUT.mkdir(parents=True, exist_ok=True)
setup = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():  # git worktree: the portable toolchain lives in the main checkout
    alt = pathlib.Path(r"C:\nioh3-engine\tools\msvc\setup_x64.bat")
    setup = alt if alt.exists() else setup
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE '
    '"{t}" "{s}\\psojournal.cpp" "{s}\\psoswap.cpp" /Fo"{o}\\\\" /Fe"{o}\\pj_harness.exe" '
    '/link d3d12.lib dxgi.lib d3dcompiler.lib kernel32.lib user32.lib'
).format(o=OUT, s=ROOT / "src", t=ROOT / "tests" / "pj_harness.cpp")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode or "warning" in r.stdout:
    print(r.stdout[-4000:], r.stderr[-2000:])
    if r.returncode:
        sys.exit("compile failed")
EXE = OUT / "pj_harness.exe"
fails = 0


def run(mode, d):
    global fails
    p = subprocess.run([str(EXE), mode, str(d)], capture_output=True, text=True, timeout=120)
    print(p.stdout.rstrip())
    if p.returncode:
        fails += 1
        print("  -> %s FAILED (rc %d) %s" % (mode, p.returncode, p.stderr[-500:]))


def journal(d):
    j = sorted(d.glob("pj_*.bin"))
    return j[0] if j else None


def chunks(data):
    off, out = 48, []
    while off + 16 <= len(data):
        tag, ln = struct.unpack_from("<II", data, off)
        out.append((off, tag, ln))
        off += 16 + ln
    return out


base = OUT / "run"
shutil.rmtree(base, ignore_errors=True)
base.mkdir()
run("rec", base)
j = journal(base)
if not j:
    sys.exit("no journal written")
print("  journal:", j.name, j.stat().st_size, "bytes")
run("warm", base)
size1 = j.stat().st_size
run("again", base)
if j.stat().st_size != size1:
    fails += 1
    print("FAIL journal grew on a launch with nothing new (%d -> %d)" % (size1, j.stat().st_size))

data = j.read_bytes()
cs = chunks(data)
tags = [struct.pack("<I", t).decode() for _, t, _ in cs]
print("  chunks:", " ".join(tags))
if tags != ["RSIG", "SHDR", "SHDR", "PSO1", "SHDR", "PSO1", "PSO1"]:
    fails += 1
    print("FAIL unexpected chunk order")


def variant(name, mutate):
    d = OUT / name
    shutil.rmtree(d, ignore_errors=True)
    shutil.copytree(base, d)
    jj = journal(d)
    b = bytearray(jj.read_bytes())
    jj.write_bytes(mutate(b))
    run(name, d)
    return jj


jt = variant("trunc", lambda b: b[:-3])
if jt.stat().st_size != cs[-1][0]:
    fails += 1
    print("FAIL the cut tail was not removed (size %d, want %d)" % (jt.stat().st_size, cs[-1][0]))


def flip_vs(b):
    off = next(o for o, t, _ in cs if t == 0x52444853)  # first SHDR = the vertex shader
    b[off + 16 + 16 + 60] ^= 0xFF
    return b


variant("badsh", flip_vs)


def other_build(b):
    b[8] ^= 1  # exe TimeDateStamp in the header
    return b


variant("build", other_build)

# PJH hand-over on copies of the warm journal (3 records): on, then installed + off
for m in ("hand", "handoff"):
    d = OUT / m
    shutil.rmtree(d, ignore_errors=True)
    shutil.copytree(base, d)
    run(m, d)

# ---- PL (boot pipeline library, 1.7.46) ------------------------------------------------------------------------------------
M64 = (1 << 64) - 1


def rotl(x, r):
    return ((x << r) | (x >> (64 - r))) & M64


def fmix(k):
    k ^= k >> 33
    k = (k * 0xFF51AFD7ED558CCD) & M64
    k ^= k >> 33
    k = (k * 0xC4CEB9FE1A85EC53) & M64
    return k ^ (k >> 33)


def hash128(b):  # psojournal.cpp Hash128
    n = len(b)
    h1, h2 = 0x9E3779B97F4A7C15 ^ n, (0xC2B2AE3D27D4EB4F + n) & M64
    i = 0
    while i + 8 <= n:
        w = int.from_bytes(b[i:i + 8], "little")
        h1 ^= (rotl((w * 0x87C37B91114253D5) & M64, 31) * 0x4CF5AD432745937F) & M64
        h1 = (rotl(h1, 27) * 5 + 0x52DCE729) & M64
        h2 = (h2 + (w ^ h1)) & M64
        h2 = (rotl(h2, 33) * 0x9E3779B97F4A7C15 + 0x38495AB5) & M64
        i += 8
    t = int.from_bytes(b[i:], "little")
    h1 ^= (rotl((t * 0x87C37B91114253D5) & M64, 31) * 0x4CF5AD432745937F) & M64
    h2 = (h2 + (t ^ h1)) & M64
    h1 = fmix((h1 + h2) & M64)
    h2 = fmix((h2 + h1) & M64)
    return h1, h2 ^ h1


plb = OUT / "plrun"
shutil.rmtree(plb, ignore_errors=True)
plb.mkdir()
for m in ("plrec", "plwarm", "plwarm2"):
    run(m, plb)
lib = sorted(plb.glob("pl_*.bin"))
if not lib:
    fails += 1
    print("FAIL no PL library written")
else:
    raw = lib[0].read_bytes()
    print("  library:", lib[0].name, len(raw), "bytes")
    blob_size = struct.unpack_from("<Q", raw, 72)[0]
    a, b = struct.unpack_from("<QQ", raw, 80)
    if blob_size != len(raw) - 128 or (a, b) != hash128(raw[128:]):
        fails += 1
        print("FAIL PL header size / checksum do not match the blob (python Hash128 port)")


def pl_variant(name, mutate):
    d = OUT / name
    shutil.rmtree(d, ignore_errors=True)
    shutil.copytree(plb, d)
    f = sorted(d.glob("pl_*.bin"))[0]
    f.write_bytes(bytes(mutate(bytearray(f.read_bytes()))))
    run(name if name.startswith("pl") else "pl" + name, d)


def flip_blob(b):
    b[128 + len(b[128:]) // 2] ^= 0x5A
    return b


def stale(b):
    b[64] ^= 1  # UMD driver version
    return b


def garbage(b):
    import random
    rnd = random.Random(11)
    blob = bytes(rnd.getrandbits(8) for _ in range(4096))
    h = bytearray(b[:128])
    struct.pack_into("<Q", h, 72, len(blob))
    struct.pack_into("<QQ", h, 80, *hash128(blob))
    return h + blob


pl_variant("plbad", flip_blob)
pl_variant("plstale", stale)
pl_variant("plgarb", garbage)
pl_variant("plpj", lambda b: b)
d2 = OUT / "pl2"
shutil.rmtree(d2, ignore_errors=True)
d2.mkdir()
run("pl2", d2)
print("ALL PJ CHECKS OK" if not fails else "PJ CHECKS FAILED: %d" % fails)
sys.exit(1 if fails else 0)
