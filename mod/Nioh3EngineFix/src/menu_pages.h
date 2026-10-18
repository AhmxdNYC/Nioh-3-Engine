// Included after menu.cpp's stable Id enum. Presentation only: existing row IDs,
// kOrder, ini keys and Remote() continue to describe exactly the same settings.
#pragma once
#include "../../menu_navigation.h"

enum MenuPage { pRoot, pSettings, pDisplay, pFrameGen, pGameplay, pSafe, pExtra,
                pExperimental, pCpu, pGpu, pScheduling, pProton, pDiagnostics, pBoot, pAll,
                pConfig, pCfgFirst, pCount = pCfgFirst + kConfigGroupN };
constexpr int kPageRow = kCount;
constexpr int kBackRow = kPageRow + pCount;
constexpr int kSettingRow = kBackRow + 1;
constexpr int kRematchRow = kSettingRow + kConfigN;
constexpr int kPracticeRow = kRematchRow + 1;  // user #13223: first page = Settings / Boss Rematch / Practice
constexpr int kMenuRowCap = kCount + pCount + kConfigN + 3;
const char* const kPageNames[] = {"Mod menu", "Settings", "Display & overlay", "Frame generation", "Gameplay",
    "Performance fixes", "Extra performance (changes the look)", "Experimental settings", "CPU experiments",
    "GPU experiments", "CPU scheduling", "GPU / Proton experiments", "Diagnostics & logging", "Next launch", "Original controls (flat order)", "Configuration by ini section"};
inline const char* PageName(int page) {
    return page >= pCfgFirst && page < pCount ? kConfigGroups[page - pCfgFirst] : kPageNames[page];
}
inline int SettingsPage(int id) {
    if (id >= kEpPre && id <= kEp9) return pExtra;
    if (id == kXTS || id == kXWS) return pDiagnostics;
    if (id == kXRTP || id == kXPCM) return pScheduling;
    if (id == kXAS || id == kXPJH || (id >= kXVSMA && id <= kXCLDR)) return pGpu;
    if (id >= kXW6 && id <= kXPCM) return pCpu;
    if (id >= kGR2a && id <= kGR1) return pProton;
    if ((id >= kVrr && id <= kLat) || id == kGpu) return pSafe;
    if (id >= kWorkerCap && id <= kFgWine) return pBoot;
    if (id == kFg || id == kFgState || id == kUpscaler) return pFrameGen;
    if (id == kAutoCont) return pGameplay;
    return pDisplay;
}
inline bool PageContains(int page, int id) {
    if (page == pAll) return true;
    if (id == kClose) return page == pRoot;
    if (id == kSave) return page == pBoot;  // user #13223: no Save row on the first page (closing the menu auto-saves); manual Save stays under Next launch
    return SettingsPage(id) == page;
}
