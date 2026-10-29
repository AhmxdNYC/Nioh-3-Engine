// Adversarial logical-generation witness, not a reachable-game failure test.
// Reuse relocation/mocks only; the existing synthetic/trace main is never run.
#define main cbvupload_existing_unused_main
#include "cbvupload_harness.cpp"
#undef main

struct Observation { unsigned calls = 0; uint32_t root = 0; uint64_t handle = 0; };
static Observation* g_observation = nullptr;
static void __stdcall ObserveTable(MockObj* self, uint32_t root, uint64_t handle) {
    CHECK(g_observation != nullptr, "missing binding observer");
    CHECK(self == *(MockObj**)(self->sim->c + 0x580), "binding through non-current list");
    if (g_observation) { ++g_observation->calls; g_observation->root = root; g_observation->handle = handle; }
}

static Observation Call(Sim& s, bool patched, Layout& layout) {
    Observation observation;
    g_observation = &observation; g_cur = &s; s.curStage = 0;
    int root = 3;
    uint64_t completed = *(uint64_t*)(s.c + 0x2510);
    if (patched) CbuTableCall(s.c, &root, 0, layout.obj, &completed);
    else ((cbvupload::UploadFn)g_stockCode.mem)(s.c, &root, 0, layout.obj, &completed);
    *(uint32_t*)(s.c + 0xB70) = 0; // native caller clears the stage mask
    g_observation = nullptr;
    CHECK(observation.calls == 1 && observation.root == 3, "wrong number/root of bindings");
    return observation;
}

static bool CurrentGeneration(Sim& s, const Observation& binding, const std::vector<uint8_t>& bytes) {
    if (binding.handle < s.svGpu || (binding.handle-s.svGpu)%kDescSz) return false;
    const uint64_t index = (binding.handle-s.svGpu)/kDescSz;
    if (index >= s.svCap) return false; // never dereference an old heap handle
    const uint8_t* descriptor = s.sv.data()+index*kDescSz;
    uint64_t va = 0, serial = 0; uint32_t size = 0;
    memcpy(&va, descriptor, 8); memcpy(&size, descriptor+8, 4); memcpy(&serial, descriptor+16, 8);
    uint64_t offset = 0;
    if (size < bytes.size() || !OffOf(s, va, bytes.size(), offset) || SafeOf(s, va, (uint32_t)bytes.size(), serial) != 1) return false;
    return memcmp((void*)va, bytes.data(), bytes.size()) == 0;
}

