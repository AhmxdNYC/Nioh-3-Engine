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
constexpr int kPracticeRow = kRematchRow + 1;  // user #13223: first page = Settings / Boss Matches (#13293 rename) / Practice
constexpr int kFrameMeterRow = kPracticeRow + 1;  // #34265: Gameplay / Overlays toggle; Practice menu keeps its own row
constexpr int kKiWindowRow = kFrameMeterRow + 1;  // #34558: dark-blue Ki window on the player frame meter
constexpr int kMenuRowCap = kCount + pCount + kConfigN + 5;
static_assert(kFrameMeterRow < kMenuRowCap, "frame meter row must fit the menu id cap");
static_assert(kKiWindowRow < kMenuRowCap, "ki window row must fit the menu id cap");
const char* const kPageNames[] = {"Mod menu", "Settings", "Gameplay / Overlays", "Frame generation", "Load / Auto",
    "Performance fixes", "Extra performance (changes the look)", "Experimental settings", "CPU experiments",
    "GPU experiments", "CPU scheduling", "GPU / Proton experiments", "Diagnostics & logging", "Next launch", "Original controls (flat order)", "Configuration by ini section"};
inline const char* PageName(int page) {
    return page >= pCfgFirst && page < pCount ? kConfigGroupLabels[page - pCfgFirst] : kPageNames[page];  // user #13296: plain group names
}
inline int SettingsPage(int id) {
    if (id == kPage) return pDisplay;  // grouped mode hides it (Visible); the two-page menu does not use this
    if (id >= kEpPre && id <= kEp9) return pExtra;
    if (id == kXTS || id == kXWS) return pDisplay;  // #37433: timing / wake diagnostics are on-screen readouts, not a fix
    if (id == kXRTP || id == kXPCM) return pScheduling;
    if (id == kXAS || id == kXPJH || (id >= kXVSMA && id <= kXCLDR)) return pGpu;
    if (id >= kXW6 && id <= kXPCM) return pCpu;
    if (id >= kGR2a && id <= kGR1) return pProton;
    if ((id >= kVrr && id <= kLat) || id == kGpu) return pSafe;
    if (id >= kWorkerCap && id <= kFgWine) return pBoot;
    if (id == kFg) return pFrameGen;
    if (id == kFgState || id == kUpscaler) return pDisplay;  // #37433: status readouts, not a setting
    if (id == kAutoCont) return pGameplay;
    return pDisplay;
}
inline bool PageContains(int page, int id) {
    if (page == pAll) return true;
    if (id == kClose) return page == pRoot;
    if (id == kSave) return page == pBoot;  // user #13223: no Save row on the first page (closing the menu auto-saves); manual Save stays under Next launch
    return SettingsPage(id) == page;
}
