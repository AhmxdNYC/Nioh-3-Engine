#ifdef _DEBUG
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#else
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif

#include "stdafx.h"
#include "version.h"
#include "helper.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <inipp/inipp.h>
#include <safetyhook.hpp>

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Ini
inipp::Ini<char> ini;
std::string sConfigFile = sFixName + ".ini";
std::filesystem::path FixPath;

// Logger
std::shared_ptr<spdlog::logger> logger;
std::string sLogFile = sFixName + ".log";
std::filesystem::path ExePath;
std::string sExeName;

// Constants
constexpr float fPi = std::numbers::pi_v<float>;
constexpr float fNativeAspect = 16.0f / 9.0f;

// Aspect ratio / FOV / HUD
struct ScreenState {
    int iCurrentResX = 0;
    int iCurrentResY = 0;
    float fAspectRatio = 16.0f / 9.0f;
    float fAspectMultiplier = 1.0f;
    float fHUDWidth = 1920.0f;
    float fHUDHeight = 1080.0f;
    float fHUDWidthOffset = 0.0f;
    float fHUDHeightOffset = 0.0f;
};
ScreenState gScreen;

// Ini settings
struct ConfigState {
   bool bIntroSkip = false;
    bool bCustomResolution = false;
    int iCustomResX = 0;
    int iCustomResY = 0;
    bool bMenuFramegen = false;
    float fBackgroundMeshQuality = 1.0f;
    float fTerrainQuality = 1.0f;
    float fCharacterDrawDistance = 1.0f;
    float fModelDrawDistance = 1.0f;
    float fPartsDrawDistance = 1.0f;
    float fHLODDrawDistance = 1.0f;
    float fMotionQuality = 1.0f;
    bool bFixFOV = false;
    bool bFixHUD = false;
    bool bFixMovies = false;
    bool bDynamicFPS = false;
    int iTargetFPS = 0;
};
ConfigState gConfig;

// Variables
float fHUDAspect = 32.0f / 9.0f;
std::uint8_t* FPSDynamicAdjustment = nullptr;

void CalculateAspectRatio(bool bLog)
{
    if (gScreen.iCurrentResX <= 0 || gScreen.iCurrentResY <= 0) return;

    gScreen.fAspectRatio = static_cast<float>(gScreen.iCurrentResX) / gScreen.iCurrentResY;
    gScreen.fAspectMultiplier = gScreen.fAspectRatio / fNativeAspect;

    if (gScreen.fAspectRatio >= fNativeAspect) {
        gScreen.fHUDWidth = static_cast<float>(gScreen.iCurrentResY) * fNativeAspect;
        gScreen.fHUDHeight = static_cast<float>(gScreen.iCurrentResY);
        gScreen.fHUDWidthOffset = (static_cast<float>(gScreen.iCurrentResX) - gScreen.fHUDWidth) / 2.0f;
        gScreen.fHUDHeightOffset = 0.0f;
    } 
    else {
        gScreen.fHUDWidth = static_cast<float>(gScreen.iCurrentResX);
        gScreen.fHUDHeight = static_cast<float>(gScreen.iCurrentResX) / fNativeAspect;
        gScreen.fHUDWidthOffset = 0.0f;
        gScreen.fHUDHeightOffset = (static_cast<float>(gScreen.iCurrentResY) - gScreen.fHUDHeight) / 2.0f;
    }

    if (bLog) {
        SPDLOG_INFO("----------");
        SPDLOG_INFO("Current Resolution: {}x{} | Aspect Ratio: {:.5f} ({:.5f}) | HUD: {:.2f}x{:.2f} ({:.2f}, {:.2f})", 
            gScreen.iCurrentResX, gScreen.iCurrentResY, 
            gScreen.fAspectRatio, gScreen.fAspectMultiplier, 
            gScreen.fHUDWidth, gScreen.fHUDHeight, 
            gScreen.fHUDWidthOffset, gScreen.fHUDHeightOffset);
        SPDLOG_INFO("----------");
    }
}

