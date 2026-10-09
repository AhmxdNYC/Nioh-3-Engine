"""run_mpcs_harness.py - MPCS caves + MpcCheck kernel map + SHA-256 guard without the game. Not a game test.

    py -I tests/run_mpcs_harness.py

Builds tests/mpcs_harness.cpp + tests/mpcs_harness.asm with the real src/mpcskip.asm, src/mpccheck.cpp and src/mlprobe.cpp (MSVC: cl
on PATH or ..\\..\\tools\\msvc\\setup_x64.bat) into build/mpcstest and runs it from there. See mpcs_harness.cpp for the checks.
"""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "mpcstest"
OUT.mkdir(parents=True, exist_ok=True)
setup = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
s, t, o = ROOT / "src", ROOT / "tests", OUT
cmd = pre + " && ".join([
    'ml64 /nologo /c /W3 /Fo"%s" "%s" >nul' % (o / "mpcskip.obj", s / "mpcskip.asm"),
    'ml64 /nologo /c /W3 /Fo"%s" "%s" >nul' % (o / "mpcs_harness_asm.obj", t / "mpcs_harness.asm"),
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /DUNICODE /D_UNICODE "%s" "%s" "%s" "%s" "%s" "%s" /Fo"%s\\\\" /Fe"%s" /link kernel32.lib user32.lib' % (
        t / "mpcs_harness.cpp", s / "mpccheck.cpp", s / "mlprobe.cpp", t / "mpcs_harness_stubs.cpp", o / "mpcskip.obj",
        o / "mpcs_harness_asm.obj", o, o / "mpcs_harness.exe"),
])
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
sys.exit(subprocess.run([str(OUT / "mpcs_harness.exe")], cwd=str(OUT)).returncode)
