"""run_psoswap_harness.py - psoswap (PSO bytecode swap) on a real D3D12 device of this PC. Not a game test.

    py -I tests/run_psoswap_harness.py

Builds tests/psoswap_harness.cpp with the real src/psoswap.cpp (same cl flags as build.bat; MSVC on PATH or
..\\..\\tools\\msvc\\setup_x64.bat) into build/psotest and runs it. Must end "ALL PSOSWAP CHECKS OK".
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "psotest"
OUT.mkdir(parents=True, exist_ok=True)
setup = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE '
    '"{t}" "{s}\\psoswap.cpp" /Fo"{o}\\\\" /Fe"{o}\\psoswap_harness.exe" '
    '/link d3d12.lib d3dcompiler.lib kernel32.lib user32.lib'
).format(o=OUT, s=ROOT / "src", t=ROOT / "tests" / "psoswap_harness.cpp")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode or "warning" in r.stdout:
    print(r.stdout[-4000:], r.stderr[-2000:])
    if r.returncode:
        sys.exit("compile failed")
# DXIL blobs for the HASH-digest test (signed dxc from RDTS, as gpu-shaders/sweep/build_sweep.py uses)
DXC = (ROOT.parent.parent / "tools" / "rdts" / "RadeonDeveloperToolSuite-2026-05-28-1806" / "utils" / "dx12" / "dxc" /
       "dxc.exe")
args = []
if DXC.exists():
    for name, mul, add in (("h_orig", 1, 800), ("h_repl", 11, 17)):
        hl = OUT / (name + ".hlsl")
        hl.write_text("RWStructuredBuffer<uint> u : register(u0);\n[numthreads(64,1,1)] void main(uint3 id : "
                      "SV_DispatchThreadID) { u[id.x] = id.x * %du + %du; }\n" % (mul, add))
        out = OUT / (name + ".dxil")
        subprocess.run([str(DXC), "-T", "cs_6_0", "-E", "main", "-O3", str(hl), "-Fo", str(out)], check=True,
                       capture_output=True)
        args.append(str(out))
sys.exit(subprocess.run([str(OUT / "psoswap_harness.exe")] + args).returncode)
