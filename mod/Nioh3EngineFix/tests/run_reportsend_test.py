"""Offline test of the Report box sender (#64882). Optional: REPORT_TEST_URL=http://127.0.0.1:8792/api/report for a live POST."""
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "reportsendtest"
OUT.mkdir(parents=True, exist_ok=True)
setup = next((p for p in (REPO / "tools/msvc/setup_x64.bat",
    pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\tools\msvc\setup_x64.bat")) if p.exists()), None)
source = ROOT / "tests/reportsend_test.cpp"
exe = OUT / "reportsend_test.exe"
cmd = (f'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{source}" /Fo"{OUT}\\\\" /Fe"{exe}" '
       f'/link kernel32.lib user32.lib winhttp.lib bcrypt.lib')
batch = OUT / "build.cmd"
batch.write_text('@echo off\n' + (f'call "{setup}" >nul\n' if setup else '') + cmd + '\nexit /b %errorlevel%\n')
r = subprocess.run(["cmd.exe", "/d", "/c", str(batch)], capture_output=True, text=True)
if r.returncode or any("warning" in l for l in r.stdout.splitlines()):
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("reportsend test build failed or warned")
r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60, cwd=str(OUT), env=os.environ)
print(r.stdout.strip())
if r.returncode or "ALL PASSED" not in r.stdout:
    sys.exit("reportsend test failed")
