// crashupload.h - automatic crash upload (bridge #64881, human #64860 / #64870; design notes/auto-report.md "crash" record).
//
// Every crash that crashreport.cpp catches (unhandled exception, abort, terminate, invalid parameter, purecall, engine fatal
// popup) is also written as one JSON record to Nioh3CrashReports\pending\ beside the .asi and then posted, with no prompt,
// to [Report] CrashUrl (default https://nioh3-mod-feedback.vercel.app/api/crash). The crash path only builds the record from
// static buffers and writes the file; it then tries one send with a short budget (the process is dying anyway; crashreport's
// 8 s cap still bounds it). Whatever is left in pending\ is sent by a low-priority background thread on the next launch
// (StartDelayMs after start, then every 60 s while records remain). Sent records are deleted; a 4xx answer moves the record
// to Nioh3CrashReports\rejected\ so it is not retried forever. The game thread never waits.
//
// Record (design "crash" event + context): t, mod, ver, event "crash", id, fields {code, module, offset, game_stamp,
// game_size, reason}, stack (module+offset), game (exe name, stamp, image/file size, version), mods (every .asi: name, size,
// sha256 prefix, exported version), loaders (proxy DLLs present), os, uptime_s, test, last (Nioh3ModEvents.jsonl tail when
// that shared log exists), input (empty until the input ring lands), log (EngineFix ring + tails of the other mod logs).
// Capped at 64 KB (log tails shrink first). No personal data: install id is 16 random bytes ([Report] InstallId, written once);
// USERPROFILE / any X:\Users\<name>, \home\<name>, /home/<name>, /Users/<name> -> <user>, USERNAME -> <user>,
// COMPUTERNAME -> <pc>, the game folder -> <game>, 17-digit Steam ids -> <steamid>, before the record touches the disk.
//
// Ini [Report]: CrashUpload = 1 (0 = keep records on disk, never post), CrashUrl = (empty = default endpoint),
// CrashBypass = (optional Vercel protection-bypass secret, preview tests only), InstallId = (written once).
// Forced test crash: [Debug] ForceCrashTest = 1 (same as CrashTest = 1, access violation) or CrashTest = 1..6; the record then
// carries "test": <mode>. Network: WinHTTP (works under Wine/Proton); any failure just leaves the record pending.
#pragma once
#include <cstdint>

struct _EXCEPTION_RECORD;
struct _CONTEXT;

namespace crashupload {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    void* selfModule = nullptr;     // HMODULE of this .asi (folder of the records)
    uintptr_t base = 0;             // Nioh3.exe image base (TimeDateStamp / SizeOfImage)
    bool knownBuild = false;
    const char* modVersion = "";    // EngineFix version string
    int startDelayMs = 20000;       // first pending scan after Start (ini [Report] CrashSendDelayMs overrides)
    int testMode = 0;               // [Debug] CrashTest value (recorded as "test")
};

// Reads the ini, loads/creates the install id, starts the info + pending sender threads. Never blocks.
void Start(const StartInfo& info);

// Crash path (crashreport's writer thread, own stack). Builds + stores one record, then tries one send within sendBudgetMs
// (0 = store only). logRing = the EngineFix last-log-lines ring, oldest first. SEH-guarded: never throws or faults out.
void OnCrash(const char* reason, const _EXCEPTION_RECORD* er, const _CONTEXT* ctx, const char* const* logRing, int ringLines,
             unsigned sendBudgetMs);

// Test / harness hooks: send every pending record once now (returns records accepted), number pending, the scrubber, the
// pending folder, the install id.
int SendPendingNow();
int PendingCount();
void ScrubForTest(const char* in, char* out, unsigned cap);
const wchar_t* PendingDir();
const char* InstallId();

}  // namespace crashupload
