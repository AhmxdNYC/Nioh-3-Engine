// Offline regression for site report #7 (KeepGear): the real hooks.cpp entry thunk + InstallVeto + ParseNative + SkipNative on a
// synthetic script-binding wrapper with the game's layout (mov rdx,rcx; lea rcx,[ctx]; call ctor; ...; movzx ebx,word [ctx+0x10C];
// lea rcx,[ctx+0x110]; call fin). Checks: veto off = the original body runs; veto on = body skipped, ctor/fin still run with the VM
// argument, the binding's return word is returned, an OnEntry handler on the same slot still runs, rbx/rsp survive.
// Build + run: tests\run_keepgear_veto.cmd (MSVC x64).
#include "../src/addon.h"
#include "../src/sites_gen.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
namespace addon {
Image g_img;
static uintptr_t g_fakeSite[sites::kCount];
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
bool SiteHookedByOther(int) { return false; }
bool GroupOk(const char*, const char**) { return true; }
}  // namespace addon
#include "../src/hooks.cpp"

static int g_ctorN, g_finN, g_bodyN, g_entryN;
static uint64_t g_ctorArg, g_ctorCtx, g_finCtx;
static bool g_veto;
extern "C" void FakeCtor(uint8_t* ctx, uint64_t vm) {
    ++g_ctorN;
    g_ctorArg = vm;
    g_ctorCtx = (uint64_t)ctx;
    const uint16_t r = 7;
    memcpy(ctx + 0x10C, &r, 2);
}
extern "C" void FakeFin(uint8_t* p) {
    ++g_finN;
    g_finCtx = (uint64_t)p;
}
extern "C" void FakeBody() { ++g_bodyN; }
static void Entry(uint64_t rcx, uint64_t, uint64_t, uint64_t, const uint64_t*) {
    if (rcx == 0x1234) ++g_entryN;
}
static bool Veto(int) { return g_veto; }

int main() {
    const size_t sz = 0x10000;
    auto* img = (uint8_t*)VirtualAlloc(nullptr, sz, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!img) return 2;
    memset(img, 0xCC, sz);
    addon::g_img.base = (uintptr_t)img;
    addon::g_img.end = (uintptr_t)img + sz;
    // stubs at +0x800: movabs rax, fn ; jmp rax
    auto stub = [&](size_t off, void* fn) {
        uint8_t* p = img + off;
        p[0] = 0x48; p[1] = 0xB8; memcpy(p + 2, &fn, 8); p[10] = 0xFF; p[11] = 0xE0;
        return p;
    };
    uint8_t* sCtor = stub(0x2000, (void*)&FakeCtor);
    uint8_t* sFin = stub(0x2020, (void*)&FakeFin);
    uint8_t* sBody = stub(0x2040, (void*)&FakeBody);
    uint8_t* w = img + 0x1000;  // the wrapper (Item::ReserveAllItemAll shape: 40 53 48 81 EC 60 01 00 00)
    uint8_t* q = w;
    auto put = [&](std::initializer_list<uint8_t> b) { for (uint8_t x : b) *q++ = x; };
    auto call = [&](uint8_t* to) { *q++ = 0xE8; int32_t r = (int32_t)(to - (q + 4)); memcpy(q, &r, 4); q += 4; };
    put({0x40, 0x53, 0x48, 0x81, 0xEC, 0x60, 0x01, 0x00, 0x00});   // push rbx; sub rsp,160h
    put({0x48, 0x8B, 0xD1});                                       // mov rdx,rcx
    put({0x48, 0x8D, 0x4C, 0x24, 0x20});                           // lea rcx,[rsp+20h]
    call(sCtor);
    call(sBody);
    put({0x0F, 0xB7, 0x9C, 0x24, 0x2C, 0x01, 0x00, 0x00});         // movzx ebx,word [rsp+12Ch]
    put({0x48, 0x8D, 0x8C, 0x24, 0x30, 0x01, 0x00, 0x00});         // lea rcx,[rsp+130h]
    call(sFin);
    put({0x0F, 0xB7, 0xC3, 0x48, 0x81, 0xC4, 0x60, 0x01, 0x00, 0x00, 0x5B, 0xC3});  // movzx eax,bx; add rsp,160h; pop rbx; ret
    const int si = addon::SiteIndex("Item::ReserveAllItemAll");
    if (si < 0) { puts("FAIL: no site"); return 3; }
    addon::g_fakeSite[si] = (uintptr_t)w;
    uint64_t c = 0, f = 0;
    if (!addon::ParseNative((uint64_t)w, &c, &f) || c != (uint64_t)sCtor || f != (uint64_t)sFin) { puts("FAIL: ParseNative"); return 4; }
    addon::OnEntry("Item::ReserveAllItemAll", &Entry);
    const char* why = nullptr;
    if (!addon::InstallVeto("Item::ReserveAllItemAll", &Veto, &why)) { printf("FAIL: InstallVeto %s\n", why ? why : "?"); return 5; }
    if (w[0] != 0xE9) { puts("FAIL: not hooked"); return 6; }
    auto fn = (uint64_t (*)(uint64_t))w;
    g_veto = false;
    uint64_t r = fn(0x1234);
    if (r != 7 || g_bodyN != 1 || g_ctorN != 1 || g_finN != 1 || g_entryN != 1 || g_ctorArg != 0x1234 || g_finCtx != g_ctorCtx + 0x110) {
        printf("FAIL: pass-through r=%llu body=%d ctor=%d fin=%d entry=%d\n", (unsigned long long)r, g_bodyN, g_ctorN, g_finN, g_entryN);
        return 7;
    }
    g_veto = true;
    r = fn(0x1234);
    if (r != 7 || g_bodyN != 1 || g_ctorN != 2 || g_finN != 2 || g_entryN != 2 || g_ctorArg != 0x1234 || g_finCtx != g_ctorCtx + 0x110) {
        printf("FAIL: veto r=%llu body=%d ctor=%d fin=%d entry=%d\n", (unsigned long long)r, g_bodyN, g_ctorN, g_finN, g_entryN);
        return 8;
    }
    g_veto = false;
    r = fn(0x1234);
    if (r != 7 || g_bodyN != 2) { puts("FAIL: pass-through after veto"); return 9; }
    // a layout that is not a binding wrapper: refused, no hook
    uint64_t c2 = 0, f2 = 0;
    if (addon::ParseNative((uint64_t)sCtor, &c2, &f2)) { puts("FAIL: parsed a non-wrapper"); return 10; }
    puts("PASS: keepgear veto - pass-through, skip (ctor/fin/return word kept, body skipped), entry handler shared, non-wrapper refused");
    return 0;
}
