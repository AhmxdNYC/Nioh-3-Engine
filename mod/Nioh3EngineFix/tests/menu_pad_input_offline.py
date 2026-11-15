"""Pure-logic pad/keyboard input transitions of the mod menu (UO open.md #11, #105721). No game.

Compiles the PRODUCTION code from src/menu.cpp (input state + PadNav/KeyNav/Request/ForceCloseIfDead/PadFeed, KeyFeed,
NavRepeat) against small stubs, with a fake clock, and checks every pad transition the game can see:
held/released edges, stick hysteresis, the Start triple-tap open gesture (swallow + one-tap replay + hold pass-through),
the hold gesture, the release latch after open/close, blanking while open (camera stick live), the MaskGameButtons
lease, the pad-vs-keyboard source split (g_padPressed drives g_okFromPad), the Up/Down/Left/Right repeat, and (UO #14) the
input modality: it switches only on trusted press edges (VPad virtual pad/keys never flip it) and pad bits held at the switch
to keyboard are snapshotted, so returning to the pad gives no phantom press, repeat or held state.
Nothing in src/ is changed: the text is cut out of menu.cpp at fixed markers, so a refactor that moves a marker fails here.
"""
from pathlib import Path
import os
import re
import subprocess

root = Path(__file__).resolve().parents[1]
src = (root / 'src/menu.cpp').read_text(encoding='utf-8-sig')


def cut(begin, end):
    a = src.index(begin)
    return src[a:src.index(end, a)]


state_and_pad = cut('volatile LONG g_open = 0;', '\nvoid PadFeedSafe(')
key_feed = cut('void KeyFeed(uint8_t* keys, bool own) {', '\nvoid KeyFeedSafe(')
nav_repeat = cut('static const uint32_t kNavDirs[4]', '\nvoid OnPresentImpl()')
consts = cut('enum : uint32_t { N_UP = 1', 'volatile LONG g_open = 0;')

# The stub Cfg uses the SHIPPED tap defaults, and they must agree in all three places: Nioh3EngineFix.ini, the Cfg struct
# default and the GetPrivateProfileIntW fallback in menu.cpp. TapGapMs is pinned to 300 (a change must be a deliberate test edit).
ini = (root / 'Nioh3EngineFix.ini').read_text(encoding='utf-8-sig')
shipped = {}
for key, field in (('TapWindowMs', 'tapWindowMs'), ('TapGapMs', 'tapGapMs'), ('SwallowTaps', 'swallowTaps')):
    in_ini = int(re.search(r'^' + key + r'\s*=\s*(\d+)', ini, re.M).group(1))
    in_struct = int(re.search(r'\bint ' + field + r' = (\d+);', src).group(1))
    in_read = int(re.search(r'L"' + key + r'", (\d+), f\)', src).group(1))
    assert in_ini == in_struct == in_read, f'{key}: ini {in_ini}, Cfg default {in_struct}, ini-read fallback {in_read} differ'
    shipped[key] = in_ini
assert shipped['TapGapMs'] == 300, f"shipped TapGapMs is {shipped['TapGapMs']}, the tap timings below assume 300"
print(f"shipped defaults: TapWindowMs {shipped['TapWindowMs']}, TapGapMs {shipped['TapGapMs']}, SwallowTaps {shipped['SwallowTaps']} (ini = Cfg = fallback)")

prelude = r'''
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <xinput.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include "paddebug.h"
ULONGLONG g_fakeNow = 1000;
#define GetTickCount64() g_fakeNow
namespace padviz { template <class... A> void Feed(A...) {} }
namespace padcard { template <class... A> void Feed(A...) {} }  // UO #8 status card feed (read-only)
namespace inputlog { template <class... A> void ObservePad(A...) {} template <class... A> void ObserveKey(A...) {} }
char g_mlog[32768]; size_t g_mlogLen = 0; int g_mlogLines = 0;  // #105941: captured MENU log lines, newline separated
void MlogAdd(const char* b) { size_t n = strlen(b); if (g_mlogLen + n + 2 < sizeof g_mlog) { memcpy(g_mlog + g_mlogLen, b, n); g_mlogLen += n; g_mlog[g_mlogLen++] = 10; g_mlog[g_mlogLen] = 0; } ++g_mlogLines; }
#define MLOG(...) do { char b_[512]; snprintf(b_, sizeof b_, __VA_ARGS__); MlogAdd(b_); } while (0)
struct Cfg {
    UINT vk = VK_F10; uint8_t dik = 0x44; char keyName[16] = "F10";
    uint32_t holdMask = XINPUT_GAMEPAD_START; int holdMs = 5000; int gesture = 1;
    int tapWindowMs = TAPWINDOW, tapGapMs = TAPGAP, swallowTaps = SWALLOW; uint8_t escDik = 0x01;  // shipped defaults (checked below)
};
Cfg g_cfg;
volatile LONG g_disabled = 0;
std::atomic<uint64_t> g_coverToken{0}, g_coverCancelUntil{0};
std::atomic<bool> g_coverCancel{false};
bool g_coverCaptures = false;
void CancelCoverInput() { g_coverCancel.store(true); g_coverCancelUntil = GetTickCount64() + 150; }
bool CoverCaptures(ULONGLONG) { return g_coverCaptures; }
int g_syncFreeze = 0;
void SyncFreeze() { ++g_syncFreeze; }
ULONGLONG g_rep[4] = {};
'''

