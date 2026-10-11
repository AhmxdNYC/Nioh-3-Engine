"""run_vpad_harness.py - VirtualPad / KeepRunningUnfocused offline tests (src/vpad_core.h, src/vpad.cpp). Not a game test.

    py -I tests/run_vpad_harness.py [path/to/Nioh3.dump.exe]

1. Builds tests/vpad_harness.cpp + src/vpad.cpp (MSVC: cl on PATH or the main checkout's tools\\msvc\\setup_x64.bat) and runs it:
   parser, composer rounds rule, the real XInputGetState hook on fake IAT slots (exclusive / disarm / merge), a fake 60 Hz game
   thread + the real named-pipe server + pipe clients (press / hold / stick / key / menu / state / errors / 2 clients), real keys
   hidden while unfocused, the KeepRunningUnfocused thunk run natively.
2. Thunk disassembly (capstone): exactly call [IsAppActive] / test al / jne / cmp switch / je / mov al,1 / lock inc / ret; the only
   registers written are al (+ rax inside the original) and the flags; rip targets = +20h pointer, +28h switch, +2Ch count.
3. Dump: IsAppActive 436CF8 writes only rax + flags (re\\scripts_re\\hook_liveness.py clob); its only reader of GameMain+1ABh;
   the three retargeted sites call it and test al right after; the WndProc writes +1ABh only on WM_ACTIVATEAPP.
4. Wiring: vpad::Start before padlat::Start and osd::Start, key filter handed to the menu, menu remote on the Present thread,
   VP group never applied by the patch loop, build files, ini keys.
Must end ALL PASSED.
"""
import pathlib
import re
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
MAIN = pathlib.Path(r"")
DUMP = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else MAIN / "re" / "Nioh3.dump.exe"
OUT = ROOT / "build" / "vptest"
OUT.mkdir(parents=True, exist_ok=True)
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


# ---- 1. harness ----
setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():  # git worktree: tools\msvc is not tracked, use the main checkout's portable MSVC
    setup = MAIN / "tools" / "msvc" / "setup_x64.bat"
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- /DUNICODE /D_UNICODE "{t}\\vpad_harness.cpp" "{s}\\vpad.cpp" '
    '/Fo"{o}\\\\" /Fe"{o}\\vpad_harness.exe" /link kernel32.lib user32.lib winmm.lib'
).format(o=OUT, t=ROOT / "tests", s=ROOT / "src")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
warn = [l for l in r.stdout.splitlines() if "warning" in l]
check(not warn, "compiler warnings: %s" % warn[:3])
r = subprocess.run([str(OUT / "vpad_harness.exe")], capture_output=True, text=True, timeout=300)
out = r.stdout.strip()
print("\n".join(l for l in out.splitlines() if not l.startswith("THUNK")))
check(r.returncode == 0 and "ALL PASSED" in out, "harness failed")

# ---- 2. thunk disassembly ----
from capstone import CS_ARCH_X86, CS_MODE_64, Cs  # noqa: E402

m = re.search(r"^THUNK((?: [0-9A-F]{2})+)$", out, re.M)
if check(m is not None, "no THUNK line"):
    code = bytes(int(x, 16) for x in m.group(1).split())
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    ins = list(md.disasm(code[:0x1D], 0))
    got = [(i.address, i.mnemonic, i.op_str) for i in ins]
    want = [(0x00, "call", "qword ptr [rip + 0x1a]"), (0x06, "test", "al, al"), (0x08, "jne", "0x1c"),
            (0x0A, "cmp", "byte ptr [rip + 0x17], 0"), (0x11, "je", "0x1c"), (0x13, "mov", "al, 1"),
            (0x15, "lock inc", "dword ptr [rip + 0x10]"), (0x1C, "ret", "")]
    check(got == want, "thunk disassembly %s" % got)
    rip = {i.address + i.size + int(re.search(r"0x([0-9a-f]+)", i.op_str).group(1), 16) for i in ins if "rip" in i.op_str}
    check(rip == {0x20, 0x28, 0x2C}, "thunk rip targets %s" % sorted(map(hex, rip)))
    print("thunk: call [IsAppActive]; test al; jne ret; cmp [switch],0; je ret; mov al,1; lock inc [count]; ret - al/flags only")

# ---- 3. dump facts ----
img = DUMP.read_bytes()
tool = MAIN / "re" / "scripts_re" / "hook_liveness.py"
if check(tool.exists(), "hook_liveness.py missing"):
    r = subprocess.run([sys.executable, "-I", str(tool), "clob", "436CF8"], capture_output=True, text=True, timeout=600)
    check(r.stdout.strip().endswith("['flags', 'rax']"), "IsAppActive clobbers %s" % r.stdout.strip()[-80:])
