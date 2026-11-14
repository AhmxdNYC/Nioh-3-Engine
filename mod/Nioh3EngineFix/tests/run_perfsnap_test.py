"""Offline test of the report / crash performance snapshot (perfsnap.cpp, TASK #97638). No game. Builds with /W4 (a warning fails),
runs the unit test, checks the JSON with Python's json module and prints it."""
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "perfsnaptest"
OUT.mkdir(parents=True, exist_ok=True)
setup = next((p for p in (REPO / "tools/msvc/setup_x64.bat",
    pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\tools\msvc\setup_x64.bat")) if p.exists()), None)
exe = OUT / "perfsnap_test.exe"
cmd = (f'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{ROOT / "tests/perfsnap_test.cpp"}" '
       f'"{ROOT / "src/perfsnap.cpp"}" /Fo"{OUT}\\\\" /Fe"{exe}" /link kernel32.lib user32.lib')
batch = OUT / "build.cmd"
batch.write_text('@echo off\n' + (f'call "{setup}" >nul\n' if setup else '') + cmd + '\nexit /b %errorlevel%\n')
r = subprocess.run(["cmd.exe", "/d", "/c", str(batch)], capture_output=True, text=True)
if r.returncode or any("warning" in l for l in r.stdout.splitlines()):
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("perfsnap test build failed or warned")
jpath, tpath = OUT / "perf.json", OUT / "perf.txt"
r = subprocess.run([str(exe), str(jpath), str(tpath)], capture_output=True, text=True, timeout=60, cwd=str(OUT))
print(r.stdout.strip())
if r.returncode or "ALL PASSED" not in r.stdout:
    print(r.stderr)
    sys.exit("perfsnap test failed")
doc = json.loads(jpath.read_text())
for k in ("avg_fps", "low1_fps", "avg_ms", "bound", "cpu", "gpu", "driver", "ram_mb", "vram_mb", "backbuffer", "upscaler",
          "frame_gen", "game_settings", "mod_settings", "parts", "busiest_thread", "threads", "threads_window_s", "thread_count"):
    if k not in doc:
        sys.exit(f"perf JSON lacks {k}")
print("JSON OK:", len(jpath.read_bytes()), "bytes")