void Logging()
{
    // Get path to DLL
    WCHAR dllPath[MAX_PATH] = {0};
    GetModuleFileNameW(thisModule, dllPath, MAX_PATH);
    FixPath = dllPath;
    FixPath = FixPath.remove_filename();

    // Get game name and exe path
    WCHAR gameExePath[MAX_PATH] = {0};
    GetModuleFileNameW(exeModule, gameExePath, MAX_PATH);
    ExePath = gameExePath;
    sExeName = ExePath.filename().string();
    ExePath = ExePath.remove_filename();

    // spdlog initialisation
    try {
        std::ofstream file(ExePath / sLogFile, std::ios::trunc);
        if (file.is_open()) file.close();

        logger = std::make_shared<spdlog::logger>(sFixName, std::make_shared<spdlog::sinks::rotating_file_sink_st>((ExePath / sLogFile).string(), 10 * 1024 * 1024, 1));
        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
        spdlog::flush_on(spdlog::level::debug);
        spdlog::set_level(spdlog::level::debug); 
        
        SPDLOG_INFO("----------");
        SPDLOG_INFO("{} v{}", sFixName, sFixVersion);
        SPDLOG_INFO("Build Date: {} {}", __DATE__, __TIME__);
        SPDLOG_INFO("----------");
        SPDLOG_INFO("Log File: {}", (FixPath / sLogFile).string());
        SPDLOG_INFO("----------");
        SPDLOG_INFO("Module Name: {}", sExeName);
        SPDLOG_INFO("Module Path: {}", ExePath.string());
        SPDLOG_INFO("Module Address: 0x{:x}", reinterpret_cast<uintptr_t>(exeModule));
        SPDLOG_INFO("Module Timestamp: {:d}", Util::GetModuleTimestamp(exeModule));
        SPDLOG_INFO("----------");
    }
    catch (const spdlog::spdlog_ex &ex) {
        Util::ConsoleExit(std::string(sFixName + ": Log initialisation failed: ") + ex.what(), thisModule);
    }
}

void Configuration()
{
    // Inipp initialisation
    std::ifstream iniFile(FixPath / sConfigFile);
    if (!iniFile) {
        SPDLOG_ERROR("{}: Could not locate config file. Make sure {} is located in {}", sFixName, sConfigFile, FixPath.string());
        spdlog::shutdown(); // Flush log
        Util::ConsoleExit(sFixName + ": Could not locate config file.\nMake sure " + sConfigFile + " is located in " + FixPath.string(), thisModule);
    }
    
    SPDLOG_INFO("Config File: {}", (FixPath / sConfigFile).string());
    ini.parse(iniFile);
    ini.strip_trailing_comments();
    
    // Read ini
    gConfig.bIntroSkip = Util::ParseConfig(ini, "Intro Skip", "Enabled", false);
    gConfig.bCustomResolution = Util::ParseConfig(ini, "Custom Resolution", "Enabled", false);
    gConfig.iCustomResX = Util::ParseConfig(ini, "Custom Resolution", "Width", 0, 0, INT_MAX);
    gConfig.iCustomResY = Util::ParseConfig(ini, "Custom Resolution", "Height", 0, 0, INT_MAX);
    gConfig.bMenuFramegen = Util::ParseConfig(ini, "Menu Framegen", "Enabled", false);

    gConfig.fBackgroundMeshQuality = Util::ParseConfig(ini, "Graphical Tweaks", "BackgroundMeshQuality", 1.0f, 0.1f, 10.0f);
    gConfig.fTerrainQuality = Util::ParseConfig(ini, "Graphical Tweaks", "TerrainQuality", 1.0f, 0.1f, 10.0f);
    gConfig.fCharacterDrawDistance = Util::ParseConfig(ini, "Graphical Tweaks", "CharacterDrawDistance", 1.0f, 0.1f, 10.0f);
    gConfig.fModelDrawDistance = Util::ParseConfig(ini, "Graphical Tweaks", "ModelDrawDistance", 1.0f, 0.1f, 10.0f);
    gConfig.fPartsDrawDistance = Util::ParseConfig(ini, "Graphical Tweaks", "PartsDrawDistance", 1.0f, 0.1f, 10.0f);
    gConfig.fHLODDrawDistance = Util::ParseConfig(ini, "Graphical Tweaks", "HLODDrawDistance", 1.0f, 0.1f, 10.0f);
    gConfig.fMotionQuality = Util::ParseConfig(ini, "Graphical Tweaks", "MotionQuality", 1.0f, 0.1f, 10.0f);

    gConfig.bFixFOV = Util::ParseConfig(ini, "Fix FOV", "Enabled", false);
    gConfig.bFixHUD = Util::ParseConfig(ini, "Fix HUD", "Enabled", false);
    gConfig.bFixMovies = Util::ParseConfig(ini, "Fix Movies", "Enabled", false);

    gConfig.bDynamicFPS = Util::ParseConfig(ini, "Dynamic FPS", "Enabled", false);
    gConfig.iTargetFPS = Util::ParseConfig(ini, "Dynamic FPS", "TargetFPS", 0, 0, 500);

    SPDLOG_INFO("----------");
}

