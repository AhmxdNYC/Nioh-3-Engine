"""#95567 (player report #59): a wave enemy spawned right after a soul-core press is never the press's seal. Production code, no game."""
from pathlib import Path
import subprocess, sys, tempfile, shutil
root = Path(__file__).resolve().parent.parent
s = (root / 'src/bosslog.cpp').read_text(encoding='utf-8')
def between(a, b): i = s.index(a); return s[i:s.index(b, i)]
note = between('void FormKeySealNote(uint64_t chr, uint32_t motion, float dist) {', 'double primaryAt = 0;')
assert note.index('PressSpawnForeign') < note.index('SealEndOnHitNoteSpawn'), 'foreign check must run before the hit-source note'
desc = between('void FormKeySealDesc(uint32_t id) {', '\n')
assert desc.index('PressSpawnForeign') < desc.index('g_fkSealDesc.store') < desc.index('PickLearn'), 'foreign id must not be learned'
assert 'L"FormHelperMs", 1500' in s
pre = r'''
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
double clockNow = 0; double Now() { return clockNow; }
int logs = 0; void Log(const char*, ...) { ++logs; }
std::atomic<double> g_pickT{-100}; std::atomic<uint32_t> g_pickCore{0}; std::atomic<bool> g_equippedReady{true};
'''
code = pre + between('std::atomic<int> g_formHelperMs{1500};', '// seal spawn hit: learn which core') + r'''
int n = 0; void check(bool v, const char* w) { ++n; if (!v) { printf("FAIL %d %s\n", n, w); exit(1); } }
void press(double t, uint32_t core) { g_pickT = t; g_pickCore = core; clockNow = t; }
int main() {
  const uint32_t ibaraki = 0x5129B, helper = 0xEE49E, crab = 0x3A1C0;
  // report #59: the crab of a wave comes out 0.4 s after the press, before the yokai
  press(100, ibaraki); clockNow = 100.4; check(PressSpawnForeign(crab, 1), "crab before the yokai is foreign");
  clockNow = 100.9; check(!PressSpawnForeign(ibaraki, 2), "the pressed core's yokai is the seal");
  clockNow = 101.3; check(!PressSpawnForeign(helper, 3), "helper 0.4 s after the yokai belongs to the press");
  clockNow = 102.6; check(PressSpawnForeign(crab, 4), "a wave enemy after the helper window is foreign");
  // a new press resets the helper window
  press(200, ibaraki); clockNow = 200.2; check(PressSpawnForeign(helper, 5), "helper with no yokai yet this press is foreign");
  // equipment map not published: old rule (nothing filtered)
  g_equippedReady = false; press(300, ibaraki); clockNow = 300.3; check(!PressSpawnForeign(crab, 6), "no equipment map = old rule");
  g_equippedReady = true;
  press(400, 0); clockNow = 400.3; check(!PressSpawnForeign(crab, 7), "unknown core = old rule");
  press(500, ibaraki); clockNow = 511; check(!PressSpawnForeign(crab, 8), "no press in flight = old rule");
  g_formHelperMs = 0; press(600, ibaraki); clockNow = 600.3; check(!PressSpawnForeign(crab, 9), "FormHelperMs=0 turns the rule off");
  g_formHelperMs = 1500;
  press(700, 0xA2ACB); clockNow = 700.5; check(!PressSpawnForeign(0xA2ACB, 10), "DOWN core A2ACB still forms");
  check(!PressSpawnForeign(0xA2ACB, 10), "A2ACB reuses one chr: a repeat spawn is still the core");
  printf("foreign spawn ok: %d checks, %d log lines\n", n, logs);
  return 0;
}
'''
td = Path(tempfile.mkdtemp())
try:
    (td / 't.cpp').write_text(code, encoding='utf-8')
    subprocess.run([sys.executable, '-m', 'ziglang', 'c++', '-std=c++17', '-target', 'x86_64-windows-gnu', str(td / 't.cpp'), '-o', str(td / 't.exe')], check=True)
    subprocess.run([str(td / 't.exe')], check=True)
finally:
    shutil.rmtree(td, ignore_errors=True)
