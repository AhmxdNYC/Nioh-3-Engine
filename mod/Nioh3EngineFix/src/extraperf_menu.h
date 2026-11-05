#pragma once
// Preserve effective startup behavior when the menu writes explicit levels and Preset=0.
namespace epmenu {
inline int HalfForSave(int selectedPreset, int configuredHalf, int originalPreset, bool enabled) {
    if (selectedPreset >= 0) return selectedPreset == 3 || selectedPreset == 4 ? 2 : 0;
    if (configuredHalf < 0) return enabled && (originalPreset == 3 || originalPreset == 4) ? 2 : 0;
    return configuredHalf > 2 ? 2 : configuredHalf;
}
}
