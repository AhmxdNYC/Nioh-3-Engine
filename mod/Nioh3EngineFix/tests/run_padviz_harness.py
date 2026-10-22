"""Offline regression of padviz.h (controller input display). No game or hooks."""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "padviztest"
OUT.mkdir(parents=True, exist_ok=True)
setup = next((p for p in (REPO / "tools/msvc/setup_x64.bat",
    pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\tools\msvc\setup_x64.bat")) if p.exists()), None)
source = ROOT / "tests/padviz_harness.cpp"
exe = OUT / "padviz_harness.exe"
batch = OUT / "build.cmd"
batch.write_text('@echo off\n' + (f'call "{setup}" >nul\nif errorlevel 1 exit /b 1\n' if setup else '')
    + f'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- "{source}" /Fo"{OUT}\\\\" /Fe"{exe}"\nexit /b %errorlevel%\n')
r = subprocess.run(["cmd.exe", "/d", "/c", str(batch)], capture_output=True, text=True)
warnings = [l for l in r.stdout.splitlines() if "warning" in l]
if r.returncode or warnings:
    print(r.stdout[-6000:], r.stderr[-2000:])
    sys.exit("padviz harness build failed or warned")
r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
print(r.stdout.strip())
if r.returncode or "ALL PASSED" not in r.stdout:
    sys.exit("padviz harness failed")