void IntroSkip()
{
    if (gConfig.bIntroSkip) {
        if (auto* IntroLogos = Memory::FindPattern("Intro Skip", exeModule, sExeName.c_str(), "41 ?? ?? ?? 04 00 00 00 EB ?? 41 ?? ?? ?? 03 00 00 00 EB ?? 8A ?? ?? 41 88 ?? ??")) {
            Memory::PatchBytes(IntroLogos + 0x4, "\x07", 1);  // AutoSave (4) -> Stage (7)
            Memory::PatchBytes(IntroLogos + 0x1F, "\x03", 1); // Logos (2) -> ShaderComp (3)
        }
    }
}

void Resolution() 
{
    if (gConfig.bCustomResolution) {
        if (gConfig.iCustomResX <= 0 || gConfig.iCustomResY <= 0) {
            std::tie(gConfig.iCustomResX, gConfig.iCustomResY) = Util::GetPhysicalDesktopDimensions();
            SPDLOG_INFO("Custom Resolution: Desktop Resolution: {}x{}", gConfig.iCustomResX, gConfig.iCustomResY);
        }

        static std::wstring sNewResolution = std::to_wstring(gConfig.iCustomResX) + L" x " + std::to_wstring(gConfig.iCustomResY);

        // Resolution string
        static SafetyHookMid ResolutionString_sh{};
        Hook::CreateMidHook(ResolutionString_sh, "Resolution String", exeModule, sExeName.c_str(), "48 8D ?? ?? ?? 48 89 ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 89 ?? ?? ?? 66 0F ?? ?? ?? ?? 48 C7 ?? ?? ?? 0C 00 00 00 48 FF ?? 48 8D ?? ?? 66 ?? ?? 75 ?? E9 ?? ?? ?? ?? 32 ??", 0, [](SafetyHookContext& ctx) {
            if (!ctx.rax) return;

            auto* resString = reinterpret_cast<wchar_t*>(ctx.rax);

            if (wcscmp(resString, L"3840 x 2160") == 0 && sNewResolution.size() <= wcslen(resString)) {  
                wcscpy_s(resString, wcslen(resString) + 1, sNewResolution.c_str());
                SPDLOG_INFO("Resolution String: Replaced 3840 x 2160 with {} x {}", gConfig.iCustomResX, gConfig.iCustomResY);
            }
        });

        // Add custom resolution
        if (auto* ResolutionListWindow = Memory::FindPattern("Resolution List: Window", exeModule, sExeName.c_str(), "4C 8D ?? ?? ?? ?? ?? 48 ?? ?? 41 ?? ?? ?? 75 ?? 8B ?? ?? ?? ?? ??")) {
            std::uint8_t* WindowResList = Memory::GetRelativeAddr(ResolutionListWindow + 0x3);
            Memory::Write(WindowResList + 0x8, gConfig.iCustomResX);
            Memory::Write(WindowResList + 0xC, gConfig.iCustomResY);
            SPDLOG_INFO("Resolution List: Replaced 3840x2160 with {}x{}", gConfig.iCustomResX, gConfig.iCustomResY);
        }

        if (auto* ResolutionListInternal = Memory::FindPattern("Resolution List: Internal", exeModule, sExeName.c_str(), "48 8B ?? 49 ?? ?? 48 ?? ?? 04 48 ?? ?? 48 63 ?? ?? ?? ?? ?? ?? 48 8B ?? ?? ?? ?? ?? ??")) {
            static SafetyHookMid ResolutionListInternalIndex_sh{};
            ResolutionListInternalIndex_sh = safetyhook::create_mid(ResolutionListInternal,
                [](SafetyHookContext& ctx) {
                    ctx.r10 = ctx.rax; // Store res index in r10 temporarily, it will be overwritten later
                });

            static SafetyHookMid ResolutionListInternalResult_sh{};
            ResolutionListInternalResult_sh = safetyhook::create_mid(ResolutionListInternal + 0x1D,
                [](SafetyHookContext& ctx) {
                    // Only change the table for resolution index 1: 3840x2160
                    if (static_cast<int>(ctx.r10) == 1) {
                        int scaleStep = static_cast<int>(ctx.r11);
                        float resScale = 1.0f;

                        // Match scaling logic of the pre-populated table
                        if (scaleStep <= 2) {
                            resScale = 1.0f / 3.0f; // 33%
                        } 
                        else if (scaleStep <= 5) {
                            resScale = 0.5f; // 50%
                        } 
                        else {
                            resScale = (25.0f + (scaleStep * 5.0f)) / 100.0f; // 55-100%
                        }
                        
                        std::uint32_t scaledWidth = static_cast<std::uint32_t>(gConfig.iCustomResX * resScale);
                        std::uint32_t scaledHeight = static_cast<std::uint32_t>(gConfig.iCustomResY * resScale);

                        ctx.rax = (static_cast<std::uint64_t>(scaledHeight) << 32) | scaledWidth;
                    }
                });
        }
    }

    // Current Resolution
    static SafetyHookMid CurrentResolution_sh{};
    Hook::CreateMidHook(CurrentResolution_sh, "Current Resolution", exeModule, sExeName.c_str(), "48 89 ?? ?? 75 ?? 8B ?? ?? ?? ?? ?? 4C 8D ?? ?? 89 ?? ?? ?? 45 33 ??", 0, [](SafetyHookContext& ctx) {
        int ResX = static_cast<int>(ctx.rcx & 0xFFFFFFFF);
        int ResY = static_cast<int>((ctx.rcx >> 32) & 0xFFFFFFFF);

        if (gScreen.iCurrentResX != ResX || gScreen.iCurrentResY != ResY) {
            gScreen.iCurrentResX = ResX;
            gScreen.iCurrentResY = ResY;
            CalculateAspectRatio(true);
        }
    });
}

