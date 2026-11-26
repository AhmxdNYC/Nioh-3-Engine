"""#136368 (human #136365/#136374): the equipped cores' yokai load once when equipment is known, so the first use has no
cold-load wait. Compiles the production CorePreloadTick block against stub game calls. Never touches the game."""
from pathlib import Path
import subprocess, tempfile, shutil, sys
root = Path(__file__).resolve().parent.parent
s = (root / 'src/bosslog.cpp').read_text(encoding='utf-8')
def between(a, b): i = s.index(a); return s[i:s.index(b, i)]
block = between('// #136368 (human #136365', 'uint64_t FkRec(uint32_t id) {')
poll = between('void PickPoll() {', 'static int heldPrev')
assert poll.index('PickEquipmentTick();') < poll.index('CorePreloadTick();'), 'preload runs after the equipment read (game thread)'
assert 'L"FormPreloadCores", 1' in s
ini = (root / 'Nioh3BossRematch.ini').read_text(encoding='utf-8')
assert 'FormPreloadCores = 1' in ini
rv = (root / 'rvas.json').read_text(encoding='utf-8')
assert '"0x3F9E58"' in rv and '"0x1C1DF80"' in rv, 'both game calls have a pattern for other builds'
code = r'''
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
namespace gamerva { bool g_known = true; }
double clockNow = 0; double Now() { return clockNow; }
int logs = 0; void Log(const char* f, ...) { ++logs; va_list a; va_start(a, f); vprintf(f, a); va_end(a); printf("\n"); }
template <class T> bool Rd(uint64_t a, T& v) { memcpy(&v, (void*)a, sizeof v); return true; }
std::atomic<bool> g_equippedReady{false}; std::atomic<uint32_t> g_equippedCore[2];
struct Entry { uint8_t pad[0x2C8]; uint8_t ready; };
std::map<uint32_t, Entry> res; std::map<uint32_t, int> refs; int loads = 0, releases = 0;
uint64_t FkGameKey(uint32_t id) { return 0x300000000ull + id; }
uint64_t FkPack(uint64_t key) { auto it = res.find((uint32_t)(key - 0x300000000ull)); return it == res.end() ? 0 : (uint64_t)&it->second; }
void StubLoad(uint32_t id, uint32_t kind, uint32_t z) { if (kind != 2 || z) exit(3); ++loads; ++refs[id]; res[id].ready = 0; }
void StubRelease(uint32_t id, uint32_t z) { if (z) exit(4); ++releases; --refs[id]; }
bool formOn = false, ghostOn = false; bool PickFormActive() { return formOn; } bool PickGhostAlive() { return ghostOn; }
uint64_t g_base = 0; uint32_t TestGrva(uint32_t rva) {
  return (uint32_t)((rva == 0x3F9E58 ? (uint64_t)&StubLoad : rva == 0x1C1DF80 ? (uint64_t)&StubRelease : (exit(5), 0)) - g_base); }
#define Grva TestGrva
''' + block + r'''
int n = 0; void check(bool v, const char* w) { ++n; if (!v) { printf("FAIL %d %s\n", n, w); exit(1); } }
void tick(double t) { clockNow = t; CorePreloadTick(); }
int main() {
  g_base = ((uint64_t)&StubLoad < (uint64_t)&StubRelease ? (uint64_t)&StubLoad : (uint64_t)&StubRelease) - 0x1000;
  const uint32_t y1 = 0x5216E, y2 = 0x80D5F, y3 = 0x5129B;
  tick(1); check(loads == 0, "no equipment map yet: nothing requested");
  res[y2].ready = 1;  // 80D5F already in memory (the game loaded it)
  g_equippedCore[0] = y1; g_equippedCore[1] = y2; g_equippedReady = true;
  tick(2); check(loads == 1 && refs[y1] == 1, "cold Y1 core requested once"); check(refs[y2] == 0, "loaded Y2 core: no ref taken");
  tick(2.5); tick(3); check(loads == 1, "no repeat request while loading");
  check(!CoreResReady(y1), "Y1 still loading");
  res[y1].ready = 1; tick(3.5); check(CoreResReady(y1), "Y1 ready (the first use now binds without the cold wait)");
  g_equippedReady = false; tick(10); check(releases == 0, "load screen (equipment not ready): the ref stays");
  g_equippedReady = true; tick(11); check(loads == 1 && releases == 0, "same cores after the load: nothing new");
  g_equippedCore[0] = y3; tick(12); check(releases == 1 && refs[y1] == 0, "unequipped core: our one ref released");
  check(loads == 2 && refs[y3] == 1, "new core requested");
  // #136427: Equip core swap while that core's form is out: the release waits for the form and the ghost seal to end
  res[y3].ready = 1; formOn = true; g_equippedCore[0] = y1; tick(13);
  check(releases == 1 && refs[y3] == 1, "swap mid-form: old core's ref kept while the form is alive");
  formOn = false; ghostOn = true; tick(14); check(releases == 1 && refs[y3] == 1, "ghost seal still out: ref kept");
  ghostOn = false; tick(15); check(releases == 2 && refs[y3] == 0, "form + ghost over: deferred ref released once");
  tick(16); check(releases == 2, "no second release");
  g_formPreload = 0; g_equippedCore[0] = y2; tick(17); check(releases == 2, "FormPreloadCores=0: no new request or release");
  printf("core preload ok: %d checks, %d log lines\n", n, logs);
  return 0;
}
'''
td = Path(tempfile.mkdtemp())
try:
    (td / 'p.cpp').write_text(code, encoding='utf-8')
    setup = Path('C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat')
    cmd = f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W3 /Od /Fo"{td / "p.obj"}" /Fe"{td / "p.exe"}" "{td / "p.cpp"}"'
    r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
    if r.returncode: print(r.stdout[-3000:], r.stderr[-2000:]); sys.exit(1)
    subprocess.run([str(td / 'p.exe')], check=True)
finally:
    shutil.rmtree(td, ignore_errors=True)
