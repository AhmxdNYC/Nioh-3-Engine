"""Offline build+run of menurepeat_test.cpp (#137071 hold-to-repeat in game menus) + checks menu.cpp wires it into the pad hook. No game needed."""
import pathlib, subprocess, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "menutest"
OUT.mkdir(parents=True, exist_ok=True)
setup = next((p for p in (REPO / "tools/msvc/setup_x64.bat", pathlib.Path(r"D:/nioh3-wt/rc-187/tools/msvc/setup_x64.bat"), pathlib.Path.home() / "nioh3-engine/tools/msvc/setup_x64.bat") if p.exists()), None)
mc = (ROOT / "src/menu.cpp").read_text(encoding="utf-8-sig")
for need, why in (("menurepeat::Step(g_mrep[idx], raw, act, t, g_cfg.gmRepeatSpeed)", "pad hook must run the repeat step"),
                  ("g_cfg.gmRepeat && g_gameMenuOpen && !g_open && !s.latch", "repeat only while a game menu is open and the mod menu is closed"),
                  ("g_gameMenuOpen = (g_menuShown || g_stopPaused || ctlShown)", "gate must follow the game's menu signal / pause flags / control-off (#137478)"),
                  ('!strcmp(key, "GameMenuRepeat")', "rows must be on Settings > Game"),
                  ("ctlNow = menurepeat::ControlOff(ok, ctlAgg);", "#183 control-off gate must be the game's bit-0 predicate, no reason-id list"),
                  ('crashreport::SetStateF("game menu gate"', "#183 report snapshot must show which game signal holds the gate"),
                  ("g_gameMenuStick = menurepeat::StickHeldBack(g_menuShown, g_stopPaused)", "#140978 stick hold-back must follow the menu signal and game time"),
                  ("if (act && alive && g_gameMenuStick && (g.sThumbLX || g.sThumbLY))", "#140978 pad hook must centre the left stick in time-running game menus")):
    if need not in mc: sys.exit("FAIL: " + why)
src = ROOT / "tests/menurepeat_test.cpp"
exe = OUT / "menurepeat_test.exe"
batch = OUT / "menurepeat_build.cmd"
batch.write_text('@echo off\n' + (f'call "{setup}" >nul\nif errorlevel 1 exit /b 1\n' if setup else '') +
    f'cl /nologo /O2 /MT /Gy /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{src}" /Fo"{OUT}\\\\" /Fe"{exe}" /link /OPT:REF\nexit /b %errorlevel%\n')
r = subprocess.run(["cmd.exe", "/d", "/c", str(batch)], capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-6000:], r.stderr[-2000:]); sys.exit("build failed")
r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
print(r.stdout.strip(), r.stderr.strip())
sys.exit(r.returncode)
