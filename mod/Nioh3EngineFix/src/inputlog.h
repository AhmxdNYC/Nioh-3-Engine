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
void NoteTyped();  // report box: never recorded
void NoteMenu(const char* what, const char* page, const char* row, const char* oldv, const char* newv);
int Count();
// Comma-separated JSON objects, oldest of the tail first. No surrounding brackets.
int WriteTail(char* dst, int cap, int maxN);

}  // namespace inputlog
