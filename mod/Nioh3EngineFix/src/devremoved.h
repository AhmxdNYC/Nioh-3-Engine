// devremoved.h - GPU removal reason for crash records (#96825; port of the 1.7.61 devlost.cpp idea, reason line only).
// The game's "Fatal error! DXGI_ERROR_DEVICE_REMOVED" box (0xE0000001) says only that ITS check saw 0x887A0005; the real
// cause is what GetDeviceRemovedReason returns (TDR vs access error). The overlay already holds the game's device, so no
// new hook: it hands the pointer over, and the crash report asks it once, at fatal time, off every hot path.
// DRED (auto-breadcrumbs + page-fault data) costs GPU time: only with [Debug] Dred = 1 (default 0), set before the device exists.
#pragma once
#include <cstddef>

struct ID3D12Device;

namespace devremoved {

// osd.cpp: the device of the presenting queue (borrowed; Forget before the overlay releases it).
void NoteDevice(ID3D12Device* dev);
void Forget(ID3D12Device* dev);

// [Debug] Dred = 1: enable DRED settings (call before the game creates its device; native Windows only).
void Start(const wchar_t* iniPath, bool wine);

// Name of a removal HRESULT ("DXGI_ERROR_DEVICE_REMOVED"), "?" when unknown. Pure.
const char* ReasonName(long hr);

// Crash path. Writes "GetDeviceRemovedReason = 887A0005 DXGI_ERROR_DEVICE_REMOVED" (or why there is none) into out.
// SEH-guarded, no allocation, no locks. With Dred on, appends the not-completed breadcrumb count and page-fault VA.
void Describe(char* out, size_t cap);

// Same, for an explicit device (offline test with a fake device).
void DescribeDevice(void* dev /* ID3D12Device* */, char* out, size_t cap);

}  // namespace devremoved
