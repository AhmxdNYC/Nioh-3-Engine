"""Offline: v1.8.19 walk-fix defaults (#106691). KidstsCap + FileLoadPrio default 1 everywhere (code, shipped ini, menu
defaults) and the one-time WalkFixDefaultV migration (copied verbatim from dllmain.cpp LoadConfig) flips a shipped 0 once,
keeps a later manual 0, leaves other values alone, and runs before KidstsCap is read. No game needed."""
import pathlib, re, subprocess, sys, tempfile
ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
fail = []
dm = (ROOT / "src/dllmain.cpp").read_text(encoding="utf-8-sig")
ts = (ROOT / "src/thrsched.cpp").read_text(encoding="utf-8-sig")
md = (ROOT / "src/menu_shipped_defaults.h").read_text(encoding="utf-8-sig")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8-sig")
m = re.search(r'\n(    if \(I\(L"Patches", L"WalkFixDefaultV", 0\) < 1\) \{.*?\n    \})\n', dm, re.S)
if not m: sys.exit("FAIL: WalkFixDefaultV migration block not found in dllmain.cpp")
block = m.group(1)
if not (m.start() < dm.find('I(L"Patches", L"KidstsCap", 1)')): fail.append("migration must run before KidstsCap is read (default 1)")
if dm.find('I(L"Patches", L"KidstsCap", 1)') < 0: fail.append("dllmain KidstsCap default must be 1")
if 'I(L"Patches", L"FileLoadPrio", 1) == 1' not in ts: fail.append("thrsched FileLoadPrio default must be 1")
for k in ("KidstsCap", "FileLoadPrio"):
    if '{"Patches", "%s", "1"}' % k not in md: fail.append("menu_shipped_defaults %s must be 1" % k)
    if not re.search(r'(?m)^%s = 1\s*$' % k, ini): fail.append("shipped ini %s must be 1" % k)
if fail: sys.exit("FAIL: " + "; ".join(fail))
setup = next((p for p in (REPO / "tools/msvc/setup_x64.bat", pathlib.Path(r"C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat")) if p.exists()), None)
OUT = ROOT / "build" / "walkfixdef"; OUT.mkdir(parents=True, exist_ok=True)
src = OUT / "walkfix_default_test.cpp"
src.write_text(r'''#include <windows.h>
#include <cstdio>
#include <string>
static void Mig(const wchar_t* f) {
    auto I = [&](const wchar_t* sec, const wchar_t* key, int def) { return (int)GetPrivateProfileIntW(sec, key, def, f); };
''' + block + r'''
}
static int G(const wchar_t* f, const wchar_t* k) { return (int)GetPrivateProfileIntW(L"Patches", k, -9, f); }
int wmain(int, wchar_t** argv) {
    std::wstring d = argv[1]; int bad = 0;
    auto mk = [&](const wchar_t* n, const char* body) { std::wstring p = d + L"\\" + n; FILE* fp = _wfopen(p.c_str(), L"wb"); fputs(body, fp); fclose(fp); return p; };
    auto chk = [&](const char* what, bool ok) { printf("%s %s\n", ok ? "PASS" : "FAIL", what); bad += !ok; };
    std::wstring a = mk(L"a.ini", "[Patches]\r\nKidstsCap = 0\r\nFileLoadPrio = 0\r\n");
    Mig(a.c_str()); chk("shipped 0/0 -> 1/1 + marker", G(a.c_str(), L"KidstsCap") == 1 && G(a.c_str(), L"FileLoadPrio") == 1 && G(a.c_str(), L"WalkFixDefaultV") == 1);
    WritePrivateProfileStringW(L"Patches", L"KidstsCap", L"0", a.c_str()); Mig(a.c_str());
    chk("manual 0 after marker sticks", G(a.c_str(), L"KidstsCap") == 0 && G(a.c_str(), L"FileLoadPrio") == 1);
    std::wstring b = mk(L"b.ini", "[Patches]\r\nKidstsCap = 4\r\n");
    Mig(b.c_str()); chk("explicit N kept, missing key not written", G(b.c_str(), L"KidstsCap") == 4 && G(b.c_str(), L"FileLoadPrio") == -9 && G(b.c_str(), L"WalkFixDefaultV") == 1);
    std::wstring c = mk(L"c.ini", "[Patches]\r\nWalkFixDefaultV = 1\r\nKidstsCap = 0\r\nFileLoadPrio = 0\r\n");
    Mig(c.c_str()); chk("marker present -> untouched", G(c.c_str(), L"KidstsCap") == 0 && G(c.c_str(), L"FileLoadPrio") == 0);
    printf(bad ? "walkfix default: FAIL\n" : "walkfix default: 4/4 PASS\n");
    return bad;
}
''', encoding="utf-8")
exe = OUT / "walkfix_default_test.exe"; batch = OUT / "build.cmd"
batch.write_text('@echo off\n' + (f'call "{setup}" >nul\nif errorlevel 1 exit /b 1\n' if setup else '') +
    f'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{src}" /Fo"{OUT}\\\\" /Fe"{exe}" /link kernel32.lib\nexit /b %errorlevel%\n')
r = subprocess.run(["cmd.exe", "/d", "/c", str(batch)], capture_output=True, text=True)
if r.returncode: print(r.stdout[-4000:], r.stderr[-2000:]); sys.exit("build failed")
with tempfile.TemporaryDirectory() as td:
    r = subprocess.run([str(exe), td], capture_output=True, text=True, timeout=30)
print(r.stdout.strip(), r.stderr.strip()); sys.exit(r.returncode)
