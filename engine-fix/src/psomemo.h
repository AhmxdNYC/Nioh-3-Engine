// psomemo.h - PCM: memo in front of the graphics-PSO cache lookup on the D3D12 replay thread (1.7.58, default off).
// notes\cpu-bound-midrange.md.
//
// Every draw whose PSO state is dirty calls PsoGetOrCreateGfx 119670 on SynergisticDispatcherTask: hash the 0x80-byte
// key (PsoKeyHash), EnterCriticalSection, PsoCacheFindGfx 11976C (call site 1196BD) = a B-tree walk (PsoCacheBTreeFind
// 119820: per node up to 18 entries of 0x90 bytes, u32 hash compare, then a 128-byte memcmp) that copies the found
// 0x90-byte entry {hash, pad, key[128], PSO*} over the caller's key. On pc2 (5600X, ReBAR) that walk is ~11 % of the
// dispatcher (~2.5-3 ms/frame), and the dispatcher is the frame's tail: [App]Game spins ~6 ms/frame in
// WaitDispatcherDrainSpin 418F40 waiting for it.
//
// PCM retargets the call at 1196BD to PmFind: a direct-mapped table (4096 entries, index = hash) holding copies of entries
// the original returned. A hit needs the same hash AND the same 128-byte key and copies the stored 0x90 bytes - exactly
// what PsoCacheFindGfx would have copied, since a cached entry is never changed after insert (a failed re-insert keeps the
// old PSO). The table is flushed whenever the cache's entry count (cache+28h) changes (insert, grow, clear), and it serves
// only the first cache it sees; all calls run under the cache's own critical section (taken by 119670 before the call).
// Misses go to the original and fill the table. Same PSOs, same order of D3D12 calls.
//   [Patches] PCM = 0 | 1 | 2   (0 = nothing; 1 = installed + ON; 2 = installed, switch OFF = original lookup only).
//   Live menu row; "stats/s PCM:" line every 30 s (lookups, hits, misses, flushes).
#pragma once
#include <cstdint>

namespace psomemo {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;       // Nioh3.exe image base
    bool knownBuild = false;  // v2.02 only
};
void Start(const StartInfo& info);
volatile uint32_t* Switch();  // nullptr when not installed
void StatusLine(char* buf, unsigned n, unsigned secs);
#ifdef PSOMEMO_TEST
void TestInit(bool (*orig)(void*, uint8_t*));
bool TestFind(void* cache, uint8_t* key);
#endif

}  // namespace psomemo
