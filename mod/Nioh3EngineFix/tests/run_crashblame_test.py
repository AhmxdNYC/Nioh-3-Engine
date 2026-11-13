"""Build crashblame_test plus a tiny test DLL (copied as ours / proxy / game-folder / unloaded) and run it.

Run from an MSVC developer environment. No game process.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

MOD = Path(__file__).resolve().parents[1]

DLL = r'''
#include <windows.h>
extern "C" __declspec(dllexport) void Crash() { *(volatile int*)8 = 1; }
extern "C" __declspec(dllexport) void Call(void (*f)()) { f(); volatile int keep = 0; (void)keep; }
extern "C" __declspec(dllexport) void CrashInWindows() { lstrlenA(nullptr); RaiseException(0xE0000001, 0, 0, nullptr); }
'''
RC = r'''
1 VERSIONINFO
FILEVERSION 1,0,0,0
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904b0"
    BEGIN
      VALUE "CompanyName", "Blame Test Co"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x409, 1200
  END
END
'''


def run(args, cwd, check=True):
    r = subprocess.run(args, cwd=cwd, capture_output=True, text=True, timeout=120)
    if check and r.returncode:
        print(r.stdout, r.stderr)
        raise SystemExit(f"failed: {args[0]} ({r.returncode})")
    return r


def main(t: Path) -> int:
    (t / "blamedll.cpp").write_text(DLL)
    (t / "blamedll.rc").write_text(RC)
    run(["rc", "/nologo", "blamedll.rc"], t)
    run(["cl", "/nologo", "/LD", "/MT", "/Od", "blamedll.cpp", "blamedll.res", "/Feblamedll.dll", "/link", "user32.lib"], t)
    for n in ("Nioh3Fake.asi", "Nioh3Gone.asi", "winmm.dll", "other.dll"):
        shutil.copyfile(t / "blamedll.dll", t / n)
    run(["cl", "/nologo", "/O2", "/MT", "/EHsc", "/std:c++17", "/W4", "/DUNICODE", "/D_UNICODE",
         str(MOD / "tests/crashblame_test.cpp"), str(MOD / "src/crashblame.cpp"), "/Fecrashblame_test.exe",
         "/link", "version.lib"], t)
    r = run([str(t / "crashblame_test.exe")], t, check=False)
    print(r.stdout)
    print(r.stderr)
    print(f"exit code {r.returncode:#x}")
    return 0 if r.returncode == 0 and "ALL PASSED" in r.stdout else 1


if len(sys.argv) > 1:  # keep the build folder for debugging
    Path(sys.argv[1]).mkdir(parents=True, exist_ok=True)
    raise SystemExit(main(Path(sys.argv[1]).resolve()))
with tempfile.TemporaryDirectory(prefix="n3-blame-") as d:
    raise SystemExit(main(Path(d)))
