// gamerva.h - every fixed game address the modules use goes through Grva(rva) (tools/check_fixed_rvas.py enforces it).
// rva is the v2.02 RVA. Known build (TimeDateStamp + SizeOfImage match): returns rva unchanged - zero cost, no behaviour
// change. Another build: the v2.02 RVA's byte pattern (rvas.json -> src/rvas_gen.h, made by tools/gen_rvas.py) is looked
// up instead and the RVA on THIS build is returned, 0 if the pattern is missing / not unique (callers then read nothing).
// Header-only state so the tests (which never call gamerva::Init) see the identity mapping.
#pragma once
#include <cstdint>

namespace gamerva {
inline bool g_known = true;                             // set by gamerva::Init (dllmain) from the PE header
inline uint32_t (*g_resolve)(uint32_t rva) = nullptr;   // unknown-build lookup (gamerva.cpp)
inline uint32_t Grva(uint32_t rva) { return g_known || !g_resolve ? rva : g_resolve(rva); }
#ifdef _WIN32
// dllmain: Init right after reading the PE header; Verify once .text is decrypted (logs "game RVAs: ..." and, on an
// unknown build, resolves every entry so later lookups are a table read).
void Init(uintptr_t base, bool known, void (*log)(const char*, ...));
int Verify();  // number of entries NOT found at their expected place (known) / not resolved (unknown)
#endif
}  // namespace gamerva
using gamerva::Grva;
