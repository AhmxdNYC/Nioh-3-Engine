// crashblame.h - "who crashed" for the crash report and the crash upload (human #94391/#94396).
// Names the owner of the crash address and of every stack frame: our mods (Nioh3*.asi), the game exe, Windows,
// a device driver (DriverStore), a proxy DLL in the game folder (dxgi.dll / dinput8.dll / version.dll ... = a mod
// loader or injector such as ReShade, OptiScaler, an ASI loader), any other DLL in the game folder, or another
// program's DLL injected from outside (overlays, recorders). Code that was unloaded before the crash is named from
// the Windows unload trace. Nothing is hardcoded per mod: classes come from paths, file names and version resources.
// Crash-path safe enough for the report helper thread (no DXGI, no locks of ours); every memory read is guarded.
#pragma once
#include <windows.h>
#include <cstddef>

namespace crashblame {

enum Kind {
    kOurs = 0,        // Nioh3*.asi
    kGame,            // the process exe
    kWindows,         // under the Windows directory, not DriverStore
    kDriver,          // Windows\System32\DriverStore (GPU / device driver user-mode part)
    kProxy,           // game-folder DLL named like a Windows system DLL (mod loader / injector)
    kGameFolder,      // other DLL or .asi in the game folder (shipped with the game or added by a mod)
    kOtherProgram,    // DLL from outside the game folder and Windows (overlay, recorder, tool)
    kUnloaded,        // address inside a module that was unloaded before the crash
    kNoModule,        // no module (dynamic code: hook trampoline, JIT) or a garbage address
};

struct Owner {
    Kind kind = kNoModule;
    char name[64] = {};      // file name (no path)
    char company[48] = {};   // version resource CompanyName, when present
    unsigned long long off = 0;
};

const char* KindText(Kind k);

// Owner of an address. Uses the version resource (allocates) only when withCompany.
void Classify(const void* addr, Owner& out, bool withCompany = true);

// Full analysis. text: CRLF lines for the .txt report (section body, no header). shortLines: up to maxShort lines of at
// most 118 chars for the upload ("last" entries). Either output may be null. Returns the number of short lines.
int Analyze(const EXCEPTION_RECORD* er, const CONTEXT* ctx, char* text, size_t textCap, char (*shortLines)[120], int maxShort);

}  // namespace crashblame
