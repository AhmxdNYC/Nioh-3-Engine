"""TASK #137007 (human #137004): F3 / F4 flip Yang 1 / Yang 2's core between summon and Transform (detection decides moving),
and the soul core overlay look rows (size, place, opacity, background, colors, font). Compiles the production ScCallKey, ScOvStep,
ScOvClamp and the theme table against fakes. Never touches the game."""
from pathlib import Path
import subprocess, tempfile, shutil, sys
root = Path(__file__).resolve().parent.parent
s = (root / 'src/soul_core_tuning_menu.h').read_text(encoding='utf-8')
ui = (root / 'src/ui.cpp').read_text(encoding='utf-8')
rm = (root / 'src/rematch.cpp').read_text(encoding='utf-8')
def cut(a, b):
    i = s.index(a); return s[i:s.index(b, i)]
rows = cut('constexpr int uScOverlay = 951;', 'std::atomic<int> g_scOv[kScOvN];')
clamp = cut('int ScOvClamp(int w, int v)', 'int ScOvGet(int w) {')
step = cut('int ScOvStep(int w, int v, int dir) {', 'void ScOvValueText(')
theme = cut('const uint32_t kScOvTheme[3][4] = {', 'int g_scEqSlot = 0;')
key = cut('void ScCallKey(int y, bool back = false) {', 'bool ScEqChange(int item, int dir) {')
assert 'FormCoreSummonKinds' not in key and 'sc_call_step(cur, back ? -1 : 1)' in key, 'TASK #141276: one cycle for every core, no per-core summon memory'
# wiring: F3 / F4 read in the same place as F2, the overlay thread reads the look and is woken by the keys
assert 'KeyEdge(VK_F3), f4 = KeyEdge(VK_F4)' in ui and 'rematch::SoulCallKeyTick(mine && f3 && !alt, mine && f4 && !alt, sh)' in ui and 'GetAsyncKeyState(VK_MENU)' in ui, 'F3 / F4 wired next to F2'
assert 'ScOverlayKick()' in ui and 'WaitForSingleObject(g_kick, 250)' in ui, 'F2 / F3 / F4 redraw the overlay at once'
assert 'addon::rematch::ScOverlayLook(look)' in ui and 'N3StrokeWindowAlpha(g_wnd, PaintOv' in ui, 'overlay uses the look rows'
assert 'if (g_uiPages.page == uScOverlay) { add(kItScOverlay);' in rm, 'look page lists the on/off row first'
assert 'add(kItScOvOpen);' in s, 'Player > Soul cores has the look page row'
assert '(op * 255 + 50) / 100' in ui, 'opacity rounds: 92% = alpha 235 (#137543)'
assert "colors[n] = (tag[0] == 'S' || tag[0] == 'F') ? th[0]" in rm, "'?' lines use the T colour (white), not the S gray (#137543)"
assert 'SC_CALL_TRANSFORM_MOVE' not in key, 'F3 / F4 never choose Transform moveable (#136987)'
code = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <cwchar>
#include <string>
#include "../../shared/soul_core_api.h"
const uint32_t cWhite = 0xFFFFFF, cGray = 0xB4B4B4, cGreen = 0x6CFF6C, cYellow = 0x40E6FF;
std::string lastToast; int toasts = 0;
void ToastF(uint32_t, unsigned long long, const char* f, ...) { char b[256]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); lastToast = b; ++toasts; }
void Log(const char*, ...) {}
uint32_t eqIds[2] = {0x5129B, 0xA2ACB}; bool eqReady = true, formsOn = true; uint16_t slotItems[2] = {0x6DB5, 0x6E53};
std::map<uint32_t, int> calls;
std::map<std::wstring, std::wstring> ini;  // [Spirit] key -> value, survives a 'restart'
const wchar_t* IniPath() { return L"x.ini"; }
unsigned long GetPrivateProfileStringW(const wchar_t*, const wchar_t* k, const wchar_t* d, wchar_t* b, unsigned long n, const wchar_t*) {
  auto it = ini.find(k); const std::wstring v = it == ini.end() ? d : it->second; wcsncpy(b, v.c_str(), n - 1); b[n - 1] = 0; return (unsigned long)wcslen(b); }
