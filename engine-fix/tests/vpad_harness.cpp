// vpad_harness.cpp - offline tests of VirtualPad / KeepRunningUnfocused (src/vpad_core.h, src/vpad.cpp). Not a game test.
// Built and run by tests/run_vpad_harness.py; must print ALL PASSED.
//   1. parser: every command shape, bad input rejected with a reason
//   2. composer: rounds rule (a press is seen by >= 2 game frames, calls < 2 ms apart are one frame), timed vs held, release,
//      stick / trigger merge, keys, cancel
//   3. the real XInputGetState hook on a fake IAT slot: exclusive (slot 0 virtual, 1-3 not connected, the real API never
//      called), packet numbers, disarm pass-through, merge mode (real + virtual, virtual slot without a real pad)
//   4. fake game thread (60 Hz pad + keyboard reads) + the real named-pipe server + a pipe client: ping, state, press, hold,
//      stick, key (real keys hidden while the game is not foreground), menu set/get through a fake menu, errors, 2 clients
//   5. KeepRunningUnfocused thunk: answers "active" only while the switch is on and the real answer is "inactive", counts it;
//      prints its bytes for the disassembly check in run_vpad_harness.py
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <timeapi.h>
#include <xinput.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include "../src/vpad.h"
#include "../src/vpad_core.h"

using namespace vpad;

