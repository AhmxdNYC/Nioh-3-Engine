"""Report #67: the real SealQueueDespawn / SealDeferRelease must never hand the game's despawn queue an actor that is not in the
actor table (a freed seal crashed the drain at Nioh3.exe+2BF847). Compiles the two functions from src/bosslog.cpp with stubs."""
from pathlib import Path
import os, subprocess, sys, tempfile
root = Path(__file__).resolve().parent.parent
src = Path(sys.argv[1] if len(sys.argv) > 1 else root / 'src/bosslog.cpp').read_text(encoding='utf-8')
def fn(start):
    i = src.index(start + ' {'); j = src.index('\n}', i)
    return src[i:j + 2]
body = fn('void SealDeferRelease(const char* why)') + '\n' + fn('int SealQueueDespawn(uint64_t cap)')
pre = r'''
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
int present = 1, queued = 0, origCalls = 0; uint64_t lastQueued = 0;
void Log(const char*, ...) {}
double Now() { return 1.0; }
static void FakeWrapper(uint64_t a) { ++queued; lastQueued = a; }
static void FakeOrig(uint64_t, uint64_t a) { ++origCalls; lastQueued = a; }
uint64_t g_base = 0;
uint64_t Grva(uint64_t r) { return r == 0x681E9C ? (uint64_t)&FakeWrapper : r; }
bool Copy(void* d, uint64_t, size_t n) { static const uint8_t w[] = {0x48,0x83,0xEC,0x28,0x48,0x8B,0xD1,0x48,0x8B,0x0D}; memcpy(d, w, n < sizeof(w) ? n : sizeof(w)); return true; }
uint64_t ActorTableGlobal() { return 1; }
int FormHideSealPresent(uint64_t, uint64_t) { return present; }
uint64_t g_seoForce = 0;
void (*g_sdOrig)(uint64_t, uint64_t) = FakeOrig;
uint64_t g_sdHeld = 0, g_sdList = 0; int g_sdSkips = 0; double g_sdHeldT = 0;
'''
main = r'''
int main() {
    int fails = 0;
    #define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
    for (int p : {0, -1}) { present = p; queued = 0; CHECK(SealQueueDespawn(0x1234) == -2); CHECK(queued == 0); }
    present = 1; queued = 0; CHECK(SealQueueDespawn(0x1234) == 1); CHECK(queued == 1 && lastQueued == 0x1234); CHECK(g_seoForce == 0);
    for (int p : {0, -1}) { present = p; origCalls = 0; g_sdHeld = 0x55; g_sdList = 7; g_sdSkips = 3; SealDeferRelease("t");
        CHECK(origCalls == 0); CHECK(g_sdHeld == 0); CHECK(g_sdSkips == 0); }
    present = 1; origCalls = 0; g_sdHeld = 0x55; SealDeferRelease("t"); CHECK(origCalls == 1 && lastQueued == 0x55); CHECK(g_sdHeld == 0);
    printf(fails ? "DESPAWN GATE FAILED (%d)\n" : "DESPAWN GATE OK (%d)\n", fails);
    return fails != 0;
}
'''
d = Path(tempfile.mkdtemp(prefix='dg67_'))
(d / 't.cpp').write_text(pre + body + main, encoding='utf-8')
r = subprocess.run(['cl', '/nologo', '/EHa', '/std:c++17', 't.cpp'], cwd=d, capture_output=True, text=True)
if r.returncode: print(r.stdout[-3000:], r.stderr[-2000:]); sys.exit(1)
r = subprocess.run([str(d / 't.exe')], capture_output=True, text=True)
print(r.stdout.strip()); sys.exit(r.returncode)
