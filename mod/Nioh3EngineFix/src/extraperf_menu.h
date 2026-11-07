#pragma once
// Preserve effective startup behavior when the menu writes explicit levels and Preset=0.
namespace epmenu {
inline int HalfForSave(int selectedPreset, int configuredHalf, int originalPreset, bool enabled) {
    // #80829: no preset turns XHALF on (flicker). A picked preset clears it; otherwise an explicit value is kept
    // (it only runs with XHALF_Force = 1, skipfx::HalfLevel).
    (void)originalPreset; (void)enabled;
    if (selectedPreset >= 0 || configuredHalf < 0) return 0;
    return configuredHalf > 2 ? 2 : configuredHalf;
}
}
