"""Pure-logic FormBlockPad transitions (UO open.md #11, #105721). No game.

Compiles the PRODUCTION FormBlockPad code from src/bosslog.cpp (state, HkFormBlockPad, FormBlockSet) against stubs: a fake
game XInputGetState behind a fake IAT slot, a fake clock and a fake player style byte. Checks: the IAT hook taken on the first
form start only, the [Spirit] FormBlockPad mask / FormBlockRT / FormBlockLS, camera stick untouched, the 30 s safety, errors
passed through, the style-shift chord tracking (RT tap / RT+A queued, toggled, B/X/Y ignored, RT held at the form start
ignored, 60 ms release debounce, per pad), FormStyleLive pass-through, the replay pulse after the form (timing, confirm,
retry, give up), "switched live" = no replay, and the #83466 queued d-pad press OR'd in.
Nothing in src/ is changed: the text is cut out of bosslog.cpp at fixed markers.
"""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
src = (root / 'src/bosslog.cpp').read_text(encoding='utf-8-sig')


def cut(begin, end):
    a = src.index(begin)
    return src[a:src.index(end, a)]


state_and_hook = cut('std::atomic<int> g_fbMask{0xFF4F}', '\nvoid** FbXInputSlot() {')
form_block_set = cut('void FormBlockSet(bool on) {', '\n// pc #56435:')

prelude = r'''
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "press_queue.h"
ULONGLONG g_fakeNow = 1000;
#define GetTickCount64() g_fakeNow
double Now() { return (double)g_fakeNow / 1000.0; }
char g_lastLog[512];
int g_logs = 0;
void Log(const char* f, ...) { va_list a; va_start(a, f); vsnprintf(g_lastLog, sizeof g_lastLog, f, a); va_end(a); ++g_logs; }
int g_fakeStyle = 0; bool g_styleOk = true;
template <class T> bool RdB(uint64_t, T& v) { if (!g_styleOk) return false; v = (T)g_fakeStyle; return true; }
pressq::State g_pressQ;
'''

