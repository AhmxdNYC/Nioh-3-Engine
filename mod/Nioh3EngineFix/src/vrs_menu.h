#pragma once
// Shared by menu persistence and the offline preset regression test.
namespace vrsmenu {
inline bool Supported(bool queryOk, unsigned tier) { return queryOk && tier >= 1; }
// #79098 live tuning page. No Windows or D3D here: range, stepping, what each row can do this session, and persistence.
struct Values { int mode = 0, layouts = 7, rate = 0; bool skip = true, keep = true; int wide = 0; };
enum Row { kRowLevel, kRowAdvanced, kRowMode, kRowLayouts, kRowRate, kRowSkip, kRowKeep, kRowWide, kRowReset, kRowN };
// #79382 the one setting most players need: higher = more fps, softer picture. All levels stay in Selective,
// because the whole-frame modes also coarsen menus and effects and sparkle.
// #79445 surface layouts and coarse rate always at their max (All, 2x2): the human saw no difference, so no level trades them away.
enum Level { kLevelCustom = -1, kLevelOff, kLevelMedium, kLevelHigh, kLevelTop = kLevelHigh };
// Same out-of-range answers as gpuopt's ini read: an unknown mode is off, unknown layouts are all, unknown rate is 2x2.
inline Values Clamped(Values v) {
    if (v.mode < 0 || v.mode > 3) v.mode = 0;
    if (v.layouts < 0 || v.layouts > 7) v.layouts = 7;
    if (v.rate < 0 || v.rate > 2) v.rate = 0;
    if (v.wide < 0 || v.wide > 255) v.wide = 0;  // #119891 VrsWideExclude mask
    return v;
}
inline Values LevelValues(int level) {
    Values v;  // Off, and the defaults every level starts from: all surfaces, 2x2, both protections on
    if (level >= kLevelMedium && level <= kLevelHigh) v.mode = 3;
    if (level == kLevelHigh) v.skip = v.keep = false;
    return v;
}
// Any mode 0 is Off whatever the other values are; anything else that is not exactly a level is Custom.
inline int LevelOf(const Values& v) {
    if (v.mode == 0) return kLevelOff;
    for (int level = kLevelMedium; level <= kLevelTop; ++level) {
        const Values l = LevelValues(level);
        if (v.mode == l.mode && v.layouts == l.layouts && v.rate == l.rate && v.skip == l.skip && v.keep == l.keep) return level;
    }
    return kLevelCustom;
}
inline const char* LevelName(int level) {
    return level == kLevelOff ? "Off" : level == kLevelMedium ? "Medium" : level == kLevelHigh ? "High" : "Custom";
}
inline int StepInt(int v, int dir, int top) {
    if (dir == 0) return v >= top ? 0 : v + 1;  // A / Enter wraps, left / right stop at the ends
    const int next = v + (dir < 0 ? -1 : 1);
    return next < 0 ? 0 : next > top ? top : next;
}
// true = the value changed. The Reset row only answers A / Enter, so a stray left / right cannot wipe a tune.
inline bool Step(Values& v, int row, int dir) {
    const Values was = v;
    switch (row) {
        case kRowLevel: {
            const int level = LevelOf(v);  // Custom has no place on the scale: any press lands on Medium
            v = LevelValues(level == kLevelCustom ? kLevelMedium : StepInt(level, dir, kLevelTop));
            if (level == kLevelOff && LevelOf(v) == kLevelOff) v = was;  // already at the end: keep what Off is holding
            break;
        }
        case kRowMode: v.mode = StepInt(v.mode, dir, 3); break;
        case kRowLayouts: v.layouts = StepInt(v.layouts, dir, 7); break;
        case kRowRate: v.rate = StepInt(v.rate, dir, 2); break;
        case kRowSkip: v.skip = !v.skip; break;
        case kRowKeep: v.keep = !v.keep; break;
        case kRowWide:  // Off (0) or the tested glitter mask (blend + depth-off = 6). A custom mask steps onto one of those.
            if (v.wide != 0 && v.wide != 6) v.wide = dir < 0 ? 0 : 6;
            else if (dir < 0) v.wide = 0;
            else if (dir > 0) v.wide = 6;
            else v.wide = v.wide == 6 ? 0 : 6;
            break;
        case kRowReset: if (dir == 0) v = Values{}; break;
        default: break;
    }
    return was.mode != v.mode || was.layouts != v.layouts || was.rate != v.rate || was.skip != v.skip || was.keep != v.keep || was.wide != v.wide;
}
enum Effect { kLive, kNextLaunch, kSelectiveOnly };
// bootMode / bootTracking are what the game started with: the draw hooks and the shader data cannot be added later.
inline Effect RowEffect(int row, const Values& v, int bootMode, bool bootTracking) {
    if (!bootMode) return kNextLaunch;
    if (row == kRowReset || row == kRowAdvanced) return kLive;
    // The mask is applied on the next draw only when this boot already captured draw classes and shading is whole-frame.
    if (row == kRowWide) return bootTracking && (v.mode == 1 || v.mode == 2) ? kLive : kNextLaunch;
    if (row == kRowMode || row == kRowLevel) return v.mode == 3 && !bootTracking ? kNextLaunch : kLive;
    if (!bootTracking) return kNextLaunch;
    return v.mode == 3 ? kLive : kSelectiveOnly;
}
// The mode the draw hooks may use now. Selective without shader data would quietly draw everything at full rate.
inline int LiveMode(int wanted, int current, int bootMode, bool bootTracking) {
    if (!bootMode) return 0;
    return wanted == 3 && !bootTracking ? current : wanted;
}
inline const char* ModeName(int mode) {
    return mode == 1 ? "Whole frame 2x2" : mode == 2 ? "Whole frame 4x4" : mode == 3 ? "Selective" : "Off";
}
inline const char* RateName(int rate) { return rate == 1 ? "1x2" : rate == 2 ? "2x1" : "2x2"; }
inline const char* WideName(int wide) { return wide == 6 ? "On" : wide == 0 ? "Off" : "Custom"; }
inline const char* LayoutsName(int layouts) {
    static const char* const k[8] = {"None", "6 targets", "7 targets", "6 + 7", "8 targets", "6 + 8", "7 + 8", "All"};
    return layouts >= 0 && layouts <= 7 ? k[layouts] : k[7];
}
// The enable key goes last, as in the preset below.
template<class Write> void SaveValues(const Values& raw, Write write) {
    static const wchar_t* const digit[8] = {L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7"};
    const Values v = Clamped(raw);
    write(L"Perf", L"VrsLayouts", digit[v.layouts]);
    write(L"Perf", L"VrsRate", digit[v.rate]);
    write(L"Perf", L"VrsKeepCharacters", v.keep ? L"1" : L"0");
    write(L"Perf", L"VrsSkipSkinned", v.skip ? L"1" : L"0");
    wchar_t wide[4];
    const int w = v.wide;
    auto place = [](int n) { return static_cast<wchar_t>(L'0' + n); };
    if (w >= 100) { wide[0] = place(w / 100); wide[1] = place((w / 10) % 10); wide[2] = place(w % 10); wide[3] = 0; }
    else if (w >= 10) { wide[0] = place(w / 10); wide[1] = place(w % 10); wide[2] = 0; }
    else { wide[0] = place(w); wide[1] = 0; }
    write(L"Perf", L"VrsWideExclude", wide);  // before the enable key, same order as the other values
    write(L"Perf", L"VrsCeiling", digit[v.mode]);
}
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
