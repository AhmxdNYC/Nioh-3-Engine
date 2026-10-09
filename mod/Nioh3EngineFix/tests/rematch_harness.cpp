// rematch_harness.cpp - offline tests of src/rematch.cpp (boss rematch, ASI 1.7.40). Not a game test.
// Built and run by tests\run_rematch_harness.py. Prints "ALL PASSED" on success.
// The module is included as source, so its internal (anonymous-namespace) functions are reachable. A fake "image" (one RWX
// region the size of Nioh3.exe) holds the globals the module reads, tiny stub functions at the call targets and a fake
// MissionLoadStep at its real RVA for the thunk-chaining test.
#include "../src/rematch.cpp"

#include <cstdio>

using namespace rematch;

static int g_fails = 0;
#define CHECK(c, ...)                         \
    do {                                      \
        if (!(c)) {                           \
            ++g_fails;                        \
            printf("FAIL %d: ", __LINE__);    \
            printf(__VA_ARGS__);              \
            printf("\n");                     \
        }                                     \
    } while (0)

static void Log(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    printf("  log: ");
    vprintf(fmt, a);
    printf("\n");
    va_end(a);
}

static uint8_t* B = nullptr;  // fake image base
constexpr size_t kImg = 0x5000000;
template <class T>
static T& At(uint64_t off) { return *(T*)(B + off); }
static void Put(uint64_t off, std::initializer_list<uint8_t> b) {
    uint8_t* p = B + off;
    for (uint8_t x : b) *p++ = x;
}
static void PutAbs(uint64_t off, const void* p) { memcpy(B + off + 2, &p, 8); }  // mov rax, imm64 operand at +2

// ---- fake game state ----
static volatile int g_fakeReason = 0;
static int FakePause() { return g_fakeReason; }
static volatile uint8_t g_busyFlag = 0;
static struct { uint64_t rcx; uint32_t edx; uint8_t r8b; } volatile g_openArgs;
static volatile LONG g_openCalls = 0;
// MissionRequestById stand-in (R-b): records the arguments and, like 2239600 -> 223953C offline, writes the request into the first
// free queue slot (u32 +4 == 0 && u8 +8 == 0); g_byIdTake = false models a call that enqueues nothing.
static struct {
    LONG calls;
    uint32_t id, sub;
    uint8_t slot;
    bool aligned;
    uint32_t p[6];
} g_byId;
static bool g_byIdTake = true;
static void FakeById(uint32_t id, uint8_t slot, uint32_t sub, const uint32_t* p) {
    ++g_byId.calls;
    g_byId.id = id;
    g_byId.slot = slot;
    g_byId.sub = sub;
    g_byId.aligned = ((uintptr_t)p & 15) == 0;
    memcpy(g_byId.p, p, sizeof(g_byId.p));
    if (!g_byIdTake) return;
    for (int i = 0; i < 3; ++i) {
        uint8_t* q = B + kRvaQueue + i * 0x58;
        if (*(uint32_t*)(q + 4) != 0 || q[8] != 0) continue;
        memset(q, 0, 0x58);
        q[0] = slot;
        *(uint32_t*)(q + 4) = id;
        q[8] = 0x96;                    // record +0x15C
        *(uint32_t*)(q + 0xC) = 72636;  // 1C17D8C(id)
        *(uint64_t*)(q + 0x14) = 2;
        *(uint32_t*)(q + 0x24) = 1;
        *(uint16_t*)(q + 0x28) = 1;
        *(uint64_t*)(q + 0x30) = ~0ull;
        memcpy(q + 0x40, p, 0x18);
        return;
    }
}

constexpr uint64_t oG = 0x4800000, oPT = 0x4801000, oPl = 0x4802000, oMs0 = 0x4803000, oMs = 0x4804000, oCh0 = 0x4805000, oCh = 0x4806000,
                   oDb = 0x4807000, oDm = 0x4808000, oTab = 0x4810000, oMm = 0x4830000, oMenu = 0x4831000, oVt = 0x4832000, oBusy = 0x4833000;
static uint64_t A(uint64_t off) { return (uint64_t)(B + off); }

