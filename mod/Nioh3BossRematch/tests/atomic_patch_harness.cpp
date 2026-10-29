// Execute the real hook patcher against private synthetic executable memory.
#define main legacy_rematch_main
#include "rematch_harness.cpp"
#undef main
int main() {
    auto p = (uint8_t*)VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!p) return 2;
    uint8_t bytes[16]; memset(bytes, 0x90, sizeof bytes);
    for (int offset = 0; offset < 16; ++offset) for (int len = 5; len <= 14; ++len) {
        memset(p, 0xCC, 64);
        bool ok = addon::Patch(p + 16 + offset, bytes, len);
        if (ok != (offset != 15)) return 3;
        for (int i = 0; i < 64; ++i) {
            uint8_t expected = ok && i >= 16 + offset && i < 16 + offset + len ? 0x90 : 0xCC;
            if (p[i] != expected) return 4;
        }
    }
    addon::g_img.base = (uintptr_t)p;
    addon::g_img.end = (uintptr_t)p + 4096;
    const uint8_t endBody[] = {0x40,0x53,0x48,0x81,0xEC,0x60,1,0,0,
        0x48,0x81,0xC4,0x60,1,0,0,0x5B,0xC3};
    memcpy(p + 0x170, endBody, sizeof endBody);
    addon::g_fakeSite[addon::SiteIndex("BossBattleModeEnd")] = (uintptr_t)p + 0x170;
    addon::OnEntry("BossBattleModeEnd", &H1);
    const char* why = nullptr;
    if (!addon::InstallGroup("FIGHT", &why)) return 5;
    ((void (*)(uint64_t,uint64_t,uint64_t,uint64_t))(p + 0x170))(1,2,3,4);
    VirtualFree(p, 0, MEM_RELEASE);
    puts("PASS: 150 real patches + 10 unsupported alignments refused; BossBattleModeEnd installed and executed; neighboring bytes preserved");
    return 0;
}
