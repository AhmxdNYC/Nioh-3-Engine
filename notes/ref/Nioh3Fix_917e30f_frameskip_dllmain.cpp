#include "stdafx.h"
#include "version.h"
#include "helper.hpp"

#ifdef _DEBUG
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#else
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif

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

// Aspect ratio / FOV / HUD
std::pair<int, int> DesktopDimensions = { 0, 0 };
constexpr float fPi = std::numbers::pi_v<float>;
constexpr float fNativeAspect = 16.0f / 9.0f;
float fAspectRatio = 16.0f / 9.0f;
float fAspectMultiplier = 1.0f;
float fHUDWidth = 1920.0f;
float fHUDWidthOffset = 0.0f;
float fHUDHeight = 1080.0f;
float fHUDHeightOffset = 0.0f;

// Ini settings
bool bIntroSkip = false;
bool bCustomResolution = false;
int iCustomResX = 0;
int iCustomResY = 0;
bool bMenuFramegen = false;
float fLODDistanceMultiplier = 1.0f;
bool bFixFOV = false;
bool bFixHUD = false;
bool bFixMovies = false;
bool bDynamicFPS = false;
int iTargetFPS = 0;
bool bFrameskip = true;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;
float fHUDAspect = 32.0f / 9.0f;
std::uint8_t* FPSDynamicAdjustment = nullptr;

template<typename T>
T GetConfigAndLog(inipp::Ini<char>& ini, const std::string& section, const std::string& key, T defaultValue)
{
    T value = defaultValue;
    inipp::get_value(ini.sections[section], key, value);
    SPDLOG_INFO("Config Parse: [{}]: {} = {}", section, key, value);
    return value;
}

void CalculateAspectRatio(bool bLog)
{
    if (iCurrentResX <= 0 || iCurrentResY <= 0)
        return;

    fAspectRatio = static_cast<float>(iCurrentResX) / iCurrentResY;
    fAspectMultiplier = fAspectRatio / fNativeAspect;

    fHUDWidth = static_cast<float>(iCurrentResY) * fNativeAspect;
    fHUDHeight = static_cast<float>(iCurrentResY);
    fHUDWidthOffset = (static_cast<float>(iCurrentResX) - fHUDWidth) / 2.0f;
    fHUDHeightOffset = 0.0f;

    if (fAspectRatio < fNativeAspect) {
        fHUDWidth = static_cast<float>(iCurrentResX);
        fHUDHeight = static_cast<float>(iCurrentResX) / fNativeAspect;
        fHUDWidthOffset = 0.0f;
        fHUDHeightOffset = (static_cast<float>(iCurrentResY) - fHUDHeight) / 2.0f;
    }

    if (bLog) {
        SPDLOG_INFO("----------");
        SPDLOG_INFO("Current Resolution: Resolution: {:d}x{:d}", iCurrentResX, iCurrentResY);
        SPDLOG_INFO("Current Resolution: fAspectRatio: {}", fAspectRatio);
        SPDLOG_INFO("Current Resolution: fAspectMultiplier: {}", fAspectMultiplier);
        SPDLOG_INFO("Current Resolution: fHUDWidth: {}", fHUDWidth);
        SPDLOG_INFO("Current Resolution: fHUDHeight: {}", fHUDHeight);
        SPDLOG_INFO("Current Resolution: fHUDWidthOffset: {}", fHUDWidthOffset);
        SPDLOG_INFO("Current Resolution: fHUDHeightOffset: {}", fHUDHeightOffset);
        SPDLOG_INFO("----------");
    }
}

