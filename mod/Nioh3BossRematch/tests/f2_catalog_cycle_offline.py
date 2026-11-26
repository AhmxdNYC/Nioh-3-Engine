"""TASK #136451 (human #136449): F2 walks every soul core in the game (SoulCoreCatalog) pair by pair to the last entry, then wraps;
Shift+F2 walks back. Compiles the production ScPairCatPick against a fake catalog. Never touches the game."""
from pathlib import Path
import subprocess, tempfile, shutil, sys
root = Path(__file__).resolve().parent.parent
s = (root / 'src/soul_core_tuning_menu.h').read_text(encoding='utf-8')
i = s.index('int ScPairCatPick('); fn = s[i:s.index('void ScPairStep(int dir) {', i)]
step = s[s.index('void ScPairStep(int dir) {'):s.index('void ScPairFlag() {')]
assert 'Nioh3Training_SoulCoreEquipItem' in step and step.index('ScCatRead(&cat)') < step.index('ScPairList L;'), 'catalog first, owned list only as fallback'
assert 'Nioh3Training_SoulCoreEquipItem' in s[s.index('int ScPairSend(int slot) {'):s.index('bool ScPairDone(')]
fin = s[s.index('void ScPairFinish(bool ok, int r) {'):s.index('void ScPairStep(int dir) {', s.index('void ScPairFinish(bool ok, int r) {'))]
assert 'g_scPairCat && g_scPairSkips < 8' in fin and 'ScPairStep(g_scPairCatDir)' in fin, 'a refused catalog pair is skipped (bounded), not retried forever'
assert 'g_scPairCatP)' in step and 'g_scPairCatP = p' in step, 'the next F2 counts from the last pair tried'
code = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
''' + fn + r'''
int n = 0; void check(bool v, const char* w) { ++n; if (!v) { printf("FAIL %d %s\n", n, w); exit(1); } }
void walk(int cnt, int dir) {
  uint16_t ids[512]; for (int i = 0; i < cnt; ++i) ids[i] = (uint16_t)(0x5000 + i * 7);
  const int np = (cnt + 1) / 2; uint16_t y1 = 0xFFFF; std::set<int> seen; int steps = 0, first = -1, lastA = -1, lastB = -1;
  for (int k = 0; k < np; ++k) {
    int a, b; const int p = ScPairCatPick(ids, cnt, y1, dir, &a, &b); ++steps;
    check(a >= 0 && a < cnt && b >= 0 && b < cnt && a != b, "pair inside the list, two different cores");
    if (k == 0) first = p; seen.insert(a); seen.insert(b); y1 = ids[a]; lastA = a; lastB = b;
  }
  check((int)seen.size() == cnt, "every core in the game reached");
  check(steps == np, "steps = ceil(cores / 2)");
  check(dir > 0 ? (lastA == (cnt - 1) / 2 * 2) : lastA == 0, "the walk reaches the end of the list");
  int a, b; const int p = ScPairCatPick(ids, cnt, y1, dir, &a, &b);
  check(p == first, "after the last pair it wraps to the first");
  (void)lastB;
}
int main() {
  walk(83, 1); walk(83, -1); walk(84, 1); walk(2, 1); walk(3, -1);
  uint16_t ids[4] = {1, 2, 3, 4}; int a, b;
  ScPairCatPick(ids, 4, 0x999, 1, &a, &b); check(a == 0 && b == 1, "unknown core in Yang 1: F2 starts at pair 1");
  ScPairCatPick(ids, 4, 0x999, -1, &a, &b); check(a == 2 && b == 3, "unknown core in Yang 1: Shift+F2 starts at the last pair");
  // live 11-25 8:38 pm: pair 24 was refused (Yang 1 stayed on pair 23's core) and every F2 retried pair 24
  uint16_t big[10]; for (int i = 0; i < 10; ++i) big[i] = (uint16_t)(0x100 + i);
  int p = ScPairCatPick(big, 10, big[4], 1, &a, &b, 3); check(p == 4 && a == 8, "Yang 1 still on pair index 2, pair index 3 tried: F2 goes to pair index 4");
  p = ScPairCatPick(big, 10, big[4], 1, &a, &b, 2); check(p == 3, "Yang 1 matches the last pair: normal step");
  p = ScPairCatPick(big, 10, 0x999, -1, &a, &b, 0); check(p == 4, "Shift+F2 from pair 1 tried: wraps to the last pair");
  printf("f2 catalog cycle ok: %d checks\n", n); return 0;
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
