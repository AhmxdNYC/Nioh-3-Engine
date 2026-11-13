// vtdisk.h - read a vtable slot's ORIGINAL value from the module file on disk (GitHub #38).
//
// Another overlay (framepacer.dll in #38) can hook the DXGI swapchain vtable before us and then re-hook after us, so its
// saved "original" is our hook while ours is its hook: Present loops between the two until the stack overflows.
// The loop breaker in osd.cpp needs the real DXGI function. A vtable lives in the module's read-only data with an
// absolute pointer that the loader relocates, so: real = (value in the file) - (file's preferred ImageBase) + (load base).
// The result is only used when it lands inside an executable section of that same module.
#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace vtdisk {

// Returns the original function pointer for the vtable slot at `slot`, or nullptr when it cannot be proven.
inline void* OriginalSlotValue(void* const* slot) {
    HMODULE mod = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(slot), &mod) || !mod)
        return nullptr;
    const uintptr_t base = reinterpret_cast<uintptr_t>(mod);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return nullptr;
    const uintptr_t imageSize = nt->OptionalHeader.SizeOfImage;
    const uintptr_t rva = reinterpret_cast<uintptr_t>(slot) - base;
    if (rva + sizeof(uint64_t) > imageSize) return nullptr;

    wchar_t path[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameW(mod, path, MAX_PATH);
    if (!n || n >= MAX_PATH) return nullptr;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return nullptr;
    void* result = nullptr;
    do {
        auto readAt = [&](uint64_t off, void* dst, DWORD len) {
            LARGE_INTEGER li; li.QuadPart = (LONGLONG)off;
            DWORD got = 0;
            return SetFilePointerEx(f, li, nullptr, FILE_BEGIN) && ReadFile(f, dst, len, &got, nullptr) && got == len;
        };
        IMAGE_DOS_HEADER fdos{};
        if (!readAt(0, &fdos, sizeof fdos) || fdos.e_magic != IMAGE_DOS_SIGNATURE) break;
        IMAGE_NT_HEADERS64 fnt{};
        if (!readAt((uint64_t)fdos.e_lfanew, &fnt, sizeof fnt) || fnt.Signature != IMAGE_NT_SIGNATURE ||
            fnt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            break;
        // same build as the loaded image (a file replaced after load would give a wrong value)
        if (fnt.FileHeader.TimeDateStamp != nt->FileHeader.TimeDateStamp || fnt.OptionalHeader.SizeOfImage != imageSize) break;
        const WORD nsec = fnt.FileHeader.NumberOfSections;
        if (!nsec || nsec > 96) break;
        IMAGE_SECTION_HEADER secs[96];
        const uint64_t secOff = (uint64_t)fdos.e_lfanew + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + fnt.FileHeader.SizeOfOptionalHeader;
        if (!readAt(secOff, secs, (DWORD)(nsec * sizeof(IMAGE_SECTION_HEADER)))) break;
        uint64_t fileOff = 0;
        bool mapped = false;
        for (WORD i = 0; i < nsec; ++i) {
            const auto& s = secs[i];
            if (rva >= s.VirtualAddress && rva + 8 <= (uintptr_t)s.VirtualAddress + s.SizeOfRawData) {
                fileOff = (uint64_t)s.PointerToRawData + (rva - s.VirtualAddress);
                mapped = true;
                break;
            }
        }
        if (!mapped) break;
        uint64_t onDisk = 0;
        if (!readAt(fileOff, &onDisk, sizeof onDisk)) break;
        const uint64_t pref = fnt.OptionalHeader.ImageBase;
        if (onDisk < pref || onDisk - pref >= imageSize) break;
        const uintptr_t fnRva = (uintptr_t)(onDisk - pref);
        for (WORD i = 0; i < nsec; ++i) {
            const auto& s = secs[i];
            const uintptr_t vs = s.Misc.VirtualSize ? s.Misc.VirtualSize : s.SizeOfRawData;
            if ((s.Characteristics & IMAGE_SCN_MEM_EXECUTE) && fnRva >= s.VirtualAddress && fnRva < (uintptr_t)s.VirtualAddress + vs) {
                result = reinterpret_cast<void*>(base + fnRva);
                break;
            }
        }
    } while (false);
    CloseHandle(f);
    return result;
}

}  // namespace vtdisk