harness = r'''
uint64_t FkPlayer() { return 0x10000; }
FbState g_in[4]; DWORD g_ret[4] = {};
DWORD WINAPI FakeGet(DWORD user, void* st) {
    if (user >= 4) return ERROR_DEVICE_NOT_CONNECTED;
    if (st) *(FbState*)st = g_in[user];
    return g_ret[user];
}
int checks = 0, fails = 0;
#define CHECK(c) do { ++checks; if (!(c)) { ++fails; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
// one game read through the (possibly hooked) IAT slot
FbState Read(ULONGLONG t, WORD buttons = 0, BYTE lt = 0, BYTE rt = 0, SHORT lx = 0, SHORT ly = 0, SHORT rx = 0, SHORT ry = 0, DWORD user = 0, DWORD* ret = nullptr) {
    g_fakeNow = t;
    g_in[user] = FbState{7, FbPad{buttons, lt, rt, lx, ly, rx, ry}};
    FbState st = {};
    const DWORD r = ((FbXGet)g_fakeIat)(user, &st);
    if (ret) *ret = r;
    return st;
}
constexpr WORD A = 0x1000, B = 0x2000, X = 0x4000, Y = 0x8000, START = 0x0010, BACK = 0x0020, R3 = 0x0080;
bool LogHas(const char* s) { return strstr(g_lastLog, s) != nullptr; }
void On(ULONGLONG t) { g_fakeNow = t; FormBlockSet(true); }
void Off(ULONGLONG t) { g_fakeNow = t; FormBlockSet(false); }

int main() {
    g_fakeIat = (void*)&FakeGet;
    // ---- mask, triggers and left stick all 0 = the feature is off: no hook, no block ----
    g_fbMask = 0; g_fbRT = 0; g_fbLS = 0;
    On(1000); CHECK(g_fakeIat == (void*)&FakeGet && !g_fbActive.load() && g_logs == 0);
    Off(1010);
    g_fbMask = 0xFF4F; g_fbRT = 1; g_fbLS = 1;
    // ---- before any form: the game import is untouched, reads pass through ----
    { auto s = Read(1020, 0xFFFF, 200, 200, 1, 2, 3, 4); CHECK(s.pad.buttons == 0xFFFF && s.pad.rt == 200 && s.pad.lx == 1); }

    // ---- first form start takes the IAT slot (outermost) and blocks ----
    g_fakeStyle = 0;
    On(1100);
    CHECK(g_fakeIat == (void*)&HkFormBlockPad && g_fbPrev == (FbXGet)&FakeGet && g_fbActive.load() && g_fbSince.load() == 1100);
    CHECK(g_fbPressStyle.load() == -1);  // no free style read yet (only the hooked import reads it, outside forms)
    { auto s = Read(1110, 0xFFFF, 100, 100, 1000, -1000, 2000, -2000);
      CHECK(s.pad.buttons == (START | BACK | R3));            // 0xFF4F: everything but Start, Back and R3
      CHECK(s.pad.lt == 0 && s.pad.rt == 0 && s.pad.lx == 0 && s.pad.ly == 0);
      CHECK(s.pad.rx == 2000 && s.pad.ry == -2000 && s.packet == 7);  // camera free, packet kept
      CHECK(g_fbDropped.load() == 1); }
    { auto s = Read(1120, START, 0, 0, 0, 0, 500, 0); CHECK(s.pad.buttons == START && s.pad.rx == 500 && g_fbDropped.load() == 1); }  // nothing blocked: not counted
    Read(1130, 0, 1); CHECK(g_fbDropped.load() == 2);  // a trigger alone counts as a blocked read
    Read(1140, 0, 0, 0, 0, 1); CHECK(g_fbDropped.load() == 3);
    // a failed read is passed through untouched
    { g_ret[0] = ERROR_DEVICE_NOT_CONNECTED; DWORD r = 0; auto s = Read(1150, A, 0, 0, 9, 9, 0, 0, 0, &r);
      CHECK(r == ERROR_DEVICE_NOT_CONNECTED && s.pad.buttons == A && s.pad.lx == 9); g_ret[0] = 0; }
    // every pad is masked (built-in + virtual pad)
    { auto s = Read(1160, A | START, 0, 0, 0, 0, 0, 0, 2); CHECK(s.pad.buttons == START); }
    // the ini mask decides which buttons are held back
    g_fbMask = A; g_fbRT = 0; g_fbLS = 0;
    { auto s = Read(1170, A | B, 50, 50, 100, 100); CHECK(s.pad.buttons == B && s.pad.lt == 50 && s.pad.rt == 50 && s.pad.lx == 100); }
    g_fbMask = 0xFF4F; g_fbRT = 1; g_fbLS = 1;
    // a second start while active changes nothing
    g_fbDropped = 5; On(1180); CHECK(g_fbDropped.load() == 5 && g_fbSince.load() == 1100);

    // ---- FormStyleLive=1: RT (>128) and A while RT is held reach the game at once; LT and other buttons stay blocked ----
    g_fbStyle = 1; g_fbStyleLive = 1;
    { auto s = Read(1200, A | X, 200, 200); CHECK(s.pad.rt == 200 && s.pad.lt == 0 && s.pad.buttons == A); }
    { auto s = Read(1210, A, 0, 128); CHECK(s.pad.rt == 0 && s.pad.buttons == 0); }  // RT 128 is not held
    g_fbStyleLive = 0;
    { auto s = Read(1220, A, 0, 200); CHECK(s.pad.rt == 0 && s.pad.buttons == 0); }
    g_fbStyle = 0; g_fbStyleLive = 1;
    { auto s = Read(1230, A, 0, 200); CHECK(s.pad.rt == 0 && s.pad.buttons == 0); }  // FormStyleSwitch=0 also stops the live pass
    g_fbStyle = 1;

    // ---- 30 s safety: never masks longer than 30 s even without a restore ----
    { auto s = Read(1100 + 29999, A); CHECK(s.pad.buttons == 0); }
    { auto s = Read(1100 + 30000, A, 9, 9, 9, 9); CHECK(s.pad.buttons == A && s.pad.lt == 9 && s.pad.lx == 9); }
    Off(31200); CHECK(!g_fbActive.load() && LogHas("released"));
    CHECK(g_fakeIat == (void*)&HkFormBlockPad);  // the hook stays installed, it is idle outside forms
    { auto s = Read(31300, 0xFFFF, 200, 200, 5, 5); CHECK(s.pad.buttons == 0xFFFF && s.pad.rt == 200 && s.pad.lx == 5); }
    CHECK(g_styleFree.load() == 0);  // outside a form pad 0 reads keep the free style current (every 50 ms)

    // ---- style chord tracking during a form (FormStyleSwitch=1, FormStyleLive=0: mask + replay) ----
    g_fbStyleLive = 0;
    ULONGLONG t = 40000;
    On(t); CHECK(g_fbStyleQ.load() == 0 && g_fbPressStyle.load() == 0);
    // RT already held at the form start is not a style shift, even after its release
    Read(t + 10, 0, 0, 255); Read(t + 50, 0); Read(t + 200, 0); CHECK(g_fbStyleQ.load() == 0 && !g_fbUser[0].rt);
    // RT tap: queued once RT has been released for 60 ms (a single read without RT is not a release)
    Read(t + 300, 0, 0, 255); Read(t + 340, 0); CHECK(g_fbUser[0].rt && g_fbStyleQ.load() == 0);
    Read(t + 359, 0); CHECK(g_fbStyleQ.load() == 0);
    Read(t + 360, 0); CHECK(g_fbStyleQ.load() == 1 && g_fbInjUser.load() == 0 && LogHas("RT tap") && LogHas("queued"));
    // a second tap toggles it off (two shifts = same style)
    Read(t + 400, 0, 0, 255); Read(t + 470, 0); CHECK(g_fbStyleQ.load() == 0 && LogHas("cancelled"));
    // RT + A: Type E style shift; the A press time is remembered for FormStyleFxHide
    g_fbStyleT = -1e9;
    Read(t + 500, 0, 0, 255); Read(t + 520, A, 0, 255); CHECK(g_fbStyleT.load() == (double)(t + 520) / 1000.0);
    Read(t + 540, A, 0, 255); CHECK(g_fbStyleT.load() == (double)(t + 520) / 1000.0);  // only the first A of the chord
    Read(t + 560, 0); Read(t + 620, 0); CHECK(g_fbStyleQ.load() == 2 && LogHas("RT+A"));
    // RT + B / X / Y (burst break, guardian skill) is not a style shift: the queue is untouched
    for (WORD other : {B, X, Y}) {
        Read(t + 700, 0, 0, 255); Read(t + 720, other, 0, 255); Read(t + 740, 0); Read(t + 800, 0);
        CHECK(g_fbStyleQ.load() == 2); t += 200;
    }
    // a second pad's tap is tracked on its own and becomes the replay pad
    Read(t + 900, 0, 0, 0, 0, 0, 0, 0, 1); Read(t + 961, 0, 0, 0, 0, 0, 0, 0, 1);  // pad 1 lets go of the start-held RT state
    Read(t + 1000, 0, 0, 255, 0, 0, 0, 0, 1); Read(t + 1061, 0, 0, 0, 0, 0, 0, 0, 1);
    CHECK(g_fbStyleQ.load() == 0 && g_fbInjUser.load() == 1);  // toggled the RT+A off (queue holds one chord)
    Read(t + 1100, 0, 0, 255, 0, 0, 0, 0, 1); Read(t + 1161, 0, 0, 0, 0, 0, 0, 0, 1);
    CHECK(g_fbStyleQ.load() == 1 && g_fbInjUser.load() == 1);

    // ---- form end with a queued tap: replay pulse on the queued pad, confirm by the style byte ----
    ULONGLONG e = t + 2000;
    Off(e); CHECK(g_fbInjT0.load() == e && g_fbStyle0.load() == 0 && g_fbTries.load() == 1 && LogHas("replaying"));
    { auto s = Read(e + 29, 0, 0, 0, 0, 0, 0, 0, 1); CHECK(s.pad.rt == 0); }
    { auto s = Read(e + 30, 0, 0, 0, 0, 0, 0, 0, 1); CHECK(s.pad.rt == 255 && s.pad.buttons == 0); }  // RT tap: no A
    { auto s = Read(e + 30); CHECK(s.pad.rt == 0); }                                                // other pad: untouched
    { auto s = Read(e + 149, 0, 0, 0, 0, 0, 0, 0, 1); CHECK(s.pad.rt == 255); }
    { auto s = Read(e + 150, 0, 0, 0, 0, 0, 0, 0, 1); CHECK(s.pad.rt == 0 && g_fbInjT0.load() == e); }  // style unchanged: wait
    Read(e + 399, 0, 0, 0, 0, 0, 0, 0, 1); CHECK(g_fbTries.load() == 1);
    Read(e + 400, 0, 0, 0, 0, 0, 0, 0, 1); CHECK(g_fbTries.load() == 2 && g_fbInjT0.load() == e + 400 && LogHas("retry 2"));
    { auto s = Read(e + 430, 0, 0, 0, 0, 0, 0, 0, 1); CHECK(s.pad.rt == 255); }  // second pulse
    g_fakeStyle = 1;
    Read(e + 550, 0, 0, 0, 0, 0, 0, 0, 1); CHECK(g_fbInjT0.load() == 0 && g_fbStyleQ.load() == 0 && LogHas("style changed"));
    { auto s = Read(e + 600, 0, 0, 0, 0, 0, 0, 0, 1); CHECK(s.pad.rt == 0); }

    // ---- RT+A replay: A only 60..120 ms inside the RT pulse; gives up after 8 tries ----
    g_fakeStyle = 1; t = e + 1000;
    Read(t, 0); Read(t + 60, 0);  // free style read (pad 0, outside a form) -> 1
    On(t + 100); CHECK(g_fbPressStyle.load() == 1);
    Read(t + 110, 0); Read(t + 170, 0);  // pad 0 drops the start-held RT state
    Read(t + 200, 0, 0, 255); Read(t + 210, A, 0, 255); Read(t + 230, 0); Read(t + 290, 0); CHECK(g_fbStyleQ.load() == 2 && g_fbInjUser.load() == 0);
    e = t + 400; Off(e); CHECK(g_fbStyle0.load() == 1);
    { auto s = Read(e + 59); CHECK(s.pad.rt == 255 && s.pad.buttons == 0); }
    { auto s = Read(e + 60); CHECK(s.pad.rt == 255 && s.pad.buttons == A); }
    { auto s = Read(e + 119); CHECK(s.pad.buttons == A); }
    { auto s = Read(e + 120); CHECK(s.pad.rt == 255 && s.pad.buttons == 0); }
    ULONGLONG tt = e;
    for (int i = 0; i < 7; ++i) { tt += 400; Read(tt); }
    CHECK(g_fbTries.load() == 8 && g_fbInjT0.load() == tt);
    Read(tt + 400); CHECK(g_fbInjT0.load() == 0 && g_fbStyleQ.load() == 0 && LogHas("gave up"));
    { auto s = Read(tt + 430); CHECK(s.pad.rt == 0); }
    // style unreadable at the confirm: treated as done (no endless pulses)
    g_fakeStyle = 1; t = tt + 1000;
    On(t); Read(t + 10); Read(t + 70); Read(t + 100, 0, 0, 255); Read(t + 170); CHECK(g_fbStyleQ.load() == 1);
    Off(t + 300); g_styleOk = false;
    Read(t + 450); CHECK(g_fbInjT0.load() == 0 && LogHas("unreadable"));
    g_styleOk = true;

    // ---- FormStyleLive=1: the style changed live during the form -> no replay ----
    g_fbStyleLive = 1; g_fakeStyle = 0; t += 2000;
    Read(t); Read(t + 60);  // free style 0
    On(t + 100); Read(t + 110); Read(t + 170);
    { auto s = Read(t + 200, A, 0, 255); CHECK(s.pad.rt == 255 && s.pad.buttons == A); }  // the game switches at once
    g_fakeStyle = 1; Read(t + 220); Read(t + 280); CHECK(g_fbStyleQ.load() == 2);
    Off(t + 400); CHECK(g_fbStyleQ.load() == 0 && g_fbInjT0.load() == 0 && LogHas("switched live"));
    // ... but if the game ignored the live chord (style unchanged), the replay still runs
    g_fakeStyle = 0; t += 1000;
    Read(t); Read(t + 60);
    On(t + 100); Read(t + 110); Read(t + 170); Read(t + 200, 0, 0, 255); Read(t + 270); CHECK(g_fbStyleQ.load() == 1);
    Off(t + 400); CHECK(g_fbInjT0.load() == t + 400);
    g_fakeStyle = 1; Read(t + 560); CHECK(g_fbInjT0.load() == 0);
    // a new form start clears a pending replay
    g_fakeStyle = 0; t += 1000;
    On(t); Read(t + 10); Read(t + 70); Read(t + 100, 0, 0, 255); Read(t + 170); g_fbStyleLive = 0; Off(t + 300); CHECK(g_fbInjT0.load() == t + 300);
    On(t + 310); CHECK(g_fbInjT0.load() == 0 && g_fbStyleQ.load() == 0);
    { auto s = Read(t + 340); CHECK(s.pad.rt == 0); }
    Off(t + 400); CHECK(g_fbInjT0.load() == 0);
    // RT counts as held only above 128 (bosslog.cpp pad.rt > 128): mid pulls 64 / 128 are no RT, 129 / 200 are (a threshold change fails here)
    g_fakeStyle = 0; t += 1000;
    On(t); Read(t + 10); Read(t + 70);
    Read(t + 100, 0, 0, 64); CHECK(!g_fbUser[0].rt);
    Read(t + 110, 0, 0, 128); CHECK(!g_fbUser[0].rt);
    Read(t + 180); CHECK(g_fbStyleQ.load() == 0);
    Read(t + 200, 0, 0, 129); CHECK(g_fbUser[0].rt);
    Read(t + 270); CHECK(g_fbStyleQ.load() == 1);   // an RT tap at 129: queued
    Read(t + 300, 0, 0, 200); CHECK(g_fbUser[0].rt);
    Read(t + 370); CHECK(g_fbStyleQ.load() == 0);   // a second tap at 200 toggles it off
    Read(t + 400, A, 0, 64); Read(t + 410, A, 0, 64); Read(t + 480); CHECK(g_fbStyleQ.load() == 0 && !g_fbUser[0].rt);  // A with RT 64: no chord
    Off(t + 500); CHECK(g_fbInjT0.load() == 0);
    // FormStyleSwitch=0: no tracking at all
    g_fbStyle = 0; t += 1000;
    On(t); Read(t + 10); Read(t + 70); Read(t + 100, 0, 0, 255); Read(t + 170); CHECK(g_fbStyleQ.load() == 0);
    Off(t + 300); CHECK(g_fbInjT0.load() == 0);
    g_fbStyle = 1;

    // ---- #83466 queued soul-core d-pad press: OR'd in for the 120 ms pulse, also during a form ----
    t += 1000;
    pressq::Queue(g_pressQ, 1, t);
    CHECK(pressq::Tick(g_pressQ, t + 10, false, false, false) == pressq::Step::Pulse);
    { auto s = Read(t + 10, A); CHECK(s.pad.buttons == (A | 0x0004)); }
    { auto s = Read(t + 129); CHECK(s.pad.buttons == 0x0004); }
    { auto s = Read(t + 130); CHECK(s.pad.buttons == 0); }
    pressq::Queue(g_pressQ, 2, t + 200); On(t + 200);
    CHECK(pressq::Tick(g_pressQ, t + 210, false, false, false) == pressq::Step::Pulse);
    { auto s = Read(t + 220, 0x0002 | A); CHECK(s.pad.buttons == 0x0002); }  // real d-pad down masked, the replay pulse added
    Off(t + 300);

    printf("form block pad: %d checks, %d failed (production bosslog.cpp HkFormBlockPad/FormBlockSet; offline)\n", checks, fails);
    return fails ? 1 : 0;
}
'''