static void BuildWorld() {
    At<uint64_t>(kRvaGameMain) = A(oG);
    At<uint64_t>(kRvaPlayerTab) = A(oPT);
    At<uint64_t>(oPT + 0xCD * 0x18) = A(oPl);
    At<uint64_t>(kRvaMissionState) = A(oMs0);
    At<uint64_t>(oMs0 + 8) = A(oMs);
    At<uint32_t>(oMs + 0x14) = 0x500;  // field "mission"
    At<uint64_t>(kRvaChMgr) = A(oCh0);
    At<uint64_t>(oCh0) = A(oCh);
    At<uint64_t>(kRvaClearDb) = A(oDb);
    At<uint64_t>(kRvaDataMgr) = A(oDm);
    At<uint64_t>(oDm) = A(oTab);
    At<uint32_t>(oTab + 4) = 130;
    for (uint32_t i = 0; i < 130; ++i) {
        const uint64_t r = oTab + 8 + (uint64_t)i * kRecSize;
        At<uint16_t>(r + kRecId) = (uint16_t)(0x1000 + i);
        At<uint32_t>(r + kRecFlags) = 0x20002;
    }
    for (int k = 0; k < 3; ++k) {  // the three hidden duels at rows 113, 114, 116
        const uint64_t r = oTab + 8 + (uint64_t)(113 + k + (k == 2)) * kRecSize;
        At<uint16_t>(r + kRecId) = kHiddenIds[k];
        At<uint32_t>(r + kRecFlags) = 0x24180;
    }
    At<uint64_t>(kRvaMenuMgr) = A(oMm);
    At<uint64_t>(oMm + kOffReplayMenu) = A(oMenu);
    At<uint64_t>(oMenu) = A(oVt);
    At<uint64_t>(oVt + 0x48) = A(oBusy);
    Put(oBusy, {0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0x8A, 0x00, 0xC3});  // mov rax,&g_busyFlag; mov al,[rax]; ret
    PutAbs(oBusy, (const void*)&g_busyFlag);
    // MenuOpenMissionReplay stub: records rcx / edx / r8b, counts calls
    Put(kRvaOpenReplay, {0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0x48, 0x89, 0x08, 0x89, 0x50, 0x08, 0x44, 0x88, 0x40, 0x0C,  // mov [rax],rcx ...
                         0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xF0, 0xFF, 0x00, 0xC3});                               // lock inc dword [rax]; ret
    PutAbs(kRvaOpenReplay, (const void*)&g_openArgs);
    PutAbs(kRvaOpenReplay + 20, (const void*)&g_openCalls);
    Put(kRvaRequestById, {0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xE0});  // mov rax, FakeById; jmp rax
    PutAbs(kRvaRequestById, (const void*)&FakeById);
    At<uint8_t>(oMs + kOffMsSlot) = 1;  // journey / slot byte (L4 lvl=1 in the 2026-10-08 session)
    Put(kRvaClearedGet, {0x89, 0xD0, 0x83, 0xE0, 0x01, 0xC3});  // cleared = odd mission id
    Put(kRvaUnlockedGet, {0xB0, 0x01, 0xC3});
}
static void SetSt(int i, uint8_t v) { At<uint8_t>(oG + 0x588 + i) = v; }

// ---- thunk chaining ----
static int g_seq = 0, g_oursAt = 0, g_otherAt = 0, g_oursN = 0, g_otherN = 0;
static uint64_t g_oursArgs[2], g_otherArgs[2];
static void OursHook(uint64_t a, uint64_t b, uint64_t, uint64_t, const uint64_t*) {
    g_oursAt = ++g_seq;
    ++g_oursN;
    g_oursArgs[0] = a;
    g_oursArgs[1] = b;
}
static void OtherHook(uint64_t a, uint64_t b, uint64_t, uint64_t, const uint64_t*) {
    g_otherAt = ++g_seq;
    ++g_otherN;
    g_otherArgs[0] = a;
    g_otherArgs[1] = b;
}

