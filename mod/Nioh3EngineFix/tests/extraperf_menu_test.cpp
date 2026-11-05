#include "../src/extraperf_menu.h"
#include "../src/vrs_menu.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
int main() {
    // Merely saving must preserve inherited and explicit startup settings for all old presets.
    for (int preset = 0; preset < 6; ++preset) {
        for (int enabled = 0; enabled < 2; ++enabled) {
            assert(epmenu::HalfForSave(-1, -1, preset, enabled != 0) ==
                   (enabled && (preset == 3 || preset == 4) ? 2 : 0));
            for (int half = 0; half <= 2; ++half)
                assert(epmenu::HalfForSave(-1, half, preset, enabled != 0) == half);
        }
        // Explicit picks enable Extreme/Risky half-rate and remove it on departure.
        assert(epmenu::HalfForSave(preset, 2, 4, true) == (preset == 3 || preset == 4 ? 2 : 0));
    }
    std::map<std::wstring, std::wstring> ini;
    ini[L"ExtraPerf.XRES"] = L"3";
    ini[L"ExtraPerf.XHALF"] = L"2";
    auto write = [&](const wchar_t* section, const wchar_t* key, const wchar_t* value) {
        ini[std::wstring(section) + L"." + key] = value;
    };
    for (bool vrs : {true, false}) {
        vrsmenu::Save(vrs, write);
        assert(ini[L"ExtraPerf.XRES"] == L"3" && ini[L"ExtraPerf.XHALF"] == L"2");
        assert(ini[L"Perf.VrsCeiling"] == (vrs ? L"3" : L"0"));
    }
    puts("Extra performance menu: existing defaults, explicit preset switches, VRS isolation PASS");
}
