// tour.h - Tour mode ([Debug] Tour = 0 by default, 1.7.36): the mod tours the game's stages by itself for the stability /
// performance test (notes\tour-mode.md, remaining-work 6.2). No input is injected: it loads each TourList mission through
// the game's own mission-request queue (MissionRequestById 2239600, the Mission Replay path) and moves the player with the
// game's own fast-travel sequencer (FastTravelRequestPost 22395AC, request filled by FillWarpPoint 3B2528 from the game's
// warp-point table) to a few points per stage, dwells TourDwellSec at each and logs one "Tour P ..." line per point
// (frame-time p50/p99, fps, CPU, memory, handles, VRAM, load / warp seconds). With [Profiler] GPUProf = 2 / SP = 1 their
// windows follow the dwells ("GP window tour ...", "SP window tour ...").
//   Tour = 1: only missions the save has unlocked or cleared.   Tour = 2: the whole list (saves blocked, wrapper restores).
// SAVE SAFETY (both modes, whole process lifetime): the user-save writer call 5B6F37 (SaveFileWriteAtomic 5B7BB4) and the
// BACKUP.BIN writer calls 1FDF2D0 / 1FDF62A (1FD9A48) are retargeted to thunks that report success without writing
// (patches.json SB1-SB3); the game's CreateFileW / MoveFileExW / MoveFileW / DeleteFileW / CopyFileW / SetFileAttributesW
// imports deny any write-type call into ...\KoeiTecmo\NIOH3\Savedata (count logged, expected 0). Refuses to run online.
// Game-thread tick: the E8 at 56529C (PerFrameUpdater -> FastTravelSeqTickCall 3B29E4, patches.json TT1) goes through a thunk
// that saves rcx/rdx/r8-r11/rax/xmm0-5, calls the tour state machine, and jumps to the original.
// Resume: Nioh3EngineFix_tour\progress.txt (tools\tour\run_tour.ps1 deletes it for a fresh run).
#pragma once
#include <cstdint>

namespace tour {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [Debug] Tour, TourDwellSec, TourPointsPerStage, TourList, TourPasses, ...
    uintptr_t base = 0;                // Nioh3.exe image base
    bool knownBuild = false;           // v2.02 only
};

// [Debug] Tour from the ini (0 off, 1 unlocked missions, 2 all)
int Mode(const wchar_t* iniPath);
// Installs the save block, the IAT guard and the tick hook and starts the worker (Tour != 0 and known build only).
// Must run before the player can reach a save point (dllmain calls it with the other ASI modules after unpacking).
void Start(const StartInfo& info);
// true once the save block is installed (the menu / OSD may show "TOUR: saves blocked")
bool SavesBlocked();

// tests\tour_harness.cpp: builds the three thunks on `page` (RWX, 4 KB) around arbitrary callees / tick function.
struct TestThunkSet {
    void *tick = nullptr, *sb1 = nullptr, *sb2 = nullptr;
    volatile uint8_t* block = nullptr;
    volatile long *sb1Count = nullptr, *sb2Count = nullptr;
};
bool TestBuildThunks(uint8_t* page, void* tickCallee, void* sb1Callee, void* backupCallee, void (*tickFn)(), TestThunkSet* out);

}  // namespace tour
