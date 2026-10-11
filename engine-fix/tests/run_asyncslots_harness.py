"""run_asyncslots_harness.py - AS caves (src/asyncslots.asm) against the original v2.02 AsyncSlotGetCtx from re\\Nioh3.dump.exe.
Not a game test.

    py -I tests/run_asyncslots_harness.py [dump]     must end ALL PASSED

Assembles the real src/asyncslots.asm and tests/drawprep_harness.asm (RunCave / LandA), builds tests/asyncslots_harness.cpp
(MSVC: cl on PATH, ..\\..\\tools\\msvc\\setup_x64.bat, or the same file in the main checkout of a git worktree) into
build/astest and runs it.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "astest"
OUT.mkdir(parents=True, exist_ok=True)


def find_setup():
    cand = [ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"]
    try:  # a worktree: the portable toolchain lives in the main checkout (tools/ is git-ignored)
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
    'ml64 /nologo /c /W3 /Fo"{o}\\asyncslots_caves.obj" "{s}\\asyncslots.asm" >nul && '
    'ml64 /nologo /c /W3 /Fo"{o}\\drawprep_harness_asm.obj" "{t}\\drawprep_harness.asm" >nul && '
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 "{t}\\asyncslots_harness.cpp" "{o}\\asyncslots_caves.obj" '
    '"{o}\\drawprep_harness_asm.obj" /Fo"{o}\\\\" /Fe"{o}\\asyncslots_harness.exe" /link kernel32.lib'
).format(o=OUT, s=ROOT / "src", t=ROOT / "tests")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
sys.exit(subprocess.run([str(OUT / "asyncslots_harness.exe")] + sys.argv[1:]).returncode)