int WritePrivateProfileStringW(const wchar_t*, const wchar_t* k, const wchar_t* v, const wchar_t*) { ini[k] = v; return 1; }
uint16_t ScSlotItem(int y) { return slotItems[y]; }
namespace bosslog {
bool FormCoreEquippedIds(uint32_t out[2]) { out[0] = eqIds[0]; out[1] = eqIds[1]; return eqReady; }
int FormCoreCallGet(uint32_t id) { auto it = calls.find(id); return it == calls.end() ? SC_CALL_TRANSFORM : it->second; }
int FormCoreCallSet(uint32_t id, int c) { calls[id] = sc_call_norm(c); return calls[id]; }
bool SoulCoreForms(bool*) { return formsOn; }
}
''' + rows + clamp + step + theme + key + r'''
int n = 0; void check(bool v, const char* w) { ++n; if (!v) { printf("FAIL %d %s\n", n, w); exit(1); } }
int main() {
  // TASK #141276 (human #141268): F3 / F4 = Summon -> Transform -> Fast summon -> Summon for every core; Shift = the other way
  ScCallKey(0); check(calls[0x5129B] == SC_CALL_FAST_SUMMON, "F3 Transform -> Fast summon");
  check(calls.find(0xA2ACB) == calls.end(), "F3 leaves Yang 2 alone");
  check(lastToast.find("Yang 1 core 6DB5: Fast summon") == 0, "toast names slot, core and call");
  ScCallKey(0); check(calls[0x5129B] == SC_CALL_GAME, "F3 Fast summon -> Summon");
  ScCallKey(0); check(calls[0x5129B] == SC_CALL_TRANSFORM, "F3 Summon -> Transform (3 presses = full circle)");
  ScCallKey(0, true); check(calls[0x5129B] == SC_CALL_GAME, "Shift+F3 Transform -> Summon");
  ScCallKey(0, true); check(calls[0x5129B] == SC_CALL_FAST_SUMMON, "Shift+F3 Summon -> Fast summon");
  ScCallKey(0, true); check(calls[0x5129B] == SC_CALL_TRANSFORM, "Shift+F3 Fast summon -> Transform");
  // same order on a second core whatever it was before (the old per-core memory made cores differ)
  calls[0xA2ACB] = SC_CALL_GAME; ScCallKey(1); check(calls[0xA2ACB] == SC_CALL_TRANSFORM, "F4 Summon -> Transform");
  ScCallKey(1); check(calls[0xA2ACB] == SC_CALL_FAST_SUMMON, "F4 Transform -> Fast summon (Yang 2 same order)");
  ScCallKey(1); check(calls[0xA2ACB] == SC_CALL_GAME, "F4 Fast summon -> Summon");
  ini[L"FormCoreSummonKinds"] = L"A2ACB:3"; calls[0xA2ACB] = SC_CALL_TRANSFORM; ScCallKey(1);
  check(calls[0xA2ACB] == SC_CALL_FAST_SUMMON && ini.size() == 1, "an old FormCoreSummonKinds entry changes nothing, nothing new written");
  for (int i = 0; i < 9; ++i) { ScCallKey(i & 1, i % 3 == 0); check(calls[eqIds[i & 1]] != SC_CALL_TRANSFORM_MOVE, "never Transform moveable"); }
  // not detected yet: no change, a toast
  calls.clear(); ini.clear(); eqReady = false; const int t0 = toasts; ScCallKey(0);
  check(calls.empty() && toasts == t0 + 1 && lastToast.find("not detected") != std::string::npos, "undetected slot: toast, nothing saved");
  eqReady = true; eqIds[1] = 0; slotItems[1] = 0xFFFF; ScCallKey(1);
  check(calls.empty() && lastToast.find("no soul core") != std::string::npos, "empty slot: toast, nothing saved");
  { const int t1 = toasts; ScCallKey(1, true); check(toasts == t1 + 1 && lastToast.find("no soul core") != std::string::npos, "Shift+F4 empty slot: toast only"); }
  eqIds[1] = 0xA2ACB; slotItems[1] = 0x6E53;
  formsOn = false; calls[0x5129B] = SC_CALL_FAST_SUMMON; ScCallKey(0); check(calls[0x5129B] == SC_CALL_GAME, "forms OFF: summon still saved");
  ScCallKey(0); check(calls[0x5129B] == SC_CALL_TRANSFORM && lastToast.find("OFF in the menu") != std::string::npos, "forms OFF: Transform saved, toast says forms OFF");
  formsOn = true; calls.clear(); ini.clear();
  // look rows
  check(ScOvStep(kScOvSize, 100, 1) == 115 && ScOvStep(kScOvSize, 100, -1) == 85, "size steps");
  check(ScOvStep(kScOvSize, 200, 1) == 200 && ScOvStep(kScOvSize, 50, -1) == 50, "size stops at the ends");
  check(ScOvStep(kScOvSize, 200, 0) == 50, "A past the top wraps");
  check(ScOvStep(kScOvX, 92, 1) == 100 && ScOvStep(kScOvX, 92, -1) == 80, "x 1% steps from the 9.2% centre default (#138501)");
  check(ScOvStep(kScOvX, 1000, 1) == 1000 && ScOvStep(kScOvY, 0, -1) == 0, "position clamps 0..100%");
  check(ScOvStep(kScOvAlpha, 20, -1) == 20 && ScOvStep(kScOvAlpha, 90, 1) == 92 && ScOvStep(kScOvAlpha, 92, 1) == 100 && kScOv[kScOvAlpha].def == 92, "opacity steps");
  check(ScOvStep(kScOvBox, 1, -1) == 0 && ScOvStep(kScOvBox, 0, 0) == 1, "background on/off");
  check(ScOvStep(kScOvColors, 2, 1) == 0 && ScOvStep(kScOvColors, 0, -1) == 2 && ScOvStep(kScOvFont, 0, 0) == 1 && ScOvStep(kScOvFont, 3, 1) == 0 && ScOvStep(kScOvFont, 0, -1) == 3, "choices wrap (font has Game = 3, #138501)");
  check(ScOvClamp(kScOvAlpha, 5) == 20 && ScOvClamp(kScOvColors, 9) == 2 && ScOvClamp(kScOvSize, 999) == 200, "bad ini values clamp");
  for (int w = 0; w < kScOvN; ++w) check(ScOvClamp(w, kScOv[w].def) == kScOv[w].def && kScOv[w].foot && kScOv[w].foot[0], "defaults in range, every row has a footer");
  check(kScOv[kScOvX].def == 92 && kScOv[kScOvY].def == 970 && kScOv[kScOvSize].def == 130 && kScOv[kScOvBox].def == 0 && kScOv[kScOvColors].def == 0 && kScOv[kScOvFont].def == 3 && !wcscmp(kScOv[kScOvX].key, L"SoulCoreOverlayCenterX"),
        "defaults = #138501: centred under the left item HUD, Game letters, no panel, menu colours");
  check(kScOvTheme[0][0] == cGray && kScOvTheme[0][1] == cWhite && kScOvTheme[0][2] == cGreen, "Menu theme = #137000 colours");
  printf("sc hotkeys + overlay look ok: %d checks\n", n); return 0;
}
'''
td = Path(tempfile.mkdtemp())
try:
    (td / 'p.cpp').write_text('#include <cstdarg>\n' + code.replace('"../../shared/soul_core_api.h"', '"' + str(root.parent / 'shared/soul_core_api.h').replace('\\', '/') + '"'), encoding='utf-8')
    setup = Path('C:/nioh3-engine/tools/msvc/setup_x64.bat')
    cmd = f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W3 /Od /UNDEBUG /Fo"{td / "p.obj"}" /Fe"{td / "p.exe"}" "{td / "p.cpp"}"'
    r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
    if r.returncode: print(r.stdout[-3000:], r.stderr[-2000:]); sys.exit(1)
    subprocess.run([str(td / 'p.exe')], check=True)
finally:
    shutil.rmtree(td, ignore_errors=True)
