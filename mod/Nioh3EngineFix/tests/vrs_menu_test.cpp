#include "../src/vrs_menu.h"
#include <cassert>
#include <map>
#include <string>
#include <cstdio>
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
    assert(ini.size()==6);
    assert(ini[L"VrsCeiling"]==L"3" && ini[L"VrsLayouts"]==L"7" && ini[L"VrsRate"]==L"0");
    assert(ini[L"VrsKeepCharacters"]==L"1" && ini[L"VrsSkipSkinned"]==L"1" && ini[L"VrsGBufferMotion1x1"]==L"0");
    vrsmenu::Save(false,write);
    assert(ini[L"VrsCeiling"]==L"0");
    ini.clear(); vrsmenu::Save(false,write);
    assert(ini.size()==1 && ini[L"VrsCeiling"]==L"0");
    puts("VRS menu: capability failures, Tier1/2, exact six-key ON preset, ON->OFF and fresh OFF PASS");
}
