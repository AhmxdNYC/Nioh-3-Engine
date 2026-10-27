#pragma once
#include <cstddef>
#include "updatecheck_core.h"
namespace updatecheck {
// Tick is called by the existing stats worker; it schedules at most one background request.
void Tick(const char* installedVersion);
// Menu reads a small immutable snapshot; no network, allocations or waits on HTTP here.
bool CopyNotice(char* dst, size_t capacity);
}
