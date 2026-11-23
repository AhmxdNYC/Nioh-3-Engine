#include "../src/vrs_menu.h"
#include <cassert>
#include <map>
#include <string>
#include <cstdio>
#include <cstdlib>
int main() {
    assert(!vrsmenu::Supported(false, 2));
    assert(!vrsmenu::Supported(true, 0));
    assert(vrsmenu::Supported(true, 1));
    assert(vrsmenu::Supported(true, 2));
    std::map<std::wstring,std::wstring> ini;
    auto write=[&](const wchar_t* section,const wchar_t* key,const wchar_t* value) {
        assert(std::wstring(section)==L"Perf"); ini[key]=value;
    };
    vrsmenu::Save(true,write);
    assert(ini.size()==2);  // #121288 On = whole-frame 2x2 + glitter guard mask 6
    assert(ini[L"VrsCeiling"]==L"1" && ini[L"VrsWideExclude"]==L"6");
    vrsmenu::Save(false,write);
    assert(ini[L"VrsCeiling"]==L"0");
    ini.clear(); vrsmenu::Save(false,write);
    assert(ini.size()==1 && ini[L"VrsCeiling"]==L"0");
    using namespace vrsmenu;
    // Range: junk from the ini reads the same way gpuopt reads it.
    Values junk; junk.mode = 9; junk.layouts = 8; junk.rate = -1;
    junk = Clamped(junk);
    assert(junk.mode == 0 && junk.layouts == 7 && junk.rate == 0);
    junk.mode = -1; junk.layouts = -1; junk.rate = 3;
    junk = Clamped(junk);
    assert(junk.mode == 0 && junk.layouts == 7 && junk.rate == 0);
    // Stepping stays in range from every value, in every direction.
    for (int row : {kRowMode, kRowLayouts, kRowRate}) {
        const int top = row == kRowMode ? 3 : row == kRowLayouts ? 7 : 2;
        for (int start = 0; start <= top; ++start) for (int dir : {-1, 0, 1}) {
            Values v; (row == kRowMode ? v.mode : row == kRowLayouts ? v.layouts : v.rate) = start;
            Step(v, row, dir);
            const int got = row == kRowMode ? v.mode : row == kRowLayouts ? v.layouts : v.rate;
            assert(got >= 0 && got <= top);
            if (dir < 0) assert(got == (start ? start - 1 : 0));
            if (dir > 0) assert(got == (start < top ? start + 1 : top));
            if (dir == 0) assert(got == (start < top ? start + 1 : 0));
        }
    }
    {
        Values v; assert(Step(v, kRowSkip, 1) && !v.skip); assert(Step(v, kRowKeep, -1) && !v.keep);
        v.mode = 3; v.rate = 2; v.layouts = 1;
        assert(!Step(v, kRowReset, 1) && !Step(v, kRowReset, -1) && v.mode == 3);  // left / right cannot wipe a tune
        assert(Step(v, kRowReset, 0));
        assert(v.mode == 0 && v.layouts == 7 && v.rate == 0 && v.skip && v.keep && v.wide == 0);
        assert(!Step(v, kRowReset, 0));  // already default: nothing to save
        assert(!Step(v, kRowN, 0));
    }
    // Persist: read back what was written, enable key last, and junk never reaches the file.
    {
        std::map<std::wstring,std::wstring> f; std::wstring last;
        auto w = [&](const wchar_t* section, const wchar_t* key, const wchar_t* value) {
            assert(std::wstring(section) == L"Perf"); f[key] = value; last = key;
        };
        Values v; v.mode = 3; v.layouts = 5; v.rate = 2; v.skip = false; v.keep = false;
        SaveValues(v, w);
        assert(f.size() == 6 && last == L"VrsCeiling" && f[L"VrsWideExclude"] == L"0");
        Values back;
        back.mode = _wtoi(f[L"VrsCeiling"].c_str()); back.layouts = _wtoi(f[L"VrsLayouts"].c_str()); back.rate = _wtoi(f[L"VrsRate"].c_str());
        back.skip = _wtoi(f[L"VrsSkipSkinned"].c_str()) != 0; back.keep = _wtoi(f[L"VrsKeepCharacters"].c_str()) != 0;
        back = Clamped(back);
        assert(back.mode == 3 && back.layouts == 5 && back.rate == 2 && !back.skip && !back.keep);
        Values bad; bad.mode = 42; bad.layouts = 42; bad.rate = 42;
        SaveValues(bad, w);
        assert(f[L"VrsCeiling"] == L"0" && f[L"VrsLayouts"] == L"7" && f[L"VrsRate"] == L"0" && f[L"VrsWideExclude"] == L"0");
        Values masked; masked.wide = 6; masked.mode = 1;
        SaveValues(masked, w);
        assert(f[L"VrsWideExclude"] == L"6" && f[L"VrsCeiling"] == L"1");
        Values customMask; customMask.wide = 128;
        SaveValues(customMask, w);
        assert(f[L"VrsWideExclude"] == L"128");
        Values junkMask; junkMask.wide = 999;
        SaveValues(junkMask, w);
        assert(f[L"VrsWideExclude"] == L"0");
    }
    // What each row can do this session.
    {
        Values on; on.mode = 3; Values off;
        for (int row = 0; row < kRowN; ++row) assert(RowEffect(row, on, 0, false) == kNextLaunch);  // started off: nothing is live
        for (int row = 0; row < kRowN; ++row) assert(RowEffect(row, on, 3, true) == (row == kRowWide ? kNextLaunch : kLive));
        Values whole; whole.mode = 1; whole.wide = 6;
        assert(RowEffect(kRowWide, whole, 1, true) == kLive && RowEffect(kRowWide, whole, 1, false) == kNextLaunch);
        assert(WideName(0) == std::string("Off") && WideName(6) == std::string("On") && WideName(4) == std::string("Custom"));
        Values guard;
        assert(Step(guard, kRowWide, -1) == false && guard.wide == 0);
        assert(Step(guard, kRowWide, 1) && guard.wide == 6);
        assert(Step(guard, kRowWide, 1) == false && guard.wide == 6);
        assert(Step(guard, kRowWide, 0) && guard.wide == 0);
        guard.wide = 128; assert(Step(guard, kRowWide, -1) && guard.wide == 0);
        guard.wide = 128; assert(Step(guard, kRowWide, 0) && guard.wide == 6);
        assert(RowEffect(kRowRate, off, 3, true) == kSelectiveOnly && RowEffect(kRowMode, off, 3, true) == kLive);
        assert(RowEffect(kRowMode, on, 1, false) == kNextLaunch && RowEffect(kRowMode, off, 1, false) == kLive);
        assert(RowEffect(kRowRate, on, 2, false) == kNextLaunch && RowEffect(kRowReset, off, 1, false) == kLive);
        assert(LiveMode(3, 0, 0, false) == 0 && LiveMode(1, 0, 0, false) == 0);  // started off: never on
        assert(LiveMode(3, 1, 1, false) == 1 && LiveMode(0, 1, 1, false) == 0 && LiveMode(2, 1, 1, false) == 2);
        assert(LiveMode(3, 0, 3, true) == 3 && LiveMode(1, 3, 3, true) == 1 && LiveMode(0, 3, 3, true) == 0);
    }
    // #79382 the level row: one scale, in order, never a whole-frame mode, and it reads back from what was saved.
    {
        assert(LevelOf(Values{}) == kLevelOff && LevelOf(LevelValues(kLevelCustom)) == kLevelOff);
        for (int level = kLevelOff; level <= kLevelTop; ++level) {
            const Values v = LevelValues(level);
            assert(LevelOf(v) == level);
            assert(v.mode == (level == kLevelOff ? 0 : 3));
            assert(RowEffect(kRowLevel, v, 3, true) == kLive && RowEffect(kRowLevel, v, 0, false) == kNextLaunch);
        }
        Values def; def.mode = 3;  // the old ON row's preset is Medium
        assert(LevelOf(def) == kLevelMedium);
        assert(LevelValues(kLevelMedium).rate == 0 && LevelValues(kLevelHigh).rate == 0 && LevelValues(kLevelMedium).layouts == 7 && LevelValues(kLevelHigh).layouts == 7);  // #79445 always max
        Values half; half.mode = 3; half.rate = 1; assert(LevelOf(half) == kLevelCustom);  // an old Low tune reads as Custom
        assert(!LevelValues(kLevelHigh).keep && !LevelValues(kLevelHigh).skip && LevelValues(kLevelMedium).keep && LevelValues(kLevelMedium).skip);
        Values v;
        assert(Step(v, kRowLevel, 1) && LevelOf(v) == kLevelMedium);
        assert(Step(v, kRowLevel, 1) && LevelOf(v) == kLevelHigh);
        assert(!Step(v, kRowLevel, 1) && LevelOf(v) == kLevelHigh);    // right stops at High
        assert(Step(v, kRowLevel, 0) && LevelOf(v) == kLevelOff);      // A wraps
        assert(!Step(v, kRowLevel, -1) && LevelOf(v) == kLevelOff);    // left stops at Off
        Values custom; custom.mode = 3; custom.layouts = 5;
        assert(LevelOf(custom) == kLevelCustom && Step(custom, kRowLevel, -1) && LevelOf(custom) == kLevelMedium);
        Values whole; whole.mode = 1;
        assert(LevelOf(whole) == kLevelCustom);
        Values held; held.layouts = 5;  // Off holding custom values: a press at the end leaves them alone
        assert(LevelOf(held) == kLevelOff && !Step(held, kRowLevel, -1) && held.layouts == 5);
        Values adv; assert(!Step(adv, kRowAdvanced, 0) && !Step(adv, kRowAdvanced, 1));  // showing rows is not an edit
        assert(std::string(LevelName(kLevelCustom)) == "Custom" && std::string(LevelName(kLevelHigh)) == "High");
    }
    assert(std::string(ModeName(0)) == "Off" && std::string(ModeName(3)) == "Selective" && std::string(RateName(2)) == "2x1" && std::string(LayoutsName(7)) == "All" && std::string(LayoutsName(0)) == "None");
    puts("VRS menu: capability failures, Tier1/2, exact two-key ON preset (2x2 + glitter guard), ON->OFF and fresh OFF, value range, stepping, persist, row effect PASS");
}
