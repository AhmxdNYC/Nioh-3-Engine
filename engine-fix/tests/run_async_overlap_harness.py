"""run_async_overlap_harness.py - real D3D12 device on this PC: a latency-bound compute chain on the graphics queue vs on a
COMPUTE queue with the KTGL async-slot fence pattern (notes\\gpu-async-wins.md). Not a game test; uses the GPU for ~10 s.

    py -I tests/run_async_overlap_harness.py [K] [iterations]     must end ALL PASSED (defaults 170 dispatches, 60 iterations)

Builds tests/async_overlap_harness.cpp (MSVC: cl on PATH, ..\\..\\tools\\msvc\\setup_x64.bat, or the main checkout's copy in a
git worktree) into build/aotest. Check %TEMP%\\nioh3_measuring.lock first: never run it during a game measurement.
"""
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "aotest"
OUT.mkdir(parents=True, exist_ok=True)
if os.path.exists(os.path.join(os.environ.get("TEMP", ""), "nioh3_measuring.lock")) and "--force" not in sys.argv:
    sys.exit("nioh3_measuring.lock present: a game measurement is running (pass --force only if it is stale)")
args = [a for a in sys.argv[1:] if a != "--force"]


def find_setup():
    cand = [ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"]
    try:
        common = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--git-common-dir"], capture_output=True, text=True).stdout.strip()
        if common:
            cand.append((ROOT / common).resolve().parent / "tools" / "msvc" / "setup_x64.bat")
    except OSError:
        pass
    for c in cand:
        if c.exists():
            return c
    return None


setup = find_setup()
pre = ('call "%s" >nul && ' % setup) if setup else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 "{t}\\async_overlap_harness.cpp" /Fo"{o}\\\\" /Fe"{o}\\async_overlap_harness.exe" '
    '/link d3d12.lib d3dcompiler.lib dxgi.lib kernel32.lib'
).format(o=OUT, t=ROOT / "tests")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
sys.exit(subprocess.run([str(OUT / "async_overlap_harness.exe")] + args).returncode)