static int g_fail = 0;
#define CHECK(c, ...)                                   \
    do {                                                \
        if (!(c)) {                                     \
            ++g_fail;                                   \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

static void Log(const char* fmt, ...) {
    (void)fmt;  // quiet: the mod log is not under test here
}

// ---- 1. parser ----
static void TestParser() {
    Cmd c = Parse("press A");
    CHECK(c.op == Op::Press && c.mask == 0x1000 && c.ms == 100, "press A default 100 ms");
    c = Parse("PRESS lb+rb+x 250ms");
    CHECK(c.op == Op::Press && c.mask == (0x0100 | 0x0200 | 0x4000) && c.ms == 250, "chord + ms suffix");
    c = Parse("press cross 0");
    CHECK(c.op == Op::Press && c.mask == 0x1000 && c.ms == 0, "PS name, 0 ms");
    c = Parse("hold START");
    CHECK(c.op == Op::Hold && c.mask == 0x0010, "hold");
    c = Parse("release all");
    CHECK(c.op == Op::ReleaseAll, "release all");
    c = Parse("release up+down");
    CHECK(c.op == Op::Release && c.mask == 3, "release chord");
    c = Parse("stick L 0 1 2000");
    CHECK(c.op == Op::Stick && c.side == 'L' && c.x == 0 && c.y == 1 && c.ms == 2000, "stick timed");
    c = Parse("stick right -0.5 0.25");
    CHECK(c.op == Op::Stick && c.side == 'R' && c.ms == -1, "stick held");
    c = Parse("trigger RT 1 100");
    CHECK(c.op == Op::Trigger && c.side == 'R' && TriggerFrom(c.x) == 255 && c.ms == 100, "trigger");
    CHECK(TriggerFrom(0.5) == 128 && TriggerFrom(200) == 200 && TriggerFrom(-1) == 0, "trigger values");
    CHECK(AxisFrom(1) == 32767 && AxisFrom(-1) == -32767 && AxisFrom(0) == 0 && AxisFrom(0.5) == 16384, "axis values");
    c = Parse("key esc");
    CHECK(c.op == Op::Key && c.dik == 0x01 && c.ms == 100, "key name");
    c = Parse("key 0x1C 40");
    CHECK(c.op == Op::Key && c.dik == 0x1C && c.ms == 40, "key hex");
    c = Parse("menu set GF on");
    CHECK(c.op == Op::MenuSet && !strcmp(c.key, "GF") && c.value == 1, "menu set on");
    c = Parse("menu set R1 4");
    CHECK(c.op == Op::MenuSet && c.value == 4, "menu set int");
    c = Parse("menu get MQ");
    CHECK(c.op == Op::MenuGet && !strcmp(c.key, "MQ"), "menu get");
    c = Parse("  log   hello  world \r");
    CHECK(c.op == Op::Log && !strncmp(c.text, "hello  world", 12), "log keeps the text (%s)", c.text);
    c = Parse("wait 500");
    CHECK(c.op == Op::Wait && c.ms == 500, "wait");
    for (const char* bad : {"", "press", "press Z", "press A -5", "press A 1 2", "stick L 2 0", "stick M 0 0", "stick L x 0",
                            "trigger L 300", "key NOPE", "menu set GF", "menu frob", "wait 70000", "frobnicate", "hold A B",
                            "press A+", "press +A"}) {
        c = Parse(bad);
        CHECK(c.op == Op::Bad && c.text[0], "'%s' must be rejected with a reason", bad);
    }
}

// ---- 2. composer ----
static void TestComposer() {
    Composer k;
    // press A 0 ms: seen by exactly 2 rounds
    k.AddButtons(0x1000, 0);
    int seen = 0;
    for (int f = 0; f < 5; ++f)
        for (int call = 0; call < 5; ++call)  // 5 calls per frame, 0.1 ms apart = one round
            if (k.ReadPad(f * 16667 + call * 100).buttons & 0x1000) seen += call == 0;
    CHECK(seen == 2, "0 ms press seen by %d frames, want 2", seen);
    CHECK(k.PadRound() == 5, "rounds %u", k.PadRound());
    // press B 100 ms at 60 Hz: frames 0..6 (t 0..100 ms) then gone
    Composer k2;
    const uint32_t id = k2.AddButtons(0x2000, 100);
    CHECK(k2.Status(id) == 1, "waiting before the first read");
    int first = -1, last = -1;
    for (int f = 0; f < 12; ++f) {
        if (k2.ReadPad(1000000 + f * 16667).buttons & 0x2000) {
            if (first < 0) first = f;
            last = f;
        }
        if (f == 0) CHECK(k2.Status(id) == 2, "running after the first read");
    }
    CHECK(first == 0 && last == 5 && k2.Status(id) == 0, "100 ms press frames %d..%d (want 0..5: t 0..83 ms shown, gone at 100 ms), status %d", first, last, k2.Status(id));
    // held + timed stick: timed overrides while running, held comes back
    Composer k3;
    k3.SetStick('L', 100, 200);
    k3.AddStick('L', 0, 32767, 30);
    Pad p = k3.ReadPad(0);
    CHECK(p.lx == 0 && p.ly == 32767, "timed stick wins");
    k3.ReadPad(16667);
    k3.ReadPad(33333);
    p = k3.ReadPad(50000);
    CHECK(p.lx == 100 && p.ly == 200, "held stick back (%d %d)", p.lx, p.ly);
    // triggers: max of held and timed
    k3.SetTrigger('R', 50);
    k3.AddTrigger('R', 200, 10);
    CHECK(k3.ReadPad(60000).rt == 200, "timed trigger max");
    k3.ReadPad(80000);
    k3.ReadPad(100000);
    CHECK(k3.ReadPad(120000).rt == 50, "held trigger back");
    // hold / release / release all
    Composer k4;
    k4.Hold(0x1000 | 0x0010);
    CHECK(k4.ReadPad(0).buttons == 0x1010, "hold");
    k4.Release(0x0010);
    CHECK(k4.ReadPad(20000).buttons == 0x1000, "release one");
    k4.AddButtons(0x4000, 1000);
    CHECK(k4.ReadPad(40000).buttons == 0x5000, "timed + held");
    k4.ReleaseAll();
    CHECK(k4.ReadPad(60000).buttons == 0x4000, "release all: a running timed press still meets its 2 rounds");
    CHECK(k4.ReadPad(80000).buttons == 0, "then gone");
    CHECK(!k4.AnyActive(), "nothing active");
    // cancel
    const uint32_t c = k4.AddButtons(0x8000, 5000);
    k4.Cancel(c);
    CHECK(k4.Status(c) == 0 && k4.ReadPad(100000).buttons == 0, "cancel");
    // keys: own round counter (DirectInput read once per frame)
    Composer k5;
    k5.AddKey(0x01, 0);
    int ks = 0;
    for (int f = 0; f < 4; ++f) {
        uint8_t keys[256] = {};
        k5.ReadKeys(f * 16667, keys);
        ks += keys[1] == 0x80;
    }
    CHECK(ks == 2 && k5.KeyRound() == 4, "key seen %d frames, want 2", ks);
    // table full
    Composer k6;
    int ok = 0;
    for (int i = 0; i < kMaxTimed + 5; ++i) ok += k6.AddButtons(1, 10) != 0;
    CHECK(ok == kMaxTimed, "table holds %d", ok);
}

// ---- 3. hook on fake IAT slots ----
static std::atomic<int> g_realCalls{0};
static std::atomic<WORD> g_realButtons{0};
static std::atomic<bool> g_realConnected{true};
static DWORD WINAPI FakeXInput(DWORD idx, XINPUT_STATE* st) {
    ++g_realCalls;
    if (!g_realConnected || idx != 0) return ERROR_DEVICE_NOT_CONNECTED;
    memset(st, 0, sizeof(*st));
    st->dwPacketNumber = 77;
    st->Gamepad.wButtons = g_realButtons;
    return ERROR_SUCCESS;
}
using XI_t = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
static void* g_slotA = (void*)&FakeXInput;  // exclusive
static void* g_slotB = (void*)&FakeXInput;  // merge (hooked second, so the hook runs in merge mode afterwards)

static std::atomic<bool> g_stopGame{false};
static std::atomic<int> g_framesA{0}, g_framesB{0}, g_framesStick{0}, g_framesEsc{0}, g_framesRealW{0};
static std::atomic<uint32_t> g_lastPacket{0};
static void GameThread() {  // 60 Hz: 5 pad reads per frame (like PadMgrUpdate) + one keyboard read
    while (!g_stopGame) {
        XINPUT_STATE s{};
        DWORD r = 0;
        for (DWORD i = 0; i < 4; ++i) {
            XINPUT_STATE t{};
            const DWORD x = reinterpret_cast<XI_t>(g_slotA)(i, &t);
            if (i == 0) s = t, r = x;
        }
        reinterpret_cast<XI_t>(g_slotA)(0, &s);
        if (r == ERROR_SUCCESS) {
            g_lastPacket = s.dwPacketNumber;
            if (s.Gamepad.wButtons & XINPUT_GAMEPAD_A) ++g_framesA;
            if (s.Gamepad.wButtons & XINPUT_GAMEPAD_B) ++g_framesB;
            if (s.Gamepad.sThumbLY == 32767) ++g_framesStick;
        }
        uint8_t keys[256] = {};
        keys[0x11] = 0x80;  // the "user" holds W in another window
        if (KeyFilter(keys, true)) {
            if (keys[0x01] & 0x80) ++g_framesEsc;
            if (keys[0x11] & 0x80) ++g_framesRealW;
        }
        Sleep(16);
    }
}

static int FakeMenu(const char* key, int set, int value, char* reply, size_t n, unsigned) {
    if (_stricmp(key, "GF") != 0) {
        snprintf(reply, n, "no menu item '%s'", key);
        return -1;
    }
    snprintf(reply, n, "GPU wait fix (GF) = %s (value %d)", set ? (value ? "ON" : "OFF") : "OFF", set ? value : 0);
    return 0;
}
static int FakePause() { return 0; }

static std::string Exec(const char* line) {
    char r[640] = {};
    TestExec(line, r, sizeof(r));
    return r;
}

static void TestHookExclusive() {
    StartInfo si;
    si.log = &Log;
    si.version = "test";
    si.menuRemote = &FakeMenu;
    si.pauseReason = &FakePause;
    TestSetHost(si);
    CHECK(TestHookSlot(&g_slotA, (uint8_t*)1, 1), "hook not installed on the fake slot");
    CHECK(g_slotA != (void*)&FakeXInput, "slot not swapped");
    XINPUT_STATE s{};
    g_realCalls = 0;
    CHECK(reinterpret_cast<XI_t>(g_slotA)(1, &s) == ERROR_DEVICE_NOT_CONNECTED, "slot 1 must be 'not connected'");
    CHECK(reinterpret_cast<XI_t>(g_slotA)(3, &s) == ERROR_DEVICE_NOT_CONNECTED, "slot 3 must be 'not connected'");
    CHECK(reinterpret_cast<XI_t>(g_slotA)(0, &s) == ERROR_SUCCESS && s.Gamepad.wButtons == 0, "slot 0 neutral virtual pad");
    const DWORD p0 = s.dwPacketNumber;
    reinterpret_cast<XI_t>(g_slotA)(0, &s);
    CHECK(s.dwPacketNumber == p0, "unchanged state keeps its packet number");
    CHECK(g_realCalls == 0, "exclusive mode must never call the real XInputGetState (%d calls)", g_realCalls.load());

    std::thread game(GameThread);
    std::string r = Exec("press A 50");
    CHECK(r.rfind("ok press 1000 50 ms", 0) == 0, "press reply: %s", r.c_str());
    Sleep(60);
    CHECK(g_framesA >= 3 && g_framesA <= 6, "A seen by %d frames (50 ms at 60 Hz: 3-5)", g_framesA.load());
    r = Exec("hold B");
    CHECK(r.find("held=2000") != std::string::npos && r.find("seen by the game") != std::string::npos, "hold reply: %s", r.c_str());
    Sleep(100);
    r = Exec("release all");
    CHECK(r.find("held=0000") != std::string::npos, "release reply: %s", r.c_str());
    const int b = g_framesB;
    Sleep(80);
    CHECK(b >= 6 && g_framesB == b, "B held %d frames then released (%d)", b, g_framesB.load());
    r = Exec("stick L 0 1 100");
    CHECK(r.rfind("ok stick L 0 32767 100 ms", 0) == 0 && g_framesStick >= 6, "stick: %s / %d frames", r.c_str(), g_framesStick.load());
    r = Exec("key ESC 40");
    CHECK(r.rfind("ok key 01 40 ms", 0) == 0 && g_framesEsc >= 2, "key: %s / %d frames", r.c_str(), g_framesEsc.load());
    CHECK(g_framesRealW == 0, "real key W reached the game while it is not foreground (%d frames)", g_framesRealW.load());
    r = Exec("menu set GF 1");
    CHECK(r == "ok GPU wait fix (GF) = ON (value 1)", "menu set: %s", r.c_str());
    r = Exec("menu get NOPE");
    CHECK(r.rfind("err no menu item", 0) == 0, "menu get unknown: %s", r.c_str());
    r = Exec("state");
    CHECK(r.rfind("ok game=? menu_pause=0 fg=0 armed=1 pad_reads=", 0) == 0 && r.find("pad_age_ms=") != std::string::npos, "state: %s", r.c_str());
    r = Exec("disarm");
    CHECK(r.rfind("ok disarmed", 0) == 0, "disarm: %s", r.c_str());
    g_realCalls = 0;
    Sleep(50);
    CHECK(g_realCalls > 0, "disarmed: the real API is called again");
    r = Exec("press A");
    CHECK(r.rfind("err disarmed", 0) == 0, "press while disarmed: %s", r.c_str());
    r = Exec("arm");
    CHECK(r == "ok armed=1", "arm: %s", r.c_str());

    // ---- 4. pipe ----
    wchar_t name[96];
    swprintf_s(name, L"\\\\.\\pipe\\nioh3vpad_test_%lu", GetCurrentProcessId());
    CHECK(TestStartPipe(name), "pipe thread");
    HANDLE h = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 100 && h == INVALID_HANDLE_VALUE; ++i) {
        h = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) Sleep(20);
    }
    CHECK(h != INVALID_HANDLE_VALUE, "pipe client connect (%lu)", GetLastError());
    HANDLE h2 = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    for (int i = 0; i < 50 && h2 == INVALID_HANDLE_VALUE; ++i) {
        Sleep(20);
        h2 = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    }
    CHECK(h2 != INVALID_HANDLE_VALUE, "second client");
    if (h != INVALID_HANDLE_VALUE) {
        const char* script = "ping\r\nstate\npress A+B 30\nfrobnicate\nmenu set GF 0\nlog hello from the test\n";
        DWORD wr = 0;
        WriteFile(h, script, (DWORD)strlen(script), &wr, nullptr);
        std::string all;
        char buf[4096];
        const ULONGLONG t0 = GetTickCount64();
        int lines = 0;
        while (lines < 6 && GetTickCount64() - t0 < 5000) {
            DWORD got = 0;
            if (!ReadFile(h, buf, sizeof(buf) - 1, &got, nullptr) || !got) break;
            buf[got] = 0;
            all += buf;
            lines = 0;
            for (char ch : all) lines += ch == '\n';
        }
        CHECK(lines == 6, "6 reply lines, got %d: %s", lines, all.c_str());
        const char* want[] = {"ok vpad test mode=exclusive armed=1", "ok game=?", "ok press 3000 30 ms", "err unknown command",
                              "ok GPU wait fix (GF) = OFF (value 0)", "ok logged"};
        size_t pos = 0;
        for (const char* w : want) {
            const size_t e = all.find('\n', pos);
            const std::string ln = all.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
            CHECK(ln.rfind(w, 0) == 0, "pipe reply '%s' should start with '%s'", ln.c_str(), w);
            pos = e == std::string::npos ? all.size() : e + 1;
        }
        CloseHandle(h);
    }
    if (h2 != INVALID_HANDLE_VALUE) {
        DWORD wr = 0, got = 0;
        WriteFile(h2, "ping\n", 5, &wr, nullptr);
        char buf[256] = {};
        ReadFile(h2, buf, sizeof(buf) - 1, &got, nullptr);
        CHECK(!strncmp(buf, "ok vpad", 7), "second client ping: %s", buf);
        CloseHandle(h2);
    }
    g_stopGame = true;
    game.join();
    // a press while the game does not read the pad: timeout, cancelled, reported
    const ULONGLONG t0 = GetTickCount64();
    r = Exec("press X 10");
    CHECK(r.rfind("err timeout", 0) == 0 && GetTickCount64() - t0 < 4500, "no reader: %s", r.c_str());
}

