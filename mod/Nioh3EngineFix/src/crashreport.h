// crashreport.h - crash report writer ([Debug] CrashReport, 1.7.x; bridge #38361 / human #38315).
// When the game dies (unhandled exception, abort/terminate/invalid-parameter/purecall, or the engine's modal "Fatal error"
// dialog) a small text report is written BESIDE the .asi (never in a save folder; nothing in this file reads or writes save
// data): Nioh3EngineFix-crash-<date>-<time>.txt + Nioh3EngineFix-crash-latest.txt. It holds the exception code/address, the
// module+offset of every stack frame, registers, the build shas of the Nioh3*.asi files, the loaded module list, the mod's
// last published state (SetState) and the last log lines, so a player can send us one file.
//
// Safety: the unhandled-exception filter only signals a helper thread (own stack, so stack overflow works) and waits up to 8 s
// for it; the report is then written by that thread and the filter returns EXCEPTION_CONTINUE_SEARCH (stock crash behaviour is
// unchanged: Windows/Steam crash handling still runs). No vectored handler: first-chance (handled) faults never make a report.
//
// Ini ([Debug]): CrashReport = 1 (default; 0 = off, nothing installed), CrashDump = 0 (1 = also write a MiniDump .dmp beside the
// report), CrashTest = 0 (forced-crash hook for pc: 1 access violation, 2 stack overflow, 3 abort(), 4 fatal-error popup,
// 5 terminate (unhandled C++ exception), 6 invalid CRT parameter), CrashTestDelaySec = 30 (seconds after Start).
#pragma once
#include <cstdint>

namespace crashreport {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    void* selfModule = nullptr;  // HMODULE of this .asi (report folder + sha of the sibling .asi files)
    uintptr_t base = 0;          // Nioh3.exe image base
    bool knownBuild = false;
};

void Start(const StartInfo& info);

// Mod state published for the report ("last mod state"). Cheap, lock-free, safe from any thread; key/value are truncated
// (key 24, value 96 chars); the same key overwrites its slot (max 32 keys).
void SetState(const char* key, const char* value);
void SetStateF(const char* key, const char* fmt, ...);

// Menu "Report" row (#38437): writes the same report as a crash, without a crash, to Nioh3EngineFix-report-<date>-<time>.txt
// + Nioh3EngineFix-report-latest.txt beside the .asi. Blocks up to 3 s (call it from the menu thread, not a hot path). Returns
// false (path empty) when crash reports are off or the write failed. The menu Report row always opens that
// folder and the report website (human #43461). [Report] Url is the site. [Report] Open is kept for old ini files.
bool Snapshot(char* pathOut, unsigned cap);
const char* ReportUrl();
bool ReportOpenWanted();
bool CopyReportUrl();      // URL to the clipboard (text)
void OpenReportUrl();      // ShellExecute "open"
void OpenReportFolder(const char* path);  // Explorer selects the snapshot, or opens the report folder

// Feeds the last-log-lines ring (called from the mod's Log()).
void NoteLog(const char* line);

}  // namespace crashreport
