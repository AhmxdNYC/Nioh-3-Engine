// Included after menu.cpp's stable Id enum. Presentation only: existing row IDs,
// kOrder, ini keys and Remote() continue to describe exactly the same settings.
#pragma once
#include "../../menu_navigation.h"

enum MenuPage { pRoot, pSettings, pDisplay, pFrameGen, pGameplay, pSafe, pExtra,
                pExperimental, pCpu, pGpu, pScheduling, pProton, pDiagnostics, pBoot, pAll,
                pConfig, pOvController, pOvFrameMeter, pOvInput, pOvFps, pCamera, pSCpu, pSGpu, pSGame, pOvHud,
                pCfgFirst, pCount = pCfgFirst + kConfigGroupN };
constexpr int kPageRow = kCount;
constexpr int kBackRow = kPageRow + pCount;
constexpr int kSettingRow = kBackRow + 1;
constexpr int kRematchRow = kSettingRow + kConfigN;
constexpr int kPracticeRow = kRematchRow + 1;  // user #13223: first page = Settings / Boss Matches (#13293 rename) / Practice
constexpr int kFrameMeterRow = kPracticeRow + 1;  // #34265: Display Overlays > Frame meter settings (the only frame meter toggle)
constexpr int kKiWindowRow = kFrameMeterRow + 1;  // #34558: dark-blue Ki window on the player frame meter
constexpr int kModMenuRow = kKiWindowRow + 1;  // #38077: first row of every page: jump to the menu's top-level page
constexpr int kCloseTopRow = kKiWindowRow + 2;  // #38077: second row of every page: close the menu
constexpr int kReportRow = kKiWindowRow + 3;  // #38437: third always-visible top row: state snapshot + report website
constexpr int kPsToggleN = 16;                // #38437: Training frame-meter toggles (menu.cpp kPsToggles) as rows of the Frame meter page
constexpr int kPsRowFirst = kReportRow + 1;
constexpr int kPsAllRowsRow = kPsRowFirst + kPsToggleN;  // 'All frame rows' master (moved here from Practice, human #38180 rows)
constexpr int kPsAllKeysRow = kPsAllRowsRow + 1;         // 'All key indicators' master
constexpr int kCamRowFirst = kPsAllKeysRow + 1;  // Camera settings page: left / right / forward / back / return to default
constexpr int kCamRowN = 5;
constexpr int kCamRight = 0, kCamUp = 1, kCamDistance = 2, kCamLens = 3, kCamReset = 4;
constexpr int kTimeOfDayRow = kCamRowFirst + kCamRowN;
constexpr int kWeatherRow = kTimeOfDayRow + 1;
constexpr int kUpdatePageRow = kWeatherRow + 1;
constexpr int kFrameMeterStyleRow = kUpdatePageRow + 1;
constexpr int kFrameMeterSizeRow = kFrameMeterStyleRow + 1;
constexpr int pSaveFiles = -2; // private subpage, keeps all existing page/row IDs stable
constexpr int kSaveFilesRow = kFrameMeterSizeRow + 1;
constexpr int kSaveSlotRow = kSaveFilesRow + 1;
constexpr int kSaveBackupRow = kSaveSlotRow + 1;
constexpr int kSaveSwitchRow = kSaveBackupRow + 1;
constexpr int kSaveCancelRow = kSaveSwitchRow + 1;
constexpr int pSaveActions = -3;
// Keep the removed relaunch row ID reserved so later row IDs stay stable.
constexpr int kSaveRefreshRow = kSaveCancelRow + 2;
constexpr int kSaveLiveRow = kSaveRefreshRow + 1;
constexpr int kSaveOpenRow = kSaveLiveRow + 1;
constexpr int kSaveNamedFirst = kSaveOpenRow + 1;
constexpr int kReverseModsRow = kSaveNamedFirst + savefiles::kMaxSlots;  // human #45507: first page, after 'Update + Page'
constexpr int kMenuRowCap = kReverseModsRow + 1;
static_assert(kFrameMeterStyleRow < kMenuRowCap, "frame meter style must fit the menu id cap");
static_assert(kWeatherRow < kMenuRowCap, "Display sky rows must fit the menu id cap");
static_assert(kPsAllKeysRow < kMenuRowCap, "frame meter master rows must fit the menu id cap");
static_assert(kFrameMeterRow < kMenuRowCap, "frame meter row must fit the menu id cap");
static_assert(kPsRowFirst + kPsToggleN <= kMenuRowCap, "top and frame meter rows must fit the menu id cap");
static_assert(kCloseTopRow < kMenuRowCap, "top rows must fit the menu id cap");
static_assert(kKiWindowRow < kMenuRowCap, "ki window row must fit the menu id cap");
const char* const kPageNames[] = {"Mod menu", "Optimizations", "Display Overlays", "Frame Gen", "Load / Auto",
    "Performance fixes", "Extra performance (changes the look)", "Experimental settings", "CPU experiments",
    "GPU experiments", "CPU scheduling", "GPU / Proton experiments", "Diagnostics & logging", "Next launch", "Original controls (flat order)", "Configuration by ini section",
    "Controller overlay settings", "Frame meter settings", "Input display", "FPS", "Camera settings", "CPU", "GPU", "Game", "Game HUD"};
inline const char* PageName(int page) {
    if (page == pSaveFiles) return "Save files";
    if (page == pSaveActions) return "Selected save";
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
