"""Offline regression of both real menu models and controller release handling. No game or hooks installed."""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "menutest"
OUT.mkdir(parents=True, exist_ok=True)
setup = next((p for p in (REPO / "tools/msvc/setup_x64.bat",
    pathlib.Path(r"C:\nioh3-engine\tools\msvc\setup_x64.bat")) if p.exists()), None)
for source in (ROOT / "tests/menu_harness.cpp", REPO / "mod/Nioh3BossRematch/tests/ui_harness.cpp"):
    exe = OUT / (source.stem + ".exe")
    cmd = (f'cl /nologo /O2 /MT /Gy /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE '
        f'"{source}" /Fo"{OUT}\\\\" /Fe"{exe}" /link /OPT:REF kernel32.lib user32.lib')
    batch = OUT / (source.stem + "_build.cmd")
    batch.write_text('@echo off\n' + (f'call "{setup}" >nul\nif errorlevel 1 exit /b 1\n' if setup else '')
        + cmd + '\nexit /b %errorlevel%\n')
    result = subprocess.run(["cmd.exe", "/d", "/c", str(batch)], capture_output=True, text=True)
    warnings = [line for line in result.stdout.splitlines() if "warning" in line]
    if result.returncode or warnings:
        print(result.stdout[-6000:], result.stderr[-2000:])
        sys.exit("menu harness build failed or warned")
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
    print(result.stdout.strip())
    if result.returncode or "ALL PASSED" not in result.stdout:
        sys.exit("menu harness failed")
print("ALL PASSED: both controller menus (offline; native game acceptance still required)")
