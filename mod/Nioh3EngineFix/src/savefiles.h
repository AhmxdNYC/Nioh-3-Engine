#pragma once
#include "titlegate_core.h"
namespace savefiles {
// Ally native screen validation #41491; real-root adapter requires separate opt-in.
using Sample = titlegate::Sample;
// Save actions also admit the in-title PRESS ANY BUTTON screen; the log-only TITLEGATE keeps the strict gate.
struct Gate : titlegate::Gate { Gate(){admitPressAny=true;} };
inline bool Ready(const Sample& s) { return titlegate::MenuIdle(s) && !s.loading; }
void Configure(uint8_t* base,bool known,const wchar_t* ini,void (*log)(const char*,...));
void Poll();
bool Allowed();
bool ActionAllowed(const char* action);
bool Queue(int slot,bool backup);
constexpr int kMaxSlots=64;
bool ValidSlotName(const char* name);
void RefreshSlots();
int SlotCount();
const char* SlotName(int index);
bool QueueNamed(const char* name,bool backup);
bool QueueNewBackup();
bool LiveAvailable();
// [SaveFiles] Native = 1 (default): operations run in-process (savelib.cpp), no Python; works under Wine/Proton.
bool Native();
void OpenFolder();
bool IsActive(int index);
int Progress(char* stage,size_t n);
bool QueueLive(const char* name);
bool Pending();
bool LivePending();
// One-shot: the EF menu pauses the game, which freezes the soft-reset countdown; close it first.
bool TakeMenuCloseRequest();
void CancelPending();
const char* Note();
}