harness = r'''
int checks = 0, fails = 0;
// UO #14: stand-in for vpad::InputVirtual (which pad slots / the keyboard carry VPad's scripted input on this read)
unsigned g_virtPad = 0; bool g_virtKeys = false;
bool FakeVirtual(int dev, unsigned idx) { return dev == 0 ? ((g_virtPad >> idx) & 1u) != 0 : g_virtKeys; }
#define CHECK(c) do { ++checks; if (!(c)) { ++fails; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

void ResetAll() {
    for (PadSlot& s : g_pad) s = PadSlot{};
    g_open = 0; g_pressed = 0; g_padPressed = 0; g_toggleReq = 0; g_toggleAt = 0; g_toggleSrc = 0;
    g_gameMask = 0; g_gameMaskUntil = 0; g_disabled = 0; g_rbOn = 0; g_rbKeyMsg = 0;
    for (int i = 0; i < 4; ++i) { g_padHeld[i] = 0; g_padHeldAt[i] = 0; g_rep[i] = 0; }
    g_keyHeld = 0; g_keyHeldAt = 0; g_keyLatch = 0; g_keyPrev = 0; g_holdSince = 0; g_holdSeen = 0;
    g_escTaps = 0; g_escTap0 = g_escLast = g_escSynth = g_escMuteUntil = 0; g_escPass = g_escPrevDown = false;
    g_coverToken = 0; g_coverCancelUntil = 0; g_coverCancel = false; g_coverCaptures = false;
    g_modality = MOD_PAD; g_modalitySwitches = 0; g_inputVirtual = nullptr; g_virtPad = 0; g_virtKeys = false;
    g_virtLogged = 0; g_modalityLogged = 0; g_mlog[0] = 0; g_mlogLen = 0; g_mlogLines = 0;
    g_cfg = Cfg{};
    paddebug::g_on = 1; paddebug::g_store.blocked.store(0);
    g_fakeNow = 1000; g_lastPresent = 0;
}
// One game XInputGetState read: what the game gets after the hook. alive = the overlay presented just now.
XINPUT_STATE Feed(ULONGLONG t, WORD buttons, SHORT lx = 0, SHORT ly = 0, SHORT rx = 0, SHORT ry = 0, BYTE lt = 0, BYTE rt = 0,
                  bool own = false, bool alive = true, DWORD idx = 0) {
    g_fakeNow = t;
    if (alive) g_lastPresent = (LONG64)t;
    XINPUT_STATE st = {};
    st.dwPacketNumber = 7;
    st.Gamepad.wButtons = buttons; st.Gamepad.sThumbLX = lx; st.Gamepad.sThumbLY = ly;
    st.Gamepad.sThumbRX = rx; st.Gamepad.sThumbRY = ry; st.Gamepad.bLeftTrigger = lt; st.Gamepad.bRightTrigger = rt;
    PadFeed(idx, &st, own);
    return st;
}
bool Neutral(const XINPUT_STATE& s) {
    const XINPUT_GAMEPAD& g = s.Gamepad;
    return !g.wButtons && !g.sThumbLX && !g.sThumbLY && !g.sThumbRX && !g.sThumbRY && !g.bLeftTrigger && !g.bRightTrigger;
}
uint32_t Blocked() { return paddebug::g_store.blocked.load(); }
LONG TakePressed() { return InterlockedExchange(&g_pressed, 0); }
LONG TakePadPressed() { return InterlockedExchange(&g_padPressed, 0); }
constexpr WORD A = XINPUT_GAMEPAD_A, B = XINPUT_GAMEPAD_B, Y = XINPUT_GAMEPAD_Y, X = XINPUT_GAMEPAD_X,
               START = XINPUT_GAMEPAD_START, BACK = XINPUT_GAMEPAD_BACK, LB = XINPUT_GAMEPAD_LEFT_SHOULDER,
               RB = XINPUT_GAMEPAD_RIGHT_SHOULDER, DU = XINPUT_GAMEPAD_DPAD_UP, DD = XINPUT_GAMEPAD_DPAD_DOWN,
               DL = XINPUT_GAMEPAD_DPAD_LEFT, DR = XINPUT_GAMEPAD_DPAD_RIGHT;

uint8_t g_keys[256];
void Keys(ULONGLONG t, std::initializer_list<uint8_t> down, bool own = false, bool alive = true) {
    g_fakeNow = t;
    if (alive) g_lastPresent = (LONG64)t;
    memset(g_keys, 0, sizeof g_keys);
    for (uint8_t d : down) g_keys[d] = 0x80;
    KeyFeed(g_keys, own);
}
uint32_t Nav(uint32_t pressed, ULONGLONG t) { g_fakeNow = t; return NavRepeat(pressed, t); }

int main() {
    // ---- PadNav / KeyNav mapping (what each button means in the menu) ----
    CHECK(PadNav(DU) == N_UP && PadNav(DD) == N_DOWN && PadNav(DL) == N_LEFT && PadNav(DR) == N_RIGHT);
    CHECK(PadNav(B_SU) == N_UP && PadNav(B_SD) == N_DOWN && PadNav(B_SL) == N_LEFT && PadNav(B_SR) == N_RIGHT);
    CHECK(PadNav(A) == N_OK && PadNav(B) == N_BACK && PadNav(Y) == N_CLOSE && PadNav(BACK) == N_HOME && PadNav(X) == N_X);
    CHECK(PadNav(LB) == N_TAB_PREV && PadNav(RB) == N_TAB_NEXT);
    CHECK(PadNav(START) == 0 && PadNav(B_LT | B_RT) == 0);  // #44181 Start does nothing in the menu; triggers are not navigation
    CHECK(KeyNav(K_ENTER) == N_OK && KeyNav(K_NPENTER) == N_OK && KeyNav(K_ESC) == N_BACK && KeyNav(K_BACKSP) == N_BACK);
    CHECK(KeyNav(K_PAGEUP) == N_PAGEUP && KeyNav(K_PAGEDOWN) == N_PAGEDOWN && KeyNav(K_TOGGLE) == 0);

    // ---- closed menu: the game sees the pad unchanged, no menu edges ----
    ResetAll();
    { auto s = Feed(1000, A | DU, 100, -200, 300, 400, 50, 60);
      CHECK(s.Gamepad.wButtons == (A | DU) && s.Gamepad.sThumbLX == 100 && s.Gamepad.bRightTrigger == 60 && s.dwPacketNumber == 7);
      CHECK(g_pressed == 0 && Blocked() == 0); }
    CHECK(g_padHeld[0] == (LONG)(A | DU | B_LT | B_RT) && g_padHeldAt[0] == 1000);

    // ---- held / released edges + left-stick hysteresis (20000 to press, 12000 to stay held) ----
    ResetAll(); g_open = 1;
    Feed(1000, 0, 0, 15000); CHECK(TakePressed() == 0);                          // below the press threshold
    Feed(1016, 0, 0, 21000); CHECK(TakePressed() == N_UP && TakePadPressed() == N_UP);  // press edge
    Feed(1032, 0, 0, 21000); CHECK(TakePressed() == 0);                          // held: no second edge
    Feed(1048, 0, 0, 13000); CHECK(TakePressed() == 0 && (g_padHeld[0] & B_SU));  // still held above 12000
    Feed(1064, 0, 0, 11000); CHECK(!(g_padHeld[0] & B_SU));                      // released
    Feed(1080, 0, 0, 15000); CHECK(TakePressed() == 0 && !(g_padHeld[0] & B_SU)); // 15000 after release is no press
    Feed(1096, 0, -21000, 0); CHECK(TakePressed() == N_LEFT);
    Feed(1112, 0, 21000, 0); CHECK(TakePressed() == N_RIGHT);                    // left released + right pressed in one read
    Feed(1128, 0, 0, -21000); CHECK(TakePressed() == N_DOWN);
    Feed(1144, A); CHECK(TakePressed() == N_OK);
    Feed(1160, A | B); CHECK(TakePressed() == N_BACK);                           // only the new button is an edge
    Feed(1176, 0); Feed(1192, A | B); CHECK(TakePressed() == (N_OK | N_BACK));
    Feed(1208, 0, 0, 0, 0, 0, 200, 200); CHECK(TakePressed() == 0 && (g_padHeld[0] & (B_LT | B_RT)) == (B_LT | B_RT));
    Feed(1224, 0, 0, 0, 0, 0, 30, 31); CHECK((g_padHeld[0] & B_LT) == 0 && (g_padHeld[0] & B_RT));  // trigger threshold > 30
    // per-pad slots are independent: a press on pad 1 is an edge even while pad 0 holds the same button
    ResetAll(); g_open = 1;
    Feed(1000, A); TakePressed(); Feed(1016, A, 0, 0, 0, 0, 0, 0, false, true, 1); CHECK(TakePressed() == N_OK);

    // ---- open menu: the game gets a neutral pad (new packet number), the right stick (camera) stays live ----
    ResetAll(); g_open = 1;
    { auto s = Feed(1000, A | DU, 9000, 9000, 1234, -4321, 255, 255);
      CHECK(s.Gamepad.wButtons == 0 && s.Gamepad.sThumbLX == 0 && s.Gamepad.sThumbLY == 0 && s.Gamepad.bLeftTrigger == 0 && s.Gamepad.bRightTrigger == 0);
      CHECK(s.Gamepad.sThumbRX == 1234 && s.Gamepad.sThumbRY == -4321);
      CHECK(s.dwPacketNumber == 7 + 0x40000000u && (Blocked() & paddebug::kMenuOpen)); }
    // own polling (the mod's own XInput read) records edges but never edits the state
    ResetAll(); g_open = 1;
    { auto s = Feed(1000, A, 0, 0, 0, 0, 0, 0, true); CHECK(s.Gamepad.wButtons == A && s.dwPacketNumber == 7 && TakePressed() == N_OK); }
    // closed menu: edges are not collected
    ResetAll(); Feed(1000, A); CHECK(g_pressed == 0 && g_padPressed == 0);

    // ---- source split (modality): pad edges go to g_pressed AND g_padPressed, keyboard only to g_pressed ----
    ResetAll(); g_open = 1;
    Feed(1000, A); CHECK(TakePressed() == N_OK && TakePadPressed() == N_OK);
    Keys(1016, {DIK_RETURN}); CHECK(TakePressed() == N_OK && TakePadPressed() == 0);
    Keys(1032, {}); Keys(1048, {DIK_NUMPADENTER}); CHECK(TakePressed() == N_OK && TakePadPressed() == 0);
    // OnPresent's g_okFromPad = (g_padPressed & pressed & N_OK): a keyboard Enter in the same frame as a pad B is not "from pad"
    Feed(1064, 0); Feed(1080, B); Keys(1081, {DIK_RETURN}); Keys(1082, {});
    { const uint32_t p = (uint32_t)TakePressed(), pp = (uint32_t)TakePadPressed() & p; CHECK(p == (N_OK | N_BACK) && (pp & N_OK) == 0); }

    // ---- Start triple tap opens: every tap is hidden from the game, the 3rd requests the toggle and latches all pads ----
    ResetAll();
    { auto s = Feed(1000, START); CHECK(s.Gamepad.wButtons == 0 && s.dwPacketNumber == 7 + 0x40000000u && (Blocked() & paddebug::kTapSwallow)); }
    { auto s = Feed(1050, A); CHECK(s.Gamepad.wButtons == A && s.dwPacketNumber == 7); }  // other buttons pass between taps
    Feed(1100, START | A); CHECK(g_pad[0].nTaps == 2);
    { auto s = Feed(1100, START | A); CHECK(s.Gamepad.wButtons == A); }  // START hidden, A still visible
    Feed(1150, 0);
    CHECK(g_toggleReq == 0);
    { auto s = Feed(1200, START, 0, 0, 500, 500);
      CHECK(g_toggleReq == 1 && g_toggleSrc == 2 && g_toggleAt == 1200);
      CHECK(Neutral(s) && (Blocked() & paddebug::kLatch));  // the 3rd press: latch blanks everything, camera too
      for (const PadSlot& p : g_pad) CHECK(p.latch); }
    // latch holds until every button is released; a stick still deflected does not keep it
    { auto s = Feed(1250, START); CHECK(Neutral(s)); }
    { auto s = Feed(1300, 0, 25000, 25000); CHECK(!g_pad[0].latch && s.Gamepad.sThumbLX == 25000 && Blocked() == 0); }
    // the latch also covers the moment the menu becomes open (OnPresent consumes the request): pad blank incl. camera
    ResetAll(); Feed(1000, START); Feed(1050, 0); Feed(1100, START); Feed(1150, 0); Feed(1200, START);
    g_open = 1; g_toggleReq = 0;
    { auto s = Feed(1216, START, 0, 0, 700, 700); CHECK(Neutral(s) && (Blocked() & (paddebug::kLatch | paddebug::kMenuOpen)) == (paddebug::kLatch | paddebug::kMenuOpen)); }
    { auto s = Feed(1232, 0, 0, 0, 700, 700); CHECK(!g_pad[0].latch && s.Gamepad.sThumbRX == 700); }  // released: camera live again

    // ---- a lone Start tap is replayed to the game as one 120 ms press once the 300 ms gap passes ----
    ResetAll();
    CHECK(Feed(1000, START).Gamepad.wButtons == 0);
    CHECK(Feed(1050, 0).Gamepad.wButtons == 0);
    CHECK(Feed(1299, 0).Gamepad.wButtons == 0);                  // gap not over yet
    { auto s = Feed(1300, 0); CHECK(s.Gamepad.wButtons == START && s.dwPacketNumber == 7 + 0x40000000u); }  // replay starts
    CHECK(Feed(1419, 0).Gamepad.wButtons == START);
    CHECK(Feed(1420, 0).Gamepad.wButtons == 0);                  // 120 ms replay over
    CHECK(g_toggleReq == 0);
    // two taps: also replayed as ONE press
    ResetAll(); Feed(1000, START); Feed(1050, 0); Feed(1100, START); Feed(1150, 0);
    CHECK(Feed(1400, 0).Gamepad.wButtons == START && Feed(1530, 0).Gamepad.wButtons == 0 && g_toggleReq == 0);
    // swallowTaps=0: taps reach the game as they are, no replay; the 3rd still opens
    ResetAll(); g_cfg.swallowTaps = 0;
    CHECK(Feed(1000, START).Gamepad.wButtons == START); Feed(1050, 0);
    CHECK(Feed(1400, 0).Gamepad.wButtons == 0);
    Feed(1500, START); Feed(1550, 0); Feed(1600, START); Feed(1650, 0); Feed(1700, START); CHECK(g_toggleReq == 1);

    // ---- a Start press still held after the gap is a real hold: the game gets Start from then on ----
    ResetAll();
    CHECK(Feed(1000, START).Gamepad.wButtons == 0);
    CHECK(Feed(1200, START).Gamepad.wButtons == 0);
    CHECK(Feed(1300, START).Gamepad.wButtons == START && g_pad[0].passHold);
    CHECK(Feed(2000, START).Gamepad.wButtons == START);
    CHECK(Feed(2016, 0).Gamepad.wButtons == 0 && !g_pad[0].passHold);
    CHECK(Feed(2500, 0).Gamepad.wButtons == 0);  // no replay after a passed hold

    // ---- a tap after the gap starts a new count (slow taps never open) ----
    ResetAll();
    Feed(1000, START); Feed(1050, 0); Feed(1400, 0);  // count decided (replayed)
    Feed(1500, START); Feed(1550, 0); Feed(1600, START); Feed(1650, 0);
    CHECK(g_toggleReq == 0 && g_pad[0].nTaps == 2);
    Feed(1700, START); CHECK(g_toggleReq == 1);
    // taps spaced 299 ms apart still count (gap check is >= 300 ms since the last tap)
    ResetAll(); Feed(1000, START); Feed(1050, 0); Feed(1299, START); Feed(1349, 0); Feed(1598, START); CHECK(g_toggleReq == 1);

    // ---- overlay not presenting: the 3rd tap does not request and Start passes to the game as a hold ----
    ResetAll(); g_lastPresent = 0;
    Feed(1000, START, 0, 0, 0, 0, 0, 0, false, false); Feed(1050, 0, 0, 0, 0, 0, 0, 0, false, false);
    Feed(1100, START, 0, 0, 0, 0, 0, 0, false, false); Feed(1150, 0, 0, 0, 0, 0, 0, 0, false, false);
    { auto s = Feed(1200, START, 0, 0, 0, 0, 0, 0, false, false); CHECK(g_toggleReq == 0 && g_pad[0].passHold && s.Gamepad.wButtons == START); }

    // ---- while open, Start does nothing: no gesture, no swallow, (pad blanked as open) ----
    ResetAll(); g_open = 1;
    for (ULONGLONG t = 1000; t < 1300; t += 100) { Feed(t, START); Feed(t + 50, 0); }
    CHECK(g_toggleReq == 0 && g_pad[0].nTaps == 0 && TakePressed() == 0);
    // ... and closing from the menu is Y / B (N_CLOSE / N_BACK edges), not a toggle request
    Feed(1400, Y); CHECK(TakePressed() == N_CLOSE && g_toggleReq == 0);

    // ---- hold gesture ([Menu] Gesture=0): Start held HoldMs opens once; release re-arms ----
    ResetAll(); g_cfg.gesture = 0; g_cfg.holdMs = 5000;
    CHECK(Feed(1000, START).Gamepad.wButtons == START);  // hold mode never hides the press
    CHECK(g_holdSince == 1000);
    Feed(5999, START); CHECK(g_toggleReq == 0 && g_holdSince == 1000);
    Feed(6000, START); CHECK(g_toggleReq == 1 && g_holdSince == 0 && g_pad[0].holdFired);
    g_toggleReq = 0; Feed(12000, START); CHECK(g_toggleReq == 0);  // no second open while still held
    Feed(12016, 0); CHECK(g_pad[0].holdStart == 0 && !g_pad[0].holdFired);
    Feed(13000, START); Feed(13500, 0); CHECK(g_holdSince == 0 && g_toggleReq == 0);  // released early: no open, hint cleared
    // HoldMs=0: pressing the combo opens on the edge
    ResetAll(); g_cfg.gesture = 0; g_cfg.holdMs = 0; g_cfg.holdMask = START | BACK;
    Feed(1000, START); CHECK(g_toggleReq == 0);
    Feed(1016, START | BACK); CHECK(g_toggleReq == 1);
    g_toggleReq = 0; Feed(1032, START | BACK); CHECK(g_toggleReq == 0);  // held: no repeat
    // holdMask without Start/Back keeps the hold gesture working while the menu is open (close by the same hold)
    ResetAll(); g_cfg.gesture = 0; g_cfg.holdMs = 0; g_cfg.holdMask = LB | RB; g_open = 1;
    Feed(1000, LB | RB); CHECK(g_toggleReq == 1);

    // ---- overlay dead 1.5 s while open: menu force-closes, input passes again ----
    ResetAll(); g_open = 1; g_lastPresent = 1000;
    Feed(2499, A, 0, 0, 0, 0, 0, 0, false, false); CHECK(g_open == 1);
    { auto s = Feed(2500, A, 0, 0, 0, 0, 0, 0, false, false); CHECK(g_open == 0 && s.Gamepad.wButtons == A); }

    // ---- disabled menu (exception/ini): hides nothing, clears stale debug bits ----
    ResetAll(); g_disabled = 1; g_open = 1; paddebug::g_store.blocked.store(31);
    { auto s = Feed(1000, START | A); CHECK(s.Gamepad.wButtons == (START | A) && Blocked() == 0 && TakePressed() == 0); }

    // ---- MaskGameButtons lease (#9324) + held-past-lease (#9909) ----
    ResetAll(); g_gameMask = A; g_gameMaskUntil = 1100;
    { auto s = Feed(1000, A | B); CHECK(s.Gamepad.wButtons == B && s.dwPacketNumber == 7 + 0x40000000u && (Blocked() & paddebug::kMaskLease)); }
    { auto s = Feed(1200, A | B); CHECK(s.Gamepad.wButtons == B); }         // lease over, A still held: stays hidden
    { auto s = Feed(1216, B); CHECK(s.Gamepad.wButtons == B && s.dwPacketNumber == 7); }
    { auto s = Feed(1232, A); CHECK(s.Gamepad.wButtons == A); }             // fresh press after the lease: visible
    // the lease hides the button from the game only: the menu still sees the edge
    ResetAll(); g_open = 1; g_gameMask = A; g_gameMaskUntil = 5000; Feed(1000, A); CHECK(TakePressed() == N_OK);

    // ---- Boss Match transition cover owns the pad (B cancels the cover, captured reads are neutral) ----
    ResetAll(); g_coverToken = 5;
    { auto s = Feed(1000, B | A); CHECK(g_coverCancel.load() && s.Gamepad.wButtons == A); }          // B edge cancels, B hidden
    { auto s = Feed(1100, B); CHECK(s.Gamepad.wButtons == 0); }                                       // B hidden 150 ms after cancel
    { auto s = Feed(1150, B); CHECK(s.Gamepad.wButtons == B); }
    g_coverCaptures = true;
    { auto s = Feed(1200, A, 5, 5, 5, 5, 5, 5); CHECK(Neutral(s) && s.dwPacketNumber == 7 + 0x40000000u && (Blocked() & paddebug::kMenuOpen)); }
    g_open = 1; Feed(1300, Y); CHECK(TakePressed() == 0);  // cover path returns before menu edges

    // ---- keyboard: toggle key, nav keys hidden while open, latch, Escape x3 ----
    ResetAll();
    Keys(1000, {0x44}); CHECK(g_toggleReq == 1 && g_toggleSrc == 1 && g_keys[0x44] == 0);  // F10: request, never reaches the game
    ResetAll(); Keys(1000, {0x44}, false, false); CHECK(g_toggleReq == 0);  // overlay not presenting: no request
    ResetAll(); Keys(1000, {DIK_UP}); CHECK(g_keys[DIK_UP] == 0x80 && g_pressed == 0);  // closed: arrows go to the game
    ResetAll(); g_open = 1;
    Keys(1000, {DIK_UP, DIK_W}); CHECK(g_keys[DIK_UP] == 0 && g_keys[DIK_W] == 0x80 && TakePressed() == N_UP);
    Keys(1016, {DIK_UP}); CHECK(TakePressed() == 0);  // held: no new edge
    // nav keys held when the menu closes stay hidden until released (g_keyLatch)
    g_keyLatch = K_UP; g_open = 0;
    Keys(1032, {DIK_UP}); CHECK(g_keys[DIK_UP] == 0);
    Keys(1048, {}); Keys(1064, {DIK_UP}); CHECK(g_keys[DIK_UP] == 0x80);
    // Escape triple tap: hidden presses, request src 3, third press latched until released
    ResetAll();
    Keys(1000, {DIK_ESCAPE}); CHECK(g_keys[DIK_ESCAPE] == 0);
    Keys(1050, {}); Keys(1100, {DIK_ESCAPE}); Keys(1150, {});
    Keys(1200, {DIK_ESCAPE}); CHECK(g_toggleReq == 1 && g_toggleSrc == 3 && g_keys[DIK_ESCAPE] == 0 && (g_keyLatch & K_ESC));
    Keys(1250, {DIK_ESCAPE}); CHECK(g_keys[DIK_ESCAPE] == 0);
    Keys(1300, {}); CHECK((g_keyLatch & K_ESC) == 0);
    // a lone Escape is replayed as one 120 ms press after the gap
    ResetAll(); Keys(1000, {DIK_ESCAPE}); Keys(1050, {}); Keys(1300, {}); CHECK(g_keys[DIK_ESCAPE] == 0x80);
    Keys(1420, {}); CHECK(g_keys[DIK_ESCAPE] == 0);
    // with the menu open, Escape is Back (N_BACK edge) and the next presses stay with the mod for TapWindowMs
    ResetAll(); g_open = 1; Keys(1000, {DIK_ESCAPE}); CHECK(TakePressed() == N_BACK && g_escMuteUntil == 1900);
    g_open = 0; Keys(1050, {}); Keys(1100, {DIK_ESCAPE}); CHECK(g_keys[DIK_ESCAPE] == 0 && g_escTaps == 0);
    Keys(1150, {}); Keys(1950, {DIK_ESCAPE}); CHECK(g_keys[DIK_ESCAPE] == 0 && g_escTaps == 1);  // mute over: counts as a new tap

    // ---- NavRepeat: edge, 400 ms first repeat, then every 90 ms; release re-arms; stale state (>300 ms) is not held ----
    ResetAll();
    auto padHold = [](uint32_t raw, ULONGLONG t, int i = 0) { g_padHeld[i] = (LONG)raw; g_padHeldAt[i] = (LONG64)t; };
    padHold(DD, 1000); CHECK(Nav(N_DOWN, 1000) == N_DOWN);
    padHold(DD, 1399); CHECK(Nav(0, 1399) == 0);
    padHold(DD, 1400); CHECK(Nav(0, 1400) == N_DOWN);
    padHold(DD, 1489); CHECK(Nav(0, 1489) == 0);
    padHold(DD, 1490); CHECK(Nav(0, 1490) == N_DOWN);
    padHold(DD, 1580); CHECK(Nav(0, 1580) == N_DOWN);
    CHECK(Nav(0, 1880) == 0 && g_rep[1] == 0);  // no read for 300 ms: not held any more, timer cleared
    padHold(DD, 1900); CHECK(Nav(0, 1900) == 0);  // held again without an edge: no repeat until a fresh press
    // stick direction repeats like the d-pad; two pads holding are merged
    ResetAll(); padHold(B_SU, 1000); CHECK(Nav(N_UP, 1000) == N_UP); padHold(0, 1400); padHold(B_SU, 1400, 2); CHECK(Nav(0, 1400) == N_UP);
    // keyboard hold repeats too
    ResetAll(); g_keyHeld = K_UP; g_keyHeldAt = 1000; CHECK(Nav(N_UP, 1000) == N_UP); g_keyHeldAt = 1400; CHECK(Nav(0, 1400) == N_UP);
    // Up and Down held together both repeat (no axis exclusion inside the vertical axis)
    ResetAll(); padHold(DU | DD, 1000); CHECK(Nav(N_UP | N_DOWN, 1000) == (N_UP | N_DOWN)); padHold(DU | DD, 1400); CHECK(Nav(0, 1400) == (N_UP | N_DOWN));
    // OK / Back never repeat (NavRepeat only returns directions)
    ResetAll(); padHold(A, 1000); CHECK(Nav(N_OK, 1000) == 0);

    // ---- UO #14 input modality: switches on trusted press edges only; held pad bits are snapshotted at the switch to keyboard ----
    ResetAll();
    CHECK(g_modality == MOD_PAD && InputModality() == 0);
    Feed(1000, A); Feed(1050, 0); CHECK(g_modality == MOD_PAD && g_modalitySwitches == 0);  // pad press while on pad: no switch
    Keys(1100, {}); CHECK(g_modality == MOD_PAD);                                             // a keyboard read with no press: no switch
    // held A while switching to keyboard and back = no press
    ResetAll(); g_open = 1;
    Feed(1000, A); CHECK(TakePressed() == N_OK); TakePadPressed();                            // the real A press
    Keys(1050, {DIK_DOWN}); CHECK(g_modality == MOD_KEY && InputModality() == 1 && g_modalitySwitches == 1 && TakePressed() == N_DOWN);
    CHECK(g_pad[0].modalSnap == A);                                                           // A was held at the switch
    Feed(1100, A); CHECK(TakePressed() == 0 && (g_padHeld[0] & A) == 0);                     // still held: inert for the menu
    Keys(1150, {}); Feed(1200, A); CHECK(TakePressed() == 0 && g_modality == MOD_KEY);       // a held button is no edge: still keyboard
    Feed(1250, A | DD); CHECK(g_modality == MOD_PAD && g_modalitySwitches == 2 && TakePressed() == N_DOWN && TakePadPressed() == N_DOWN);
    Feed(1300, A); Feed(1350, A); CHECK(TakePressed() == 0 && (g_padHeld[0] & A) == 0);     // back on pad, A still held: no phantom OK
    Feed(1400, 0); CHECK(g_pad[0].modalSnap == 0);                                            // released: the snapshot lets go
    Feed(1450, A); CHECK(TakePressed() == N_OK);                                              // a fresh press works again
    // the game side is untouched by the snapshot: a held A outside the menu still reaches the game while keyboard is used
    ResetAll(); Feed(1000, A); Keys(1050, {DIK_UP}); CHECK(g_modality == MOD_KEY);
    { const XINPUT_STATE st = Feed(1100, A); CHECK(st.Gamepad.wButtons == A); }
    // a d-pad direction held across the switch neither repeats nor counts as held (no phantom scroll on return)
    ResetAll(); g_open = 1; Feed(1000, DD); CHECK(Nav(TakePressed(), 1000) == N_DOWN);
    Keys(1100, {DIK_RETURN}); Keys(1150, {}); CHECK(g_modality == MOD_KEY);
    TakePressed(); Feed(1400, DD); CHECK(Nav(TakePressed(), 1400) == 0);
    Feed(1500, DD); CHECK(Nav(0, 1500) == 0 && g_rep[1] == 0);
    Feed(1600, DD | DR); CHECK(g_modality == MOD_PAD && TakePressed() == N_RIGHT);           // a new direction switches back; Down stays inert
    // a stick held past the threshold at the switch is snapshotted too; a small stick wobble is no edge and switches nothing
    ResetAll(); g_open = 1; Feed(1000, 0, 0, 30000); TakePressed(); Keys(1050, {DIK_LEFT}); TakePressed();
    CHECK(g_modality == MOD_KEY && (g_pad[0].modalSnap & B_SU) != 0);
    Feed(1100, 0, 0, 30000); CHECK(TakePressed() == 0 && g_modality == MOD_KEY);
    Feed(1150, 0, 5000, 0); CHECK(g_modality == MOD_KEY);                                    // below 20000: no edge
    Feed(1200, 0, 25000, 0); CHECK(g_modality == MOD_PAD && TakePressed() == N_RIGHT);       // a real stick push switches back
    // the snapshot is taken once, at the switch: later key presses do not widen it
    ResetAll(); g_open = 1; Feed(1000, A); Keys(1050, {DIK_UP}); Keys(1100, {}); TakePressed();
    g_inputVirtual = &FakeVirtual; g_virtPad = 1; Feed(1150, A | X); CHECK(TakePressed() == N_X && g_modality == MOD_KEY);  // virtual X
    Keys(1200, {DIK_DOWN}); CHECK(g_pad[0].modalSnap == A && g_modalitySwitches == 1);
    // synthetic / injected input does not switch modality, but still drives the menu
    ResetAll(); g_open = 1; g_inputVirtual = &FakeVirtual; g_virtKeys = true;
    Keys(1000, {DIK_DOWN}); CHECK(g_modality == MOD_PAD && g_modalitySwitches == 0 && TakePressed() == N_DOWN);  // VPad key
    g_virtKeys = false; Keys(1050, {}); Keys(1100, {DIK_DOWN}); CHECK(g_modality == MOD_KEY && g_modalitySwitches == 1);  // real key
    g_virtPad = 1; Feed(1150, A); CHECK(g_modality == MOD_KEY && TakePressed() == (N_DOWN | N_OK));  // VPad A on slot 0: no switch
    Feed(1200, 0); Feed(1250, 0, 0, 0, 0, 0, 0, 0, false, true, 1);
    Feed(1300, A, 0, 0, 0, 0, 0, 0, false, true, 1); CHECK(g_modality == MOD_PAD && g_modalitySwitches == 2);  // real pad on slot 1
    g_virtKeys = true; Keys(1350, {}); Keys(1400, {DIK_UP}); CHECK(g_modality == MOD_PAD);    // VPad key again: stays on pad
    // own polling reads the hardware directly (VPad only answers the game's import): trusted even while a virtual flag is up
    ResetAll(); g_inputVirtual = &FakeVirtual; g_virtKeys = true; g_virtPad = 1;
    Keys(1000, {DIK_DOWN}, true); CHECK(g_modality == MOD_KEY);
    Feed(1050, A, 0, 0, 0, 0, 0, 0, true); CHECK(g_modality == MOD_PAD);
    // triggers count as pad use; a disabled menu still tracks modality (it hides nothing either way)
    ResetAll(); Keys(1000, {DIK_UP}); Feed(1050, 0, 0, 0, 0, 0, 0, 200); CHECK(g_modality == MOD_PAD);
    ResetAll(); g_disabled = 1; Keys(1000, {DIK_UP}); CHECK(g_modality == MOD_KEY);

    // ---- #105941: one log line per real modality flip (with its source); one 'VPad input: modality locked' line per session ----
    ResetAll(); Feed(1000, A); Feed(1050, 0);
    Keys(1100, {DIK_ESCAPE}); Keys(1150, {});
    CHECK(g_mlogLines == 1 && strstr(g_mlog, "input modality pad->key (Esc, game keyboard read)") != nullptr);
    Keys(1200, {DIK_DOWN}); Keys(1250, {}); CHECK(g_mlogLines == 1);                           // no flip: no line
    Feed(1300, DD); CHECK(g_mlogLines == 2 && strstr(g_mlog, "input modality key->pad (pad slot 0, buttons 000002, game pad read)") != nullptr);
    Feed(1350, 0); Keys(1400, {DIK_UP}, true); CHECK(g_mlogLines == 3 && strstr(g_mlog, "pad->key (Up, own keyboard poll)") != nullptr);
    ResetAll(); g_inputVirtual = &FakeVirtual; g_virtKeys = true;
    Keys(1000, {DIK_DOWN}); Keys(1050, {}); Keys(1100, {DIK_UP}); Keys(1150, {});
    g_virtPad = 1; Feed(1200, A); Feed(1250, 0); Feed(1300, A);
    CHECK(g_modalitySwitches == 0 && g_mlogLines == 1 && strstr(g_mlog, "VPad input: modality locked - virtual keyboard0 drives") != nullptr);
    // the log is capped: 200 lines, the 200th says so, no more after
    ResetAll();
    for (int i = 0; i < 300; ++i) { Keys(1000 + 40 * i, {DIK_DOWN}); Keys(1010 + 40 * i, {}); Feed(1020 + 40 * i, DD); Feed(1030 + 40 * i, 0); }
    CHECK(g_modalitySwitches == 600 && g_mlogLines == 200 && strstr(g_mlog, "200 switches logged, further switches are not logged") != nullptr);
    // ---- #105941 Ally boot 1: the first key after pad use flips modality AND still acts (it is never eaten by the flip) ----
    // menu open on Home after pad navigation, one Esc tap -> N_BACK (Back closes the Home page); pad A still held at the flip
    ResetAll(); g_open = 1; Feed(1000, DD); Feed(1050, 0); TakePressed(); TakePadPressed(); Feed(1100, A); TakePressed(); TakePadPressed();
    Keys(1150, {DIK_ESCAPE}); CHECK(g_modality == MOD_KEY && TakePressed() == N_BACK && TakePadPressed() == 0);
    Keys(1200, {}); CHECK(TakePressed() == 0);
    // same with the menu closed: a lone Esc after pad use still reaches the game (held back for TapGapMs, then replayed)
    ResetAll(); Feed(1000, B); Feed(1050, 0); Keys(1100, {DIK_ESCAPE}); CHECK(g_modality == MOD_KEY && g_keys[DIK_ESCAPE] == 0);
    Keys(1150, {}); Keys(1450, {}); CHECK(g_keys[DIK_ESCAPE] == 0x80);
    // the first arrow / Enter after pad use move / confirm on the same read as the flip
    ResetAll(); g_open = 1; Feed(1000, A); Feed(1050, 0); TakePressed(); Keys(1100, {DIK_DOWN}); CHECK(TakePressed() == N_DOWN);
    ResetAll(); g_open = 1; Feed(1000, A); Feed(1050, 0); TakePressed(); Keys(1100, {DIK_RETURN}); CHECK(TakePressed() == N_OK);
    printf("menu pad input: %d checks, %d failed (production menu.cpp PadFeed/KeyFeed/NavRepeat; offline)\n", checks, fails);
    return fails ? 1 : 0;
}
'''

exports = 'int InputModality() { return (int)g_modality; }' + '\n'  # UO #14 menu.h export, one line
prelude = (prelude.replace('TAPWINDOW', str(shipped['TapWindowMs'])).replace('TAPGAP', str(shipped['TapGapMs']))
           .replace('SWALLOW', str(shipped['SwallowTaps'])))
unit_text = prelude + consts + state_and_pad + '\n' + key_feed + '\n' + nav_repeat + '\n' + exports + harness
out = root.parents[1] / 'build/menu-pad-input-test'
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
       f'/Fo"{out / "test.obj"}" /Fe"{exe}" "{unit}" user32.lib')
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
print(r.stdout, end=''); print(r.stderr, end=''); r.check_returncode()
subprocess.run([str(exe)], check=True, timeout=30)
