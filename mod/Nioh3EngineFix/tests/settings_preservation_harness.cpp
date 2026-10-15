// Offline only: exercise the real menu translation unit with synthetic memory/files.
// Link the normal build objects except menu.obj; DllMain is never called for attach.
#include "../src/menu.cpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID);

static std::string ReadFileBytes(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) std::abort();
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

static void Require(bool ok, const char* why) {
    if (!ok) { fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

int wmain(int argc, wchar_t** argv) {
    Require(argc == 5, "arguments: fixture-dir wine fg mode");
    const std::filesystem::path root = argv[1];
    const auto ini = (root / L"Nioh3EngineFix.ini").wstring();
    const auto config = root / L"KoeiTecmo" / L"NIOH3" / L"config.xml";
    const std::string before = ReadFileBytes(config);
    // Any legacy path fallback is confined to the disposable fixture.
    Require(SetEnvironmentVariableW(L"LOCALAPPDATA", root.c_str()) != 0, "fixture environment");
    std::vector<uint8_t> memory(0x5300000, 0);
    *reinterpret_cast<int*>(memory.data() + 0x45BB008) = _wtoi(argv[3]);
    const auto snapshot = memory;
    menu::Host host;
    host.iniPath = ini.c_str();
    host.base = memory.data();
    host.knownBuild = true;
    host.wine = _wtoi(argv[2]) != 0;
    menu::Configure(host);
    Require(ReadFileBytes(config) == before, "startup preserved exact config bytes");
    Require(menu::ReadOnly(menu::kFg), "FG row is read-only");
    for (int dir : {-1, 0, 1}) menu::Change(menu::kFg, dir);
    Require(memory == snapshot, "FG input preserved game settings and dirty byte");
    Require(ReadFileBytes(config) == before, "FG input preserved exact config bytes");

    // Simulate a newer user edit after startup; exit/save must keep this version.
    const std::string edited = ReadFileBytes(root / L"edited.xml");
    { std::ofstream f(config, std::ios::binary | std::ios::trunc); f << edited;
      Require(bool(f), "write simulated user edit"); }
    menu::g_bootWorkerCap = 7;
    menu::StartSave();
    const ULONGLONG deadline = GetTickCount64() + 10000;
    while (menu::g_saveState == 1 && GetTickCount64() < deadline) Sleep(1);
    Require(menu::g_saveState == 2, "own-INI save completed");
    Require(GetPrivateProfileIntW(L"Patches", L"WorkerCap", 0, ini.c_str()) == 7,
            "own-INI choice persisted");
    Require(ReadFileBytes(config) == edited, "save preserved newer config bytes");
    Require(memory == snapshot, "save preserved game settings and dirty byte");
    Require(DllMain(nullptr, DLL_PROCESS_DETACH, reinterpret_cast<void*>(1)) != FALSE,
            "process exit detach");
    Require(ReadFileBytes(config) == edited, "exit preserved newer config bytes");
    Require(memory == snapshot, "exit preserved game settings and dirty byte");
    printf("PASS mode=%d wine=%d fg=%d: startup/input/save/exit + own INI\n",
           _wtoi(argv[4]), _wtoi(argv[2]), _wtoi(argv[3]));
    return 0;
}
