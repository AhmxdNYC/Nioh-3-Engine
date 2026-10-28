#pragma once
#include "titlegate_core.h"
namespace savefiles {
// Ally native screen validation #41491; real-root adapter requires separate opt-in.
using Sample = titlegate::Sample;
using Gate = titlegate::Gate;
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
bool QueueNamed(const char* name,bool backup,bool relaunch=false);
bool QueueNewBackup();
bool LiveAvailable();
bool QueueLive(const char* name);
bool Pending();
void CancelPending();
const char* Note();
}
