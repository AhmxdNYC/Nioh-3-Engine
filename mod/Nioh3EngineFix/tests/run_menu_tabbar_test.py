"""Offline build+run of menu_tabbar_test.cpp (tab bar, LB/RB loop with Home, skip of unopenable tabs). No game needed."""
import pathlib, subprocess, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "menutest"
OUT.mkdir(parents=True, exist_ok=True)
setup = next((p for p in (REPO / "tools/msvc/setup_x64.bat", pathlib.Path(r"D:/nioh3-wt/rc-187/tools/msvc/setup_x64.bat")) if p.exists()), None)
src = ROOT / "tests/menu_tabbar_test.cpp"
exe = OUT / "menu_tabbar_test.exe"
batch = OUT / "menu_tabbar_build.cmd"
batch.write_text('@echo off\n' + (f'call "{setup}" >nul\nif errorlevel 1 exit /b 1\n' if setup else '') +
    f'cl /nologo /O2 /MT /Gy /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{src}" /Fo"{OUT}\\\\" /Fe"{exe}" /link /OPT:REF kernel32.lib user32.lib\nexit /b %errorlevel%\n')
r = subprocess.run(["cmd.exe", "/d", "/c", str(batch)], capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-6000:], r.stderr[-2000:]); sys.exit("build failed")
r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
print(r.stdout.strip(), r.stderr.strip())
sys.exit(r.returncode)
