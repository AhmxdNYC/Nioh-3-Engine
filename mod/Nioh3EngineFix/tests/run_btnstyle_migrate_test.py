"""Offline build+run of btnstyle_migrate_test.cpp (#106253 ButtonStyle Xbox -> Auto ini migration) + checks menu.cpp runs it before reading ButtonStyle. No game needed."""
import pathlib, subprocess, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "menutest"
OUT.mkdir(parents=True, exist_ok=True)
setup = next((p for p in (REPO / "tools/msvc/setup_x64.bat", pathlib.Path(r"D:/nioh3-wt/rc-187/tools/msvc/setup_x64.bat")) if p.exists()), None)
mc = (ROOT / "src/menu.cpp").read_text(encoding="utf-8-sig")
i, j = mc.find("btnstyle_migrate::Run(f)"), mc.find('GetPrivateProfileStringW(L"Menu", L"ButtonStyle", L"Auto"')
if not (0 <= i < j): sys.exit("FAIL: menu.cpp must run btnstyle_migrate::Run(f) before it reads [Menu] ButtonStyle")
if 'snprintf(shown, sizeof(shown), "Auto (%s)", StyleName(menu_style::g_btnStyle))' not in mc: sys.exit("FAIL: ButtonStyle row must show 'Auto (<resolved>)' (#106442)")
src = ROOT / "tests/btnstyle_migrate_test.cpp"
exe = OUT / "btnstyle_migrate_test.exe"
batch = OUT / "btnstyle_migrate_build.cmd"
batch.write_text('@echo off\n' + (f'call "{setup}" >nul\nif errorlevel 1 exit /b 1\n' if setup else '') +
    f'cl /nologo /O2 /MT /Gy /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{src}" /Fo"{OUT}\\\\" /Fe"{exe}" /link /OPT:REF kernel32.lib user32.lib\nexit /b %errorlevel%\n')
r = subprocess.run(["cmd.exe", "/d", "/c", str(batch)], capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-6000:], r.stderr[-2000:]); sys.exit("build failed")
r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
print(r.stdout.strip(), r.stderr.strip())
sys.exit(r.returncode)
