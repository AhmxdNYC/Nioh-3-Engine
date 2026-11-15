// padrec_harness.cpp - replay target for the input recorder round-trip test (tests/run_padrec_test.py). Not a game test.
// The real VirtualPad code (src/vpad.cpp: XInputGetState hook on a fake IAT slot, exclusive mode, the real named-pipe server)
// plus a fake 60 Hz game thread that reads slot 0 like PadMgrUpdate (several calls per frame) and prints every frame:
//   F <us> <frame> <buttons hex> <lt> <rt> <lx> <ly> <rx> <ry>     (us = QueryPerformanceCounter in microseconds, the same
//                                                                   clock as Python's time.perf_counter)
//   padrec_harness.exe <pipe name> [max seconds]   prints READY once the pipe is up; stops when stdin closes or after max s
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <timeapi.h>
#include <xinput.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>

#include "../src/vpad.h"

static DWORD WINAPI FakeXInput(DWORD, XINPUT_STATE*) { return ERROR_DEVICE_NOT_CONNECTED; }  // no real pad
using XI_t = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
static void* g_slot = (void*)&FakeXInput;
static std::atomic<bool> g_stop{false};
static void Log(const char*, ...) {}

static long long NowUs() {
    static const long long f = [] { LARGE_INTEGER q; QueryPerformanceFrequency(&q); return q.QuadPart; }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (long long)((double)c.QuadPart * 1e6 / (double)f);
}

static void GameThread() {
    unsigned frame = 0;
    while (!g_stop) {
        XINPUT_STATE s{};
        for (DWORD i = 0; i < 4; ++i) {  // PadMgrUpdate: every slot, then slot 0 again
            XINPUT_STATE t{};
            reinterpret_cast<XI_t>(g_slot)(i, &t);
        }
        const long long us = NowUs();
        const DWORD r = reinterpret_cast<XI_t>(g_slot)(0, &s);
        const XINPUT_GAMEPAD& g = s.Gamepad;
        if (r == ERROR_SUCCESS)
            printf("F %lld %u %04X %u %u %d %d %d %d\n", us, frame, g.wButtons, g.bLeftTrigger, g.bRightTrigger, g.sThumbLX, g.sThumbLY,
                   g.sThumbRX, g.sThumbRY);
        ++frame;
        Sleep(16);
    }
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        printf("usage: padrec_harness <pipe name> [max seconds]\n");
        return 2;
    }
    const int maxSec = argc > 2 ? _wtoi(argv[2]) : 120;
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    timeBeginPeriod(1);
    vpad::StartInfo si;
    si.log = &Log;
    si.version = "padrec-test";
    vpad::TestSetHost(si);
    if (!vpad::TestHookSlot(&g_slot, (uint8_t*)1, 1) || g_slot == (void*)&FakeXInput) {
        printf("FAIL hook not installed\n");
        return 1;
    }
    if (!vpad::TestStartPipe(argv[1])) {
        printf("FAIL pipe thread\n");
        return 1;
    }
    for (int i = 0; i < 200; ++i) {  // the pipe exists once a client can open it
        HANDLE h = CreateFileW(argv[1], GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            break;
        }
        Sleep(10);
    }
    std::thread game(GameThread);
    printf("READY\n");
    fflush(stdout);
    std::thread watch([] {  // stdin closed = the test is done
        char b[64];
        while (fgets(b, sizeof(b), stdin)) {
        }
        g_stop = true;
    });
    watch.detach();
    const long long t0 = NowUs();
    while (!g_stop && NowUs() - t0 < (long long)maxSec * 1000000) Sleep(20);
    g_stop = true;
    game.join();
    printf("DONE\n");
    fflush(stdout);
    ExitProcess(0);  // the pipe server thread never returns
}