void FOV()
{
    if (gConfig.bFixFOV) {
        // Fix vert- FOV in cutscenes for ultrawide aspects
        if (auto* CutsceneFOVUltrawide = Memory::FindPattern("FOV: Cutscene Ultrawide", exeModule, sExeName.c_str(), "0F 28 ?? 48 83 ?? ?? C3 48 83 ?? ?? F3 0F ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ??")) {
            static SafetyHookMid CutsceneFOVUltrawide1_sh{};
            CutsceneFOVUltrawide1_sh = safetyhook::create_mid(CutsceneFOVUltrawide,
                [](SafetyHookContext& ctx) {
                    if (gScreen.fAspectRatio > fNativeAspect)
                        ctx.xmm4.f32[0] = 1.0f;
                });

            static SafetyHookMid CutsceneFOVUltrawide2_sh{};
            CutsceneFOVUltrawide2_sh = safetyhook::create_mid(CutsceneFOVUltrawide + 0x3F,
                [](SafetyHookContext& ctx) {
                    if (gScreen.fAspectRatio > fNativeAspect)
                        ctx.xmm4.f32[0] = 1.0f;
                });
        }
    }
}

void Movies()
{
    if (gConfig.bFixMovies) {
        // Fix vert- cropping in movies
        static SafetyHookMid MovieCrop_sh{};
        Hook::CreateMidHook(MovieCrop_sh, "Movies", exeModule, sExeName.c_str(), "89 ?? ?? ?? ?? ?? 8B ?? 48 69 ?? ?? ?? ?? ?? 80 ?? ?? ?? ?? ?? ?? 00 75 ??", 0, [](SafetyHookContext& ctx) {
            if (gScreen.fAspectRatio > fNativeAspect)
                ctx.rax = 100;
        });
    }
}