mid = '\nvoid* g_fakeIat = nullptr;\nvoid** FbXInputSlot() { return &g_fakeIat; }  // the game import slot (fake)\n'
unit_text = prelude + state_and_hook + mid + form_block_set + '\n' + harness
out = root.parents[1] / 'build/form-block-pad-test'
out.mkdir(parents=True, exist_ok=True)
unit = out / 'test.cpp'
unit.write_text(unit_text, encoding='utf-8')
def msvc_setup():  # MSVC_SETUP, else this machine's pmsvc, else the repo's tools/msvc/setup_x64.bat
    if os.environ.get('MSVC_SETUP'):
        return os.environ['MSVC_SETUP']
    for c in (Path('C:/Users/User/pmsvc/msvc/setup_x64.bat'), root.parents[1] / 'tools/msvc/setup_x64.bat'):
        if c.exists():
            return str(c)
    raise SystemExit('no MSVC: set MSVC_SETUP, or install pmsvc / tools/msvc/setup_x64.bat in the repo')


setup = msvc_setup()
exe = out / 'test.exe'
cmd = (f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W3 /wd4244 /I"{root / "src"}" '
       f'/Fo"{out / "test.obj"}" /Fe"{exe}" "{unit}"')
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
print(r.stdout, end=''); print(r.stderr, end=''); r.check_returncode()
subprocess.run([str(exe)], check=True, timeout=30)
