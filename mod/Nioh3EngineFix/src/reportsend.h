// reportsend.h - player report from the menu Report box (bridge #64882, human #64860 / #64870; design notes/auto-report.md).
//
// The menu's Report row opens a text box (menu_reportbox.h). Send hands the typed note to this module, which on its own thread:
//   1. writes the existing report text (crashreport::Snapshot -> Nioh3EngineFix-report-latest.txt) and reads it back,
//   2. adds the recent event log: the tails of Nioh3EngineFix.log / Nioh3BossRematch.log / Nioh3Training.log /
//      Nioh3CamKiProbe.log and Nioh3ModEvents.jsonl (when present), plus the last menu navigation edges (no typed characters),
//   3. scrubs every string (user name, computer name, user profile and any \Users\<name>, /home/<name>, /Users/<name> path part
//      become <user> / <pc>; 17-digit Steam ids become <steamid>),
//   4. keeps a local copy in Nioh3Reports\report-<date>-<time>.json beside the .asi (every report, sent or not),
//   5. POSTs it as JSON to [Report] ReportUrl (default <[Report] Url>/api/report) with WinHTTP (works under Wine/Proton).
// The game thread never waits. Body cap 256 KB (the snapshot and the log tails shrink first).
//
// Ini [Report]: ReportUrl = (empty = Url + /api/report), ReportBypass = (optional Vercel protection-bypass token, preview tests
// only; falls back to CrashBypass), InstallId = (optional; else Nioh3CrashReports\install-id.txt, shared with crashupload),
// SteamKeyboard = 1 (controller: try Steam's gamepad text input first; 0 = always the built-in key grid).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace reportsend {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    void* selfModule = nullptr;   // HMODULE of this .asi (folder of the logs and of Nioh3Reports\)
    const char* modVersion = "";
};
void Start(const StartInfo& info);  // reads the ini and the install id; never blocks, starts nothing

enum class State : int { Idle = 0, Sending = 1, Sent = 2, Failed = 3 };
// Starts one send on a background thread. via = "keyboard" / "pad" / "steam". false when a send is already running or the
// text is empty (msg says why). The text is copied.
bool Send(const char* playerText, const char* via, char* msg, size_t cap);
State Status(char* msg, size_t cap);  // last result line for the box ("Sent ...", "Not sent: ...")
void ClearStatus();
const char* Endpoint();

// Menu navigation edge (dev "pad" / "key", what "up" / "ok" / ...). Ring of 400, copied into a report. Never typed text.
void NoteInput(const char* dev, const char* what);

// Steam gamepad text input (steam_api64.dll is already loaded by the game; looked up by export name, no vtable guess).
// SteamShow returns true when Steam accepted the request (its keyboard is up). SteamPoll: 0 still open, 1 submitted (text copied
// to out), 2 dismissed / failed. SteamEnabled: [Report] SteamKeyboard and the exports exist.
bool SteamEnabled();
bool SteamShow(const char* existing, unsigned maxChars);
int SteamPoll(char* out, size_t cap);

// ---- pure helpers (tests) ----
std::string Scrub(const std::string& s);
void SetScrubNamesForTest(const char* user, const char* pc, const char* profile);
std::string JsonString(const std::string& s);  // quoted, escaped, ASCII only
std::string BuildBody(const std::string& id, const std::string& ver, const std::string& via, const std::string& text,
                      const std::string& snapshot, const std::string& eventsJson, int eventCount, const std::string& inputJson,
                      int inputCount, size_t cap);

}  // namespace reportsend
