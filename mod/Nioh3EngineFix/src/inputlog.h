// inputlog.h - memory ring of controller edges, mod hotkeys, and mod-menu selections (human #64888,
// notes/auto-report.md "Input ring"). Not written to Nioh3ModEvents.jsonl. A crash record copies the last 150.
// Typed report-box characters are ignored on purpose.
#pragma once

namespace inputlog {

constexpr int kRing = 4000;
constexpr int kCrashTail = 150;

void Reset();
// One pad sample. Buttons are XInput wButtons. Triggers are 0..255. Sticks are raw -32768..32767.
// Held frames and a stick that stays inside the dead zone add nothing.
void ObservePad(int slot, unsigned buttons, int lt, int rt, int lx, int ly, int rx, int ry);
// Hotkey level (1 held, 0 up). allowed=0 drops it, including typed letters.
void ObserveKey(const char* name, int down, int allowed);
// #118266 gameplay keys: the 256-byte DirectInput keyboard state the game reads (after the mod's masking).
// Edges become {"dev":"kb","what":"<key>","edge":"down|up","ms":..,"held":..}. menuOpen=1 records nothing.
// Every input edge carries "ms" (since the first input); up edges add "held" (ms down).
void ObserveKeyboard(const unsigned char* keys, int menuOpen);
// Mouse buttons: bit0 left, 1 right, 2 middle, 3 x1, 4 x2. dev "mouse". menuOpen=1 records nothing.
void ObserveMouse(unsigned buttons, int menuOpen);
void NoteTyped();  // report box: never recorded
void NoteMenu(const char* what, const char* page, const char* row, const char* oldv, const char* newv);
int Count();
// GH #154: the newest real release (kb / mouse / pad up edge, mod menu closed). ageMs = ms since it, heldMs = how long that
// key was down. Returns false before the first release. Lock-free, any thread.
bool LastRelease(long long* ageMs, long long* heldMs);
// Comma-separated JSON objects, oldest of the tail first. No surrounding brackets.
int WriteTail(char* dst, int cap, int maxN);

}  // namespace inputlog