void HUD()
{
    if (gConfig.bFixHUD) {
        // HUD: Objects: Realtime
        if (auto* HUDObjectsRealtime = Memory::FindPattern("HUD: Objects: Realtime", exeModule, sExeName.c_str(), "89 ?? ?? 49 8B ?? ?? 48 8B ?? FF 90 ?? ?? ?? ?? 8B ?? 33 ??")) {
            static SafetyHookMid HUDObjectsRealtime_sh{};
            HUDObjectsRealtime_sh = safetyhook::create_mid(HUDObjectsRealtime,
                [](SafetyHookContext& ctx) {
                    if (!ctx.r13) return;
        
                    if (gScreen.fAspectRatio > fHUDAspect || gScreen.fAspectRatio < fNativeAspect) {
                        std::uint8_t* pHUDObj = *reinterpret_cast<std::uint8_t**>(ctx.r13 + 0x08);

                        const char* ObjName = *reinterpret_cast<char**>(pHUDObj + 0xB0);
                        short* iHUDObjectX = reinterpret_cast<short*>(pHUDObj + 0xF0);
                        short* iHUDObjectY = reinterpret_cast<short*>(pHUDObj + 0xF2);   
                        float* fScale = reinterpret_cast<float*>(pHUDObj + 0x168);

                        float fOrigX = static_cast<float>(*iHUDObjectX) / *fScale;
                        float fOrigY = static_cast<float>(*iHUDObjectY) / *fScale;

                        // Backgrounds
                        if (fOrigX > 3840.0f && fOrigY > 2150.0f && ObjName) {
                            std::string_view sObj(ObjName);
                            if (sObj == "bg_blackout" || sObj == "cmn_black" || sObj == "map_BG" || sObj == "area_common_bg" || sObj == "list_mask" || 
                                sObj == "copy_RenderTarget" || sObj == "skilltree_bg" || sObj == "bg_screen" || sObj == "bg_black" || sObj == "bg_black_0000" || sObj == "bg_black_0001") {
                                if (gScreen.fAspectRatio > fHUDAspect)
                                    ctx.rax = (static_cast<uintptr_t>(*iHUDObjectY) << 16) | (static_cast<uint16_t>(ceilf(static_cast<float>(*iHUDObjectX) * (gScreen.fAspectRatio / fHUDAspect))));
                                else if (gScreen.fAspectRatio < fNativeAspect)
                                    ctx.rax = (static_cast<uintptr_t>(static_cast<uint16_t>(ceilf(static_cast<float>(*iHUDObjectY) / gScreen.fAspectMultiplier))) << 16) | (static_cast<uintptr_t>(static_cast<uint16_t>(*iHUDObjectX)));
                            }
                        }
                    }                
                });
        }

        // HUD: Objects: Res Change
        if (auto* HUDObjectsResChange = Memory::FindPattern("HUD: Objects: Res Change", exeModule, sExeName.c_str(), "4C 8B ?? 45 84 ?? 0F 85 ?? ?? ?? ?? 80 ?? ?? ?? 00 41 ?? 01 00 00 00")) {
            static SafetyHookMid HUDObjectsResChange_sh{};
            HUDObjectsResChange_sh = safetyhook::create_mid(HUDObjectsResChange,
                [](SafetyHookContext& ctx) {
                    const char* ObjName = *reinterpret_cast<char**>(ctx.rdx + 0xB0);
                    short* iHUDObjectX = reinterpret_cast<short*>(ctx.rdx + 0xF0);
                    short* iHUDObjectY = reinterpret_cast<short*>(ctx.rdx + 0xF2);   
                    float* fScale = reinterpret_cast<float*>(ctx.rdx + 0x168);

                    float fOrigX = static_cast<float>(*iHUDObjectX) / *fScale;
                    float fOrigY = static_cast<float>(*iHUDObjectY) / *fScale;

                    // Backgrounds
                    if (fOrigX > 7670.0f && fOrigY > 2150.0f && ObjName) {
                        std::string_view sObj(ObjName);
                        if (sObj == "Blur_bg") {
                            if (gScreen.fAspectRatio > fHUDAspect)
                                ctx.r10 = (static_cast<uintptr_t>(2160) << 16) | (static_cast<uint16_t>(ceilf(7680.0f * (gScreen.fAspectRatio / fHUDAspect))));
                            else if (gScreen.fAspectRatio < fNativeAspect)
                                ctx.r10 = (static_cast<uintptr_t>(static_cast<uint16_t>(ceilf(2160.0f / gScreen.fAspectMultiplier))) << 16) | static_cast<uintptr_t>(7680);
                            else
                                ctx.r10 = (static_cast<uintptr_t>(2160) << 16) | static_cast<uintptr_t>(7680);
                        }
                    }
                });
        }

        // HUD Size (<16:9)
        static SafetyHookMid HUDSize_sh{};
        Hook::CreateMidHook(HUDSize_sh, "HUD: Size", exeModule, sExeName.c_str(), "F3 0F ?? ?? ?? 48 8B ?? 89 ?? ?? ?? 48 8B ?? ?? ?? 48 85 ?? 74 ?? 48 81 ?? ?? ?? ?? ?? 48 89 ?? ?? ??", 0, [](SafetyHookContext& ctx) {
            float aspectRatio = ctx.xmm1.f32[0] / ctx.xmm2.f32[0];
            if (aspectRatio < fNativeAspect)
                ctx.xmm2.f32[0] = ctx.xmm1.f32[0] / fNativeAspect;
        });


        // Mouse Input (<16:9)
        if (auto* HUDMouseInput1 = Memory::FindPattern("HUD: Mouse Input: 1", exeModule, sExeName.c_str(), "E8 ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F7 ?? 66 0F ?? ?? 0F ?? ??")) {
            static SafetyHookMid HUDMouseInput1_sh{};
            HUDMouseInput1_sh = safetyhook::create_mid(HUDMouseInput1 + 0x5,
                [](SafetyHookContext& ctx) {
                    if (gScreen.fAspectRatio < fNativeAspect)
                        ctx.xmm0.f32[0] = static_cast<float>(gScreen.iCurrentResY);
                });
        }

        if (auto* HUDMouseInput2 = Memory::FindPattern("HUD: Mouse Input: 2", exeModule, sExeName.c_str(), "E8 ?? ?? ?? ?? 0F ?? ?? 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? 0F ?? ?? 76 ??")) {
            static SafetyHookMid HUDMouseInput2_sh{};
            HUDMouseInput2_sh = safetyhook::create_mid(HUDMouseInput2 + 0x5,
                [](SafetyHookContext& ctx) {
                    if (gScreen.fAspectRatio < fNativeAspect)
                        ctx.xmm0.f32[0] = static_cast<float>(gScreen.iCurrentResY);
                });
        }
    }
}