static void TestThunks() {
    printf("[thunks]\n");
    const Site& ml = kSites[0];
    CHECK(ml.rva == 0x223AF50 && ml.head == 10, "kSites[0] is MissionLoadStep");
    memcpy(B + ml.rva, ml.b, ml.len);  // prologue: 2 spills, push rsi/rdi/r14, sub rsp,0B0h
    Put(ml.rva + ml.len, {0x8D, 0x04, 0x11, 0x48, 0x81, 0xC4, 0xB0, 0x00, 0x00, 0x00, 0x41, 0x5E, 0x5F, 0x5E, 0xC3});  // lea eax,[rcx+rdx]; epilogue
    auto f = (int (*)(int, int))(B + ml.rva);
    CHECK(f(3, 4) == 7, "fake function itself");
    uint64_t prev = 0;
    CHECK(Probe(ml, &prev) == 0, "fresh site probes as original bytes");
    g_thunks = AllocNear((uintptr_t)B, (uintptr_t)B + kImg);
    CHECK(g_thunks != nullptr, "thunk page near the fake image");
    Site other = ml;  // stands in for bosslog.cpp's hook on the same entry (same head length, fresh install)
    other.fn = &OtherHook;
    CHECK(Install(other, 0, 0) == 0, "first (bosslog-like) hook installs");
    CHECK(B[ml.rva] == 0xE9 && B[ml.rva + 5] == 0x90 && B[ml.rva + 9] == 0x90, "first hook = E9 rel32 + NOPs over head");
    CHECK(f(10, 20) == 30 && g_otherN == 1 && g_otherArgs[0] == 10 && g_otherArgs[1] == 20, "first hook runs and passes the args");
    CHECK(Probe(ml, &prev) == 1, "hooked site probes as chained");
    Site ours = ml;
    ours.fn = &OursHook;
    CHECK(Install(ours, 1, prev) == 0, "chained install");
    g_seq = 0;
    CHECK(f(5, 6) == 11, "function result through both thunks");
    CHECK(g_oursN == 1 && g_otherN == 2, "both hooks ran once (ours %d, other %d)", g_oursN, g_otherN);
    CHECK(g_oursAt == 1 && g_otherAt == 2, "ours first, then the earlier hook");
    CHECK(g_oursArgs[0] == 5 && g_oursArgs[1] == 6 && g_otherArgs[0] == 5 && g_otherArgs[1] == 6, "args intact through the chain");
    CHECK(B[ml.rva + 5] == 0x90 && B[ml.rva + 9] == 0x90, "chained install changed only the rel32");
    uint8_t save = B[ml.rva + 12];
    B[ml.rva + 12] ^= 0xFF;
    CHECK(Probe(ml, &prev) == -1, "tail mismatch is refused");
    B[ml.rva + 12] = save;
    save = B[ml.rva + 7];
    B[ml.rva + 7] = 0xCC;  // E9 rel32 but not followed by NOPs up to head (a different hook length) -> refused
    CHECK(Probe(ml, &prev) == -1, "foreign hook shape is refused");
    B[ml.rva + 7] = save;
    // a check-only site with head 0 must match exactly
    CHECK(Probe(kSites[3], &prev) == -1, "MenuOpenMissionReplay stub != real bytes -> refused");
}

// ---- catalogue ----
static void TestCatalogue(const wchar_t* tsv) {
    printf("[catalogue]\n");
    g_rows.clear();
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring p = std::wstring(tmp) + L"rematch_harness.tsv";
    const char* t =
        "# comment\r\nid\tname\tmission_name\tregion\troute\treplay_mission_id\thidden\tverified\r\n"
        "R1\tBoss One\tMission One\tSengoku\tR\t0x1CBC\t0\t1\r\n"
        "E1\tField Boss\t-\tHeian\tE1\t0x0\t0\t0\r\n"
        "R2\tDuel\tMission Two\tHeian\tR\t45613\t1\t0\r\n"
        "R3\tNo id\tx\ty\tR\t-\t0\t1\r\n";
    HANDLE h = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    DWORD w = 0;
    WriteFile(h, t, (DWORD)strlen(t), &w, nullptr);
    CloseHandle(h);
    CHECK(LoadCatalogue(p) == 2, "2 route-R rows with an id");
    CHECK(g_rows.size() == 2 && g_rows[0].missionId == 0x1CBC && !g_rows[0].hidden && g_rows[0].verified, "row 1 fields");
    CHECK(g_rows.size() == 2 && g_rows[1].missionId == 45613 && g_rows[1].hidden && !g_rows[1].verified, "row 2: decimal id, hidden, unverified");
    DeleteFileW(p.c_str());
    g_rows.clear();
    const int n = LoadCatalogue(tsv);
    printf("  real catalogue: %d rows\n", n);
    CHECK(n >= 50, "real catalogue loads (%d rows)", n);
    int hid = 0;
    for (const Row& r : g_rows) hid += r.hidden;
    CHECK(hid == 3, "3 hidden duels in the real catalogue (%d)", hid);
}

// ---- gates, request, R-a, un-hide, lifecycle, scan, page ----
static void Wait(DWORD ms) {
    const ULONGLONG t0 = GetTickCount64();
    while (GetTickCount64() - t0 < ms) {
        Tick();
        Sleep(20);
    }
}
static uint32_t RecFlags(int k) { return At<uint32_t>(oTab + 8 + (uint64_t)(113 + k + (k == 2)) * kRecSize + kRecFlags); }