static void TestHookMerge() {
    CHECK(TestHookSlot(&g_slotB, (uint8_t*)1, 2), "merge hook");
    XINPUT_STATE s{};
    g_realButtons = XINPUT_GAMEPAD_X;
    g_realConnected = true;
    CHECK(reinterpret_cast<XI_t>(g_slotB)(0, &s) == ERROR_SUCCESS && s.Gamepad.wButtons == XINPUT_GAMEPAD_X && s.dwPacketNumber == 77,
          "merge: real pad passes untouched without virtual input");
    CHECK(reinterpret_cast<XI_t>(g_slotB)(1, &s) == ERROR_DEVICE_NOT_CONNECTED, "merge: other slots as the real API says");
    Exec("arm");
    std::thread([] { Sleep(1); }).join();
    std::atomic<bool> stop{false};
    std::atomic<int> both{0};
    std::thread g([&] {
        while (!stop) {
            XINPUT_STATE t{};
            if (reinterpret_cast<XI_t>(g_slotB)(0, &t) == ERROR_SUCCESS && (t.Gamepad.wButtons & (XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_A)) ==
                                                                               (XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_A))
                ++both;
            Sleep(16);
        }
    });
    const std::string r = Exec("press A 40");
    CHECK(r.rfind("ok press 1000", 0) == 0 && both >= 2, "merge press: %s, both %d", r.c_str(), both.load());
    g_realConnected = false;
    Exec("hold Y");
    Sleep(40);
    XINPUT_STATE u{};
    CHECK(reinterpret_cast<XI_t>(g_slotB)(0, &u) == ERROR_SUCCESS && u.Gamepad.wButtons == XINPUT_GAMEPAD_Y,
          "merge: virtual slot 0 exists without a real pad");
    Exec("release all");
    stop = true;
    g.join();
    // realpads off (test popup's block toggle): the real pad is hidden, the real API is not called
    g_realConnected = true;
    g_realButtons = XINPUT_GAMEPAD_X;
    std::string rr = Exec("realpads off");
    CHECK(rr == "ok real pads hidden from the game", "realpads off: %s", rr.c_str());
    g_realCalls = 0;
    XINPUT_STATE w{};
    CHECK(reinterpret_cast<XI_t>(g_slotB)(0, &w) == ERROR_SUCCESS && w.Gamepad.wButtons == 0, "realpads off: slot 0 without the real X");
    CHECK(reinterpret_cast<XI_t>(g_slotB)(1, &w) == ERROR_DEVICE_NOT_CONNECTED && g_realCalls == 0, "realpads off: real API not called");
    rr = Exec("realpads on");
    CHECK(rr == "ok real pads reach the game", "realpads on: %s", rr.c_str());
    CHECK(reinterpret_cast<XI_t>(g_slotB)(0, &w) == ERROR_SUCCESS && w.Gamepad.wButtons == XINPUT_GAMEPAD_X, "realpads on: real X back");
    CHECK(Parse("realpads maybe").op == Op::Bad, "realpads usage");
}