void Graphics()
{
    if (gConfig.fBackgroundMeshQuality != 1.0f) {
        // Background Mesh Quality
        static SafetyHookMid BackgroundMeshQuality_sh{};
        Hook::CreateMidHook(BackgroundMeshQuality_sh, "LOD: Background Mesh Quality", exeModule, sExeName.c_str(), "F3 0F ?? ?? 8D ?? ?? F3 0F ?? ?? ?? ?? ?? ?? ?? A9 ?? ?? ?? ?? 75 ??", 0, [](SafetyHookContext& ctx) {
            ctx.xmm1.f32[0] = std::round(2.0f / gConfig.fBackgroundMeshQuality) * 0.5f; // Low = 2, Med = 1.5, High = 1.0
        });
    }

    if (gConfig.fTerrainQuality != 1.0f) {
        // Terrain Quality
        static SafetyHookMid TerrainQuality_sh{};
        Hook::CreateMidHook(TerrainQuality_sh, "LOD: Terrain Quality", exeModule, sExeName.c_str(), "F3 0F ?? ?? ?? ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? ?? 89 ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? C7 ?? ?? ?? ?? ?? 00 00 80 3F", 0, [](SafetyHookContext& ctx) {
            ctx.xmm0.f32[0] = 0.8f * gConfig.fTerrainQuality;                                           // vLow = 0.3, Low = 0.35, Med = 0.4, High = 0.5, vHigh = 0.8
            ctx.rax = std::bit_cast<uint32_t>(std::min(4000.0f * gConfig.fTerrainQuality, 10000.0f));   // vLow = 1000, Low = 1250, Med = 1500, High = 2000, vHigh = 4000
            ctx.rip = TerrainQuality_sh.target_address() + 0x10;                                        // jmp over loading preset values
        });
    }

    if (gConfig.fCharacterDrawDistance != 1.0f) {
        // Character Draw Distance
        static SafetyHookMid CharacterDrawDistance_sh{};
        Hook::CreateMidHook(CharacterDrawDistance_sh, "LOD: Character Draw Distance", exeModule, sExeName.c_str(), "83 ?? ?? ?? ?? ?? ?? FF 48 8B ?? ?? ?? ?? ?? 48 85 ?? 74 ?? F7 ?? ?? ?? ?? ?? ?? ?? ?? ?? 0F 85 ?? ?? ?? ??", 0, [](SafetyHookContext& ctx) {
            ctx.xmm8.f32[0] = std::min(5000.0f * gConfig.fCharacterDrawDistance, 20000.0f); // Default = 6000?
        });
    }

    if (gConfig.fModelDrawDistance != 1.0f) {
        // Model Draw Distance
        if (auto* ModelDrawDistance = Memory::FindPattern("LOD: Model Draw Distance", exeModule, sExeName.c_str(), "80 ?? ?? ?? ?? ?? 00 74 ?? F3 ?? 0F ?? ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? 0F ?? ?? 73 ??")) {
            Memory::Write(Memory::GetRelativeAddr(ModelDrawDistance + 0x16), std::max(260.0f / gConfig.fModelDrawDistance, 100.0f));
            Memory::Write(Memory::GetRelativeAddr(ModelDrawDistance + 0x3F), std::min(15000.0f * gConfig.fModelDrawDistance, 25000.0f));

            static SafetyHookMid ModelDrawDistance_sh{};
            ModelDrawDistance_sh = safetyhook::create_mid(ModelDrawDistance,
                [](SafetyHookContext& ctx) {
                    *reinterpret_cast<BYTE*>(ctx.rax + 0x11F7) = 1;
                });
        }
    }

    if (gConfig.fPartsDrawDistance != 1.0f) {
        // Parts Draw Distance
        static SafetyHookMid PartsDrawDistance_sh{};
        Hook::CreateMidHook(PartsDrawDistance_sh, "LOD: Parts Draw Distance", exeModule, sExeName.c_str(), "74 ?? 38 ?? ?? ?? ?? ?? 0F 85 ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? 48 83 ?? ?? ?? C3", 0x2, [](SafetyHookContext& ctx) {
            if (gConfig.fPartsDrawDistance != 1.0f) {
                *reinterpret_cast<BYTE*>(ctx.rax + 0x1754) = 1;
                *reinterpret_cast<float*>(ctx.rax + 0x1750) = std::min(5000.0f * gConfig.fPartsDrawDistance, 25000.0f);
            }
            else {
                *reinterpret_cast<BYTE*>(ctx.rax + 0x1754) = 0;
                *reinterpret_cast<float*>(ctx.rax + 0x1750) = 0.0f;
            }
        });
    }

    if (gConfig.fHLODDrawDistance != 1.0f) {
        // HLOD Draw Distance
        static SafetyHookMid HLODDrawDistance_sh{};
        Hook::CreateMidHook(HLODDrawDistance_sh, "LOD: HLOD Draw Distance", exeModule, sExeName.c_str(), "8B ?? 48 8D ?? 78 E8 ?? ?? ?? ?? 0F 2F ?? ??", 0xB, [](SafetyHookContext& ctx) {
            float* dist = reinterpret_cast<float*>(ctx.rbp + 0x78);
            *dist = std::min(*dist * gConfig.fHLODDrawDistance, 20000.0f);
        });
    }

    if (gConfig.fMotionQuality != 1.0f) {
        // Motion Quality
        if (auto* MotionQuality = Memory::FindPattern("LOD: Motion Quality", exeModule, sExeName.c_str(), "BA 08 00 00 00 83 ?? 01 74 ?? 83 ?? 01 75 ??"))
            Memory::Write(MotionQuality + 0x1, static_cast<int>(8.0f * gConfig.fMotionQuality));
    }
}