isact = bytes.fromhex("48 8B 05 11 2C 32 04 48 85 C0 74 07 8A 80 AB 01 00 00 C3 C3")
check(img[0x436CF8:0x436CF8 + 20] == isact, "IsAppActive bytes")
check(0x436CFF + struct.unpack_from("<i", img, 0x436CFB)[0] == 0x4759910, "IsAppActive reads [0x4759910] (GameMain)")
for site, after in ((0x436758, "45 33 ED 84 C0"), (0x436A20, "84 C0"), (0x89B2FD, "84 C0")):
    check(img[site] == 0xE8 and site + 5 + struct.unpack_from("<i", img, site + 1)[0] == 0x436CF8, "site %X calls IsAppActive" % site)
    check(img[site + 5:site + 5 + len(after.split())] == bytes.fromhex(after), "site %X: al tested right after" % site)
# every E8 caller of IsAppActive: the 3 pad / pause sites retargeted + the 9 left alone (mouse, keyboard, cursor, rumble)
callers = []
for mm in re.finditer(rb"\xE8", img[0x1000:0x3800000]):
    a = 0x1000 + mm.start()
    if a + 5 + struct.unpack_from("<i", img, a + 1)[0] == 0x436CF8:
        callers.append(a)
check(sorted(callers) == [0x4357F4, 0x4359EF, 0x435D68, 0x435D94, 0x435E69, 0x4360AC, 0x4360B9, 0x436758, 0x436A20, 0x4387FB,
                          0x89B2FD, 0x1FE63C5], "IsAppActive callers %s" % [hex(c) for c in callers])
# WM_ACTIVATEAPP handler in WndProc 878054: cmp edx,1Ch ... mov [rax+1ABh], cl
check(img.find(bytes.fromhex("88 88 AB 01 00 00"), 0x878054, 0x878054 + 0x970) > 0, "WndProc stores the WM_ACTIVATEAPP flag at +1ABh")
print("dump: IsAppActive 436CF8 writes rax+flags only; 12 callers (3 retargeted: 436758 436A20 89B2FD); WM_ACTIVATEAPP -> GameMain+1ABh")

# ---- 4. wiring ----
src = lambda n: (ROOT / "src" / n).read_text(encoding="utf-8")  # noqa: E731
dm, menu = src("dllmain.cpp"), src("menu.cpp")
i_vp, i_pl, i_osd = dm.find("vpad::Start(vi)"), dm.find("padlat::Start(li)"), dm.find("osd::Start(si)")
check(0 < i_vp < i_pl < i_osd, "dllmain: vpad::Start must run before padlat::Start and osd::Start")
check("if (vpad::KeysWanted()) menu::SetKeyFilter(&vpad::KeyFilter);" in dm, "dllmain: key filter")
check("vi.menuRemote = &menu::Remote;" in dm and "vi.pauseReason = &menu::GamePauseReason;" in dm, "dllmain: menu remote / pause")
check('if (!strcmp(g, "VP")) return false;' in dm, "dllmain: VP group is check-only")
check("ProcessRemote();" in menu[menu.find("void OnPresentImpl()"):menu.find("void OnPresentImpl()") + 400], "menu: remote on the Present thread")
check("if (f(static_cast<uint8_t*>(data), SUCCEEDED(hr))) hr = DI_OK;" in menu, "menu: DirectInput key filter")
bb = (ROOT / "build.bat").read_text(encoding="utf-8")
check("src\\vpad.cpp" in bb and "build\\vpad.obj" in bb.split("link /nologo")[1], "build.bat")
check("src/vpad.cpp" in (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"), "CMakeLists.txt")
ini = (ROOT / "Nioh3EngineFix.ini").read_text(encoding="utf-8")
for k in ("VirtualPad = 0", "KeepRunningUnfocused = 0", "VirtualPadIdleReleaseSec = 600"):
    check(re.search(r"^%s$" % re.escape(k), ini, re.M), "ini key %s" % k)
pj = (ROOT / "patches.json").read_text(encoding="utf-8")
check(all('"id": "VP%d", "group": "VP"' % i in pj for i in range(4)), "patches.json VP0-VP3")
print("wiring: start order, key filter, menu remote, VP check-only, build files, ini keys, patches.json")

if fails:
    print("FAILED (%d):" % len(fails))
    for f in fails:
        print("  - " + f)
    sys.exit(1)
print("VPAD: ALL PASSED")