// ---- 5. thunk ----
static volatile uint8_t g_fakeActive = 0;
static volatile int g_fakeCalls = 0;
__declspec(noinline) static uint8_t FakeIsActive() {
    ++g_fakeCalls;
    return g_fakeActive;
}
static void TestThunk() {
    uint8_t* t = TestBuildThunk((void*)&FakeIsActive);
    CHECK(t != nullptr, "thunk page");
    if (!t) return;
    auto f = reinterpret_cast<uint8_t (*)()>(t);
    volatile uint8_t* sw = t + 0x28;
    volatile long* cnt = (volatile long*)(t + 0x2C);
    CHECK(*sw == 1 && *cnt == 0, "switch on, count 0 after build");
    g_fakeActive = 0;
    CHECK(f() == 1 && *cnt == 1, "inactive + switch on -> active, counted");
    *sw = 0;
    CHECK(f() == 0 && *cnt == 1, "switch off -> the real answer");
    g_fakeActive = 1;
    *sw = 1;
    CHECK(f() == 1 && *cnt == 1, "really active -> active, not counted");
    CHECK(g_fakeCalls == 3, "the original is called every time (%d)", g_fakeCalls);
    printf("THUNK");
    for (int i = 0; i < 0x30; ++i) printf(" %02X", t[i]);
    printf("\n");
}

int main() {
    timeBeginPeriod(1);  // 16 ms fake frames, not 31
    TestParser();
    TestComposer();
    TestThunk();
    TestHookExclusive();
    TestHookMerge();
    if (g_fail) {
        printf("%d FAILED\n", g_fail);
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
