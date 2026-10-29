// Offline regression: execute the actual patch writer on private pages only.
#include "../src/addon.h"
#include "../src/sites_gen.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
// ---- stubs for hooks.cpp ----
namespace addon {
Image g_img;
static uintptr_t g_fakeSite[sites::kCount];
static bool g_fakeOther[sites::kCount];
void Log(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    vprintf(fmt, a);
    va_end(a);
    printf("\n");
}
bool Copy(void* d, uint64_t s, size_t n) {
    memcpy(d, (const void*)s, n);
    return true;
}
int SiteIndex(const char* name) {
    for (int i = 0; i < sites::kCount; ++i)
        if (!strcmp(sites::kDefs[i].name, name)) return i;
    return -1;
}
uintptr_t Site(const char* name) {
    const int i = SiteIndex(name);
    return i < 0 ? 0 : g_fakeSite[i];
}
bool SiteHookedByOther(int i) { return g_fakeOther[i]; }
bool GroupOk(const char* group, const char** why) {
    for (int i = 0; i < sites::kCount; ++i)
        if (!strcmp(sites::kDefs[i].group, group) && !g_fakeSite[i] && sites::kDefs[i].kind != sites::kNative && sites::kDefs[i].kind != sites::kHook) {
            if (why) *why = "unresolved";
        }
    return true;
}
}  // namespace addon
#include "../src/hooks.cpp"


int main() {
    auto* page = (uint8_t*)VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!page) return 2;
    const uint8_t patch[16] = {0xE9,1,2,3,4,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90};
    unsigned cases = 0;
    for (size_t offset = 0; offset < 15; ++offset) {
        for (size_t len = 2; len <= 16; ++len) {
            memset(page, 0xCC, 64);
            DWORD old;
            VirtualProtect(page, 4096, PAGE_EXECUTE_READ, &old);
            if (!addon::Patch(page + 16 + offset, patch, len)) { printf("FAIL offset=%zu len=%zu error=%lu\n", offset,len,GetLastError()); return 3; }
            for (size_t i = 0; i < 64; ++i) {
                const uint8_t want = i >= 16 + offset && i < 16 + offset + len ? patch[i-16-offset] : 0xCC;
                if (page[i] != want) return 4;
            }
            MEMORY_BASIC_INFORMATION mbi;
            VirtualQuery(page, &mbi, sizeof(mbi));
            if (mbi.Protect != PAGE_EXECUTE_READ) return 5;
            VirtualProtect(page, 4096, PAGE_READWRITE, &old);
            ++cases;
        }
    }
    VirtualFree(page, 0, MEM_RELEASE);
    printf("PASS %u patch placements: exact bytes, neighbors intact, RX protection restored\n", cases);
}