void Logging()
{
    // Get path to DLL
    WCHAR dllPath[_MAX_PATH] = {0};
    GetModuleFileNameW(thisModule, dllPath, MAX_PATH);
    FixPath = dllPath;
    FixPath = FixPath.remove_filename();

    // Get game name and exe path
    WCHAR gameExePath[_MAX_PATH] = {0};
    GetModuleFileNameW(exeModule, gameExePath, MAX_PATH);
    ExePath = gameExePath;
    sExeName = ExePath.filename().string();
    ExePath = ExePath.remove_filename();

    // spdlog initialisation
    try
    {
        std::ofstream file(ExePath / sLogFile, std::ios::trunc);
        if (file.is_open()) file.close();

        logger = std::make_shared<spdlog::logger>(sFixName, std::make_shared<spdlog::sinks::rotating_file_sink_st>((ExePath / sLogFile).string(), 10 * 1024 * 1024, 1));
        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
        spdlog::flush_on(spdlog::level::debug);
        spdlog::set_level(spdlog::level::debug); 
        
        SPDLOG_INFO("----------");
        SPDLOG_INFO("{:s} v{:s}", sFixName, sFixVersion);
        SPDLOG_INFO("----------");
        SPDLOG_INFO("Log File: {}", (FixPath / sLogFile).string());
        SPDLOG_INFO("----------");
        SPDLOG_INFO("Module Name: {:s}", sExeName);
        SPDLOG_INFO("Module Path: {}", ExePath.string());
        SPDLOG_INFO("Module Address: 0x{:x}", reinterpret_cast<uintptr_t>(exeModule));
        SPDLOG_INFO("Module Timestamp: {:d}", Memory::GetModuleTimestamp(exeModule));
        SPDLOG_INFO("----------");
    }
    catch (const spdlog::spdlog_ex &ex) {
        Util::ConsoleExit(std::string("Log initialisation failed: ") + ex.what(), thisModule);
    }
}

void Configuration()
{
    // Inipp initialisation
    std::ifstream iniFile(FixPath / sConfigFile);
    if (!iniFile) {
        spdlog::shutdown(); // Flush log
        Util::ConsoleExit("Could not locate config file.\nMake sure " + sConfigFile + " is located in " + FixPath.string() + sConfigFile, thisModule);
    }
    
    SPDLOG_INFO("Config File: {}", (FixPath / sConfigFile).string());
    ini.parse(iniFile);
    ini.strip_trailing_comments();
    
    // Read ini
    bIntroSkip = GetConfigAndLog(ini, "Intro Skip", "Enabled", false);
    bCustomResolution = GetConfigAndLog(ini, "Custom Resolution", "Enabled", false);
    iCustomResX = GetConfigAndLog(ini, "Custom Resolution", "Width", 0);
    iCustomResY = GetConfigAndLog(ini, "Custom Resolution", "Height", 0);
    bMenuFramegen = GetConfigAndLog(ini, "Menu Framegen", "Enabled", false);
    fLODDistanceMultiplier = GetConfigAndLog(ini, "LOD Distance", "Multiplier", 1.0f);
    
    if (fLODDistanceMultiplier < 0.1f || fLODDistanceMultiplier > 10.0f) {
        fLODDistanceMultiplier = std::clamp(fLODDistanceMultiplier, 0.1f , 10.0f);
        SPDLOG_WARN("Config Parse: [LOD Distance]: Multiplier: Value invalid, clamped to = {}", fLODDistanceMultiplier);
    }

    bFixFOV = GetConfigAndLog(ini, "Fix FOV", "Enabled", false);
    bFixHUD = GetConfigAndLog(ini, "Fix HUD", "Enabled", false);
    bFixMovies = GetConfigAndLog(ini, "Fix Movies", "Enabled", false);

    bDynamicFPS = GetConfigAndLog(ini, "Dynamic FPS", "Enabled", false);
    iTargetFPS = GetConfigAndLog(ini, "Dynamic FPS", "TargetFPS", 0);
    bFrameskip = GetConfigAndLog(ini, "Dynamic FPS", "Frameskip", true);

    // Clamp settings
    iTargetFPS = std::clamp(iTargetFPS, 0, 500);

    SPDLOG_INFO("----------");
}

