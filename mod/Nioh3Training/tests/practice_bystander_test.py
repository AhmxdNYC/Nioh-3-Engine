"""Human #122020: in a Practice arena session the arena world's own characters near the boss spot are AI-held (they killed the Training
dummy in the Dojo) and released when the session ends; a load drops the list without touching freed actors. Runs the production ByTick /
ByRelease source against a fake actor table."""
from pathlib import Path
import subprocess, sys, tempfile
s = (Path(__file__).resolve().parents[1] / 'src/training.c').read_text()
a = s.index('enum { BY_MAX = 64 };'); b = s.index('static void BhTick(void) {', a)
prod = s[a:b]
pre = r'''
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
typedef unsigned long DWORD;
#define Log(...) ((void)0)
static DWORD tick = 1000; static DWORD GetTickCount(void) { return tick; }
static struct { int active, arena, practice; float bp[3]; } g_tr;
static uint8_t gs[3]; static const uint8_t* GameState(void) { return gs; }
static float act[5][8]; static int inTable[5] = {1,1,1,1,1}; static uint64_t B;
static uint64_t Grva(uint64_t r) { return r; }
static int offs, ons; static uint64_t lastOn;
static int Actors(uint64_t* o, int m) { int n = 0; for (int i = 0; i < 5 && n < m; ++i) if (inTable[i]) o[n++] = (uint64_t)act[i]; return n; }
static uint64_t PlayerChar(void) { return (uint64_t)act[0]; }
static int InSp(uint64_t a) { return a == (uint64_t)act[1]; }
#define __try if (1)
#define __except(x) else if (0)
#define EXCEPTION_EXECUTE_HANDLER 1
static int hides, shows; static int Rd64(uint64_t a, uint64_t* o) { *o = a; return 1; }
static void FadeTo(uint64_t c, float t, float f) { (void)c; (void)f; if (t == 0.0f) ++hides; else ++shows; }
'''
main = r'''
static void Off(uint64_t a) { (void)a; ++offs; }
static void On(uint64_t a) { lastOn = a; ++ons; }
int main(void) {
    // positions at +0x110 = float index 0x110/4 = 68 -> use a separate layout: actor memory big enough
    return 0;
}
'''
# Build a real fake: actors are byte buffers; the AI calls go through B + Grva(rva) -> map rva to functions.
pre = pre.replace('static float act[5][8];', 'static unsigned char act[5][0x200];')
prod2 = prod.replace('((void (*)(uint64_t, float, float))(B + Grva(0x2BC5DC)))', 'FadeTo').replace('((void (*)(uint64_t))(B + Grva(0x66982C)))', 'AiOn').replace('((void (*)(uint64_t))(B + Grva(0x668D74)))', 'AiOff')
assert 'AiOn' in prod2 and 'AiOff' in prod2 and 'FadeTo' in prod2
main = r'''
static void SetPos(int i, float x, float z) { float p[4] = {x, 0, z, 0}; memcpy(act[i] + 0x110, p, sizeof p); }
int main(void) {
    g_tr.bp[0] = 1000; g_tr.bp[2] = 1000;
    SetPos(0, 1000, 1100); SetPos(1, 1000, 1000); SetPos(2, 1500, 1000); SetPos(3, 1000, 4900); SetPos(4, 9000, 9000);  // player, our boss, near, near edge, far
    ByTick(); assert(offs == 0 && !g_by.on);  // no session
    g_tr.active = 1; g_tr.arena = 1; g_tr.practice = 0; ByTick(); assert(offs == 0);  // real Boss Match arena: untouched
    g_tr.practice = 1; ByTick(); assert(g_by.n == 2 && offs == 2 && hides == 2);  // hidden once each (#122041); only the two bystanders in range, never the player or our boss
    ByTick(); assert(offs == 2);  // re-apply once per second
    tick += 1000; ByTick(); assert(offs == 4 && g_by.n == 2);
    g_tr.active = 0; inTable[3] = 0; ByTick(); assert(ons == 1 && shows == 1 && hides == 2 && lastOn == (uint64_t)act[2] && g_by.n == 0 && !g_by.on);  // release only actors still in the table
    g_tr.active = 1; tick += 1000; ByTick(); assert(g_by.n == 1);
    gs[2] = 1; ons = 0; ByTick(); assert(ons == 0 && g_by.n == 0);  // load: list dropped, no call on freed actors
    gs[2] = 0; g_cfg_by_hide = 0; hides = 0; tick += 1000; ByTick(); assert(g_by.n == 1 && hides == 0); g_tr.active = 0; ByTick(); g_tr.active = 1; g_cfg_by_hide = 1;  // PracticeBystanderHide 0 = AI hold only
    gs[2] = 0; g_cfg_by_radius = 0; offs = 0; ByTick(); assert(offs == 0);  // radius 0 = off
    puts("PASS practice-bystander: Practice arena holds nearby world actors, releases on session end, drops on load, Boss Match untouched");
    return 0;
}
'''
code = pre + 'static int ao, an; static void AiOff(uint64_t a){ (void)a; ++offs; } static void AiOn(uint64_t a){ lastOn = a; ++ons; }\n' + prod2 + main
with tempfile.TemporaryDirectory() as d:
    p = Path(d); (p / 'by.c').write_text(code)
    subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-O0', '-UNDEBUG', str(p / 'by.c'), '-o', str(p / 'by.exe')], check=True)
    subprocess.run([str(p / 'by.exe')], check=True)