struct Key {
    uint8_t* context; uint64_t tag, list, staging; uint32_t size, n;
    bool operator==(const Key& other) const {
        return context == other.context && tag == other.tag && list == other.list && staging == other.staging && size == other.size && n == other.n;
    }
};
static Key GetKey(Sim& s) {
    return {s.c, s.Tag(), *(uint64_t*)(s.c+0x580), *(uint64_t*)(s.c+0x940),
            *(uint32_t*)(s.c+0xF30), *(uint32_t*)(s.c+0xB90)};
}
struct Case {
    const char* name; bool replaceCb, replaceHeap, advanceTag; int cbt;
    bool clearNativeStage = false;
    Sim stock, patched;
    std::vector<uint8_t*> retiredCb;
    std::vector<std::vector<uint8_t>> retiredHeaps;
};
static void Recreate(Case& c, Sim& s) {
    // Deliberately preserve the same raw SynCtx storage and key fields.
    // This does NOT emulate or assert the native constructor's initial masks.
    if (c.replaceCb) {
        memset(s.ring, 0xDE, (size_t)s.ringCap); // poison retired backing, retain allocation for safe observation
        c.retiredCb.push_back(s.ring);
        s.ring = (uint8_t*)VirtualAlloc(nullptr, (size_t)s.ringCap, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
        CHECK(s.ring != nullptr, "new CB backing allocation");
        if (!s.ring) return;
        memset(s.ring, 0xCD, (size_t)s.ringCap);
        s.head = 0; s.live.clear();
        s.granTag.assign(s.granTag.size(), 0); s.granSer.assign(s.granSer.size(), 0);
        uint64_t* ring = (uint64_t*)(s.c+0xB28); ring[2] = ring[3] = (uint64_t)s.ring;
    }
    if (c.replaceHeap) {
        c.retiredHeaps.push_back(std::move(s.sv)); // old heap stays alive, but is no longer the current heap
        s.sv.assign((size_t)s.svCap*kDescSz, 0);
        s.svGpu += 0x100000;
        s.stamps.assign(s.svCap, 0);
        *(uint64_t**)(s.c+0x9B0) = s.stamps.data();
        *(uint32_t*)(s.c+0x9C4) = 0; // DescRing cursor
        *(uint32_t*)(s.c+0x9CC) = 0; // DescRing wrap count
        *(uint64_t*)(s.c+0x9A0) = (uint64_t)s.sv.data();
        *(uint64_t*)(s.c+0x9A8) = s.svGpu;
    }
    if (c.advanceTag) ++s.Tag();
    if (c.clearNativeStage) {
        // Model only the proven initializer barrier, not a complete native construction.
        memset(s.c+0xF30, 0, 112*sizeof(uint32_t));
        memset(s.c+0xB70, 0, 8*sizeof(uint32_t));
        memset(s.c+0xB90, 0, 8*sizeof(uint32_t));
    }
}

int main(int argc, char** argv) {
    const bool requireSafe = argc > 2 && strcmp(argv[2], "--require-safe") == 0;
    FILE* file = nullptr;
    if (argc < 2 || fopen_s(&file, argv[1], "rb") || !file) { printf("usage: witness <dump> [--require-safe]\n"); return 2; }
    // Only the needed image prefix, not the whole native image/trace corpus.
    g_dump.resize(kDesc+kDescLen);
    const size_t read = fread(g_dump.data(), 1, g_dump.size(), file); fclose(file);
    CHECK(read == g_dump.size(), "short native image");
    CHECK(Fnv(kFn, kFnLen) == 0xBAE85B33u && Fnv(kDesc, kDescLen) == 0x2542B674u, "native functions differ from v2.02");
    if (g_fail) return 1;
    g_devVt[0x88/8] = (void*)&MockCreateCbv; g_devVt[0xC0/8] = (void*)&MockCopyDesc;
    g_listVt[0x100/8] = (void*)&ObserveTable;
    g_stockCode = Build((void*)&memcpy); g_patchCode = Build((void*)&CbuStreamCopy);
    Case cases[] = {
        {"same lifetime reuse control", false, false, false, 1},
        {"CBN backing recreation repeated key", true, false, false, 0},
        {"CBN backing recreation changed tag", true, false, true, 0},
        {"CBT heap recreation repeated key", false, true, false, 1},
        {"CBT heap recreation changed tag", false, true, true, 1},
        {"native fresh-size barrier control", true, true, false, 1, true},
    };
    Layout layout; layout.ent = {64, 0}; layout.n = 1; layout.Fill();
    std::vector<uint8_t> bytes(64); for (size_t i=0; i<bytes.size(); ++i) bytes[i]=(uint8_t)(i*3+7);
    unsigned counterexamples = 0;
    for (Case& c : cases) {
        InitSim(c.stock, "stock", 64ull<<10, 64); InitSim(c.patched, "patched", 64ull<<10, 64);
        cbvupload::TestSetup((cbvupload::UploadFn)g_patchCode.mem, 0, 1, c.cbt, 0);
        // No cbvupload::TestReset: all context allocations remain unique/alive
        // until every case finishes, and each modeled recreation retains its cache.
        WriteSlot(c.stock, 0, 0, bytes.data(), 64); WriteSlot(c.patched, 0, 0, bytes.data(), 64);
        CHECK(CurrentGeneration(c.stock, Call(c.stock, false, layout), bytes), "stock seed generation");
        CHECK(CurrentGeneration(c.patched, Call(c.patched, true, layout), bytes), "patched seed generation");
        const Key before = GetKey(c.patched);
        const auto countBefore = cbvupload::TestGet();
        Recreate(c, c.stock); Recreate(c, c.patched);
        Key after = GetKey(c.patched); if (c.advanceTag) { CHECK(after.tag == before.tag+1, "tag control"); after.tag = before.tag; }
        if (c.clearNativeStage) {
            CHECK(after.size == 0 && after.n == 0, "native fresh-stage barrier");
            after.size = before.size; after.n = before.n;
        }
        CHECK(before == after, "recreation changed other key fields");
        WriteSlot(c.stock, 0, 0, bytes.data(), 64); WriteSlot(c.patched, 0, 0, bytes.data(), 64);
        const bool stockCurrent = CurrentGeneration(c.stock, Call(c.stock, false, layout), bytes);
        const Observation bound = Call(c.patched, true, layout);
        const bool patchedCurrent = CurrentGeneration(c.patched, bound, bytes);
        const auto countAfter = cbvupload::TestGet();
        CHECK(stockCurrent, "stock did not bind current-generation bytes");
        const bool expectCounterexample = (c.replaceCb || c.replaceHeap) && !c.advanceTag && !c.clearNativeStage;
        CHECK(patchedCurrent != expectCounterexample, "modeled result changed; inspect identity contract before updating witness");
        const bool forcedMiss = c.advanceTag || c.clearNativeStage;
        CHECK((countAfter.skipped > countBefore.skipped) != forcedMiss, "skip control did not reflect identity barrier");
        if (c.cbt) CHECK((countAfter.tblHit > countBefore.tblHit) != forcedMiss, "table-hit control did not reflect identity barrier");
        if (expectCounterexample && !patchedCurrent) ++counterexamples;
        printf("%s: stock_current=%d patched_current=%d skipped=%llu table_hits=%llu%s\n", c.name, stockCurrent, patchedCurrent,
            (unsigned long long)(countAfter.skipped-countBefore.skipped), (unsigned long long)(countAfter.tblHit-countBefore.tblHit),
            expectCounterexample ? " MODELED COUNTEREXAMPLE" : " CONTROL PASS");
    }
    for (Case& c : cases) { FreeSim(c.stock); FreeSim(c.patched); for (uint8_t* p : c.retiredCb) VirtualFree(p, 0, MEM_RELEASE); }
    if (g_fail || counterexamples != 2) { printf("WITNESS INVALID: %d checks, %u counterexamples\n", g_fail, counterexamples); return 1; }
    printf("REPRODUCED 2 MODELED IDENTITY COUNTEREXAMPLES; 4 CONTROLS PASS. Native reachability unproven.\n");
    return requireSafe ? 3 : 0;
}
