"""run_w1b_rpa_harness.py - W1B / RPA caves (src/drawprep.asm) against the original v2.02 bytes from re\\Nioh3.dump.exe.
Not a game test.

    py -I tests/run_w1b_rpa_harness.py              all checks, must end ALL PASSED
    py -I tests/run_w1b_rpa_harness.py --derive     print the RPA copy ranges (tools/gen_rpa_copy.py reads them)

Assembles the real src/drawprep.asm and tests/drawprep_harness.asm (RunCave), builds tests/w1b_rpa_harness.cpp (MSVC: cl on
PATH, ..\\..\\tools\\msvc\\setup_x64.bat, or the same file in the main checkout of a git worktree) into build/w1btest.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "w1btest"
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
    'ml64 /nologo /c /W3 /Fo"{o}\\drawprep_caves.obj" "{s}\\drawprep.asm" >nul && '
    'ml64 /nologo /c /W3 /Fo"{o}\\drawprep_harness_asm.obj" "{t}\\drawprep_harness.asm" >nul && '
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /D_CRT_SECURE_NO_WARNINGS "{t}\\w1b_rpa_harness.cpp" "{o}\\drawprep_caves.obj" '
    '"{o}\\drawprep_harness_asm.obj" /Fo"{o}\\\\" /Fe"{o}\\w1b_rpa_harness.exe" /link kernel32.lib'
).format(o=OUT, s=ROOT / "src", t=ROOT / "tests")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
sys.exit(subprocess.run([str(OUT / "w1b_rpa_harness.exe")] + sys.argv[1:]).returncode)
