#pragma once
#include <cstddef>
#include "updatecheck_core.h"
namespace updatecheck {
// Call once on the stats worker before its first Tick; configuration is immutable thereafter.
void Configure(const wchar_t* iniPath);
// Tick is called by the existing stats worker; it schedules at most one background request.
void Tick(const char* installedVersion);
// Menu reads a small immutable snapshot; no network, allocations or waits on HTTP here.
void OpenPage(); // fixed website + module-folder updater selection, asynchronous shell launches
bool RunningUnderWine();
bool CopyFolderNotice(char* dst, size_t capacity, bool focused);
bool CopyNotice(char* dst, size_t capacity);
}