void IntroSkip()
{
    if (bIntroSkip)
    {
        // Intro Logos
        std::uint8_t* IntroLogosScanResult = Memory::PatternScan(exeModule, "41 ?? ?? ?? 04 00 00 00 EB ?? 41 ?? ?? ?? 03 00 00 00 EB ?? 8A ?? ?? 41 88 ?? ??");
        if (IntroLogosScanResult) {
            SPDLOG_INFO("Intro Skip: Logos: Address is {:s}+{:x}", sExeName, IntroLogosScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            Memory::PatchBytes(IntroLogosScanResult + 0x4, "\x07", 1);  // AutoSave (4) -> Stage (7)
            Memory::PatchBytes(IntroLogosScanResult + 0x1F, "\x03", 1); // Logos (2) -> ShaderComp (3)
        }
        else {
            SPDLOG_ERROR("Intro Skip: Pattern scan(s) failed.");
        } 
    }
}

void Resolution() 
{
    if (bCustomResolution)
    {
        if (iCustomResX <= 0 || iCustomResY <= 0) {
            DesktopDimensions = Util::GetPhysicalDesktopDimensions();
            iCustomResX = DesktopDimensions.first;
            iCustomResY = DesktopDimensions.second;
            SPDLOG_INFO("Custom Resolution: Desktop Resolution: {}x{}", DesktopDimensions.first, DesktopDimensions.second);
        }

        static std::wstring sNewResolution = std::to_wstring(iCustomResX) + L" x " + std::to_wstring(iCustomResY);

        // Resolution string
        std::uint8_t* ResolutionStringScanResult = Memory::PatternScan(exeModule, "48 8D ?? ?? ?? 48 89 ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 89 ?? ?? ?? 66 0F ?? ?? ?? ?? 48 C7 ?? ?? ?? 0C 00 00 00 48 FF ?? 48 8D ?? ?? 66 ?? ?? 75 ?? E9 ?? ?? ?? ?? 32 ??");
        if (ResolutionStringScanResult) {
            SPDLOG_INFO("Resolution String: Address is {:s}+{:x}", sExeName.c_str(), ResolutionStringScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid ResolutionStringMidHook{};
            ResolutionStringMidHook = safetyhook::create_mid(ResolutionStringScanResult,
                [](SafetyHookContext& ctx) {
                    if (!ctx.rax) return;

                    auto* resString = reinterpret_cast<wchar_t*>(ctx.rax);

                    if (wcscmp(resString, L"3840 x 2160") == 0 && sNewResolution.size() <= wcslen(resString)) {  
                        wcscpy_s(resString, wcslen(resString) + 1, sNewResolution.c_str());
                        SPDLOG_INFO("Resolution String: Replaced 3840 x 2160 with {:s}", Util::wstring_to_string(sNewResolution));
                    }
                });
        }
        else {
            SPDLOG_ERROR("Resolution String: Pattern scan failed.");
        }

        // Add custom resolution
        std::uint8_t* ResolutionListWindowScanResult = Memory::PatternScan(exeModule, "4C 8D ?? ?? ?? ?? ?? 49 8B ?? 4C 8D ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 3B ?? 74 ??");
        std::uint8_t* ResolutionListInternalScanResult = Memory::PatternScan(exeModule, "48 8B ?? 49 ?? ?? 48 ?? ?? 04 48 ?? ?? 48 63 ?? ?? ?? ?? ?? ?? 48 8B ?? ?? ?? ?? ?? ??");
        if (ResolutionListWindowScanResult && ResolutionListInternalScanResult) {
            SPDLOG_INFO("Resolution List: Window: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListWindowScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            
            std::uint8_t* WindowResList = Memory::GetRelativeAddr(ResolutionListWindowScanResult + 0x14);
            Memory::Write(WindowResList + 0x8, iCustomResX);
            Memory::Write(WindowResList + 0xC, iCustomResY);

            SPDLOG_INFO("Resolution List: Replaced 3840x2160 with {}x{}", iCustomResX, iCustomResY);

            SPDLOG_INFO("Resolution List: Internal: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListInternalScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid ResolutionListInternalIndexMidHook{};
            ResolutionListInternalIndexMidHook = safetyhook::create_mid(ResolutionListInternalScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.r10 = ctx.rax; // Store res index in r10 temporarily, it will be overwritten later
                });

            static SafetyHookMid ResolutionListInternalResultMidHook{};
            ResolutionListInternalResultMidHook = safetyhook::create_mid(ResolutionListInternalScanResult + 0x1D,
                [](SafetyHookContext& ctx) {
                    // Only change the table for resolution index 1: 3840x2160
                    if (static_cast<int>(ctx.r10) == 1) {
                        int scaleStep = static_cast<int>(ctx.r11);
                        float resScale = 1.0f;

                        // Match scaling logic of the pre-populated table
                        if (scaleStep <= 2) {
                            resScale = 1.0f / 3.0f; // 33%
                        } else if (scaleStep <= 5) {
                            resScale = 0.5f;        // 50%
                        } else {
                            resScale = (25.0f + (scaleStep * 5.0f)) / 100.0f; // 55-100%
                        }
                        
                        std::uint32_t scaledWidth = static_cast<std::uint32_t>(iCustomResX * resScale);
                        std::uint32_t scaledHeight = static_cast<std::uint32_t>(iCustomResY * resScale);

                        ctx.rax = (static_cast<std::uint64_t>(scaledHeight) << 32) | scaledWidth;
                    }
                });
        }
        else {
            SPDLOG_ERROR("Resolution List: Pattern scan(s) failed.");
        }
    }

    // Get current resolution
    std::uint8_t* CurrentResolutionScanResult = Memory::PatternScan(exeModule, "48 89 ?? ?? 75 ?? 8B ?? ?? ?? ?? ?? 4C 8D ?? ?? 89 ?? ?? ?? 45 33 ??");
    if (CurrentResolutionScanResult) {
        SPDLOG_INFO("Current Resolution: Address is {:s}+{:x}", sExeName, CurrentResolutionScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
        static SafetyHookMid CurrentResolutionMidHook{};
        CurrentResolutionMidHook = safetyhook::create_mid(CurrentResolutionScanResult,
            [](SafetyHookContext& ctx) {
                int ResX = static_cast<int>(ctx.rcx & 0xFFFFFFFF);
                int ResY = static_cast<int>((ctx.rcx >> 32) & 0xFFFFFFFF);
  
                if (iCurrentResX != ResX || iCurrentResY != ResY) {
                    iCurrentResX = ResX;
                    iCurrentResY = ResY;
                    CalculateAspectRatio(true);
                }
            });
    }
    else {
        SPDLOG_ERROR("Current Resolution: Pattern scan failed.");
    } 
}

void FOV()
{
    if (bFixFOV)
    {
        // Fix vert- FOV in cutscenes for ultrawide aspects
        std::uint8_t* CutsceneFOVUltrawideScanResult = Memory::PatternScan(exeModule, "0F 28 ?? 48 83 ?? ?? C3 48 83 ?? ?? F3 0F ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ??");
        if (CutsceneFOVUltrawideScanResult) {
            SPDLOG_INFO("FOV: Cutscene Ultrawide: Address is {:s}+{:x}", sExeName, CutsceneFOVUltrawideScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid CutsceneFOVUltrawide1MidHook{};
            CutsceneFOVUltrawide1MidHook = safetyhook::create_mid(CutsceneFOVUltrawideScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm4.f32[0] = 1.0f;
                });

            static SafetyHookMid CutsceneFOVUltrawide2MidHook{};
            CutsceneFOVUltrawide2MidHook = safetyhook::create_mid(CutsceneFOVUltrawideScanResult + 0x3F,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.xmm4.f32[0] = 1.0f;
                });
        }
        else {
            SPDLOG_ERROR("FOV: Cutscene Ultrawide: Pattern scan failed.");
        }
    }
}

void Movies()
{
    if (bFixMovies)
    {
        // Fix vert- cropping in movies
        std::uint8_t* MovieCropScanResult = Memory::PatternScan(exeModule, "89 ?? ?? ?? ?? ?? 8B ?? 48 69 ?? ?? ?? ?? ?? 80 ?? ?? ?? ?? ?? ?? 00 75 ??");
        if (MovieCropScanResult) {
            SPDLOG_INFO("Movies: Address is {:s}+{:x}", sExeName, MovieCropScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid MovieCropMidHook{};
            MovieCropMidHook = safetyhook::create_mid(MovieCropScanResult,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio > fNativeAspect)
                        ctx.rax = 100;
                });
        }
        else {
            SPDLOG_ERROR("Movies: Pattern scan failed.");
        } 
    }
}

void HUD()
{
    if (bFixHUD) 
    {
        // HUD Objects
        std::uint8_t* HUDObjectsRealtimeScanResult = Memory::PatternScan(exeModule, "89 ?? ?? 49 8B ?? ?? 48 8B ?? FF 90 ?? ?? ?? ?? 8B ?? 33 ??");
        std::uint8_t* HUDObjectsResChangeScanResult = Memory::PatternScan(exeModule, "4C 8B ?? 45 84 ?? 0F 85 ?? ?? ?? ?? 80 ?? ?? ?? 00 41 ?? 01 00 00 00");
        if (HUDObjectsRealtimeScanResult && HUDObjectsResChangeScanResult) { 
            SPDLOG_INFO("HUD: Objects: Realtime: Address is {:s}+{:x}", sExeName, HUDObjectsRealtimeScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid HUDObjectsRealtimeMidHook{};
            HUDObjectsRealtimeMidHook = safetyhook::create_mid(HUDObjectsRealtimeScanResult,
                [](SafetyHookContext& ctx) {
                    if (!ctx.r13) return;
        
                    if (fAspectRatio > fHUDAspect || fAspectRatio < fNativeAspect) {
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
                                if (fAspectRatio > fHUDAspect)
                                    ctx.rax = (static_cast<uintptr_t>(*iHUDObjectY) << 16) | (static_cast<uint16_t>(ceilf(static_cast<float>(*iHUDObjectX) * (fAspectRatio / fHUDAspect))));
                                else if (fAspectRatio < fNativeAspect)
                                    ctx.rax = (static_cast<uintptr_t>(static_cast<uint16_t>(ceilf(static_cast<float>(*iHUDObjectY) / fAspectMultiplier))) << 16) | (static_cast<uintptr_t>(static_cast<uint16_t>(*iHUDObjectX)));
                            }
                        }
                    }                
                });

            SPDLOG_INFO("HUD: Objects: Res Change: Address is {:s}+{:x}", sExeName, HUDObjectsResChangeScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid HUDObjectsResChangeMidHook{};
            HUDObjectsResChangeMidHook = safetyhook::create_mid(HUDObjectsResChangeScanResult,
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
                            if (fAspectRatio > fHUDAspect)
                                ctx.r10 = (static_cast<uintptr_t>(2160) << 16) | (static_cast<uint16_t>(ceilf(7680.0f * (fAspectRatio / fHUDAspect))));
                            else if (fAspectRatio < fNativeAspect)
                                ctx.r10 = (static_cast<uintptr_t>(static_cast<uint16_t>(ceilf(2160.0f / fAspectMultiplier))) << 16) | static_cast<uintptr_t>(7680);
                            else
                                ctx.r10 = (static_cast<uintptr_t>(2160) << 16) | static_cast<uintptr_t>(7680);
                        }
                    }
                });
        }
        else {
            SPDLOG_ERROR("HUD: Objects: Pattern scan(s) failed.");
        }

        // HUD Size (<16:9)
        std::uint8_t* HUDSizeScanResult = Memory::PatternScan(exeModule, "F3 0F ?? ?? ?? 48 8B ?? 89 ?? ?? ?? 48 8B ?? ?? ?? 48 85 ?? 74 ?? 48 81 ?? ?? ?? ?? ?? 48 89 ?? ?? ??");
        if (HUDSizeScanResult) { 
            SPDLOG_INFO("HUD: Size: Address is {:s}+{:x}", sExeName, HUDSizeScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid HUDSizeMidHook{};
            HUDSizeMidHook = safetyhook::create_mid(HUDSizeScanResult,
                [](SafetyHookContext& ctx) {
                    float aspectRatio = ctx.xmm1.f32[0] / ctx.xmm2.f32[0];
                    if (aspectRatio < fNativeAspect)
                        ctx.xmm2.f32[0] = ctx.xmm1.f32[0] / fNativeAspect;
                });
        }
        else {
            SPDLOG_ERROR("HUD: Size: Pattern scan failed.");
        }

        // Mouse Input (<16:9)
        std::uint8_t* HUDMouseInput1ScanResult = Memory::PatternScan(exeModule, "E8 ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F7 ?? 66 0F ?? ?? 0F ?? ??");
        std::uint8_t* HUDMouseInput2ScanResult = Memory::PatternScan(exeModule, "E8 ?? ?? ?? ?? 0F 57 ?? 44 0F ?? ?? F3 0F ?? ?? 0F 28 ?? F3 ?? 0F ?? ?? ??");
        if (HUDMouseInput1ScanResult && HUDMouseInput2ScanResult) { 
            SPDLOG_INFO("HUD: Mouse Input: 1: Address is {:s}+{:x}", sExeName, HUDMouseInput1ScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid HUDMouseInput1MidHook{};
            HUDMouseInput1MidHook = safetyhook::create_mid(HUDMouseInput1ScanResult + 0x5,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm0.f32[0] = static_cast<float>(iCurrentResY);
                });

            SPDLOG_INFO("HUD: Mouse Input: 2: Address is {:s}+{:x}", sExeName, HUDMouseInput2ScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid HUDMouseInput2MidHook{};
            HUDMouseInput2MidHook = safetyhook::create_mid(HUDMouseInput2ScanResult + 0x5,
                [](SafetyHookContext& ctx) {
                    if (fAspectRatio < fNativeAspect)
                        ctx.xmm0.f32[0] = static_cast<float>(iCurrentResY);
                });
        }
        else {
            SPDLOG_ERROR("HUD: Mouse Input: Pattern scan(s) failed.");
        }
    }
}
void Graphics()
{
    if (fLODDistanceMultiplier > 1.0f)
    {
        // Background Mesh Quality
        std::uint8_t* BackgroundMeshQualityScanResult = Memory::PatternScan(exeModule, "F3 0F ?? ?? 8D ?? ?? F3 0F ?? ?? ?? ?? ?? ?? ?? A9 ?? ?? ?? ?? 75 ??");
        if (BackgroundMeshQualityScanResult) { 
            SPDLOG_INFO("LOD: Background Mesh Quality: Address is {:s}+{:x}", sExeName, BackgroundMeshQualityScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid BackgroundMeshQualityMidHook{};
            BackgroundMeshQualityMidHook = safetyhook::create_mid(BackgroundMeshQualityScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.xmm1.f32[0] = 1.0f / fLODDistanceMultiplier;    // Low = 2, Med = 1.5, High = 1.0
                });
        }
        else {
            SPDLOG_ERROR("LOD: Background Mesh Quality: Pattern scan failed.");
        }

        // Terrain Quality
        std::uint8_t* TerrainQualityScanResult = Memory::PatternScan(exeModule, "F3 0F ?? ?? ?? ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? ?? 89 ?? ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? C7 ?? ?? ?? ?? ?? 00 00 80 3F");
        if (TerrainQualityScanResult) { 
            SPDLOG_INFO("LOD: Terrain Quality: Address is {:s}+{:x}", sExeName, TerrainQualityScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid TerrainQualityMidHook{};
            TerrainQualityMidHook = safetyhook::create_mid(TerrainQualityScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.xmm0.f32[0] = 0.8f * fLODDistanceMultiplier;                                            // vLow = 0.3, Low = 0.35, Med = 0.4, High = 0.5, vHigh = 0.8
                    ctx.rax = std::bit_cast<uint32_t>(std::min(4000.0f * fLODDistanceMultiplier, 10000.0f));    // vLow = 1000, Low = 1250, Med = 1500, High = 2000, vHigh = 4000 
                    ctx.rip = TerrainQualityMidHook.target_address() + 0x10;                                    // jmp over loading preset values
                });
        }
        else {
            SPDLOG_ERROR("LOD: Terrain Quality: Pattern scan failed.");
        }

        // Character Draw Distance
        std::uint8_t* CharacterDrawDistanceScanResult = Memory::PatternScan(exeModule, "83 ?? ?? ?? ?? ?? ?? FF 48 8B ?? ?? ?? ?? ?? 48 85 ?? 74 ?? F7 ?? ?? ?? ?? ?? ?? ?? ?? ?? 0F 85 ?? ?? ?? ??");
        if (CharacterDrawDistanceScanResult) { 
            SPDLOG_INFO("LOD: Character Draw Distance: Address is {:s}+{:x}", sExeName, CharacterDrawDistanceScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid CharacterDrawDistanceMidHook{};
            CharacterDrawDistanceMidHook = safetyhook::create_mid(CharacterDrawDistanceScanResult,
                [](SafetyHookContext& ctx) {
                    ctx.xmm8.f32[0] = std::min(5000.0f * fLODDistanceMultiplier, 20000.0f);  // 6000 default
                });
        }
        else {
            SPDLOG_ERROR("LOD: Character Draw Distance: Pattern scan failed.");
        }

        // Model Draw Distance
        std::uint8_t* ModelDrawDistanceFarScanResult = Memory::PatternScan(exeModule, "0F 28 ?? F3 0F ?? ?? ?? ?? ?? ?? 85 ?? 74 ?? 4D 85 ?? 74 ??");
        std::uint8_t* ModelDrawDistanceNearScanResult = Memory::PatternScan(exeModule, "?? 0F 28 ?? 85 ?? 74 ?? 4D 85 ?? 74 ?? 49 8B ?? E8 ?? ?? ?? ??");
        if (ModelDrawDistanceFarScanResult && ModelDrawDistanceNearScanResult) { 
            SPDLOG_INFO("LOD: Model Draw Distance: Far: Address is {:s}+{:x}", sExeName, ModelDrawDistanceFarScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid ModelDrawDistanceFarMidHook{};
            ModelDrawDistanceFarMidHook = safetyhook::create_mid(ModelDrawDistanceFarScanResult + 0x3,
                [](SafetyHookContext& ctx) {
                    if (fLODDistanceMultiplier != 1.0f)
                        ctx.xmm6.f32[0] = std::min(ctx.xmm6.f32[0] * fLODDistanceMultiplier, 20000.0f);
                });

            SPDLOG_INFO("LOD: Model Draw Distance: Near: Address is {:s}+{:x}", sExeName, ModelDrawDistanceNearScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid ModelDrawDistanceNearMidHook{};
            ModelDrawDistanceNearMidHook = safetyhook::create_mid(ModelDrawDistanceNearScanResult + 0x4,
                [](SafetyHookContext& ctx) {
                    if (fLODDistanceMultiplier != 1.0f)
                        ctx.xmm8.f32[0] = std::min(ctx.xmm8.f32[0] * fLODDistanceMultiplier, 20000.0f);
                });
        }
        else {
            SPDLOG_ERROR("LOD: Model Draw Distance: Pattern scan(s) failed.");
        }

        // Parts Draw Distance
        std::uint8_t* PartsDrawDistanceScanResult = Memory::PatternScan(exeModule, "74 ?? 38 ?? ?? ?? ?? ?? 0F 85 ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? 48 83 ?? ?? ?? C3");
        if (PartsDrawDistanceScanResult) { 
            SPDLOG_INFO("LOD: Parts Draw Distance: Address is {:s}+{:x}", sExeName, PartsDrawDistanceScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid PartsDrawDistanceMidHook{};
            PartsDrawDistanceMidHook = safetyhook::create_mid(PartsDrawDistanceScanResult + 0x2,
                [](SafetyHookContext& ctx) {
                    if (fLODDistanceMultiplier != 1.0f) {
                        *reinterpret_cast<BYTE*>(ctx.rax + 0x1754) = 1;
                        *reinterpret_cast<float*>(ctx.rax + 0x1750) = std::min(5000.0f * fLODDistanceMultiplier, 20000.0f);
                    }
                    else { 
                        *reinterpret_cast<BYTE*>(ctx.rax + 0x1754) = 0;
                        *reinterpret_cast<float*>(ctx.rax + 0x1750) = 0.0f;
                    }
                });
        }
        else {
            SPDLOG_ERROR("LOD: Parts Draw Distance: Pattern scan failed.");
        }

        // HLOD Draw Distance
        std::uint8_t* HLODDrawDistanceScanResult = Memory::PatternScan(exeModule, "0F 2F ?? ?? 76 ?? 4C 8B ?? 48 8D ?? ?? 49 8B ?? E8 ?? ?? ?? ?? 80 ?? 01");
        if (HLODDrawDistanceScanResult) { 
            SPDLOG_INFO("LOD: HLOD Draw Distance: Address is {:s}+{:x}", sExeName, HLODDrawDistanceScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid HLODDrawDistanceMidHook{};
            HLODDrawDistanceMidHook = safetyhook::create_mid(HLODDrawDistanceScanResult,
                [](SafetyHookContext& ctx) {
                    if (fLODDistanceMultiplier != 1.0f) {
                        float* dist = reinterpret_cast<float*>(ctx.rbp + 0x78); // 7500 default
                        *dist = std::min(*dist * fLODDistanceMultiplier, 20000.0f);
                    }
                });
        }
        else {
            SPDLOG_ERROR("LOD: HLOD Draw Distance: Pattern scan failed.");
        }

        // Motion Quality
        std::uint8_t* MotionQualityScanResult = Memory::PatternScan(exeModule, "BA 08 00 00 00 83 ?? 01 74 ?? 83 ?? 01 75 ??");
        if (MotionQualityScanResult) {
            SPDLOG_INFO("LOD: Motion Quality: Address is {:s}+{:x}", sExeName, MotionQualityScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            Memory::Write(MotionQualityScanResult + 0x1, static_cast<int>(8.0f * fLODDistanceMultiplier)); // 8 at "High Quality"
        }
        else {
            SPDLOG_ERROR("LOD: Motion Quality: Pattern scan failed.");
        }
    }
}

void Framerate()
{
    if (bMenuFramegen)
    {
        // Allow framegen in menus
        std::uint8_t* FramegenStatusScanResult = Memory::PatternScan(exeModule, "75 ?? E8 ?? ?? ?? ?? 41 ?? ?? 84 ?? 74 ?? 40 ?? ?? 49 8B ?? ?? ?? ?? ??");
        if (FramegenStatusScanResult) {
            SPDLOG_INFO("Framerate: Menu Framegen: Address is {:s}+{:x}", sExeName, FramegenStatusScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            Memory::PatchBytes(FramegenStatusScanResult, "\x90\x90", 2);
        }
        else {
            SPDLOG_ERROR("Framerate: Menu Framegen: Pattern scan failed.");
        }
    }

    if (bDynamicFPS)
    {
        // Frameskip
        std::uint8_t* FramerateFrameskipScanResult = Memory::PatternScan(exeModule, "F7 ?? ?? ?? ?? ?? ?? 0F 85 ?? ?? ?? ?? 4D 8B ?? ?? ?? ?? ?? 4D 85 ??");
        if (FramerateFrameskipScanResult) {
            SPDLOG_INFO("Framerate: Frameskip: Address is {:s}+{:x}", sExeName, FramerateFrameskipScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid FramerateFrameskipMidHook{};
            FramerateFrameskipMidHook = safetyhook::create_mid(FramerateFrameskipScanResult + 0x7,
                [](SafetyHookContext& ctx) {
                    if (!bFrameskip && *reinterpret_cast<int*>(FPSDynamicAdjustment) == 1)
                        ctx.rflags &= ~0x40; // Clear ZF to disable frameskip
                });
        }
        else {
            SPDLOG_ERROR("Framerate: Frameskip: Pattern scan failed.");
        } 

        // Dynamic Framerate Target
        std::uint8_t* DynamicFramerateSettingScanResult = Memory::PatternScan(exeModule, "44 39 ?? ?? ?? ?? ?? 75 ?? 45 89 ?? ?? E9 ?? ?? ?? ?? 45 38 ?? ?? 0F 85 ?? ?? ?? ??");
        std::uint8_t* DynamicFramerateTargetScanResult = Memory::PatternScan(exeModule, "45 89 ?? ?? 48 8B ?? ?? ?? ?? ?? 48 85 ?? 74 ?? 89 ?? ?? ?? ?? ?? 48 83 ?? ?? C3");
        if (DynamicFramerateSettingScanResult && DynamicFramerateTargetScanResult) {
            SPDLOG_INFO("Framerate: Dynamic FPS: Setting: Address is {:s}+{:x}", sExeName, DynamicFramerateSettingScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            FPSDynamicAdjustment = Memory::GetRelativeAddr(DynamicFramerateSettingScanResult + 0x3);

            SPDLOG_INFO("Framerate: Dynamic FPS: Target: Address is {:s}+{:x}", sExeName, DynamicFramerateTargetScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid DynamicFramerateTargetMidHook{};
            DynamicFramerateTargetMidHook = safetyhook::create_mid(DynamicFramerateTargetScanResult + 0x10,
                [](SafetyHookContext& ctx) {
                    float averagedFrametime = *reinterpret_cast<float*>(ctx.r8 + 0x74);
                    float averagedFPS = 1000.0f / averagedFrametime;
                    
                    int newTargetFPS = static_cast<int>(averagedFPS);
                    
                    if (newTargetFPS > 500) newTargetFPS = 500;
                    if (newTargetFPS < 30) newTargetFPS = 30;

                    if (iTargetFPS != 0)
                        newTargetFPS = iTargetFPS;

                    ctx.rax = static_cast<uintptr_t>(newTargetFPS);
                });
        }
        else {
            SPDLOG_ERROR("Framerate: Dynamic FPS: Pattern scan(s) failed.");
        } 

        // Menu Input
        std::uint8_t* FramerateMenuInputScanResult = Memory::PatternScan(exeModule, "48 8B ?? ?? 0F 28 ?? 48 8B ?? 48 ?? ?? 74 ?? 48 8B ?? ??");
        if (FramerateMenuInputScanResult) {
            SPDLOG_INFO("Framerate: Menu Input: Address is {:s}+{:x}", sExeName, FramerateMenuInputScanResult - reinterpret_cast<std::uint8_t*>(exeModule));
            static SafetyHookMid FramerateMenuInputMidHook{};
            FramerateMenuInputMidHook = safetyhook::create_mid(FramerateMenuInputScanResult,
                [](SafetyHookContext& ctx) {
                    // Fix repeat inputs being dropped
                    if (FPSDynamicAdjustment && *reinterpret_cast<int*>(FPSDynamicAdjustment) == 1) {
                        int fps = static_cast<int>(ctx.rax);
                        
                        if (fps > 0)
                            ctx.xmm0.f32[0] = std::round((60.0f / static_cast<float>(fps)) * 4.0f) / 4.0f; // Round to 0.25
                    }
                });
        }
        else {
            SPDLOG_ERROR("Framerate: Menu Input: Pattern scan failed.");
        }
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
    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) 
    {
    case DLL_PROCESS_ATTACH:
    {  
        DisableThreadLibraryCalls(hModule);
        thisModule = hModule;

        HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
        if (mainHandle)
        {
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