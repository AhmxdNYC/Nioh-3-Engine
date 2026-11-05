#pragma once
// Shared by menu persistence and the offline preset regression test.
namespace vrsmenu {
inline bool Supported(bool queryOk, unsigned tier) { return queryOk && tier >= 1; }
template<class Write> void Save(bool on, Write write) {
    // Write the enable key after the preset parameters.
    if (on) {
        write(L"Perf", L"VrsLayouts", L"7");
        write(L"Perf", L"VrsRate", L"0");
        write(L"Perf", L"VrsKeepCharacters", L"1");
        write(L"Perf", L"VrsSkipSkinned", L"1");
        write(L"Perf", L"VrsGBufferMotion1x1", L"0");
    }
    write(L"Perf", L"VrsCeiling", on ? L"3" : L"0");
}
}
