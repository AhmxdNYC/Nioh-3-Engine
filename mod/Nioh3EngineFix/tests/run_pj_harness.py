r"""run_pj_harness.py - PJ (PSO journal + prewarm, src/psojournal.cpp) on a real D3D12 WARP device. Not a game test.

    py -I tests/run_pj_harness.py

Builds tests/pj_harness.cpp with the real src/psojournal.cpp + src/psoswap.cpp (the device reaches PJ through
psoswap's observe-only D3D12CreateDevice IAT hook, as in the mod; same cl flags as build.bat; MSVC on PATH or
..\..\tools\msvc\setup_x64.bat) into build/pjtest and runs six "launches" against one journal folder:
rec (journal), warm (prewarm; descs below PJ equal the engine's), again (no growth), then on copies: trunc (cut tail),
badsh (corrupt shader chunk), build (other exe stamp -> new journal). Must end "ALL PJ CHECKS OK".
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
    alt = pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\tools\msvc\setup_x64.bat")
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
print("ALL PJ CHECKS OK" if not fails else "PJ CHECKS FAILED: %d" % fails)
sys.exit(1 if fails else 0)
