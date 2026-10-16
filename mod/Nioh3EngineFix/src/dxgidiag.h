#pragma once
#include "dxgidiag_core.h"
#include <windows.h>
#include <dxgi1_4.h>
namespace osd { struct StartupInfo; }
namespace dxgidiag {
void Start(const osd::StartupInfo& info);
bool Enabled();
bool Passthrough();
struct Call { Event event{}; bool trace=false; };
Call Begin(IDXGISwapChain* sc, Method method, UINT sync, UINT flags, uint32_t depth,
           const void* target, const void* caller, const DXGI_PRESENT_PARAMETERS* params=nullptr);
void Mark(Call& call, Stage stage, UINT sync, UINT flags);
void Result(Call& call, HRESULT hr, IDXGISwapChain* sc);
void End(Call& call);
void Note(Method method, Stage stage, const void* object=nullptr, const void* target=nullptr,
          HRESULT hr=S_OK, bool hasHr=false, uint64_t extra=0);
void QueueInfo(const void* chain, const void* queue, bool fallback);
}
