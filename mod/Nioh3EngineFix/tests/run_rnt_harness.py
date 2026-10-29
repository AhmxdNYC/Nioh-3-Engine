"""Offline test for the render-node timer. No game.

    py -I mod/Nioh3EngineFix/tests/run_rnt_harness.py
"""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "rnttest"
OUT.mkdir(parents=True, exist_ok=True)
setup = pathlib.Path(r"C:\nioh3-engine\tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE /I "{s}" '
    '"{t}\\rnt_harness.cpp" "{s}\\rnt.cpp" /Fo"{o}\\\\" /Fe"{o}\\rnt_harness.exe" /link kernel32.lib user32.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
print(r.stdout)
print(r.stderr)
if r.returncode:
    raise SystemExit(r.returncode)
run = subprocess.run([str(OUT / "rnt_harness.exe")], capture_output=True, text=True)
print(run.stdout)
print(run.stderr)
raise SystemExit(run.returncode)
