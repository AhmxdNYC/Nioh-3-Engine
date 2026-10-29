// savelib.h - native save-library operations (no Python). Ports tools/saves/{saveslot,bootstrap,live_title,real_root}.py and
// re/scripts_re/savecrypt.py to plain Win32 file APIs (CreateFileW / FindFirstFileW / MoveFileW / BCrypt), which Wine/Proton
// implement, so the Saves page works on Linux without an external interpreter. Same on-disk library format as the Python
// helpers (sandbox.json, slots/<name>/{manifest.json,data}, transactions/, request-<token>/receipt.json), so libraries made by
// either side stay usable.
//
// Every operation runs on a worker thread started by savefiles.cpp; the thread handle replaces the old helper process handle
// (exit code 0 = success). Gate: the worker never reads game memory; it asks GateFn (savefiles' fresh idle-title verdict, set by
// the present-thread Poll) before and after every mutation, like the Python helper's ReadProcessMemory gate.
#pragma once
#include <cstdint>
#include <string>

namespace savelib {

using GateFn = bool (*)();

// Results / errors of the last operation (thread-safe to read after the worker exited).
const char* LastResult();

// Starts a worker thread; returns its handle (nullptr: not started, LastResult says why). Exit code 0 = ok, 1 = refused.
void* StartBootstrap(const std::wstring& sandbox, const std::wstring& saveRoot);
void* StartLiveImport(const std::wstring& sandbox, const std::wstring& saveRoot, const std::string& slot, const std::wstring& token,
                      bool makeNewest, GateFn gate);
// Snapshot of the whole save root into a new immutable slot (real-root "backup"), at the idle title.
void* StartBackup(const std::wstring& sandbox, const std::wstring& saveRoot, const std::string& slot, const std::wstring& token, GateFn gate);
// Cancel the running operation (also honoured: the cancel-<token> marker file).
void RequestCancel();

// ---- exposed for the offline harness ----
bool ValidateSave(const std::string& blob, const std::string& account, uint64_t* savedAt, std::string* error);
uint64_t SavedAt(const std::string& blob);
std::string Restamp(const std::string& blob, uint64_t floor);
std::string DecryptForTest(const std::string& blob);
uint32_t ChecksumForTest(const std::string& plain);
// Synchronous variants (the worker bodies). Return true on success; LastResult() holds the json/error text.
bool RunBootstrap(const std::wstring& sandbox, const std::wstring& saveRoot);
bool RunLiveImport(const std::wstring& sandbox, const std::wstring& saveRoot, const std::string& slot, const std::wstring& token,
                   bool makeNewest, GateFn gate);
bool RunBackup(const std::wstring& sandbox, const std::wstring& saveRoot, const std::string& slot, const std::wstring& token, GateFn gate);
bool VerifySlot(const std::wstring& sandbox, const std::string& slot);
// Test hook: gate settle time (ms), default 1000.
void SetSettleMs(unsigned ms);

}  // namespace savelib