static void TestFlow() {
    printf("[gates]\n");
    g_live = true;
    g_pauseReason = &FakePause;
    Tick();
    Snap s = ReadSnap();
    CHECK(Gates(s, true) == nullptr, "all gates hold in the fake field (got %s)", Gates(s, true));
    struct { const char* want; void (*set)(bool); } cases[] = {
        {"loading", [](bool on) { SetSt(2, on); }},
        {"game paused", [](bool on) { SetSt(3, on); }},
        {"no save loaded", [](bool on) { At<uint64_t>(oPT + 0xCD * 0x18) = on ? 0 : A(oPl); }},
        {"in a Picture Scroll", [](bool on) { At<uint8_t>(oCh + 0x2C18) = on ? 1 : 0; }},
        {"in a boss fight", [](bool on) { g_inFight.store(on); }},
        {"game menu open", [](bool on) { g_fakeReason = on ? 1 : 0; }},
        {"online", [](bool on) {
             static uint8_t pm[0x40] = {};
             *(int32_t*)(pm + 0x34) = 2;
             At<uint64_t>(kRvaPm) = on ? (uint64_t)pm : 0;
         }},
        {"co-op session", [](bool on) {
             static uint8_t net[0x200] = {};
             net[0x1DC] = 1;
             At<uint64_t>(kRvaNet) = on ? (uint64_t)net : 0;
         }},
    };
    for (auto& c : cases) {
        c.set(true);
        s = ReadSnap();
        const char* g = Gates(s, false);
        CHECK(g && !strcmp(g, c.want), "gate '%s' (got %s)", c.want, g ? g : "none");
        c.set(false);
    }
    s = ReadSnap();
    CHECK(Gates(s, true) == nullptr, "gates hold again");
    At<uint8_t>(kRvaQueue + 8) = 0xE0;  // a consumed slot keeps its bytes: only a CHANGE counts as busy
    Tick();
    s = ReadSnap();
    CHECK(Gates(s, false) && !strcmp(Gates(s, false), "mission queue busy"), "queue change -> busy");
    g_qChangedAt.store(GetTickCount64() - kQueueQuietMs - 10);
    Tick();
    s = ReadSnap();
    CHECK(Gates(s, true) == nullptr, "a stale (unchanged) slot is not busy");

    printf("[scan + page]\n");
    PageOpen();
    CHECK(PageActive(), "page open");
    PageRow rows[90];
    int k = PageFill(rows, 90);
    CHECK(k >= 2 && strstr(rows[k - 1].label, "scanning"), "scanning note before the game thread ran");
    Tick();  // DoScan on the "game thread"
    int beaten = 0;
    for (size_t i = 0; i < g_rows.size(); ++i) beaten += g_status[i].load() == 2;
    int odd = 0;
    for (const Row& r : g_rows) odd += (r.missionId & 1) && !(r.hidden && !g_cfg.hiddenDuels);
    CHECK(beaten > 0, "scan marked rows beaten (%d)", beaten);
    k = PageFill(rows, 90);
    int listed = 0;
    for (int i = 0; i < k; ++i)
        if (strstr(rows[i].value, "beaten")) ++listed;
    CHECK(listed == odd, "page lists exactly the cleared rows (%d listed, %d expected)", listed, odd);
    CHECK(rows[1].sel && strstr(rows[1].label, "Battle Scroll replay menu"), "first selectable row = open the replay menu");

    printf("[R-a from a boss row, with HiddenDuels]\n");
    g_cfg.hiddenDuels = true;
    PageMove(+1);  // first boss row
    CHECK(!PageAct(0), "first A only shows the confirm line");
    k = PageFill(rows, 90);
    bool conf = false;
    for (int i = 0; i < k; ++i) conf |= rows[i].sel && strstr(rows[i].value, "A = confirm") != nullptr;
    CHECK(conf, "confirm line on the selected row");
    CHECK(PageAct(0), "second A queues the request (menu closes)");
    PageClose();
    const LONG calls0 = g_openCalls;
    Tick();
    CHECK(g_openCalls == calls0 + 1, "MenuOpenMissionReplay called once");
    CHECK(g_openArgs.rcx == 0 && g_openArgs.edx == 0 && g_openArgs.r8b == 6, "called with (0, 0, 6)");
    CHECK(StateCode() == kMenuOpen, "state: replay menu open");
    CHECK(RecFlags(0) == 0x20180 && RecFlags(1) == 0x20180 && RecFlags(2) == 0x20180, "3 hidden duels un-hidden (bit 0x4000 cleared)");
    char tb[160];
    uint32_t tc;
    CHECK(Toast(tb, sizeof(tb), tc) && strstr(tb, "pick '"), "toast names the mission to pick");
    // a second request while active is refused by the gates (after the 5 s grace)
    g_fakeReason = 1;  // the replay menu is up
    Wait(300);
    g_fakeReason = 0;  // closed, no start
    Wait(5600);
    CHECK(StateCode() == kIdle, "closed without a start -> idle");
    CHECK(RecFlags(0) == 0x24180 && RecFlags(1) == 0x24180 && RecFlags(2) == 0x24180, "hidden-duel flags restored");

    printf("[R-a, replay start, mission, exit]\n");
    RemoteRequest(1);
    Tick();
    CHECK(StateCode() == kMenuOpen && RecFlags(0) == 0x20180, "remote R-a opens + un-hides");
    g_busyFlag = 1;  // +0x48 says busy (menu up) while the game signal stays 0
    Wait(200);
    g_busyFlag = 0;
    At<uint32_t>(kRvaQueue + 0x58 + 0xC) = 0x3D5538;  // the replay menu enqueues into a free slot
    Tick();
    CHECK(StateCode() == kInReplay, "queue change -> in replay");
    SetSt(2, 1);
    Wait(100);
    At<uint32_t>(oMs + 0x14) = 0xB3F8;
    SetSt(2, 0);
    Wait(3400);
    CHECK(g_replayIn.load() && g_replayMs == 0xB3F8, "replay mission recognised after its load");
    RemoteRequest(2);
    Tick();
    CHECK(StateCode() == kInReplay && RecFlags(1) == 0x20180, "abort refused inside the replay mission");
    // death + retry reload: the same id comes back, no exit
    SetSt(2, 1);
    Wait(1000);
    SetSt(2, 0);
    Wait(1500);
    CHECK(StateCode() == kInReplay, "a short reload inside the mission is not an exit");
    At<uint32_t>(oMs + 0x14) = 0x500;  // back in the field
    Wait(5500);
    CHECK(StateCode() == kIdle && RecFlags(1) == 0x24180, "mission exit -> idle + restored");

    printf("[refusals]\n");
    g_qChangedAt.store(GetTickCount64() - kQueueQuietMs - 10);
    g_fightAt.store(GetTickCount64());
    g_inFight.store(true);
    RemoteRequest(1);
    const LONG c1 = g_openCalls;
    Wait(5300);
    CHECK(g_openCalls == c1 && StateCode() == kIdle, "in a fight: refused after the 5 s grace, nothing called");
    CHECK(Toast(tb, sizeof(tb), tc) && strstr(tb, "boss fight"), "refusal toast says why (%s)", tb);
    g_inFight.store(false);
    g_busyFlag = 1;
    RemoteRequest(1);
    Tick();
    CHECK(g_openCalls == c1 && StateCode() == kIdle, "replay menu busy (+0x48): refused, not called");
    g_busyFlag = 0;

    printf("[R-b direct start]\n");
    g_cfg.directStart = true;
    int rOdd = -1, rEven = -1, rHid = -1;
    for (size_t i = 0; i < g_rows.size(); ++i) {
        const Row& r = g_rows[i];
        if (r.hidden) {
            if (rHid < 0) rHid = (int)i;
            continue;
        }
        if (!r.verified) continue;
        if ((r.missionId & 1) && rOdd < 0) rOdd = (int)i;
        if (!(r.missionId & 1) && rEven < 0) rEven = (int)i;
    }
    CHECK(rOdd >= 0 && rEven >= 0 && rHid >= 0, "catalogue has rows for the R-b cases (%d %d %d)", rOdd, rEven, rHid);
    const ULONGLONG quiet = GetTickCount64() - kQueueQuietMs - 10;
    Post(kReqDirect, rEven, "harness");  // fake clear getter: even id = not cleared
    Tick();
    CHECK(g_byId.calls == 0 && StateCode() == kIdle, "R-b: an uncleared mission is refused, nothing called");
    CHECK(Toast(tb, sizeof(tb), tc) && strstr(tb, "not cleared"), "R-b refusal says not cleared (%s)", tb);
    Post(kReqDirect, rHid, "harness");
    Tick();
    CHECK(g_byId.calls == 0 && Toast(tb, sizeof(tb), tc) && strstr(tb, "unverified"), "R-b: an unverified hidden duel is refused (%s)", tb);
    for (int i = 0; i < 3; ++i) At<uint8_t>(kRvaQueue + i * 0x58 + 8) = 0xE0;  // all three slots taken
    Tick();
    g_qChangedAt.store(quiet);
    Post(kReqDirect, rOdd, "harness");
    Tick();
    CHECK(g_byId.calls == 0 && Toast(tb, sizeof(tb), tc) && strstr(tb, "queue full"), "R-b: full queue refused before the call (%s)", tb);
    memset(B + kRvaQueue, 0, 3 * 0x58);
    At<uint8_t>(kRvaQueue + 8) = 0xE0;  // slot 0 holds the load's request (as in the session), 1 and 2 free
    Tick();
    g_qChangedAt.store(quiet);
    g_byIdTake = false;
    Post(kReqDirect, rOdd, "harness");
    Tick();
    CHECK(g_byId.calls == 1 && StateCode() == kIdle && Toast(tb, sizeof(tb), tc) && strstr(tb, "did not take"),
          "R-b: a call that enqueues nothing is refused and stays idle (%s)", tb);
    g_byIdTake = true;
    Post(kReqDirect, rOdd, "harness");
    Tick();
    const uint16_t mid = g_rows[(size_t)rOdd].missionId;
    CHECK(g_byId.calls == 2 && g_byId.id == mid && g_byId.slot == 1 && g_byId.sub == 0, "MissionRequestById(id 0x%X, slot byte %u, sub %u)",
          g_byId.id, g_byId.slot, g_byId.sub);
    CHECK(g_byId.aligned, "params 16-byte aligned (movaps [r9] in 2239660)");
    CHECK(g_byId.p[0] == 0x101 && g_byId.p[1] == 0x1000000 && g_byId.p[2] == 0x10100 && !g_byId.p[3] && !g_byId.p[4] && !g_byId.p[5],
          "params = the replay confirm's {101, 1000000, 10100, 0, 0, 0}");
    CHECK(At<uint32_t>(kRvaQueue + 0x58 + 4) == mid && At<uint64_t>(kRvaQueue + 0x58 + kReqMode) == 2, "request in slot 1 with mode 2");
    CHECK(StateCode() == kInReplay && g_openCalls == c1, "R-b: in replay, the replay menu was not opened");
    CHECK(Toast(tb, sizeof(tb), tc) && strstr(tb, "starting '"), "R-b toast names the mission (%s)", tb);
    SetSt(2, 1);
    Wait(100);
    At<uint32_t>(oMs + 0x14) = 72636;
    SetSt(2, 0);
    Wait(3400);
    CHECK(g_replayIn.load() && g_replayMs == 72636, "R-b: replay mission recognised after its load");
    At<uint32_t>(oMs + 0x14) = 0x500;
    Wait(5500);
    CHECK(StateCode() == kIdle, "R-b: mission exit -> idle");
    g_qChangedAt.store(GetTickCount64() - kQueueQuietMs - 10);
    g_cfg.directStart = false;
    // abort before the start
    RemoteRequest(1);
    Tick();
    CHECK(StateCode() == kMenuOpen && RecFlags(2) == 0x20180, "opened again");
    RemoteRequest(2);
    Tick();
    CHECK(StateCode() == kIdle && RecFlags(2) == 0x24180, "abort before the start restores at once");
    // the replay menu never shows: back to idle after 6 s
    RemoteRequest(1);
    Tick();
    Wait(6300);
    CHECK(StateCode() == kIdle && RecFlags(0) == 0x24180, "menu never seen -> idle + restored");
}

int wmain(int argc, wchar_t** argv) {
    g_log = &Log;
    B = (uint8_t*)VirtualAlloc(nullptr, kImg, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!B) {
        printf("no fake image\n");
        return 2;
    }
    g_base = (uintptr_t)B;
    g_imgEnd = g_base + kImg;
    BuildWorld();
    TestThunks();
    TestCatalogue(argc > 1 ? argv[1] : L"Nioh3EngineFix_bosses.tsv");
    TestFlow();
    printf(g_fails ? "%d FAILED\n" : "ALL PASSED\n", g_fails);
    return g_fails ? 1 : 0;
}