void Framerate()
{
    if (gConfig.bMenuFramegen) {
        // Allow framegen in menus
        if (auto* FramegenStatus = Memory::FindPattern("Framerate: Menu Framegen", exeModule, sExeName.c_str(), "75 ?? E8 ?? ?? ?? ?? 41 ?? ?? 84 ?? 74 ?? 40 ?? ?? 49 8B ?? ?? ?? ?? ??"))
            Memory::PatchBytes(FramegenStatus, "\x90\x90", 2);
    }

    if (gConfig.bDynamicFPS) {
        // Dynamic Framerate Setting
        if (auto* DynamicFramerateSetting = Memory::FindPattern("Framerate: Dynamic FPS: Setting", exeModule, sExeName.c_str(), "44 39 ?? ?? ?? ?? ?? 75 ?? 45 89 ?? ?? E9 ?? ?? ?? ?? 45 38 ?? ?? 0F 85 ?? ?? ?? ??"))
            FPSDynamicAdjustment = Memory::GetRelativeAddr(DynamicFramerateSetting + 0x3);

        // Dynamic Framerate Target
        static SafetyHookMid DynamicFramerateTarget_sh{};
        Hook::CreateMidHook(DynamicFramerateTarget_sh, "Framerate: Dynamic FPS: Target", exeModule, sExeName.c_str(), "45 89 ?? ?? 48 8B ?? ?? ?? ?? ?? 48 85 ?? 74 ?? 89 ?? ?? ?? ?? ?? 48 83 ?? ?? C3", 0x10, [](SafetyHookContext& ctx) {
            float averagedFrametime = *reinterpret_cast<float*>(ctx.r8 + 0x74);
            float averagedFPS = 1000.0f / averagedFrametime;
            
            int newTargetFPS = static_cast<int>(averagedFPS);
            
            if (newTargetFPS > 500) newTargetFPS = 500;
            if (newTargetFPS < 30) newTargetFPS = 30;

            if (gConfig.iTargetFPS != 0)
                newTargetFPS = gConfig.iTargetFPS;

            ctx.rax = static_cast<uintptr_t>(newTargetFPS);
        });

        // Menu Input
        static SafetyHookMid FramerateMenuInput_sh{};
        Hook::CreateMidHook(FramerateMenuInput_sh, "Framerate: Menu Input", exeModule, sExeName.c_str(), "48 8B ?? ?? 0F 28 ?? 48 8B ?? 48 ?? ?? 74 ?? 48 8B ?? ??", 0, [](SafetyHookContext& ctx) {
            // Fix repeat inputs being dropped
            if (FPSDynamicAdjustment && *reinterpret_cast<int*>(FPSDynamicAdjustment) == 1) {
                int fps = static_cast<int>(ctx.rax);
                if (fps > 0)
                    ctx.xmm0.f32[0] = std::round((60.0f / static_cast<float>(fps)) * 4.0f) / 4.0f;
            }
        });
    }
}

DWORD __stdcall Main(void*)
{
    Logging();
    Configuration();

    IntroSkip();
    Resolution();
    FOV();
    Movies();
    HUD();
    Graphics();
    Framerate();

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {  
        DisableThreadLibraryCalls(hModule);
        thisModule = hModule;

        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
        if (mainHandle) {
            SetThreadPriority(mainHandle, THREAD_PRIORITY_HIGHEST);
            CloseHandle(mainHandle);
        }
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}