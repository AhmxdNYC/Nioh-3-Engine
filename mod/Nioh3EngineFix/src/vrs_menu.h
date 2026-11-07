#pragma once
// Shared by menu persistence and the offline preset regression test.
namespace vrsmenu {
inline bool Supported(bool queryOk, unsigned tier) { return queryOk && tier >= 1; }
// #79098 live tuning page. No Windows or D3D here: range, stepping, what each row can do this session, and persistence.
struct Values { int mode = 0, layouts = 7, rate = 0; bool skip = true, keep = true; };
enum Row { kRowMode, kRowLayouts, kRowRate, kRowSkip, kRowKeep, kRowReset, kRowN };
// Same out-of-range answers as gpuopt's ini read: an unknown mode is off, unknown layouts are all, unknown rate is 2x2.
inline Values Clamped(Values v) {
    if (v.mode < 0 || v.mode > 3) v.mode = 0;
    if (v.layouts < 0 || v.layouts > 7) v.layouts = 7;
    if (v.rate < 0 || v.rate > 2) v.rate = 0;
    return v;
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
        case kRowMode: v.mode = StepInt(v.mode, dir, 3); break;
        case kRowLayouts: v.layouts = StepInt(v.layouts, dir, 7); break;
        case kRowRate: v.rate = StepInt(v.rate, dir, 2); break;
        case kRowSkip: v.skip = !v.skip; break;
        case kRowKeep: v.keep = !v.keep; break;
        case kRowReset: if (dir == 0) v = Values{}; break;
        default: break;
    }
    return was.mode != v.mode || was.layouts != v.layouts || was.rate != v.rate || was.skip != v.skip || was.keep != v.keep;
}
enum Effect { kLive, kNextLaunch, kSelectiveOnly };
// bootMode / bootTracking are what the game started with: the draw hooks and the shader data cannot be added later.
inline Effect RowEffect(int row, const Values& v, int bootMode, bool bootTracking) {
    if (!bootMode) return kNextLaunch;
    if (row == kRowReset) return kLive;
    if (row == kRowMode) return v.mode == 3 && !bootTracking ? kNextLaunch : kLive;
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
