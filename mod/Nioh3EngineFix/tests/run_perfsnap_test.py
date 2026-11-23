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
    pathlib.Path(r"C:\nioh3-engine\tools\msvc\setup_x64.bat")) if p.exists()), None)
exe = OUT / "perfsnap_test.exe"
cmd = (f'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{ROOT / "tests/perfsnap_test.cpp"}" '
       f'"{ROOT / "src/perfsnap.cpp"}" /Fo"{OUT}\\\\" /Fe"{exe}" /link kernel32.lib user32.lib')
batch = OUT / "build.cmd"
batch.write_text('@echo off\n' + (f'call "{setup}" >nul\n' if setup else '') + cmd + '\nexit /b %errorlevel%\n')
r = subprocess.run(["cmd.exe", "/d", "/c", str(batch)], capture_output=True, text=True)
if r.returncode or any("warning" in l for l in r.stdout.splitlines()):
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("perfsnap test build failed or warned")
jpath, tpath, hpath = OUT / "perf.json", OUT / "perf.txt", OUT / "perf_hitches.json"
mpath, npath = OUT / "perf_menu_frozen.json", OUT / "perf_menu_open_closed.json"
for f in (mpath, npath):
    f.unlink(missing_ok=True)
r = subprocess.run([str(exe), str(jpath), str(tpath), str(hpath), str(mpath), str(npath)], capture_output=True, text=True, timeout=60, cwd=str(OUT))
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

# #98471: the block with 2001 freezes must pass the site's own perf checks (crash + player report APIs, 8192 bytes, depth 3)
hdoc = json.loads(hpath.read_text())
for k in ("hitch_summary", "hitch_causes", "hitches", "hitches_in_report"):
    if k not in hdoc:
        sys.exit(f"freeze perf JSON lacks {k}")
if len(json.dumps(hdoc)) > 8192:
    sys.exit(f"freeze perf JSON is {len(json.dumps(hdoc))} bytes as the site measures it (> 8192)")
r = subprocess.run([sys.executable, "-I", str(REPO / "site/tests/perf_field_test.py"), str(hpath)], capture_output=True, text=True, timeout=60)
print(r.stdout.strip())
if r.returncode or "FAIL" in r.stdout:
    print(r.stderr)
    sys.exit("site rejects the freeze perf JSON")
print("FREEZE JSON OK:", len(json.dumps(hdoc)), "bytes (json.dumps),", hdoc["hitches_in_report"], "detailed of", hdoc["hitch_summary"]["count"])

# #98578: the player-report JSON with the menu open (frozen window) and after it closed must both parse and pass the site check
for f, frozen in ((mpath, True), (npath, False)):
    try:
        d = json.loads(f.read_text())
    except Exception as e:
        sys.exit(f"{f.name} is not JSON: {e}")
    if (d.get("window") == "before_menu_open") != frozen or not isinstance(d.get("bound_why"), str):
        sys.exit(f"{f.name}: window/bound_why wrong: {d.get('window')!r} {d.get('bound_why')!r}")
    # the site's own validators (perf_field_test.py asserts its sample's 'bound gpu', which a menu window need not be)
    r = subprocess.run([sys.executable, "-I", str(ROOT / "tests/site_perf_check.py"), str(f)], capture_output=True, text=True, timeout=60)
    print(r.stdout.strip())
    if r.returncode:
        print(r.stderr[-2000:])
        sys.exit(f"site rejects {f.name}")
    print(f"MENU JSON OK (frozen={int(frozen)}):", len(json.dumps(d)), "bytes")
