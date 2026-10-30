// Nioh3EngineFix - native packaging of the Nioh 3 (Steam v2.02) engine patches:
//   W1/W2 (write-combined memory RMW fixes), VRR (variable-dt frame timing v2 + P9), W3 (message pump wait),
//   R1 (render-begin wait), L1 (high-resolution timer frame limiter).
// Every site is located by an AOB pattern from patches.json (via the generated patches_gen.h), its original bytes
// are verified, and all writes of a group happen with every other thread of the process suspended and no thread
// stopped inside a site. Anything unexpected -> the patch (group) is skipped and logged; the game is never touched
// otherwise. The SteamStub DRM is never touched: we only wait until it has decrypted the code.
//
// Logic mirrors the live-tested scripts patcher\cpu_patches.fsx, ft_vrr_patch.fsx, cpu_caves.fsx.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <intrin.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "gimerge.h"
#include "skipfx.h"
#include "gpuopt.h"
#include "gpuprof.h"
#include "sp.h"
#include "psoswap.h"
#include "psojournal.h"
#include "memstats.h"
#include "tilestats.h"
#include "loadmem.h"
#include "loadstats.h"
#include "patches_gen.h"
#include "menu.h"
#include "updatecheck.h"
#include "titlegate.h"
#include "osd.h"
#include "fnprobe.h"
#include "mlprobe.h"
#include "padlat.h"
#include "tour.h"
#include "vpad.h"
#include "autocont.h"
#include "bootnotice.h"
#include "crashreport.h"
#include "drawprep.h"
#include "asyncslots.h"
#include "loadonly.h"
#include "readfix.h"
#include "readprobe.h"
#include "jt.h"
#include "pmode.h"
#include "timerres.h"
#include "thrsched.h"
#include "psomemo.h"
#include "bkstats.h"
#include "rnt.h"
#include "frameoverlap.h"
#include "replayprof.h"
#include "cbvupload.h"
#include "samelook.h"
#include "extraperf.h"
#include "livectl.h"
#include "rebar.h"
#include "scenemirror.h"
#include "meshletwait.h"
#include "drainwait.h"
#include "hitchlog.h"

#define MOD_NAME "Nioh3EngineFix"
// 1.7.31: psojournal.cpp (PJ, notes\stutter.md 6.1)
// 1.7.29: gimerge.cpp (GIProbe / GIMerge, notes\gi-highest-audit.md s6)
// 1.7.32: drawprep.cpp (DQ retry quantum / P1E7 texture-loop prefetch, notes\mainthread-25.md)
// 1.7.34: fnprobe.cpp (FnProbe live function probe, notes\remaining-work.md 10.1; tools\playsession)
// 1.7.35: padlat.cpp (PadLatency controller input -> Present / GPU-done latency, notes\pad-latency.md)
// 1.7.36: tour.cpp (Tour mode: automatic stage / point tour with saves blocked, notes\tour-mode.md)
// 1.7.39: MA (main-thread atomic-load idiom -> plain loads, 25 byte sites; notes\pc-cpu-next.md)
// 1.7.40: BPO (BoundsFromPartObbs 71C60: quaternion memo + plain ready loads + forwarding copy; notes\bounds-partobbs.md)
// 1.7.40: rematch.cpp (boss rematch Phase 1 MVP, route R-a, [Rematch] Enable = 0; notes\boss-rematch-build.md)
// 1.7.40: diagnostics: GQ graphics-queue gap census (gpuprof.cpp) + JT job-timing probe (jt.cpp / jt.asm); notes\gq-jt-probes.md
// 1.7.41: the boss logger (BossLog / RematchLog / InvLog) and the boss rematch moved to the separate add-on Nioh3BossRematch.asi
//         (mod\Nioh3BossRematch, notes\boss-rematch-addon.md): this ASI hooks no boss site any more; old ini keys are logged as moved
// 1.7.48: VS probe ([Profiler] MemStats = 2: 1 s VRAM / texture page pool / D3D12 residency line; notes\root-cause-vram-streaming.md)
// 1.7.42: W1B (glyph padding clear 61BAC plain stores) + RPA (RenderParamsAssign 247950 range copy), drawprep.cpp; notes\w1b-rpa.md
// 1.7.43: fix boot hang with 3-4 drawprep groups on (FrozenWrite oldProt[2] overrun by 1.7.42's 4 writes; drawprep.cpp)
// 1.7.44: present mode log ([Debug] PresentModeLog = 1, PM SyncInterval writer probes) + [Latency] VrrPresent (pmode.cpp; notes\vrr-present.md)
// 1.7.44: AS (engine async-compute slots 0 / 11 / 12 at boot + live producer switch, asyncslots.cpp); notes\gpu-async-wins.md
// 1.7.45: PJ journal v2 (runtime PSOs only; the boot list filled v1) + PJH PSO hand-over (live row); notes\stutter-pso.md
// 1.7.45: TR timer-resolution hold (timerres.cpp, live row) + [Debug] SkipDebugString; notes\game-logic-audit.md
// 1.7.46: PL boot pipeline library ([Patches] PL = 0; psojournal.cpp) + boot-list timing line; notes\boot-shader-compile.md
// 1.7.47: MLP meshlet front-end probe ([Profiler] MeshletProbe = 0; mlprobe.cpp CPU half + gpuprof.cpp GPU half), MPCS MPC tail skip
//         ([Patches] MPCS = 0; mpcskip.asm, needs the `mpc` shader build) + MpcCheck self-check (mpccheck.cpp); notes\meshlet-frontend.md
// 1.7.49: same-look package (samelook.cpp / .asm, all default off, live rows): VSMA VSM waiting-tile amortisation + the engine
//         update-rate knobs LPVR / SKYR / FOGT / CLDR (render-config overrides); notes\same-look-package.md
// 1.7.50: VS probe: D3D12CreateDevice IAT hook from the decrypt wait (boot heaps counted) + bind census: root CBV/SRV/UAV,
//         VB/IB binds into system-memory heaps per engine marker (needs GPUProf = 2); notesoot-cause-vram-streaming.md
// 1.7.51: VS probe: per-PSO bind attribution (sampled GP2 frames, digests) + CBV/SRV/UAV descriptor census on system memory
// 1.7.52: CM CbvMirror ([GpuOpt] CbvMirror = 0): per-draw CBVs mirrored from the system-memory UPLOAD ring into VRAM (upmirror.cpp)
// 1.7.53: fast testing (notes\fast-testing.md): LiveCtl control file ([Debug] LiveCtl = 0) for one-launch A/B windows + live
//         [Tuning] values; ShaderHot ([ShaderHot] Enabled = 0, diagnostic): twin PSOs for loose-shader A/Bs in one launch
// 1.7.54: CM inline mode (CbvMirrorQueue = 0, default): prologue copy list on the game's own queue in the same ExecuteCommandLists,
//         no cross-queue waits (1.7.52 own-COPY-queue + waits was +15 ms on pc2); CbvMirrorComputeCopy; GPU_UPLOAD / OPTIONS16 guards
// 1.7.55: CM CbvMirrorSites ([GpuOpt], default 1 = per-draw 118F89 only, unchanged): 2 = the compute dispatch constants
//         (495B2E / 117F20, never mirrored before - the meshlet front-end kernels read them over PCIe on no-ReBAR PCs), 3 = both
// 1.7.58: TSTATE thread-state census ([Profiler] ThreadState) + SP Focus + RTP render-thread priority ([Patches] RTP, live
//         row) + PCM graphics-PSO lookup memo on the D3D12 replay thread ([Patches] PCM, live row); notes\cpu-bound-midrange.md
// 1.7.59-fo8: FO PagePool frame begin (B4 PagePoolFrameUpdate / B6 7D9C24) waits for TAIL N by default ([Tuning] FO_PPWait;
//   overlapping it with frame N's replay corrupted / freed data that frame N still replays when the replay lags = pc2 fo2 hang),
//   sub-stream crossing guard, replay-fault report (vectored handler), full thread dump, FO_ReplayDelayUs lag emulator.
// 1.7.59-fo7: FO stock frame end after an FO frame waits for the pending present first (stock GfxDevicePresent reads
//   GetCurrentBackBufferIndex before its own present spin; fo6pc hung on the first FO->stock switch), back-buffer index
//   re-anchored on the last real index every frame (a miss is tolerated, 7 misses in 1000 frames = session OFF), watchdog
//   thread dump + engine frame counter, live Tuning.FO_Test (forced switch / watchdog / self-test 100).
// 1.7.59-fo6: FO splits every sub-stream recorder in two halves and switches at each frame end (stock resets them at Present;
//   without a reset they filled after 260 frames and their never-used wrap path replayed overwritten data -> AV 119B54).
// 1.7.59-fo5: FO no longer resets sub-streams at the barrier (effect workers append before it; PC fo4pc: AV replaying a 0x90
//   sub-stream after 260 overlapped frames). Sub-streams are reset only by their own wrap path.
// 1.7.59-fo4: FO KtglCache frame end back on the TAIL with the frame's own fence (the forward fence deadlocked the GPU on the PC:
//   replay-side tile requests of frame N queued behind the tag, COPY waited GFX(N) while GFX(N) waited for them).
// 1.7.59-fo3: FO record/replay slot split - main advances the frame slot at E (timestamp allocator 6B42B0 records with
//   +4730h on main), TAIL resolves / stores for the frame's own slot; timestamp queries no longer switch FO off.
// 1.7.59-fo2: FO fix - HandleSlotIsNotFreed replacement is now a register-exact stub (LTCG callers keep r9-r11 live across it;
//   the C++ version hung pc2 FO1 at the title), nothing is patched until the switch goes ON, 5 s no-frame watchdog.
// 1.7.59-fo1 (branch frame-overlap): FO frame overlap ([GpuOpt] FrameOverlap = 0, live Tuning.FO / Tuning.FO_Pct):
//   frame N's replay tail + Present run on the replay thread while the main thread records N+1 (frameoverlap.cpp).
// 1.7.58-fov1 (branch frame-overlap): BKS "fov:" line = frame-overlap viability gate (main-thread handle / sync-point GPU waits,
//   sync-table / slot / sub-stream counts; notes/renderer/frame-overlap.md). Measurement only.
// 1.7.58-bk1 (branch renderer-backend): BKS back-end census ([Profiler] Backend) + KL kick limit (Tuning.KL_Div, live);
//         notes/renderer/backend.md
// 1.7.60-cbu1 (branch cbv-upload): CBU per-draw constant uploads ([Patches] CBS / CBN / CBT = 0, live Tuning.CBS / CBN / CBT;
//   [Profiler] CbvStats / CbvRecord): streaming copy into the UPLOAD ring, skip no-op uploads, CBV table reuse (cbvupload.cpp,
//   notes/renderer/cbv-upload.md)
// 1.7.60-cbu2: CBN + CBT default ON in the ini (pc2 +15.9 %), CBS off; CBN re-uploads its skipped slots after a ring-full
//   list cut inside SynUploadCbvTable (never leaves a slot on a block of the retired list); CBF ([Patches] CBF = 0, test only,
//   live Tuning.CBF / CBF_QUEUE / CBMODE bits 32 + 64): CB ring staged in system memory, one batched copy per game submission
//   into a VRAM ring the CBVs point at (cbvstage.cpp)
#define MOD_VERSION "1.7.87"

// ---- symbols shared with caves.asm -----------------------------------------------------------------------------
extern "C" {
void caveFps();
void caveScale();
void caveLim();
void caveTier();
void caveMenu();
void caveL1();
extern uint64_t vrr_minNs[3];
extern uint64_t vrr_maxNs[3];
extern uint64_t vrr_pNear;
extern uint64_t vrr_getDelta60;
extern uint64_t l1_callee;
extern uint32_t vrr_enable;
extern uint32_t vrr_tier;
extern uint32_t vrr_capCur;
extern uint32_t vrr_arm[3];
extern uint32_t vrr_cap[3];
BOOL WINAPI hookPeek(LPMSG msg, HWND wnd, UINT fmin, UINT fmax, UINT remove);
int L1Wait(uint64_t now, uint64_t last, uint64_t ticksPerFrame, uint64_t freq);
void fgWait(int64_t target);
void caveWakeGate();
extern uint32_t wk_enable, wk_skipped, wk_kept;
// experimental CPU-power caves (caves.asm, second half)
void caveGallop();
void cavePP();
void caveFPC();
void caveAabb();
void caveM1();
void caveX1();
void caveE1A();
void caveE1B();
void caveW6();
void caveW6Wake();
void W6Wait(uint8_t* ctx);
// 1.7.44 FSR FG under Proton (notesg-proton-fix.md): FGC copy-job null guard, FGP FG pause switch
void caveFgcSrc();
void caveFgcDst();
void caveFgp();
extern uint64_t fgc_target, fgp_target;
extern uint32_t fgc_src, fgc_srcNull, fgc_dstNull, fgp_pause, fgp_hits;
extern uint32_t gal_enable;
extern uint64_t gal_resume;
extern uint32_t pp_enable, pp_mask, pp_ran, pp_skipped;
extern uint64_t pp_target;
extern uint32_t fpc_enable, fpc_tid;
extern uint64_t fpc_rlo, fpc_rhi, fpc_gaddr, fpc_orig, fpc_epoch, fpc_hits, fpc_misses, fpc_bypass;
extern uint32_t aabb_enable;
extern uint64_t aabb_resume;
extern uint32_t m1_enable, m1_refresh;
extern uint64_t m1_resume, m1_stock;
extern uint32_t x1_enable, x1_skip, x1_pass;
extern uint64_t x1_pmgr, x1_iat;
extern uint32_t e1_enable, e1_mask, e1_pass;
extern float e1_scale;
extern uint64_t e1_upd, e1_skip, e1_aback, e1_enq;
extern uint32_t w6_waiting;
// input latency (IK / TS)
void caveIkKick();
void caveIkPadWait();
void caveTsL();
void caveTsW();
void caveTsB();
void caveTsP();
extern uint32_t lat_ik, lat_ts, lat_iklatch;
extern uint64_t lat_cap, lat_qpcNow, lat_inputKick, lat_padWait1, lat_padWaitN, lat_padEvent, lat_latMarker, lat_iatSetEvent;
extern uint64_t lat_lastkick, lat_agesum, lat_agecnt, lat_agemax, lat_timeouts, lat_ringidx, lat_kstock, lat_kik;
extern uint8_t lat_ring[];
// worker wakes (WS counters, ED EffectDisp skip)
void caveWs0();
void caveWs1();
void caveWs2();
void caveWs3();
void caveWs4();
void caveWs5();
void caveWsSleep();
void caveEdKick();
void caveEdJoin();
// 1.7.40 JT job-timing probe (jt.asm)
void caveJtJob();
void caveJtWait();
void caveJtWake1();
void caveJtWakeAll();
// 1.7.44 PM: SyncInterval writer probes (pmode.asm)
void caveSiW();
void caveSiS();
// 1.7.47 MPCS: MPC tail skip (mpcskip.asm)
void caveMpcsA();
void caveMpcsB();
void caveMpcsC();
extern uint32_t mpcs_enable;
extern uint64_t mpcs_resB, mpcs_skipB, mpcs_resC, mpcs_skipC, mpcs_n28, mpcs_n26, mpcs_seen;
extern uint32_t ws_enable, ed_enable;
extern uint64_t ws_iatSetEvent, ws_iatWait, ws_ret[3], ed_kickFn, ed_joinFn, ed_kicked, ed_skipped;
extern uint64_t ws_set[48], ws_sleeps, ws_sleepPool[64], ws_chk[32];
// GF: GPU-fence waits
void caveGf1();
void caveGf2();
void GfWait(void** fence, uint64_t value);
// WL: waitable-handle leak fix
void caveWL();
extern uint32_t wl_enable;
extern uint64_t wl_prev, wl_closed, wl_closeHandle;
extern uint64_t w6_wakeAll, w6_wakes;
// 1.6.0: TG tick governor (caveLim), WG wake gate (WS2/WS3/WSs), DS, CP, SPF
extern uint32_t tg_cap;
extern uint32_t vrr_fpsOut;
extern uint32_t wg_installed;
void caveDs();
void caveCP();
void caveSPF();
extern uint32_t cp_enable, spf_enable;
// SPR: runtime PSO create / upload-heap fallback timing (measurement only)
void caveSprG();
void caveSprU();
extern uint64_t spr_gfxFn, spr_ufFn, spr_slow, spr_gN, spr_gSum, spr_gMax, spr_gSlow, spr_uN, spr_uSum, spr_uMax, spr_uSlow;
extern uint64_t cp_resume, spf_resume, spf_gworld;
// 1.7.12: MQ (SSE matrix build), PT1 (pool lock once per texture), DL (dead part-map lookup; group CP)
void caveMQ();
void caveDL();
void capPt1Acq();
void capPt1Rel();
void cavePt1B();
void cavePt1C();
void cavePt1D();
// 1.7.14: GL1 (prefetch in the mission task-list walks), LW (loader managers wait on an event); LC is a byte patch
void caveGl1A();
void caveGl1B();
void caveLdWait();
void caveLdSig();
extern uint64_t gl1_resA, gl1_endA, gl1_resB, gl1_endB, lw_event, lw_pWait, lw_pSleep, lw_pSet, lw_resSig;
extern uint32_t lw_waiting, lw_signals;
extern uint32_t mq_enable, mq_count, dl_count, dl_enable, pt1_enable, pt1_owner, pt1_claims, pt1_saved;
extern uint64_t mq_resume, mq_calls, dl_fn, dl_resume, dl_skips;
extern uint64_t pt1_pAcq, pt1_pRel, pt1_ret, pt1_resB, pt1_resC, pt1_resD, pt1_jeD;
// 1.7.40: BPO (BoundsFromPartObbs 71C60: quaternion memo, plain ready-word loads, forwarding-friendly Vec4CopyW0)
void caveBpoQA();
void caveBpoQB();
void caveBpoL1();
void caveBpoL2();
void caveBpoV();
extern uint32_t bpo_enable, bpo_mask;
extern uint64_t bpo_imask, bpo_tbl, bpo_resQS, bpo_resQH, bpo_resQB, bpo_resL1, bpo_resL2, bpo_vStock;
BOOL WgWake(HANDLE ev, uint8_t* slot);
DWORD WgSleep(HANDLE ev, DWORD ms, uint8_t* pool, uint8_t* slot);
BOOL DsQpc(LARGE_INTEGER* now, int64_t start);
}

namespace {

// ---- near page layout (within +-2 GB of Nioh3.exe) ------------------------------------------------------------
struct NearData {
    volatile uint32_t ticks120;   // P3 x7: mov eax,[ticks120]
    volatile uint32_t ticks60;    // P6:    mov eax,[ticks60]
    volatile uint32_t r1Timeout;  // R1:    mov edx,[r1Timeout]  (0 = stock poll)
    uint32_t pad;
};
constexpr size_t kTrampOff = 0x40;
constexpr size_t kTrampSize = 16;
constexpr size_t kNearSize = 0x1000;

HMODULE g_self = nullptr;
uint8_t* g_base = nullptr;
size_t g_imageSize = 0;
uint8_t* g_near = nullptr;
FILE* g_log = nullptr;
bool g_skipOds = false;  // 1.7.45 [Debug] SkipDebugString: log lines go to the file only (no OutputDebugStringA)

// W3 / L1 runtime state (the W3/L1/F1/WK/VRR/R1 switches below are also flipped live by the mod menu, menu.cpp)
using PeekMessageA_t = BOOL(WINAPI*)(LPMSG, HWND, UINT, UINT, UINT);
PeekMessageA_t* g_peekSlot = nullptr;  // the game's IAT slot (keeps overlay hooks in the chain)
volatile LONG g_w3Enable = 0;
DWORD g_w3Ms = 1;
// Under Wine/Proton MsgWaitForMultipleObjectsEx(QS_ALLINPUT, MWMO_INPUTAVAILABLE) returns almost immediately (measured
// on a ROG Ally / Bazzite: ~24,000 wakes/s, each with a wineserver round trip), so W3 sleeps there instead.
volatile LONG g_w3UseSleep = 0;
volatile LONG g_l1Enable = 0;
HANDLE g_l1Timer = nullptr;
uint64_t g_l1MarginUs = 1000;
uint64_t g_l1MinUs = 500;
volatile LONG g_w3Count = 0;
volatile LONG g_l1Count = 0;
// F1 (FSR frame-generation presenter pacing wait) runtime state
volatile LONG g_fgEnable = 0;
HANDLE g_fgTimer = nullptr;
int64_t g_fgThreshTicks = 0;  // sleep only when more than margin+min remain
int64_t g_fgMarginTicks = 0;  // the last margin is the original QPC spin
int64_t g_qpcFreq = 0;
volatile LONG g_fgCount = 0;
// W6 (present wait, experimental) runtime state
using WaitOnAddress_t = BOOL(WINAPI*)(volatile VOID*, PVOID, SIZE_T, DWORD);
WaitOnAddress_t g_waitOnAddress = nullptr;
volatile uint32_t g_w6Enable = 0;
int64_t g_w6SpinTicks = 0;
DWORD g_w6TimeoutMs = 1;
volatile LONG g_w6Waits = 0, g_w6Sleeps = 0;
volatile LONG64 g_w6Ticks = 0;
// GF (GPU-fence waits) runtime state
volatile uint32_t g_gfEnable = 0;
DWORD g_gfTls = TLS_OUT_OF_INDEXES;  // per-thread auto-reset event (created lazily, never closed)
DWORD g_gfTimeoutMs = 100;           // per wait round; the engine's loop re-checks the fence after every return
volatile long g_gfWaits = 0, g_gfFallbacks = 0;
volatile long long g_gfTicks = 0;

// ---- WG (1.6.0): wake gate on the wake-one sites (WS2 48C6B, WS3 48D06) -----------------------------------------
// Per worker (pool slot id x worker index) a state word: kWgWaiting while the worker is inside WgSleep (the wrapper
// around its WaitForSingleObject at 1A6BC35), kWgPending once a counted wake has targeted it in that window. Per pool
// `pending` = number of workers in the pool woken by a counted wake that have not yet left their wait. A wake-one is
// skipped while pending >= WG_MaxPending (and the last counted wake is younger than WG_WindowUs). Proof of "no stalled
// job" in README (WG section) and notes: every pending unit is cleared by the woken worker itself right before it runs
// JobWorkerRunUntilIdle over the whole pool, and that clear is ordered after the skipping submitter's push.
constexpr LONG kWgWaiting = 1, kWgPending = 2;
#pragma warning(disable : 4324)  // the per-pool / per-worker states are padded to a cache line on purpose
struct alignas(64) WgPoolState {
    void* volatile owner;          // the pool object this id slot belongs to (first user); another pool -> no gating
    volatile LONG pending;
    LONG pad;
    volatile LONG64 lastTsc;       // rdtsc of the last counted wake
};
struct alignas(64) WgWorkerState {
    volatile LONG st;
};
WgPoolState g_wgPool[64];
WgWorkerState g_wgWorker[64][64];
volatile uint32_t g_wgEnable = 0;
LONG g_wgMax = 1;
uint64_t g_wgWindowTsc = 0;
volatile LONG64 g_wgSkipped = 0, g_wgCounted = 0, g_wgStale = 0, g_wgCalls = 0;

// ---- TG (1.6.0, opt-in experiment): logic tick governor. The limiter cap (caveLim) follows the rendered rate with a
// floor, so the game logic does not run ticks whose result is never shown. Needs VRR armed (variable dt keeps the game
// speed) and L1 active (otherwise the limiter slack would be spun, not slept).
volatile uint32_t g_tgEnable = 0;
volatile long g_tgState = 0;      // 0 off, 1 governing (cap > 0), 2 refused: L1 not active, 3 VRR not armed, 4 nothing to govern
int g_tgFloor = 30, g_tgRatioPct = 125;
int g_loaderCap = 10;            // 1.7.14: resource-load in-flight cap (16 when LC is applied), for LoadStats
int g_lcCapWanted = 16;          // 1.7.30: LC = 1 -> 16, LC = 11..64 -> that cap (BuildRepl rewrites the LCa/LCb immediates)
bool g_tgL1 = false;              // L1 installed this session
volatile double g_tgRendered = 0, g_tgTicks = 0;
ULONGLONG g_tgLowerSince = 0;

// ---- DS (1.6.0, opt-in): high-resolution timer sleep inside the main loop's 1 ms retry spin ----------------------
volatile uint32_t g_dsEnable = 0;
HANDLE g_dsTimer = nullptr;
volatile LONG g_dsTid = 0;
int64_t g_dsMarginUs = 150;
int64_t g_qpcFreqDs = 0;
volatile long g_dsWaits = 0;
volatile long long g_dsTicks = 0;
constexpr uint32_t kRvaEngineTimer = 0x45BA780;  // DS: engine timer object (+12h QPC mode byte, +18h frequency)

// v2.02 engine addresses used by the experimental caves (verified by tests/test_patterns.py deps / the live scripts)
constexpr uint32_t kRvaGfxGlobal = 0x45BA588;   // GfxDevice/KtglContext* g (FPC: g+3C60h stream generation)
constexpr uint32_t kRvaPadMgr = 0x45BA1E0;      // pad manager pointer (X1: +69850h + i*14h connected bytes)

// ---- logging ---------------------------------------------------------------------------------------------------
void Log(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    SYSTEMTIME st;
    GetLocalTime(&st);
    char line[1100];
    snprintf(line, sizeof(line), "[%02u:%02u:%02u.%03u] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);
    // 1.7.45 SkipDebugString: under Proton every OutputDebugStringA is a raised exception + a DBWIN mapping lookup through
    // wineserver (91 calls/s in the Ally PROTON_LOG run, all from this function); keep it only when a debugger listens
    if (!g_skipOds || IsDebuggerPresent()) OutputDebugStringA(line);
    crashreport::NoteLog(line);
    if (g_log) {
        fputs(line, g_log);
        fflush(g_log);
    }
}

std::wstring SelfPathWithExt(const wchar_t* ext) {
    wchar_t p[MAX_PATH * 2] = {};
    DWORD n = GetModuleFileNameW(g_self, p, (DWORD)(sizeof(p) / sizeof(p[0])));
    std::wstring s(p, n);
    size_t dot = s.find_last_of(L'.');
    size_t sep = s.find_last_of(L"\\/");
    if (dot != std::wstring::npos && (sep == std::wstring::npos || dot > sep)) s.resize(dot);
    return s + ext;
}

static bool s_asyncKnown = false, s_as0Applied = false;  // AS0 / async-queue stats (stats thread)

// ---- config ----------------------------------------------------------------------------------------------------
drainwait::State g_rd;
bool g_rdInstallUncertain = false;
extern "C" void caveRDWait();
extern "C" void caveRDWake();
extern "C" uint64_t rd_resume;
extern "C" bool RDWait(uint8_t* ring) { return g_rd.Poll(ring); }
extern "C" void RDComplete(uint8_t* ring) { g_rd.Complete(ring); }
meshletwait::State g_mw;
bool g_mwInstallUncertain = false;
extern "C" void caveMW();
extern "C" void caveMWWake();
extern "C" void MWWait(uint8_t* m) { g_mw.Round(m + 0x5F8); }
extern "C" void MWComplete(uint8_t* m) { g_mw.Complete(m + 0x5F8); }

struct Config {
    int mw = 0, rd = 0;
    int m2s = 0;  // narrow completion-CAS seed loads, startup only; full M2 unchanged
    int enabled = 1, log = 1, allowUnknownBuild = 0, decryptWaitSec = 60;
    int vrrOnline = -1, w1 = 1, w2 = 1, vrr = 1, w3 = 1, r1 = 1, l1 = 1, l1OnWine = 0, fg = 1, fgOnWine = 0, p12 = 0, w3WineSleep = 1, wk = 1, workerCap = 0, as0 = 0;
    int arm[3] = {1, 1, 1};
    int minF[3] = {20, 24, 48};
    int maxF[3] = {30, 60, 120};
    int maxFps = 120, menuQuantize = 0;
    int w3Ms = 1, r1Ms = 2, l1MarginUs = 1000, l1MinUs = 500;
    // experimental CPU-power patches (known build only; 0 = not installed)
    int b = 0, pp = 0, fpc = 0, aabb = 0, m1 = 0, m2 = 0, j1 = 0, x1 = 0, e1 = 0, w6 = 0, s1 = 0;
    int ppPeriod = 4, e1N = 2, w6SpinUs = 50, w6TimeoutMs = 1;
    // input latency (1.2.0)
    int ik = 0, ts = 0, ikCapMs = 0;
    bool tsForPad = false;  // 1.7.35: TS probes on only because [Debug] PadLatency needs their frame stamps
    // worker wakes (1.3.0)
    int wakeStats = 0, ed = 0;
    // GPU-fence waits (1.4.0)
    int gf = 0, gfTimeoutMs = 100;
    // waitable-handle leak fix (1.5.0): on by default (Windows leaks one handle per frame; never triggers under Proton)
    int wl = 1;
    // 1.7.44 [Debug] PresentModeLog (pmode.cpp): 1 = present-mode change lines + the PM SyncInterval writer probes (measurement only)
    int skipOds = 0;  // 1.7.45 [Debug] SkipDebugString (notes\game-logic-audit.md 4)
    int pmLog = 1;
    // 1.6.0 (notes\cpu-hotspots-ally.md, notes\worker-jobs-opt.md): all opt-in, 0 = off / not installed
    int wg = 0, wgMaxPending = 1, wgWindowUs = 2000;
    int tg = 0, tgFloor = 30, tgRatioPct = 125;
    int ds = 0, dsMarginUs = 150;
    int lk = 0, cp = 0, spf = 0;
    int ma = 0;  // 1.7.39: main-thread locked "atomic loads" (code bytes, next launch)
    int bpo = 0, bpoMask = 7, bpoTableBits = 14;  // 1.7.40: BoundsFromPartObbs (1 = on, 2 = installed off; mask 1 Q, 2 L, 4 V)
    int mq = 0, pt1 = 0, dl = 0, cpuStats = 0;  // 1.7.12 (DL rides the CP group)
    int rwb = 0, spr = 0;
    int mpcs = 0;  // 1.7.47 MPCS: MPC tail skip in MeshletPreculling (code caves, next launch; needs the mpc shader build; notes\meshlet-frontend.md)
    int esm1 = 0;  // 1.7.44 ESM1: directional ESM render target capped at 1024^2 (code bytes, next launch; notes\gpu-resolution-audit.md)
    // 1.7.23 LO (loadonly.cpp): GF/W6/RWb installed but on only inside 58A; *User = the keys as written in the ini
    int loadOnly = 0, gfUser = 0, w6User = 0, rwbUser = 0;
    int gl1 = 0, lc = 0, lw = 0;  // 1.7.14 (loading / game-logic; ini only, next launch)
    // 1.7.0: GP GPU pass profiler ([Profiler]; gpuprof.cpp reads its own keys, these only decide the code checks)
    int gpuProf = 0, gpuMarkers = 0;
    int jt = 0;  // 1.7.40 JT job-timing probe ([Profiler] JT; measurement only, sites written only when 1)
    // Legacy FrameGenToggle=3 retains only the independent Proton FGC crash guard.
    int fgc = 0, fgToggle = 0;
};

// OS-auto keys (ini value -1): resolved here, logged right after the version line (the log is not open yet).
static std::string g_autoNote;
static bool IsWineHost() {
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    return nt && GetProcAddress(nt, "wine_get_version") != nullptr;
}
Config LoadConfig() {
    Config c;
    std::wstring ini = SelfPathWithExt(L".ini");
    const wchar_t* f = ini.c_str();
    auto I = [&](const wchar_t* sec, const wchar_t* key, int def) { return (int)GetPrivateProfileIntW(sec, key, def, f); };
    // -1 = auto by OS (one ini for Windows and Proton): wineV under Wine/Proton, winV on native Windows
    const bool wineHost = IsWineHost();
    auto A = [&](const wchar_t* sec, const wchar_t* key, int def, int wineV, int winV, const char* why) {
        const int raw = I(sec, key, def);
        if (raw >= 0) return raw;
        const int v = wineHost ? wineV : winV;
        char b[400];
        snprintf(b, sizeof(b), "  auto %ls = %d (%s: %s)\n", key, v, wineHost ? "Proton/Wine" : "native Windows", why);
        g_autoNote += b;
        return v;
    };
    c.enabled = I(L"General", L"Enabled", c.enabled);
    c.log = I(L"General", L"Log", c.log);
    c.allowUnknownBuild = I(L"General", L"AllowUnknownBuild", c.allowUnknownBuild);
    c.decryptWaitSec = std::clamp(I(L"General", L"DecryptWaitSeconds", c.decryptWaitSec), 1, 600);
    c.w1 = I(L"Patches", L"W1", c.w1);
    c.w2 = I(L"Patches", L"W2", c.w2);
    c.vrr = I(L"Patches", L"VRR", c.vrr);
    c.vrrOnline = I(L"Patches", L"VRROnline", c.vrrOnline);
    c.w3 = I(L"Patches", L"W3", c.w3);
    c.r1 = I(L"Patches", L"R1", c.r1);
    c.l1 = I(L"Patches", L"L1", c.l1);
    c.l1OnWine = I(L"Patches", L"L1OnWine", c.l1OnWine);
    c.fg = I(L"Patches", L"FG", c.fg);
    c.fgOnWine = I(L"Patches", L"FGOnWine", c.fgOnWine);
    c.fgc = I(L"Patches", L"FGC", c.fgc);
    c.fgToggle = I(L"Menu", L"FrameGenToggle", c.fgToggle);
    c.p12 = I(L"Patches", L"P12", c.p12);
    c.w3WineSleep = I(L"Patches", L"W3WineSleep", c.w3WineSleep);
    c.wk = I(L"Patches", L"WK", c.wk);
    c.workerCap = I(L"Patches", L"WorkerCap", c.workerCap);
    c.as0 = I(L"Patches", L"AS0", c.as0);
    const wchar_t* armK[3] = {L"Tier30", L"Tier60", L"Tier120"};
    const wchar_t* minK[3] = {L"Min30", L"Min60", L"Min120"};
    const wchar_t* maxK[3] = {L"Max30", L"Max60", L"Max120"};
    for (int i = 0; i < 3; ++i) {
        c.arm[i] = I(L"VRR", armK[i], c.arm[i]);
        c.minF[i] = I(L"VRR", minK[i], c.minF[i]);
        c.maxF[i] = I(L"VRR", maxK[i], c.maxF[i]);
    }
    c.maxFps = I(L"VRR", L"MaxFps", c.maxFps);
    c.menuQuantize = I(L"VRR", L"MenuQuantize", c.menuQuantize);
    c.w3Ms = std::clamp(A(L"Tuning", L"W3_TimeoutMs", c.w3Ms, 3, 1, "Wine: pump poll 3 ms instead of 1 (notes/cpu-hotspots-ally.md row 4); Windows keeps 1"), 0, 16);
    c.r1Ms = std::clamp(I(L"Tuning", L"R1_TimeoutMs", c.r1Ms), 0, 16);
    c.l1MarginUs = std::clamp(I(L"Tuning", L"L1_MarginUs", c.l1MarginUs), 200, 20000);
    c.l1MinUs = std::clamp(I(L"Tuning", L"L1_MinSleepUs", c.l1MinUs), 0, 20000);
    c.b = I(L"Patches", L"B", c.b);
    c.pp = I(L"Patches", L"PP", c.pp);
    c.fpc = I(L"Patches", L"FPC", c.fpc);
    c.aabb = I(L"Patches", L"AABB", c.aabb);
    c.m1 = I(L"Patches", L"M1", c.m1);
    c.m2 = I(L"Patches", L"M2", c.m2);
    c.m2s = I(L"Patches", L"M2S", c.m2s);
    c.j1 = I(L"Patches", L"J1", c.j1);
    c.x1 = I(L"Patches", L"X1", c.x1);
    c.e1 = I(L"Patches", L"E1", c.e1);
    c.rd = I(L"Patches", L"RD", 0); // startup-only paired renderer drain
    c.mw = I(L"Patches", L"MW", 0);  // explicit opt-in, never Auto
    c.w6 = A(L"Patches", L"W6", c.w6, 1, 0, "Wine: present-wait spin -> WaitOnAddress, power goes to the GPU; Windows: ~0 measured");
    c.s1 = I(L"Patches", L"S1", c.s1);
    c.ppPeriod = I(L"Tuning", L"PP_Period", c.ppPeriod);
    if (c.ppPeriod != 2 && c.ppPeriod != 4 && c.ppPeriod != 8 && c.ppPeriod != 16) c.ppPeriod = 4;
    c.e1N = I(L"Tuning", L"E1_N", c.e1N);
    if (c.e1N != 1 && c.e1N != 2 && c.e1N != 4 && c.e1N != 8) c.e1N = 2;
    c.w6SpinUs = std::clamp(I(L"Tuning", L"W6_SpinUs", c.w6SpinUs), 0, 2000);
    c.w6TimeoutMs = std::clamp(I(L"Tuning", L"W6_TimeoutMs", c.w6TimeoutMs), 1, 16);
    c.ik = I(L"Patches", L"IK", c.ik);
    c.ts = I(L"Patches", L"TS", c.ts);
    if (!c.ts && padlat::Wanted(f)) c.ts = 1, c.tsForPad = true;  // PadLatency reads the TS ring (C/L/W/P); measurement only
    c.ikCapMs = std::clamp(I(L"Tuning", L"IK_PadWaitCapMs", c.ikCapMs), 0, 8);
    c.wakeStats = I(L"Patches", L"WakeStats", c.wakeStats);
    c.ed = I(L"Patches", L"ED", c.ed);
    c.gf = A(L"Patches", L"GF", c.gf, 1, 0, "Wine: removes ~0.6 core of fence spin; Windows: +0.38 pct fps, below the keep bar");
    c.wl = I(L"Patches", L"WL", c.wl);
    c.pmLog = std::clamp(I(L"Debug", L"PresentModeLog", c.pmLog), 0, 2);
    c.skipOds = I(L"Debug", L"SkipDebugString", c.skipOds);
    c.gfTimeoutMs = std::clamp(I(L"Tuning", L"GF_TimeoutMs", c.gfTimeoutMs), 1, 1000);
    c.wg = I(L"Patches", L"WG", c.wg);
    c.tg = I(L"Patches", L"TG", c.tg);
    c.ds = I(L"Patches", L"DS", c.ds);
    c.lk = I(L"Patches", L"LK", c.lk);
    c.ma = I(L"Patches", L"MA", c.ma);
    c.bpo = I(L"Patches", L"BPO", c.bpo);
    c.bpoMask = std::clamp(I(L"Tuning", L"BPO_Mask", c.bpoMask), 1, 7);
    c.bpoTableBits = std::clamp(I(L"Tuning", L"BPO_TableBits", c.bpoTableBits), 10, 18);
    c.cp = I(L"Patches", L"CP", c.cp);
    c.spf = I(L"Patches", L"SPF", c.spf);
    c.mq = A(L"Patches", L"MQ", c.mq, 1, 0, "Wine: bit-exact, CPU power only; Windows: no fps gain measured");
    c.pt1 = I(L"Patches", L"PT1", c.pt1);
    c.dl = I(L"Patches", L"DL", c.dl);
    c.cpuStats = I(L"Patches", L"CpuStats", c.cpuStats);
    c.rwb = I(L"Patches", L"RWb", c.rwb);
    c.esm1 = I(L"Patches", L"ESM1", c.esm1);
    c.mpcs = I(L"Patches", L"MPCS", c.mpcs);
    c.spr = I(L"Patches", L"SPR", c.spr);
    c.gl1 = I(L"Patches", L"GL1", c.gl1);
    c.lc = I(L"Patches", L"LC", c.lc);
    c.lw = I(L"Patches", L"LW", c.lw);
    c.wgMaxPending = std::clamp(I(L"Tuning", L"WG_MaxPending", c.wgMaxPending), 1, 8);
    c.wgWindowUs = std::clamp(I(L"Tuning", L"WG_WindowUs", c.wgWindowUs), 100, 100000);
    c.tgFloor = std::clamp(I(L"Tuning", L"TG_Floor", c.tgFloor), 24, 60);
    c.tgRatioPct = std::clamp(I(L"Tuning", L"TG_RatioPct", c.tgRatioPct), 100, 200);
    c.dsMarginUs = std::clamp(I(L"Tuning", L"DS_MarginUs", c.dsMarginUs), 50, 900);
    c.gpuProf = I(L"Profiler", L"GPUProf", c.gpuProf);
    c.gpuMarkers = I(L"Profiler", L"GPUProf_Markers", c.gpuMarkers);
    c.jt = I(L"Profiler", L"JT", I(L"Patches", L"JT", c.jt));  // [Patches] JT too: the abba2 harness writes unknown arm keys there
    c.loadOnly = I(L"Patches", L"LoadOnlyCpuFixes", c.loadOnly);
    c.gfUser = c.gf;
    c.w6User = c.w6;
    c.rwbUser = c.rwb;
    if (c.loadOnly) {  // install GF/W6 regardless (their words follow gfUser/w6User outside loads); RWb goes to loadonly's cave
        if (!c.gf) c.gf = 1;
        if (!c.w6) c.w6 = 1;
        c.rwb = 0;
    }
    return c;
}

// ---- PE helpers ------------------------------------------------------------------------------------------------
IMAGE_NT_HEADERS64* Nt(uint8_t* base) {
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    return reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
}

struct Range {
    uint8_t* p;
    size_t n;
};

// committed, readable sub-ranges of every executable section (same scope as tests/test_patterns.py)
std::vector<Range> ExecRanges() {
    std::vector<Range> out;
    auto nt = Nt(g_base);
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (!(sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        uint8_t* p = g_base + sec->VirtualAddress;
        uint8_t* end = p + sec->Misc.VirtualSize;
        while (p < end) {
            MEMORY_BASIC_INFORMATION mbi{};
            if (!VirtualQuery(p, &mbi, sizeof(mbi))) break;
            uint8_t* rEnd = std::min(end, (uint8_t*)mbi.BaseAddress + mbi.RegionSize);
            const DWORD readable = PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY | PAGE_READONLY |
                                   PAGE_READWRITE | PAGE_WRITECOPY;
            if (mbi.State == MEM_COMMIT && (mbi.Protect & readable) && !(mbi.Protect & PAGE_GUARD)) {
                if (!out.empty() && out.back().p + out.back().n == p)
                    out.back().n += (size_t)(rEnd - p);
                else
                    out.push_back({p, (size_t)(rEnd - p)});
            }
            p = rEnd;
        }
    }
    return out;
}

// ---- AOB scanning ----------------------------------------------------------------------------------------------
struct Pattern {
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> wild;
    size_t anchorOff = 0, anchorLen = 0;
};

bool ParsePattern(const char* s, Pattern& p) {
    while (*s) {
        while (*s == ' ') ++s;
        if (!*s) break;
        if (s[0] == '?' && s[1] == '?') {
            p.bytes.push_back(0);
            p.wild.push_back(1);
            s += 2;
            continue;
        }
        char h[3] = {s[0], s[1], 0};
        char* e = nullptr;
        unsigned long v = strtoul(h, &e, 16);
        if (e != h + 2) return false;
        p.bytes.push_back((uint8_t)v);
        p.wild.push_back(0);
        s += 2;
    }
    size_t cur = 0, cs = 0;
    for (size_t i = 0; i < p.bytes.size(); ++i) {
        if (p.wild[i]) { cur = 0; continue; }
        if (cur == 0) cs = i;
        if (++cur > p.anchorLen) { p.anchorLen = cur; p.anchorOff = cs; }
    }
    return p.anchorLen > 0;
}

// number of matches (stops at 2); *first = first match start
int FindPattern(const Pattern& p, const std::vector<Range>& ranges, uint8_t** first) {
    int n = 0;
    *first = nullptr;
    const uint8_t* aB = p.bytes.data() + p.anchorOff;
    std::boyer_moore_horspool_searcher<const uint8_t*> searcher(aB, aB + p.anchorLen);
    for (const Range& r : ranges) {
        const uint8_t* lo = r.p;
        const uint8_t* hi = r.p + r.n;
        const uint8_t* cur = lo;
        while (cur < hi) {
            auto [hit, hitEnd] = searcher(cur, hi);
            if (hit == hi) break;
            const uint8_t* st = hit - p.anchorOff;
            if (st >= lo && st + p.bytes.size() <= hi) {
                bool ok = true;
                for (size_t k = 0; k < p.bytes.size() && ok; ++k)
                    if (!p.wild[k] && st[k] != p.bytes[k]) ok = false;
                if (ok) {
                    if (n == 0) *first = const_cast<uint8_t*>(st);
                    if (++n >= 2) return n;
                }
            }
            cur = hit + 1;
        }
    }
    return n;
}

// ---- near allocation -------------------------------------------------------------------------------------------
uint8_t* AllocNear(uintptr_t lo, uintptr_t hi, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t gran = si.dwAllocationGranularity;
    const uintptr_t reach = 0x7FF00000;  // stay comfortably inside rel32 from every byte of the image
    const uintptr_t minA = (hi > reach) ? ((hi - reach + gran - 1) & ~(gran - 1)) : gran;
    const uintptr_t maxA = lo + reach - size;
    // downward from the image
    for (uintptr_t p = (lo - gran) & ~(gran - 1); p >= minA && p < lo;) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery((void*)p, &mbi, sizeof(mbi))) break;
        if (mbi.State == MEM_FREE) {
            if (void* r = VirtualAlloc((void*)p, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)r;
        }
        uintptr_t next = ((uintptr_t)mbi.AllocationBase ? (uintptr_t)mbi.AllocationBase : p) - gran;
        if (next >= p) break;
        p = next & ~(gran - 1);
    }
    // upward from the image end
    for (uintptr_t p = (hi + gran - 1) & ~(gran - 1); p <= maxA;) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery((void*)p, &mbi, sizeof(mbi))) break;
        if (mbi.State == MEM_FREE) {
            if (void* r = VirtualAlloc((void*)p, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)r;
        }
        uintptr_t next = ((uintptr_t)mbi.BaseAddress + mbi.RegionSize + gran - 1) & ~(gran - 1);
        if (next <= p) break;
        p = next;
    }
    return nullptr;
}

bool Rel32(const uint8_t* next, const void* target, int32_t* out) {
    int64_t d = (int64_t)(uintptr_t)target - (int64_t)(uintptr_t)next;
    if (d < INT32_MIN || d > INT32_MAX) return false;
    *out = (int32_t)d;
    return true;
}

void* TargetFn(gen::Target t) {
    switch (t) {
        case gen::Target::caveFps: return (void*)&caveFps;
        case gen::Target::caveScale: return (void*)&caveScale;
        case gen::Target::caveLim: return (void*)&caveLim;
        case gen::Target::caveTier: return (void*)&caveTier;
        case gen::Target::caveMenu: return (void*)&caveMenu;
        case gen::Target::hookPeek: return (void*)&hookPeek;
        case gen::Target::caveL1: return (void*)&caveL1;
        case gen::Target::fgWait: return (void*)&fgWait;
        case gen::Target::caveWakeGate: return (void*)&caveWakeGate;
        case gen::Target::caveGallop: return (void*)&caveGallop;
        case gen::Target::cavePP: return (void*)&cavePP;
        case gen::Target::caveFPC: return (void*)&caveFPC;
        case gen::Target::caveAabb: return (void*)&caveAabb;
        case gen::Target::caveM1: return (void*)&caveM1;
        case gen::Target::caveX1: return (void*)&caveX1;
        case gen::Target::caveE1A: return (void*)&caveE1A;
        case gen::Target::caveE1B: return (void*)&caveE1B;
        case gen::Target::caveMW: return (void*)&caveMW;
        case gen::Target::caveRDWait: return (void*)&caveRDWait;
        case gen::Target::caveRDWake: return (void*)&caveRDWake;
        case gen::Target::caveMWWake: return (void*)&caveMWWake;
        case gen::Target::caveW6: return (void*)&caveW6;
        case gen::Target::caveW6Wake: return (void*)&caveW6Wake;
        case gen::Target::caveIkKick: return (void*)&caveIkKick;
        case gen::Target::caveIkPadWait: return (void*)&caveIkPadWait;
        case gen::Target::caveTsL: return (void*)&caveTsL;
        case gen::Target::caveTsW: return (void*)&caveTsW;
        case gen::Target::caveTsB: return (void*)&caveTsB;
        case gen::Target::caveTsP: return (void*)&caveTsP;
        case gen::Target::caveWs0: return (void*)&caveWs0;
        case gen::Target::caveWs1: return (void*)&caveWs1;
        case gen::Target::caveWs2: return (void*)&caveWs2;
        case gen::Target::caveWs3: return (void*)&caveWs3;
        case gen::Target::caveWs4: return (void*)&caveWs4;
        case gen::Target::caveWs5: return (void*)&caveWs5;
        case gen::Target::caveWsSleep: return (void*)&caveWsSleep;
        case gen::Target::caveEdKick: return (void*)&caveEdKick;
        case gen::Target::caveEdJoin: return (void*)&caveEdJoin;
        case gen::Target::caveGf1: return (void*)&caveGf1;
        case gen::Target::caveGf2: return (void*)&caveGf2;
        case gen::Target::caveWL: return (void*)&caveWL;
        case gen::Target::caveDs: return (void*)&caveDs;
        case gen::Target::caveCP: return (void*)&caveCP;
        case gen::Target::caveSPF: return (void*)&caveSPF;
        case gen::Target::caveSprG: return (void*)&caveSprG;
        case gen::Target::caveSprU: return (void*)&caveSprU;
        case gen::Target::caveDL: return (void*)&caveDL;
        case gen::Target::caveMQ: return (void*)&caveMQ;
        case gen::Target::capPt1Acq: return (void*)&capPt1Acq;
        case gen::Target::capPt1Rel: return (void*)&capPt1Rel;
        case gen::Target::cavePt1B: return (void*)&cavePt1B;
        case gen::Target::cavePt1C: return (void*)&cavePt1C;
        case gen::Target::cavePt1D: return (void*)&cavePt1D;
        case gen::Target::caveGl1A: return (void*)&caveGl1A;
        case gen::Target::caveGl1B: return (void*)&caveGl1B;
        case gen::Target::caveLdWait: return (void*)&caveLdWait;
        case gen::Target::caveLdSig: return (void*)&caveLdSig;
        case gen::Target::caveBpoQA: return (void*)&caveBpoQA;
        case gen::Target::caveBpoQB: return (void*)&caveBpoQB;
        case gen::Target::caveBpoL1: return (void*)&caveBpoL1;
        case gen::Target::caveBpoL2: return (void*)&caveBpoL2;
        case gen::Target::caveBpoV: return (void*)&caveBpoV;
        case gen::Target::caveJtJob: return (void*)&caveJtJob;
        case gen::Target::caveJtWait: return (void*)&caveJtWait;
        case gen::Target::caveJtWake1: return (void*)&caveJtWake1;
        case gen::Target::caveJtWakeAll: return (void*)&caveJtWakeAll;
        case gen::Target::caveFgcSrc: return (void*)&caveFgcSrc;
        case gen::Target::caveFgcDst: return (void*)&caveFgcDst;
        case gen::Target::caveFgp: return (void*)&caveFgp;
        case gen::Target::caveSiW: return (void*)&caveSiW;
        case gen::Target::caveSiS: return (void*)&caveSiS;
        case gen::Target::caveMpcsA: return (void*)&caveMpcsA;
        case gen::Target::caveMpcsB: return (void*)&caveMpcsB;
        case gen::Target::caveMpcsC: return (void*)&caveMpcsC;
        case gen::Target::caveVsmaEntry: return (void*)&caveVsmaEntry;
        case gen::Target::caveVsmaBudget: return (void*)&caveVsmaBudget;
        case gen::Target::caveVsmaBind: return (void*)&caveVsmaBind;
        case gen::Target::caveRco: return (void*)&caveRco;
        case gen::Target::caveGiScale: return (void*)&caveGiScale;  // 1.7.59 Extra performance (extraperf.asm)
        case gen::Target::caveMeshLod: return (void*)&caveMeshLod;
        case gen::Target::caveFxSim: return (void*)&caveFxSim;
        case gen::Target::caveFxDraw: return (void*)&caveFxDraw;
        default: return nullptr;
    }
}

constexpr int kLastTarget = (int)gen::Target::caveRDWake;  // MUST be the last enum entry (1.7.2 left caveSPF: SPR trampolines were never written -> jump into zeroed memory); tests/test_patterns.py checks this
static_assert(0x40 + kTrampSize * (kLastTarget + 1) <= kNearSize, "trampolines must fit the near page");

uint8_t* Trampoline(gen::Target t) { return g_near + kTrampOff + kTrampSize * (size_t)t; }

void WriteTrampolines() {
    for (int t = 1; t <= kLastTarget; ++t) {
        uint8_t* p = Trampoline((gen::Target)t);
        const uint8_t jmp[6] = {0xFF, 0x25, 0, 0, 0, 0};  // jmp qword ptr [rip+0]
        memcpy(p, jmp, 6);
        uint64_t a = (uint64_t)(uintptr_t)TargetFn((gen::Target)t);
        memcpy(p + 6, &a, 8);
        p[14] = 0xCC;
        p[15] = 0xCC;
    }
    FlushInstructionCache(GetCurrentProcess(), g_near, kNearSize);
}

// ---- site resolution -------------------------------------------------------------------------------------------
struct Item {
    const gen::Site* s = nullptr;
    uint8_t* addr = nullptr;
    bool found = false;
    std::vector<uint8_t> repl;
};

bool Resolve(const gen::Site& s, const std::vector<Range>& ranges, bool known, Item& it) {
    it.s = &s;
    if (s.kind == gen::Kind::VPtr) {  // .rdata pointer slot: known build only, verified by value
        if (!known) { Log("  %-4s data slot, known build only (skipped)", s.id); return false; }
        uint64_t cur = *reinterpret_cast<const volatile uint64_t*>(g_base + s.rva);
        if (cur != (uint64_t)(uintptr_t)(g_base + s.expectRva)) {
            Log("  %-4s slot RVA %X holds %llX, expected Nioh3+%X (another mod?) (skipped)", s.id, s.rva, (unsigned long long)cur, s.expectRva);
            return false;
        }
        it.addr = g_base + s.rva;
        it.found = true;
        return true;
    }
    Pattern p;
    if (!ParsePattern(s.pattern, p)) { Log("  %-4s bad pattern", s.id); return false; }
    uint8_t* first = nullptr;
    int n = FindPattern(p, ranges, &first);
    if (n != 1) {
        Log("  %-4s pattern %s (skipped)", s.id, n == 0 ? "NOT FOUND (other mod / different build?)" : "NOT UNIQUE");
        return false;
    }
    uint8_t* a = first + s.offset;
    uint32_t rva = (uint32_t)(a - g_base);
    if (known && rva != s.rva) {
        Log("  %-4s found at RVA %X, expected %X on this build (skipped)", s.id, rva, s.rva);
        return false;
    }
    // original bytes: exact on the known build; on another build the pattern's wildcards inside the site are allowed
    for (int k = 0; k < s.len; ++k) {
        bool wildHere = p.wild[s.offset + k] != 0;
        if (a[k] != s.orig[k] && (known || !wildHere)) {
            Log("  %-4s original bytes differ at RVA %X+%d (already patched by another mod?) (skipped)", s.id, rva, k);
            return false;
        }
    }
    it.addr = a;
    it.found = true;
    return true;
}

// original code a group's caves reproduce or rely on (patches.json "deps"); known build only
uint32_t Fnv1a(const uint8_t* p, size_t n) {
    uint32_t h = 0x811C9DC5u;
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 0x01000193u;
    return h;
}
bool DepsOk(const char* group) {
    for (int i = 0; i < gen::kNumDeps; ++i) {
        const gen::Dep& d = gen::kDeps[i];
        if (strcmp(d.group, group) != 0) continue;
        const uint8_t* a = g_base + d.rva;
        const bool ok = d.bytes ? memcmp(a, d.bytes, d.len) == 0 : Fnv1a(a, d.len) == d.fnv1a;
        if (!ok) {
            Log("%s: original code at RVA %X (%u bytes) differs from v2.02 (another mod?) - group skipped", group, d.rva, d.len);
            return false;
        }
    }
    return true;
}

// ---- TS: per-stage medians from the probe ring (patcher\latency_live.fsx `stats`). Ids: 1 K pad kick, 2 C pad
// consumed, 3 W render begin, 4 B after the waitable wait, 5 P present hand-off, 6 L limiter exit.
// Input -> display on vkd3d = B(n+1) - K(n): with the latency-1 waitable, B of the next render frame is released when
// frame n was presented (vkWaitForPresentKHR). Add half a refresh for mid-screen. Menu shows it (g_tsInDispUs).
volatile LONG g_tsInDispUs = -1;
void TsAnalyse(bool logIt, double freq) {
    const uint64_t idx = lat_ringidx;
    const uint64_t n = std::min<uint64_t>(idx, 4096);
    struct Ev {
        int64_t q;
        int t;
    };
    std::vector<Ev> ev;
    ev.reserve((size_t)n);
    const volatile int64_t* ring = reinterpret_cast<const volatile int64_t*>(lat_ring);
    for (uint64_t k = idx - n; k < idx; ++k) {
        const size_t s = (size_t)(k & 4095) * 2;
        const int64_t q = ring[s];
        const int t = (int)(ring[s + 1] & 0xFF);
        if (q > 0 && t >= 1 && t <= 6) ev.push_back({q, t});
    }
    std::sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.q < b.q; });
    std::vector<int64_t> T[7];
    for (const Ev& e : ev) T[e.t].push_back(e.q);
    auto next = [&](int t, int64_t from) -> int64_t {
        if (from < 0) return -1;
        auto it = std::upper_bound(T[t].begin(), T[t].end(), from);
        return it == T[t].end() ? -1 : *it;
    };
    auto prev = [&](int t, int64_t before) -> int64_t {
        auto it = std::lower_bound(T[t].begin(), T[t].end(), before);
        return it == T[t].begin() ? -1 : *(it - 1);
    };
    const double toMs = 1000.0 / freq;
    auto med = [](std::vector<double>& v) -> double {
        if (v.empty()) return -1;
        std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
        return v[v.size() / 2];
    };
    auto gaps = [&](int a, int b) {
        std::vector<double> v;
        for (int64_t q : T[a]) {
            const int64_t q2 = next(b, q);
            if (q2 >= 0) v.push_back((double)(q2 - q) * toMs);
        }
        return v;
    };
    auto diffs = [&](int t) {
        std::vector<double> v;
        for (size_t i = 1; i < T[t].size(); ++i) v.push_back((double)(T[t][i] - T[t][i - 1]) * toMs);
        return v;
    };
    std::vector<double> kc, kp, k2b;
    for (int64_t c : T[2]) {
        const int64_t k = prev(1, c);
        if (k < 0) continue;
        kc.push_back((double)(c - k) * toMs);
        const int64_t l = next(6, c), w = next(3, l), p = next(5, w), b2 = next(4, p);
        if (p >= 0) kp.push_back((double)(p - k) * toMs);
        if (b2 >= 0) k2b.push_back((double)(b2 - k) * toMs);
    }
    std::vector<double> cl = gaps(2, 6), lw = gaps(6, 3), wb = gaps(3, 4), bp = gaps(4, 5), cc = diffs(2), pp = diffs(5);
    const double mK2b = med(k2b);
    InterlockedExchange(&g_tsInDispUs, mK2b >= 0 ? (LONG)(mK2b * 1000.0) : -1);
    if (!logIt) return;
    const double mKc = med(kc), mCl = med(cl), mLw = med(lw), mWb = med(wb), mBp = med(bp), mCc = med(cc), mPp = med(pp), mKp = med(kp);
    Log("stats latency (TS medians over the last %zu probes, ms): K->C input age %.2f | C->L update+limiter %.2f | L->W %.2f | "
        "W->B waitable wait %.2f | B->P render %.2f | C->C logic frame %.2f | P->P present %.2f | K->P sample->present %.2f | "
        "input->display B(n+1)-K(n) %.2f (+ half a refresh for mid-screen; vkd3d latency-1 semantics)",
        ev.size(), mKc, mCl, mLw, mWb, mBp, mCc, mPp, mKp, mK2b);
}

// ---- WS: 30 s wake summary. Real SetEvents per site, worker sleeps (per pool), wake checks at 40A4C split by the caller
// of JobWorkerPoll 40800, and the waste estimate from notes\worker-power.md: wasted ~= sleeps - checks from 3FE14 (a
// worker that wakes and goes straight back to sleep did one 3FE14 round that found nothing).
double g_sprTicksPerUs = 0.0;
void SprLog() {
    static uint64_t pgN, pgS, puN, puS, pgSl, puSl;
    const uint64_t gN = spr_gN, gS = spr_gSum, gM = spr_gMax, gSl = spr_gSlow, uN = spr_uN, uS = spr_uSum, uM = spr_uMax, uSl = spr_uSlow;
    const double t = g_sprTicksPerUs > 0 ? g_sprTicksPerUs : 1.0;
    auto avg = [&](uint64_t s, uint64_t n) { return n ? (double)s / (double)n / t : 0.0; };
    Log("stats spr: runtime gfx PSO creates %llu (+%llu in 30 s, avg %.0f us, max %.0f us, >=1 ms: %llu) | "
        "upload-heap dedicated fallbacks %llu (+%llu in 30 s, avg %.0f us, max %.0f us, >=1 ms: %llu)",
        (unsigned long long)gN, (unsigned long long)(gN - pgN), avg(gS - pgS, gN - pgN), (double)gM / t, (unsigned long long)gSl,
        (unsigned long long)uN, (unsigned long long)(uN - puN), avg(uS - puS, uN - puN), (double)uM / t, (unsigned long long)uSl);
    pgN = gN; pgS = gS; puN = uN; puS = uS; pgSl = gSl; puSl = uSl;
    (void)pgSl; (void)puSl;
}

void WakeStatsLog() {
    static uint64_t pSet[6], pSleeps, pPool[64], pChk[4], pEdK, pEdS;
    static bool init = false;
    uint64_t set[6], pool[64], chk[4];
    for (int i = 0; i < 6; ++i) set[i] = ws_set[i * 8];
    for (int i = 0; i < 64; ++i) pool[i] = ws_sleepPool[i];
    for (int i = 0; i < 4; ++i) chk[i] = ws_chk[i * 8];
    const uint64_t sleeps = ws_sleeps, edK = ed_kicked, edS = ed_skipped;
    if (init) {
        auto r = [](uint64_t a, uint64_t b) { return (unsigned long long)((a - b) / 30); };
        unsigned long long setAll = 0;
        for (int i = 0; i < 6; ++i) setAll += r(set[i], pSet[i]);
        // the five busiest pools
        int idx[64];
        for (int i = 0; i < 64; ++i) idx[i] = i;
        std::sort(idx, idx + 64, [&](int a, int b) { return pool[a] - pPool[a] > pool[b] - pPool[b]; });
        char pools[160] = "";
        size_t o = 0;
        for (int k = 0; k < 5 && pool[idx[k]] != pPool[idx[k]]; ++k)
            o += (size_t)snprintf(pools + o, sizeof(pools) - o, "%s%d:%llu", k ? " " : "", idx[k], r(pool[idx[k]], pPool[idx[k]]));
        const long long wasted = (long long)r(sleeps, pSleeps) - (long long)r(chk[0], pChk[0]);
        Log("stats wakes/s: SetEvent %llu (pop chain 40B0A %llu, task 40D2F %llu, wake-one 48C6B %llu / 48D06 %llu / 48DEC %llu, "
            "forward 48E3C %llu) | worker sleeps %llu (pools id:n %s) | wake checks by caller 3FE14 %llu, 403A0 %llu, "
            "207200 %llu, other %llu | wasted ~ %lld | ED Disp kicks %llu, skipped %llu",
            setAll, r(set[0], pSet[0]), r(set[1], pSet[1]), r(set[2], pSet[2]), r(set[3], pSet[3]), r(set[4], pSet[4]),
            r(set[5], pSet[5]), r(sleeps, pSleeps), pools, r(chk[0], pChk[0]), r(chk[1], pChk[1]), r(chk[2], pChk[2]),
            r(chk[3], pChk[3]), wasted, r(edK, pEdK), r(edS, pEdS));
    }
    init = true;
    memcpy(pSet, set, sizeof(set));
    memcpy(pPool, pool, sizeof(pool));
    memcpy(pChk, chk, sizeof(chk));
    pSleeps = sleeps;
    pEdK = edK;
    pEdS = edS;
}

// ---- gpuopt status: StatusLine() reports rates since its previous call, so only this thread calls it (every 2 s) and
// the mod menu reads the cached copy.
SRWLOCK g_gpuStatusLock = SRWLOCK_INIT;
char g_gpuStatus[256] = "";
void GpuOptRefresh() {
    if (gpuopt::GetState() == gpuopt::State::Off) return;
    char b[256];
    gpuopt::StatusLine(b, sizeof(b));
    AcquireSRWLockExclusive(&g_gpuStatusLock);
    snprintf(g_gpuStatus, sizeof(g_gpuStatus), "%s", b);
    ReleaseSRWLockExclusive(&g_gpuStatusLock);
}
const char* GpuOptStatus(char* buf, size_t n) {
    AcquireSRWLockShared(&g_gpuStatusLock);
    snprintf(buf, n, "%s", g_gpuStatus[0] ? g_gpuStatus : gpuopt::StateName(gpuopt::GetState()));
    ReleaseSRWLockShared(&g_gpuStatusLock);
    return buf;
}

// ---- TG governor (1.6.0). Runs on the Present-hook thread after each osd stats window (~0.5 s) with the rendered
// (real, FG excluded) fps and the engine's ticks/s. Publishes tg_cap, which caveLim applies as the limiter target while
// VRR is armed. Raises at once, lowers only after the target stayed >2 below the cap for 2 s. Never engages without
// L1 (the limiter slack would otherwise be spun, not slept) or without VRR armed (stock dt would slow the game).
void TgOnRates(double rendered, double ticks) {
    g_tgRendered = rendered;
    g_tgTicks = ticks;
    const ULONGLONG now = GetTickCount64();
    static const char* const kNames[] = {"off", "governing", "refused - L1 is not active (it must be installed and switched "
                                         "on; under Proton L1OnWine = 1)", "waiting - VRR not armed", "idle - nothing to govern"};
    auto set = [&](long st, uint32_t cap) {
        tg_cap = cap;
        const long old = InterlockedExchange(&g_tgState, st);
        if (old != st) Log("TG: %s%s", kNames[st], st == 1 ? "" : " (logic ticks: stock limiter)");
    };
    if (!g_tgEnable) { g_tgLowerSince = 0; set(0, 0); return; }
    if (!g_tgL1 || !g_l1Enable) { g_tgLowerSince = 0; set(2, 0); return; }
    if (!vrr_enable || !vrr_fpsOut) { g_tgLowerSince = 0; set(3, 0); return; }
    const int cap = (int)vrr_capCur;
    const int idx = vrr_tier == 30 ? 0 : vrr_tier == 120 ? 2 : 1;
    const int minF = vrr_maxNs[idx] ? (int)((1000000000ull + vrr_maxNs[idx] - 1) / vrr_maxNs[idx]) : 24;
    const int floorT = std::max(g_tgFloor, minF + 1);  // stay inside the VRR clamp (tier 60: 24 -> floor 30)
    int target = (int)std::ceil(rendered * (double)g_tgRatioPct / 100.0) + 2;
    target = std::max(target, floorT);
    if (rendered < 1.0 || target >= cap) { g_tgLowerSince = 0; set(4, 0); return; }
    const int cur = tg_cap ? (int)tg_cap : cap;
    int next = cur;
    if (target > cur) {
        next = target;
        g_tgLowerSince = 0;
    } else if (target + 2 < cur) {
        if (!g_tgLowerSince) g_tgLowerSince = now;
        else if (now - g_tgLowerSince >= 2000) { next = target; g_tgLowerSince = 0; }
    } else {
        g_tgLowerSince = 0;
    }
    if (next >= cap) set(4, 0);
    else set(1, (uint32_t)next);
}


// ---- VRR online guard (1.7.27): variable-dt timing is offline-only, so it is suspended while an online / co-op session
// is detected and restored afterwards. Signals: notes\online-session-detect.md #1 (net manager connected byte) and #2
// (play-mode object mode word). No hook, no call: only two data loads under SEH and a write of the caves' own
// vrr_enable switch (the same one the mod menu flips), so no register-safety exposure (notes\hook-register-audit.md).
static volatile long g_vrrOnlineState = 0;  // 0 offline/not watching, 1 suspended (online), 2 forced on, 3 forced off
static bool OnlineNow(bool* fault) {
    *fault = false;
    __try {
        const uint8_t* net = *reinterpret_cast<uint8_t* const*>(g_base + 0x45C84F8);
        if (net && net[0x1DC]) return true;
        const uint8_t* pm = *reinterpret_cast<uint8_t* const*>(g_base + 0x4709B50);
        if (pm && *reinterpret_cast<const int32_t*>(pm + 0x34) != 0) return true;
        return false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *fault = true;
        return true;  // can't read -> treat as online
    }
}
static DWORD WINAPI VrrOnlineWatch(LPVOID) {
    const int ticksToRearm = 50;  // 5 s of clean polls before restoring (a flicker must not re-arm mid-session)
    bool suspended = false;
    uint32_t saved = 1;
    int clean = 0;
    for (;;) {
        Sleep(100);
        bool fault;
        const bool on = OnlineNow(&fault);
        if (on) {
            clean = 0;
            if (!suspended) {
                suspended = true;
                saved = vrr_enable;
                vrr_enable = 0;
                g_vrrOnlineState = 1;
                Log("VRR: online session detected%s - variable-dt timing suspended (stock timing), will restore when offline", fault ? " (unreadable state)" : "");
            }
        } else if (suspended && ++clean >= ticksToRearm) {
            suspended = false;
            vrr_enable = saved;
            g_vrrOnlineState = 0;
            Log("VRR: back offline - variable-dt timing restored (%s)", saved ? "on" : "off as before");
        }
    }
}

// TG watchdog: drops the cap at once when TG is switched off, L1 goes off, VRR is switched off, or the game's presents
// stall for more than 250 ms (loading / no 3D frames: the governor gets no new rate then).
DWORD WINAPI TgWatchdog(LPVOID) {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    for (;;) {
        Sleep(100);
        if (!tg_cap) continue;
        bool off = !g_tgEnable || !g_tgL1 || !g_l1Enable || !vrr_enable;
        LARGE_INTEGER n;
        QueryPerformanceCounter(&n);
        const int64_t last = osd::LastPresentQpc();
        if (!off && (!last || n.QuadPart - last > f.QuadPart / 4)) off = true;
        if (off) {
            tg_cap = 0;
            g_tgLowerSince = 0;
        }
    }
}

bool BuildRepl(Item& it) {
    const gen::Site& s = *it.s;
    std::vector<uint8_t>& r = it.repl;
    r.clear();
    if (s.kind == gen::Kind::Bytes) {
        r.assign(s.repl, s.repl + s.len);
        // 1.7.30 LC = N: manager A `sbb r15,r15; and r15,imm8; add r15,imm8` -> and -(N-1), add N (boot flag 1 still gives 1,
        // 0 gives N); manager B `mov eax,imm32` -> N. patches.json holds N = 16; test_patterns proves the arithmetic 11..64.
        if (g_lcCapWanted != 16 && g_lcCapWanted >= 11 && g_lcCapWanted <= 64) {
            if (!strcmp(s.id, "LCa") && s.len == 12) {
                r[3] = (uint8_t)(int8_t)(1 - g_lcCapWanted);
                r[11] = (uint8_t)g_lcCapWanted;
            } else if (!strcmp(s.id, "LCb") && s.len == 5) {
                r[1] = (uint8_t)g_lcCapWanted;
            }
        }
        return true;
    }
    if (s.kind == gen::Kind::VPtr) {  // aligned data pointer -> the cave itself (no trampoline needed)
        const uint64_t p = (uint64_t)(uintptr_t)TargetFn(s.target);
        if (!p || s.len != 8) return false;
        for (int k = 0; k < 8; ++k) r.push_back((uint8_t)(p >> (8 * k)));
        return true;
    }
    if (s.kind == gen::Kind::Call || s.kind == gen::Kind::Jmp) {
        const bool jmp = s.kind == gen::Kind::Jmp;
        const int at = jmp ? 0 : s.callAt;
        for (int k = 0; k < at; ++k) r.push_back(0x90);
        int32_t rel;
        if (!Rel32(it.addr + at + 5, Trampoline(s.target), &rel)) return false;
        r.push_back(jmp ? 0xE9 : 0xE8);
        for (int k = 0; k < 4; ++k) r.push_back((uint8_t)(rel >> (8 * k)));
        static const uint8_t pad1[] = {0x90}, pad2[] = {0x66, 0x90}, pad3[] = {0x0F, 0x1F, 0x00},
                             pad5[] = {0x0F, 0x1F, 0x44, 0x00, 0x00};
        int pad = s.len - at - 5;
        const uint8_t* pp = nullptr;
        switch (pad) {
            case 0: break;
            case 1: pp = pad1; break;
            case 2: pp = pad2; break;
            case 3: pp = pad3; break;
            case 4: { static const uint8_t pad4[] = {0x0F,0x1F,0x40,0x00}; pp = pad4; break; }
            case 5: pp = pad5; break;
            default: return false;
        }
        if (pp) r.insert(r.end(), pp, pp + pad);
        return (int)r.size() == s.len;
    }
    if (s.kind == gen::Kind::RipLoad) {
        auto nd = reinterpret_cast<NearData*>(g_near);
        const void* slot = nullptr;
        switch (s.data) {
            case gen::Data::ticks120: slot = (const void*)&nd->ticks120; break;
            case gen::Data::ticks60: slot = (const void*)&nd->ticks60; break;
            case gen::Data::r1Timeout: slot = (const void*)&nd->r1Timeout; break;
            default: return false;
        }
        int32_t rel;
        if (!Rel32(it.addr + 6, slot, &rel)) return false;
        r.push_back(s.opcode[0]);
        r.push_back(s.opcode[1]);
        for (int k = 0; k < 4; ++k) r.push_back((uint8_t)(rel >> (8 * k)));
        return s.len == 6;
    }
    return false;
}

// ---- frozen write ----------------------------------------------------------------------------------------------
// Writes every item's replacement while all other threads are suspended and none of them is stopped strictly
// inside a site (a thread exactly at a site start, or at its end, is at an instruction boundary in both encodings).
// No heap / CRT / loader use while threads are suspended.
bool ApplyBatch(const char* name, std::vector<Item*>& items) {
    if (items.empty()) return true;
    const bool isRD = !strcmp(name, "RD");
    const bool strictMW = !strcmp(name, "MW") || isRD;
    using NextThreadFn = LONG(NTAPI*)(HANDLE, HANDLE, ACCESS_MASK, ULONG, ULONG, PHANDLE);
    NextThreadFn nextThread = nullptr;
    if (strictMW) {
        HMODULE nt = GetModuleHandleW(L"ntdll.dll");
        nextThread = nt ? reinterpret_cast<NextThreadFn>(GetProcAddress(nt, "NtGetNextThread")) : nullptr;
        if (!nextThread) { Log("MW: kernel thread census unavailable; stock retained"); return false; }
    }
    for (Item* it : items)
        if (!BuildRepl(*it)) { Log("%s: cannot encode %s (out of rel32 range?) - group skipped", name, it->s->id); return false; }

    std::vector<meshletwait::Site> mwSites;
    if (strictMW) for (Item* it : items) mwSites.push_back({it->addr, it->s->orig, it->repl.data(), it->repl.size()});
    std::vector<DWORD> oldProt(items.size());
    for (size_t i = 0; i < items.size(); ++i)
        if (!VirtualProtect(items[i]->addr, items[i]->s->len, PAGE_EXECUTE_READWRITE, &oldProt[i])) {
            Log("%s: VirtualProtect failed (%lu) - group skipped", name, GetLastError());
            for (size_t j = 0; j < i; ++j) { DWORD t; VirtualProtect(items[j]->addr, items[j]->s->len, oldProt[j], &t); }
            return false;
        }

    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    struct MWInstallDiag {
        unsigned snapshot=0, open=0, owner=0, suspend=0, context=0, rip=0;
        unsigned census=0, censusApi=0, censusUnknown=0, censusBound=0, original=0, rollback=0;
        unsigned terminated=0; DWORD terminatedTid=0;
        DWORD tid=0, error=0; ULONG status=0; uintptr_t ip=0;
        const char* stage="none"; const char* site="none";
    } md;
    bool written = false;
    int attempts = 0;
    for (; attempts < 200 && !written; ++attempts) {
        std::vector<HANDLE> hs;
        std::vector<DWORD> tids;
        bool freezeComplete = true;
        if (strictMW) {
            // Use the same kernel enumeration for the initial retained handles
            // and the frozen census. Toolhelp can omit a thread the kernel sees.
            // Retain returned handles directly rather than reopening a reusable TID.
            HANDLE previous = nullptr;
            bool previousRetained = false, complete = false;
            const ULONGLONG snapshotStart = GetTickCount64();
            for (unsigned n = 0; n < 4096 && GetTickCount64() - snapshotStart < 100; ++n) {
                HANDLE current = nullptr;
                const LONG status = nextThread(GetCurrentProcess(), previous,
                    SYNCHRONIZE | THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, 0, 0, &current);
                if (previous && !previousRetained) CloseHandle(previous);
                previous = current;
                previousRetained = false;
                if (static_cast<ULONG>(status) == 0x8000001Au) { complete = true; break; }
                if (status != 0 || !current) {
                    ++md.open; md.stage="NativeSnapshotApi"; md.status=static_cast<ULONG>(status); break;
                }
                const DWORD tid = GetThreadId(current);
                const DWORD state = WaitForSingleObject(current, 0);
                if (state == WAIT_OBJECT_0) {++md.terminated;md.terminatedTid=tid;continue;}
                if (state != WAIT_TIMEOUT) {++md.open;md.stage="NativeSnapshotWait";md.tid=tid;md.error=GetLastError();break;}
                if (!tid || GetProcessIdOfThread(current) != pid) {
                    ++md.owner; md.stage="NativeSnapshotOwner"; md.tid=tid; md.error=GetLastError(); break;
                }
                if (tid == self) continue; // cursor handle closed only after the next query
                hs.push_back(current);
                tids.push_back(tid);
                previousRetained = true;
            }
            if (previous && !previousRetained) CloseHandle(previous);
            if (!complete) { freezeComplete=false; ++md.snapshot; }
        } else {
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (snap != INVALID_HANDLE_VALUE) {
                THREADENTRY32 te{};
                te.dwSize = sizeof(te);
                BOOL ok = Thread32First(snap, &te);
                if (!ok) { freezeComplete = false; if(strictMW){++md.snapshot;md.stage="Thread32First";md.error=GetLastError();} }
                for (; ok; ok = Thread32Next(snap, &te))
                    if (te.th32OwnerProcessID == pid && te.th32ThreadID != self) tids.push_back(te.th32ThreadID);
                if (GetLastError() != ERROR_NO_MORE_FILES) { freezeComplete = false; if(strictMW){++md.snapshot;md.stage="Thread32Next";md.error=GetLastError();} }
                CloseHandle(snap);
            } else { freezeComplete = false; if(strictMW){++md.snapshot;md.stage="Snapshot";md.error=GetLastError();} }
            hs.reserve(tids.size());
            for (DWORD tid : tids)
                if (HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid)) {
                    if (strictMW && (GetThreadId(h) != tid || GetProcessIdOfThread(h) != pid)) {
                        ++md.owner;md.stage="ThreadOwner";md.tid=tid;md.error=GetLastError();
                        CloseHandle(h); freezeComplete = false; // reused foreign TID: never suspend it
                    } else hs.push_back(h);
                } else { freezeComplete = false; if(strictMW){++md.open;md.stage="OpenThread";md.tid=tid;md.error=GetLastError();} }
        }
        if (strictMW && !freezeComplete) {
            for (HANDLE h : hs) CloseHandle(h);
            Sleep(1 + attempts % 4);
            continue;
        }
        std::vector<uint8_t> suspended(hs.size(), 0);
        // ---- frozen section: no allocation from here until resume ----
        for (size_t i = 0; i < hs.size(); ++i) {
            if (strictMW) {
                const DWORD state=WaitForSingleObject(hs[i],0);
                if(state==WAIT_OBJECT_0){suspended[i]=2;++md.terminated;md.terminatedTid=tids[i];continue;}
                if(state!=WAIT_TIMEOUT){++md.suspend;md.stage="PreSuspendWait";md.error=GetLastError();md.tid=tids[i];continue;}
            }
            suspended[i] = (SuspendThread(hs[i]) != (DWORD)-1);
            if(strictMW && !suspended[i]){
                const DWORD suspendError=GetLastError();
                if(WaitForSingleObject(hs[i],0)==WAIT_OBJECT_0){suspended[i]=2;++md.terminated;md.terminatedTid=tids[i];}
                else {++md.suspend;md.stage="SuspendThread";md.error=suspendError;md.tid=tids[i];}
            }
        }
        bool clash = strictMW && std::find(suspended.begin(), suspended.end(), 0) != suspended.end();
        for (size_t i = 0; i < hs.size() && !clash; ++i) {
            if (suspended[i] != 1) continue;
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL;
            if (!GetThreadContext(hs[i], &ctx)) { if (strictMW) {clash = true;++md.context;md.stage="GetThreadContext";md.error=GetLastError();md.tid=tids[i];} continue; }
            for (Item* it : items) {
                uintptr_t a = (uintptr_t)it->addr;
                if (ctx.Rip > a && ctx.Rip < a + it->s->len) { clash = true; if(strictMW){++md.rip;md.stage="RipInsideSite";md.tid=tids[i];md.ip=ctx.Rip;md.site=it->s->id;} break; }
            }
        }
        if (!clash && strictMW) {
            ++md.census;
            // Kernel-only bounded census: detect threads born between the initial
            // native snapshot and suspension. No allocation/loader while frozen.
            HANDLE previous = nullptr;
            bool complete = false, censusRejected = false;
            const ULONGLONG censusStart = GetTickCount64();
            for (unsigned n = 0; n < 4096 && GetTickCount64() - censusStart < 100; ++n) {
                HANDLE current = nullptr;
                const LONG status = nextThread(GetCurrentProcess(), previous,
                    SYNCHRONIZE | THREAD_QUERY_LIMITED_INFORMATION, 0, 0, &current);
                if (previous) CloseHandle(previous);
                previous = current;
                if (static_cast<ULONG>(status) == 0x8000001Au) { complete = true; break; } // NO_MORE_ENTRIES
                if (status != 0 || !current) {++md.censusApi;md.stage="CensusApi";md.status=static_cast<ULONG>(status);censusRejected=true;break;}
                const DWORD observedTid=GetThreadId(current);
                const DWORD state=WaitForSingleObject(current,0);
                if(state==WAIT_OBJECT_0){++md.terminated;md.terminatedTid=observedTid;continue;}
                if(state!=WAIT_TIMEOUT){++md.censusApi;md.stage="CensusWait";md.tid=observedTid;md.error=GetLastError();censusRejected=true;break;}
                if (!meshletwait::KnownThread(observedTid, self, tids.data(), tids.size())) {++md.censusUnknown;md.stage="CensusUnknownThread";md.tid=observedTid;md.error=observedTid?0:GetLastError();censusRejected=true;break;}
            }
            if (previous) CloseHandle(previous);
            if (!complete) {clash = true;if(!censusRejected){++md.censusBound;md.stage="CensusTimeOrCountBound";}}
        }
        if (!clash && strictMW) {
            bool originalMismatch=false;
            for(Item* it:items) if(memcmp(it->addr,it->s->orig,it->repl.size())){originalMismatch=true;++md.original;md.stage="OriginalMismatch";md.site=it->s->id;}
            const auto result = meshletwait::Publish(mwSites.data(), mwSites.size(), [&]() {
                return FlushInstructionCache(GetCurrentProcess(), nullptr, 0) != FALSE;
            });
            written = result == meshletwait::Publication::Applied;
            if(!written && !originalMismatch){++md.rollback;md.stage="PublicationRollback";}
            (isRD ? g_rdInstallUncertain : g_mwInstallUncertain) = result == meshletwait::Publication::Uncertain;
            if (isRD ? g_rdInstallUncertain : g_mwInstallUncertain) attempts = 200; // never retry an uncertain write
        } else if (!clash) {
            for (Item* it : items) {
                if (it->s->kind == gen::Kind::VPtr)  // vtable slot: one aligned 8-byte store
                    InterlockedExchange64(reinterpret_cast<volatile LONG64*>(it->addr), *reinterpret_cast<const LONG64*>(it->repl.data()));
                else
                    memcpy(it->addr, it->repl.data(), it->repl.size());
            }
            FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
            written = true;
        }
        for (size_t i = 0; i < hs.size(); ++i)
            if (suspended[i] == 1) ResumeThread(hs[i]);
        // ---- end frozen section ----
        for (HANDLE h : hs) CloseHandle(h);
        if (!written) Sleep(1 + attempts % 4);
    }
    for (size_t i = 0; i < items.size(); ++i) { DWORD t; VirtualProtect(items[i]->addr, items[i]->s->len, oldProt[i], &t); }
    if(strictMW) Log("MW install diagnostics: snapshot=%u open=%u owner=%u suspend=%u context=%u rip=%u census=%u census_api=%u census_unknown=%u census_bound=%u original=%u rollback=%u last=%s tid=%lu error=%lu status=%08lX site=%s rip=%llX",
        md.snapshot,md.open,md.owner,md.suspend,md.context,md.rip,md.census,md.censusApi,md.censusUnknown,md.censusBound,md.original,md.rollback,md.stage,md.tid,md.error,md.status,md.site,static_cast<unsigned long long>(md.ip));
    if(strictMW) Log("MW thread lifetime: positively signaled observations=%u last_terminated_tid=%lu (no exit-code-only exclusion)",md.terminated,md.terminatedTid);
    if (!written) { Log("%s: not installed after %d attempts; %s", name, attempts, g_mwInstallUncertain && strictMW ? "UNRESOLVED rollback/cache flush - do not claim stock" : "no writes retained"); return false; }
    // MW readback and any rollback happened before resume, not after publication.
    // Other groups retain their existing behavior.
    if (!strictMW) for (Item* it : items)
        if (memcmp(it->addr, it->repl.data(), it->repl.size()) != 0) { Log("%s: READ-BACK MISMATCH at %s", name, it->s->id); return false; }
    for (Item* it : items) {
        char hex[3 * 32] = {};
        for (size_t k = 0; k < it->repl.size() && k < 31; ++k) snprintf(hex + 3 * k, 4, "%02X ", it->repl[k]);
        Log("  %-4s RVA %-8X patched: %s", it->s->id, (unsigned)(it->addr - g_base), hex);
    }
    Log("%s: applied (%zu site(s), %d attempt(s))", name, items.size(), attempts);
    return true;
}

// ---- VRR configuration (mirrors ft_vrr_patch.fsx v2 + maxFps / P9) ---------------------------------------------
void ConfigureVrr(const Config& c) {
    static const int tiers[3] = {30, 60, 120};
    const int maxFpsCfg = c.maxFps <= 0 ? 1000 : std::max(120, c.maxFps);
    for (int i = 0; i < 3; ++i) {
        int maxF = tiers[i] == 120 ? (maxFpsCfg > 120 ? maxFpsCfg : std::min(120, std::max(1, c.maxF[i])))
                                   : std::min(tiers[i], std::max(1, c.maxF[i]));
        int minF = std::clamp(c.minF[i], 1, maxF);
        vrr_arm[i] = c.arm[i] ? 1u : 0u;
        vrr_cap[i] = (uint32_t)maxF;
        vrr_minNs[i] = 1000000000ull / (uint64_t)maxF;  // shortest interval
        vrr_maxNs[i] = 1000000000ull / (uint64_t)minF;  // longest interval
        Log("VRR tier %3d: armed=%u clamp %d..%d fps, limiter cap %d", tiers[i], vrr_arm[i], minF, maxF, maxF);
    }
    // stock-equivalent values until the first P1 frame (tier assumed 60 there; overwritten every frame by caveFps)
    auto nd = reinterpret_cast<NearData*>(g_near);
    nd->ticks120 = 2;
    nd->ticks60 = 1;
    vrr_tier = 60;
    vrr_capCur = 60;
    vrr_pNear = (uint64_t)(uintptr_t)g_near;
    vrr_enable = 1;
}

bool InModule(uintptr_t a) { return a >= (uintptr_t)g_base && a < (uintptr_t)g_base + g_imageSize; }

bool g_wineEarly = false;
bool g_fgcOn = false;  // independent FG copy-job null guard

// fast-testing LiveCtl 'Tuning.<key> = v': [Tuning] values that the installed code reads at use time. PP_Period is the
// PP cave's mask (read on every sweep decision); the same-look values are read per clipmap visit / frame (samelook.cpp).
// Everything else in [Tuning] is consumed at boot (relaunch-required; notes\fast-testing.md has the table).
static bool LiveTuning(const char* key, const char* val, char* reply, size_t n) {
    if (!_stricmp(key, "PP_Period")) {
        const int v = atoi(val);
        if (v != 2 && v != 4 && v != 8 && v != 16) {
            snprintf(reply, n, "PP_Period must be 2, 4, 8 or 16 (got %s)", val);
            return false;
        }
        InterlockedExchange(reinterpret_cast<volatile LONG*>(&pp_mask), (LONG)(v - 1));
        snprintf(reply, n, "PP_Period = %d", v);
        Log("LIVE tuning: %s", reply);
        return true;
    }
    if (bkstats::SetTuning(key, val, reply, n)) return true;  // 1.7.58-bk1 KL_Div (bkstats.cpp)
    if (frameoverlap::SetTuning(key, val, reply, n)) return true;  // 1.7.59-fo1 FO / FO_Pct (frameoverlap.cpp)
    if (replayprof::SetTuning(key, val, reply, n)) return true;  // 1.7.59-rpf1 RPF on/off (replayprof.cpp)
    if (cbvupload::SetTuning(key, val, reply, n)) return true;  // 1.7.60-cbu1 CBS / CBN / CBT / CBSTATS (cbvupload.cpp)
    return samelook::SetTuning(key, val, reply, n);
}

// ---- main worker ------------------------------------------------------------------------------------------------
DWORD WINAPI Worker(LPVOID) {
    Config cfg = LoadConfig();
    g_skipOds = cfg.skipOds != 0;
    {  // 1.7.49 same-look package: ini only here; the VSMA / RCO groups are installed with the experimental patches
        samelook::StartInfo sl;
        sl.log = &Log;
        const std::wstring slIni = SelfPathWithExt(L".ini");
        sl.iniPath = slIni.c_str();
        samelook::Configure(sl);
        extraperf::StartInfo ep;  // 1.7.59 Extra performance: [ExtraPerf] levels (installed with the experimental groups)
        ep.log = &Log;
        ep.iniPath = slIni.c_str();
        extraperf::Configure(ep);
    }
    if (cfg.log) {
        std::wstring lp = SelfPathWithExt(L".log");
        g_log = _wfsopen(lp.c_str(), L"w", _SH_DENYWR);
    }
    Log(MOD_NAME " " MOD_VERSION " (built " __DATE__ ") - native port of the live-tested Nioh 3 engine patches");
    Log("OS: %s; ini keys set to -1 (auto) resolve by OS%s", IsWineHost() ? "Proton/Wine" : "native Windows", g_autoNote.empty() ? " - none are -1 in this ini" : ":");
    for (size_t p = 0, q; p < g_autoNote.size(); p = q + 1) { q = g_autoNote.find('\n', p); if (q == std::string::npos) q = g_autoNote.size(); Log("%s", g_autoNote.substr(p, q - p).c_str()); }
    if (!cfg.enabled) { Log("disabled in ini ([General] Enabled=0)"); return 0; }

    // one instance per process (e.g. both an ASI loader and Nioh3ModLoader present)
    wchar_t mtx[64];
    swprintf(mtx, 64, L"Local\\Nioh3EngineFix.%lu", GetCurrentProcessId());
    HANDLE hm = CreateMutexW(nullptr, FALSE, mtx);
    if (hm && GetLastError() == ERROR_ALREADY_EXISTS) { Log("another Nioh3EngineFix instance is already active - this copy does nothing"); return 0; }

    g_base = (uint8_t*)GetModuleHandleW(nullptr);
    auto nt = Nt(g_base);
    g_imageSize = nt->OptionalHeader.SizeOfImage;
    const bool known = nt->FileHeader.TimeDateStamp == gen::kBuildTimestamp && nt->OptionalHeader.SizeOfImage == gen::kBuildSizeOfImage;
    Log("Nioh3.exe at %p: TimeDateStamp %08lX SizeOfImage %zX -> %s", g_base, nt->FileHeader.TimeDateStamp, g_imageSize,
        known ? "known build (v2.02)" : "UNKNOWN BUILD");
    if (!known)
        Log("unknown build: each patch is installed only when its own bytes match; a raw address with no byte check stays off");

    // Diagnostic only: never feeds save-file admission; unknown builds never read RVAs.
    if (known) {
        if (HANDLE h = CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            Log("TITLEGATE v1: 100ms polling, stable >=1000ms; Enabled=0 logOnly=1");
            for (;;) { titlegate::Tick((uintptr_t)g_base, &Log); Sleep(100); }
        }, nullptr, 0, nullptr)) CloseHandle(h);
        else Log("TITLEGATE: worker creation failed (%lu); Enabled=0", GetLastError());
    }
    if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll")) g_wineEarly = GetProcAddress(ntdll, "wine_get_version") != nullptr;

    // PSO bytecode swap / shader dump (notes\gpu-post-exe.md): early, so the IAT slot swap beats the engine's device creation
    {
        psoswap::StartInfo pi;
        pi.log = &Log;
        std::wstring psoIni = SelfPathWithExt(L".ini");
        pi.iniPath = psoIni.c_str();
        // 1.7.31 PJ (psojournal.h, [Patches] PJ = 0): PSO journal + title-screen prewarm; psoswap reports the device
        pj::StartInfo ji;
        ji.log = &Log;
        ji.iniPath = psoIni.c_str();
        ji.base = (uintptr_t)g_base;
        ji.knownBuild = known;
        ji.wine = g_wineEarly;
        pj::Start(ji);
        pi.onDevice = &pj::OnDevice;
        pi.observeOnly = pj::Wanted(psoIni.c_str());
        pi.wine = nullptr;
        if (HMODULE nt = GetModuleHandleW(L"ntdll.dll"))
            if (auto wgv = reinterpret_cast<const char*(__cdecl*)()>(GetProcAddress(nt, "wine_get_version"))) pi.wine = wgv();
        psoswap::Start(pi);
        // 1.7.29 GI gather light-group probe / merge (gimerge.cpp; [Debug] GIProbe = 0, [Patches] GIMerge = 0): after
        // psoswap so its D3D12CreateDevice import hook chains psoswap's; does nothing with both keys 0
        gimerge::StartInfo gm;
        gm.log = &Log;
        gm.iniPath = psoIni.c_str();
        gm.wine = pi.wine;
        gimerge::Start(gm);
    }

    // ---- wait for SteamStub to decrypt .text (never touch the stub; just poll the bytes)
    // sentinels spread over .text: lowest site (W1), highest (P3g) and two in between
    const gen::Site* sentinels[4] = {nullptr, nullptr, nullptr, nullptr};
    for (int i = 0, k = 0; i < gen::kNumSites && k < 4; ++i) {
        const char* id = gen::kSites[i].id;
        if (!strcmp(id, "W1") || !strcmp(id, "P1") || !strcmp(id, "L1") || !strcmp(id, "P3g")) sentinels[k++] = &gen::kSites[i];
    }
    Pattern sentinelPat;
    ParsePattern(sentinels[0]->pattern, sentinelPat);
    // P12 (worker-pool penalty lift) must land before the engine sizes its pools, which happens once, soon after
    // the code is decrypted - so it is written from inside this wait loop the moment its bytes appear (known build
    // only; an unknown build can't be pattern-scanned in time).
    const gen::Site* p12 = nullptr;
    for (int i = 0; i < gen::kNumSites; ++i)
        if (!strcmp(gen::kSites[i].id, "P12")) p12 = &gen::kSites[i];
    bool p12Pending = cfg.p12 && known && p12;
    if (cfg.p12 && !known) Log("P12: skipped on an unknown build (it has to be written before the pools are sized)");
    auto tryP12 = [&]() {
        uint8_t* a = g_base + p12->rva;
        if (memcmp(a, p12->orig, p12->len) != 0) return;
        DWORD old, t;
        if (!VirtualProtect(a, p12->len, PAGE_EXECUTE_READWRITE, &old)) { Log("P12: VirtualProtect failed (%lu)", GetLastError()); p12Pending = false; return; }
        memcpy(a, gen::repl_P12, p12->len);
        VirtualProtect(a, p12->len, old, &t);
        FlushInstructionCache(GetCurrentProcess(), a, p12->len);
        p12Pending = false;
        Log("P12: worker-pool penalty lifted (KIDS_RESOURCE/StageUpdate keep 3 more threads on CPUs with <= 12 threads)");
    };
    // WC (WorkerCap, handhelds): lower the worker-pool base clamp 32 -> cap in SizeWorkerPools (1FE300B / 1FE3011),
    // so every weighted pool shrinks (Ally 16 threads: ~127 -> ~78 pool threads at cap 10). Boot-time like P12.
    const gen::Site* wca = nullptr;
    const gen::Site* wcb = nullptr;
    for (int i = 0; i < gen::kNumSites; ++i) {
        if (!strcmp(gen::kSites[i].id, "WCa")) wca = &gen::kSites[i];
        if (!strcmp(gen::kSites[i].id, "WCb")) wcb = &gen::kSites[i];
    }
    const DWORD logical = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    bool wcPending = cfg.workerCap > 0 && known && wca && wcb;
    if (wcPending && (cfg.workerCap < 6 || cfg.workerCap > 31)) { Log("WC: WorkerCap must be 6..31 - skipped"); wcPending = false; }
    if (wcPending && logical <= (DWORD)cfg.workerCap) { Log("WC: %lu logical CPUs <= WorkerCap %d - nothing to cap", logical, cfg.workerCap); wcPending = false; }
    auto tryWC = [&]() {
        uint8_t* a = g_base + wca->rva;
        uint8_t* b = g_base + wcb->rva;
        if (memcmp(a, wca->orig, wca->len) != 0 || memcmp(b, wcb->orig, wcb->len) != 0) return;
        DWORD old, t;
        if (!VirtualProtect(a, 0x10, PAGE_EXECUTE_READWRITE, &old)) { Log("WC: VirtualProtect failed (%lu)", GetLastError()); wcPending = false; return; }
        b[1] = (uint8_t)cfg.workerCap;  // `mov al,cap` first: a half-written state still behaves like stock
        a[3] = (uint8_t)cfg.workerCap;  // `cmp r11d,cap`
        VirtualProtect(a, 0x10, old, &t);
        FlushInstructionCache(GetCurrentProcess(), a, 0x10);
        wcPending = false;
        Log("WC: worker-pool base capped at %d (of %lu logical CPUs)", cfg.workerCap, logical);
    };
    // AS0 (opt-in async-compute experiment, notes\gpu-scheduling.md A.4): the renderer init (2269EC4) switches async slot 0
    // (KIDS_ASync_LLCulling_Effect) off on every non-NVIDIA GPU at 226A490 (`cmp [rbp+70h],10DEh ; je`). One byte at 226A497
    // (74 -> EB) keeps it enabled, as on NVIDIA. It only matters if written BEFORE that init: the init copies the table to
    // g_AsyncSlotTableLive (.bss 0x45C0460, zero until init; entry i = {u8 enable, f32, u32 id==i}), so a non-zero id in
    // entry 1 means the init already ran and the patch must not (and cannot usefully) be applied.
    const gen::Site* as0s = nullptr;
    for (int i = 0; i < gen::kNumSites; ++i)
        if (!strcmp(gen::kSites[i].id, "AS0")) as0s = &gen::kSites[i];
    s_asyncKnown = known;
    bool as0Pending = cfg.as0 && known && as0s;
    if (cfg.as0 && !known) Log("AS0: skipped on an unknown build (it has to be written before the renderer initialises)");
    // 1.7.44 AS (asyncslots.cpp): engine async slots added before the renderer init (slot 0 = the AS0 byte + 11 / 12 table bytes)
    static asyncslots::BootCfg asBoot;  // read again by asyncslots::Start after decryption
    bool asPending = known && asyncslots::BootWanted(SelfPathWithExt(L".ini").c_str(), asBoot, &Log);
    if (!known && GetPrivateProfileIntW(L"Patches", L"AS", 0, SelfPathWithExt(L".ini").c_str()))
        Log("AS: skipped on an unknown build (it has to be written before the renderer initialises)");
    if (as0Pending && asPending && (asBoot.want & 1u)) {
        as0Pending = false;
        Log("AS0: superseded by AS (AS_Slots has slot 0)");
    }
    auto tryAS0 = [&]() {
        uint8_t* a = g_base + as0s->rva;
        if (memcmp(a, as0s->orig, as0s->len) != 0) return;  // still encrypted / not unpacked yet
        if (*reinterpret_cast<volatile uint32_t*>(g_base + 0x45C0460 + 12 * 1 + 8) != 0) {
            Log("AS0: renderer already initialised (async slot table is live) - NOT applied");
            as0Pending = false; return;
        }
        DWORD old, t;
        if (!VirtualProtect(a, as0s->len, PAGE_EXECUTE_READWRITE, &old)) { Log("AS0: VirtualProtect failed (%lu)", GetLastError()); as0Pending = false; return; }
        memcpy(a, gen::repl_AS0, as0s->len);
        VirtualProtect(a, as0s->len, old, &t);
        FlushInstructionCache(GetCurrentProcess(), a, as0s->len);
        as0Pending = false;
        s_as0Applied = true;
        Log("AS0: slot 0 forced on (async effects queue on a non-NVIDIA GPU; EXPERIMENT - KT disabled it on purpose: faster, slower, one-frame effect lag, corruption or a GPU hang are all possible)");
    };
    // 1.7.25 TSY (TrySync pool threads) / SH (synchronous archive handles): boot-time like P12/WC (readfix.cpp)
    readfix::BootCfg rfBoot;
    bool rfPending = known && readfix::BootWanted(SelfPathWithExt(L".ini").c_str(), rfBoot);
    const int rfCap = wcPending ? cfg.workerCap : 0;
    auto tryRF = [&]() { rfPending = readfix::BootTry(reinterpret_cast<uintptr_t>(g_base), rfBoot, rfCap, logical, &Log); };
    bool vsPending = known;  // 1.7.48 VS: D3D12CreateDevice IAT hook before the device exists ([Profiler] MemStats = 2)
    const std::wstring vsIni = SelfPathWithExt(L".ini");
    const DWORD t0 = GetTickCount();
    bool ready = false;
    while (!ready) {
        if (vsPending) vsPending = !memstats::BootIat(reinterpret_cast<uintptr_t>(g_base), vsIni.c_str(), &Log);
        if (p12Pending) tryP12();
        if (wcPending) tryWC();
        if (as0Pending) tryAS0();
        if (asPending) asPending = asyncslots::BootTry(reinterpret_cast<uintptr_t>(g_base), asBoot, &Log);
        if (rfPending) tryRF();
        if (known) {
            ready = true;
            for (const gen::Site* s : sentinels)
                if (!s || memcmp(g_base + s->rva, s->orig, s->len) != 0) ready = false;
        } else {
            uint8_t* f = nullptr;
            ready = FindPattern(sentinelPat, ExecRanges(), &f) == 1;
        }
        if (ready || GetTickCount() - t0 > (DWORD)cfg.decryptWaitSec * 1000) break;
        Sleep((p12Pending || wcPending || as0Pending || asPending || rfPending) ? 0 : known ? 20 : 250);
    }
    if (vsPending) vsPending = !memstats::BootIat(reinterpret_cast<uintptr_t>(g_base), vsIni.c_str(), &Log);
    if (rfPending) tryRF();
    if (rfPending) readfix::BootGiveUp(rfBoot, &Log);
    if (p12Pending) tryP12();
    if (wcPending) tryWC();
    if (as0Pending) tryAS0();
    if (as0Pending) Log("AS0: site not in its original state after decryption - skipped");
    if (asPending) asPending = asyncslots::BootTry(reinterpret_cast<uintptr_t>(g_base), asBoot, &Log);
    if (asPending) Log("AS: gate / AsyncSlotGetCtx / slot table not in the v2.02 state after decryption - boot slots skipped");
    if (asBoot.gateWritten) s_as0Applied = true;
    if (wcPending) Log("WC: sites not in their original state after decryption - skipped");
    if (p12Pending) Log("P12: site not in its original state after decryption - skipped");
    if (ready) Sleep(200);  // let the unpacker finish whatever it does after the last byte
    Log(ready ? "code is decrypted (%lu ms after start)" : "code did not reach the expected state within %lu ms - scanning anyway, each site is still verified",
        GetTickCount() - t0);

    // ---- resolve every site of the enabled groups
    std::vector<Range> ranges = ExecRanges();
    std::vector<Item> items(gen::kNumSites);
    auto groupOn = [&](const char* g) {
        if (!strcmp(g, "W1")) return cfg.w1 != 0;
        if (!strcmp(g, "W2")) return cfg.w2 != 0;
        if (!strcmp(g, "VRR")) return cfg.vrr != 0;
        if (!strcmp(g, "VRRMENU")) return cfg.vrr != 0 && cfg.menuQuantize != 0;
        if (!strcmp(g, "W3")) return cfg.w3 != 0;
        if (!strcmp(g, "R1")) return cfg.r1 != 0;
        if (!strcmp(g, "L1")) return cfg.l1 != 0;
        if (!strcmp(g, "FG")) return cfg.fg != 0;
        if (!strcmp(g, "WK")) return cfg.wk != 0 || (cfg.wakeStats != 0 && known);  // WakeStats splits the checks here
        // experimental: a group installs only when its own bytes match (DepsOk). knownBuild is not required.
        if (!strcmp(g, "B")) return cfg.b != 0;
        if (!strcmp(g, "PP")) return cfg.pp != 0;
        if (!strcmp(g, "FPC")) return cfg.fpc != 0;
        if (!strcmp(g, "AABB")) return cfg.aabb != 0;
        if (!strcmp(g, "M1")) return cfg.m1 != 0;
        if (!strcmp(g, "M2")) return cfg.m2 != 0 || cfg.m2s != 0;
        if (!strcmp(g, "M2S")) return cfg.m2s != 0 && cfg.m2 == 0;
        if (!strcmp(g, "J1")) return cfg.j1 != 0;
        if (!strcmp(g, "X1")) return cfg.x1 != 0;
        if (!strcmp(g, "E1")) return cfg.e1 != 0;
        if (!strcmp(g, "W6") || !strcmp(g, "W6W")) return cfg.w6 != 0;
        if (!strcmp(g, "RD")) return cfg.rd != 0;
        if (!strcmp(g, "MW")) return cfg.mw != 0;
        if (!strcmp(g, "S1")) return cfg.s1 != 0;
        if (!strcmp(g, "IK")) return cfg.ik != 0 || cfg.ts != 0;  // the K/C sites carry the TS stamps too
        if (!strcmp(g, "TS")) return cfg.ts != 0;
        if (!strcmp(g, "WS")) return cfg.wakeStats != 0 || cfg.wg != 0;  // WG rides the WS sites
        if (!strcmp(g, "ED")) return cfg.ed != 0;
        if (!strcmp(g, "GF")) return cfg.gf != 0;
        if (!strcmp(g, "WL")) return cfg.wl != 0;
        if (!strcmp(g, "PM")) return cfg.pmLog != 0;
        if (!strcmp(g, "DS")) return cfg.ds != 0;
        if (!strcmp(g, "LK")) return cfg.lk != 0;
        if (!strcmp(g, "MA")) return cfg.ma != 0;
        if (!strcmp(g, "BPO")) return cfg.bpo != 0;
        if (!strcmp(g, "CP")) return cfg.cp != 0 || cfg.dl != 0;  // DL is a second site of the CP group
        if (!strcmp(g, "MQ")) return cfg.mq != 0;
        if (!strcmp(g, "PT1")) return cfg.pt1 != 0;
        if (!strcmp(g, "SPF")) return cfg.spf != 0;
        if (!strcmp(g, "RWb")) return cfg.rwb != 0;
        if (!strcmp(g, "ESM1")) return cfg.esm1 != 0;
        if (!strcmp(g, "MPCS")) return cfg.mpcs != 0;
        if (!strcmp(g, "VSMA")) return samelook::VsmaIni() != 0;  // 1.7.49 same-look package
        if (!strcmp(g, "RCO")) return samelook::RcoIni() != 0 || extraperf::Wanted();  // 1.7.59: Extra performance rides on RCO
        if (!strcmp(g, "GIS") || !strcmp(g, "EPL") || !strcmp(g, "EPF")) return extraperf::Wanted();  // 1.7.59 XGI / XLOD / XPRT
        if (!strcmp(g, "MLP")) return false;  // MeshletProbe recorder entry sites: verified offline (test_patterns), hooked by mlprobe.cpp
        if (!strcmp(g, "GL1")) return cfg.gl1 != 0;
        if (!strcmp(g, "LC")) return cfg.lc != 0;
        if (!strcmp(g, "LW")) return cfg.lw != 0;
        if (!strcmp(g, "SPR")) return cfg.spr != 0;
        if (!strcmp(g, "LG")) return false;  // LoadGates probe sites: verified offline (test_patterns), hooked by loadstats.cpp
        if (!strcmp(g, "FP")) return false;  // FnProbe entry sites: verified offline (test_patterns), hooked by fnprobe.cpp
        if (!strcmp(g, "AC")) return false;
        if (!strcmp(g, "BN")) return false;  // SkipBootNotice boot-sequencer sites: verified offline (test_patterns), hooked by bootnotice.cpp
        if (!strcmp(g, "SBM")) return false;  // 1.7.57 scene-buffer mirror call site: verified offline (test_patterns), hooked by scenemirror.cpp
        if (!strcmp(g, "CBU")) return false;  // 1.7.60-cbu1 CBV upload call sites: verified offline (test_patterns), hooked by cbvupload.cpp
        if (!strcmp(g, "PCM")) return false;  // 1.7.58 PSO lookup memo call site: verified offline (test_patterns), hooked by psomemo.cpp
        if (!strcmp(g, "VP")) return false;  // KeepRunningUnfocused IsAppActive call sites: verified offline (test_patterns), hooked by vpad.cpp
        if (!strcmp(g, "TR")) return false;  // Tour tick + save-writer sites: verified offline (test_patterns), hooked by tour.cpp
        if (!strcmp(g, "RP")) return false;  // ReadProbe / EP sites: verified offline (test_patterns), hooked by readprobe.cpp  // AutoContinueLoad tip-page site: verified offline (test_patterns), hooked by autocont.cpp
        if (!strcmp(g, "GP")) return cfg.gpuProf != 0 || cfg.gpuMarkers != 0;  // check sites: verified, never written
        if (!strcmp(g, "JT")) return cfg.jt != 0;  // 1.7.40 job-timing probe (measurement only)
        if (!strcmp(g, "FGC")) return cfg.fgc != 0 || (g_wineEarly && cfg.fgToggle == 3);  // 1.7.44
        if (!strcmp(g, "FGP")) return false;  // retired menu pause switch
        return false;  // P12 is handled in the decrypt-wait loop above
    };
    Log("resolving sites:");
    for (int i = 0; i < gen::kNumSites; ++i) {
        const gen::Site& s = gen::kSites[i];
        items[i].s = &s;
        if (!groupOn(s.group)) continue;
        if (Resolve(s, ranges, known, items[i])) Log("  %-4s OK at RVA %X", s.id, (unsigned)(items[i].addr - g_base));
    }
    auto collect = [&](const char* g) {
        std::vector<Item*> v;
        for (Item& it : items)
            if (!strcmp(it.s->group, g)) v.push_back(&it);
        return v;
    };
    auto allFound = [](const std::vector<Item*>& v) {
        for (Item* it : v)
            if (!it->found) return false;
        return !v.empty();
    };

    // ---- near page (trampolines + data the game reads RIP-relatively)
    g_near = AllocNear((uintptr_t)g_base, (uintptr_t)g_base + g_imageSize, kNearSize);
    if (!g_near) { Log("could not allocate memory within 2 GB of Nioh3.exe - only in-place patches (W1/W2) will be applied"); }
    else {
        memset(g_near, 0, kNearSize);
        WriteTrampolines();
        Log("near page at %p", g_near);
    }

    int applied = 0, skipped = 0;
    std::string skippedNames;  // OSD popup
    osd::State l1State = osd::State::Off, f1State = osd::State::Off;
    auto run = [&](const char* name, std::vector<Item*> v, bool ok) {
        if (v.empty()) return false;
        if (!ok || !ApplyBatch(name, v)) { ++skipped; skippedNames += name; skippedNames += ' '; Log("%s: SKIPPED", name); return false; }
        ++applied;
        return true;
    };

    // W1: in place
    if (cfg.w1) { auto v = collect("W1"); run("W1", v, allFound(v)); }
    // W2a-d: in place, independent
    if (cfg.w2)
        for (Item* it : collect("W2")) {
            std::vector<Item*> one{it};
            run(it->s->id, one, it->found);
        }
    // VRR: all-or-nothing
    bool vrrDone = false;
    if (cfg.vrr) {
        auto v = collect("VRR");
        bool ok = allFound(v) && g_near;
        if (!allFound(v)) Log("VRR: not every site was found - the whole group is skipped (a partial VRR install would run the game at the wrong speed)");
        if (ok) ConfigureVrr(cfg);
        // keep the scripts' safe order: data-only users first, P1 (activation) last; written together anyway
        vrrDone = run("VRR", v, ok);
        if (vrrDone && cfg.menuQuantize) {
            auto m = collect("VRRMENU");
            bool mok = allFound(m);
            if (mok) {
                uint8_t* a = m[0]->addr;
                uintptr_t callee = (uintptr_t)(a + 5 + *(int32_t*)(a + 1));
                mok = InModule(callee);
                if (mok) vrr_getDelta60 = callee;
            }
            run("VRR menu quantize (P7)", m, mok);
        }
    }
    // W3: message pump
    bool w3Done = false, r1Done = false, wkDone = false;
    if (cfg.w3) {
        auto v = collect("W3");
        bool ok = allFound(v) && g_near;
        if (ok) {
            uint8_t* a = v[0]->addr;  // FF 15 disp32
            auto slot = reinterpret_cast<PeekMessageA_t*>(a + 6 + *(int32_t*)(a + 2));
            ok = InModule((uintptr_t)slot) && *slot != nullptr;
            if (ok) {
                g_peekSlot = slot;
                g_w3Ms = (DWORD)cfg.w3Ms;
                g_w3Enable = 1;
            } else Log("W3: IAT slot looks wrong - skipped");
        }
        w3Done = run("W3", v, ok);
    }
    // R1: render-begin wait (two sites)
    if (cfg.r1) {
        auto v = collect("R1");
        bool ok = allFound(v) && g_near;
        if (ok) reinterpret_cast<NearData*>(g_near)->r1Timeout = (uint32_t)cfg.r1Ms;
        r1Done = run("R1", v, ok);
    }
    // L1: frame limiter. Under Wine/Proton the high-resolution timer flag may be accepted but not honoured, which
    // would make the limiter overshoot - so L1 stays off there unless L1OnWine=1.
    const char* wine = nullptr;
    if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll"))
        if (auto wgv = reinterpret_cast<const char*(__cdecl*)()>(GetProcAddress(ntdll, "wine_get_version"))) wine = wgv();
    if (wine) Log("running under Wine/Proton %s", wine);
    if (wine && g_w3Enable && cfg.w3WineSleep) {
        g_w3UseSleep = 1;
        Log("W3: Wine/Proton -> empty-queue wait is Sleep(%lu ms) (Wine returns from MsgWaitForMultipleObjectsEx at once)",
            g_w3Ms ? g_w3Ms : 1);
    }
    if (cfg.l1 && wine && !cfg.l1OnWine) { Log("L1: skipped under Wine/Proton (set [Patches] L1OnWine=1 to try it)"); l1State = osd::State::WineSkipped; }
    if (cfg.l1 && (!wine || cfg.l1OnWine)) {
        auto v = collect("L1");
        bool ok = allFound(v) && g_near;
        if (ok) {
            g_l1Timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
            if (!g_l1Timer) {
                Log("L1: high-resolution waitable timer not supported here (older Windows / Wine-Proton?) - L1 skipped, stock limiter kept");
                ok = false;
            }
        }
        if (ok) {
            uint8_t* a = v[0]->addr;  // 33 D2 48 8B CE E8 rel32
            uintptr_t callee = (uintptr_t)(a + 10 + *(int32_t*)(a + 6));
            ok = InModule(callee);
            if (ok) {
                l1_callee = callee;
                g_l1MarginUs = (uint64_t)cfg.l1MarginUs;
                g_l1MinUs = (uint64_t)cfg.l1MinUs;
                g_l1Enable = 1;
            }
        }
        l1State = run("L1", v, ok) ? osd::State::On : osd::State::Failed;
    }
    // F1: FSR frame-generation presenter pacing wait (only runs with FSR frame generation on). Same timer caveat
    // under Wine/Proton as L1.
    if (cfg.fg && wine && !cfg.fgOnWine) { Log("F1: skipped under Wine/Proton (set [Patches] FGOnWine=1 to try it)"); f1State = osd::State::WineSkipped; }
    if (cfg.fg && (!wine || cfg.fgOnWine)) {
        auto v = collect("FG");
        bool ok = allFound(v) && g_near;
        if (ok) {
            uint8_t* a = v[0]->addr;  // E8 rel32 -> the stock waitUntil(QPC): push rbx; ...; QueryPerformanceCounter loop
            uintptr_t callee = (uintptr_t)(a + 5 + *(int32_t*)(a + 1));
            ok = InModule(callee);
            if (!ok) Log("F1: call target outside the module - skipped");
        }
        if (ok) {
            g_fgTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
            LARGE_INTEGER f;
            QueryPerformanceFrequency(&f);
            g_qpcFreq = f.QuadPart;
            if (!g_fgTimer || g_qpcFreq <= 0) {
                Log("F1: high-resolution waitable timer not supported here - F1 skipped, stock wait kept");
                ok = false;
            } else {
                g_fgMarginTicks = (int64_t)cfg.l1MarginUs * g_qpcFreq / 1000000;
                g_fgThreshTicks = (int64_t)(cfg.l1MarginUs + cfg.l1MinUs) * g_qpcFreq / 1000000;
                g_fgEnable = 1;
            }
        }
        f1State = run("F1", v, ok) ? osd::State::On : osd::State::Failed;
    }
    // WK: job-system wake gate (skip the futile "wake one more worker" when no queue of the pool holds work)
    if (cfg.wk || (cfg.wakeStats && known)) {
        auto v = collect("WK");
        bool ok = allFound(v) && g_near;
        if (ok) wk_enable = cfg.wk ? 1u : 0u;  // installed only for WakeStats: the gate stays off (stock)
        wkDone = run("WK", v, ok);
    }

    // ---- EXPERIMENTAL CPU-power patches (ini default 0). Known build only: the caves use v2.02 engine offsets.
    // Each group: every site found + original bytes + its patches.json deps, then one frozen write (ApplyBatch).
    bool exDone[menu::kNumExp] = {};
    auto findItem = [&](const char* id) -> Item* {
        for (Item& it : items)
            if (!strcmp(it.s->id, id)) return &it;
        return nullptr;
    };
    auto calleeOf = [](uint8_t* a) { return (uintptr_t)(a + 5 + *(int32_t*)(a + 1)); };  // E8 rel32 at a
    auto expGroup = [&](const char* g, bool want, const std::function<bool()>& prep) -> bool {
        if (!want) return false;
        auto v = collect(g);
        bool ok = allFound(v) && g_near != nullptr && DepsOk(g);
        if (ok && prep) ok = prep();
        return run(g, v, ok);
    };
    if (cfg.rd) {
        HMODULE kb = GetModuleHandleW(L"kernelbase.dll");
        auto wait = kb ? reinterpret_cast<meshletwait::WaitFn>(GetProcAddress(kb, "WaitOnAddress")) : nullptr;
        auto wake = kb ? reinterpret_cast<meshletwait::WakeFn>(GetProcAddress(kb, "WakeByAddressAll")) : nullptr;
        const bool installed = expGroup("RD", wait && wake, [&]() {
            rd_resume = (uint64_t)(uintptr_t)(g_base + 0x1A7C3F5);
            g_rd.wait=wait; g_rd.wake=wake; g_rd.enabled=true;
            return true;
        });
        if (!installed && !g_rdInstallUncertain) g_rd.enabled=false;
        Log("RD: %s (paired renderer drain; startup-only; full native predicate retained)",
            installed ? "ON" : g_rdInstallUncertain ? "UNRESOLVED publication; helpers retained" : "stock retained");
    }
    // MW reader + completion writer are ONE atomic group: never timeout-only install.
    if (cfg.mw) {
        HMODULE kb = GetModuleHandleW(L"kernelbase.dll");
        auto wait = kb ? reinterpret_cast<meshletwait::WaitFn>(GetProcAddress(kb, "WaitOnAddress")) : nullptr;
        auto wake = kb ? reinterpret_cast<meshletwait::WakeFn>(GetProcAddress(kb, "WakeByAddressAll")) : nullptr;
        const bool installed = expGroup("MW", wait && wake, [&]() {
            g_mw.wait = wait; g_mw.wake = wake; g_mw.enabled = true;
            return true;
        });
        if (!installed && !g_mwInstallUncertain) g_mw.enabled = false;
        Log("MW: %s (paired meshlet completion wait/wake; startup-only; no job work skipped)",
            installed ? "ON" : g_mwInstallUncertain ? "UNRESOLVED publication; keep paired helpers valid" : "skipped, stock retained");
    }
    // Independent FG copy-job crash guard; does not change game settings.
    if (cfg.fgc || (g_wineEarly && cfg.fgToggle == 3)) {
        g_fgcOn = expGroup("FGC", true, [&]() {
            Item* i1 = findItem("FGC1");
            Item* i2 = findItem("FGC2");
            const uintptr_t t = (uintptr_t)(g_base + i1->s->expectRva);
            if (calleeOf(i1->addr) != t || calleeOf(i2->addr) != t) return false;
            fgc_target = t;
            return true;
        });
        Log("FGC: frame-generation copy-job null guard %s (%s)", g_fgcOn ? "installed" : "NOT installed",
            cfg.fgc ? "[Patches] FGC = 1" : "[Menu] FrameGenToggle = 3 under Proton");
    }
    LARGE_INTEGER qf;
    QueryPerformanceFrequency(&qf);
    // W6 first (the main thread's present wait - the biggest Ally lever)
    if (cfg.w6 && known) {
        HMODULE kb = GetModuleHandleW(L"kernelbase.dll");
        if (!kb) kb = LoadLibraryW(L"api-ms-win-core-synch-l1-2-0.dll");
        auto wait = kb ? reinterpret_cast<WaitOnAddress_t>(GetProcAddress(kb, "WaitOnAddress")) : nullptr;
        auto wake = kb ? GetProcAddress(kb, "WakeByAddressAll") : nullptr;
        if (!wait || !wake) Log("W6: WaitOnAddress/WakeByAddressAll not available - skipped");
        exDone[menu::kExpW6] = expGroup("W6", wait && wake, [&]() {
            g_waitOnAddress = wait;
            w6_wakeAll = (uint64_t)(uintptr_t)wake;
            g_w6SpinTicks = (int64_t)cfg.w6SpinUs * qf.QuadPart / 1000000;
            g_w6TimeoutMs = (DWORD)cfg.w6TimeoutMs;
            g_w6Enable = cfg.w6User ? 1u : 0u;  // LoadOnlyCpuFixes: off outside loads unless W6 is set in the ini
            return true;
        });
        if (exDone[menu::kExpW6]) {
            if (expGroup("W6W", true, nullptr))
                Log("W6: hybrid present wait on (spin %d us, then WaitOnAddress, %d ms timeout); waker at the Present "
                    "thread's flag clear installed", cfg.w6SpinUs, cfg.w6TimeoutMs);
            else
                Log("W6: waker NOT installed - waits end by the %d ms WaitOnAddress timeout instead", cfg.w6TimeoutMs);
        }
    }
    exDone[menu::kExpB] = expGroup("B", cfg.b != 0, [&]() {
        gal_resume = (uint64_t)(uintptr_t)(findItem("B")->addr + 5);  // 3BC1C: the original binary search
        gal_enable = 1;
        return true;
    });
    exDone[menu::kExpPP] = expGroup("PP", cfg.pp != 0, [&]() {
        const uintptr_t t = calleeOf(findItem("PP")->addr);
        if (t != (uintptr_t)(g_base + findItem("PP")->s->expectRva)) return false;
        pp_target = t;
        pp_mask = (uint32_t)(cfg.ppPeriod - 1);
        pp_enable = 1;
        return true;
    });
    exDone[menu::kExpFPC] = expGroup("FPC", cfg.fpc != 0, [&]() {
        auto ntH = Nt(g_base);
        auto sec = IMAGE_FIRST_SECTION(ntH);
        for (WORD i = 0; i < ntH->FileHeader.NumberOfSections; ++i, ++sec)
            if (!memcmp(sec->Name, ".rdata\0", 7)) {
                fpc_rlo = (uint64_t)(uintptr_t)(g_base + sec->VirtualAddress);
                fpc_rhi = fpc_rlo + sec->Misc.VirtualSize;
            }
        if (!fpc_rlo) { Log("FPC: no .rdata section - skipped"); return false; }
        fpc_gaddr = (uint64_t)(uintptr_t)(g_base + kRvaGfxGlobal);
        fpc_orig = (uint64_t)(uintptr_t)(g_base + findItem("FPC")->s->expectRva);
        fpc_tid = 0;  // filled in by the stats thread once [App]Game exists and frames advance
        fpc_enable = 1;
        return true;
    });
    exDone[menu::kExpAABB] = expGroup("AABB", cfg.aabb != 0, [&]() {
        aabb_resume = (uint64_t)(uintptr_t)(findItem("AABB")->addr + 5);  // 6EA55 push rbp
        aabb_enable = cfg.aabb == 1 ? 1u : 0u;  // 2 = installed with the switch off (live A/B from the menu / vpad)
        return true;
    });
    if (exDone[menu::kExpAABB]) Log("AABB: SSE character AABB cave installed, switch %s", aabb_enable ? "ON" : "off");
    exDone[menu::kExpM1] = expGroup("M1", cfg.m1 != 0, [&]() {
        uint8_t* a = findItem("M1")->addr;
        m1_stock = (uint64_t)(uintptr_t)(a + 7);              // 49138
        m1_resume = (uint64_t)(uintptr_t)(a + (0x491AC - 0x49131));  // 491AC
        m1_enable = 1;
        return true;
    });
    exDone[menu::kExpM2] = expGroup("M2", cfg.m2 != 0, nullptr);
    // Reuse M2e's single manifest row so overlapping byte patches cannot coexist.
    // Both seed loads reach an unconditional retained locked CAS before any other memory access.
    // Full M2 takes precedence if explicitly requested; do not partially combine the two groups.
    if (cfg.m2s && cfg.m2) Log("M2S: skipped because full M2 is requested; no extra seed site installed");
    if (cfg.m2s && !cfg.m2) {
        std::vector<Item*> seed{findItem("M2e"), findItem("M2S0")};
        bool ok = known && seed[0] && seed[1] && allFound(seed) && DepsOk("M2S");
        if (run("M2S", seed, ok))
            Log("M2S: ON (2 completion seed loads; retained locked CAS barriers; startup-only)");
    }
    exDone[menu::kExpJ1] = expGroup("J1", cfg.j1 != 0, nullptr);
    exDone[menu::kExpX1] = expGroup("X1", cfg.x1 != 0, [&]() {
        uint8_t* a = findItem("X1")->addr;  // FF 15 disp32
        uint8_t* slot = a + 6 + *(int32_t*)(a + 2);
        if (!InModule((uintptr_t)slot) || !*(void**)slot) { Log("X1: IAT slot looks wrong"); return false; }
        x1_iat = (uint64_t)(uintptr_t)slot;
        x1_pmgr = (uint64_t)(uintptr_t)(g_base + kRvaPadMgr);
        x1_enable = 1;
        return true;
    });
    exDone[menu::kExpE1] = expGroup("E1", cfg.e1 != 0, [&]() {
        const uintptr_t enq = calleeOf(findItem("E1B")->addr);
        if (enq != (uintptr_t)(g_base + findItem("E1B")->s->expectRva)) return false;
        e1_enq = enq;
        e1_aback = (uint64_t)(uintptr_t)(findItem("E1A")->addr + 5);  // 7E82B9
        e1_mask = (uint32_t)(cfg.e1N - 1);
        e1_scale = (float)cfg.e1N;
        e1_enable = 1;
        return true;
    });
    exDone[menu::kExpS1] = expGroup("S1", cfg.s1 != 0, nullptr);
    // RWb (notes\stutter.md 3.5): 22 code bytes at ResStateWaitSwap's cold spin; the wake moves behind the Sleep(0) wrap test
    // (one futile pool-1 wake per 256 spins instead of one per spin). Next launch only; no menu item.
    expGroup("RWb", cfg.rwb != 0, nullptr);
    // ESM1 (notes\gpu-resolution-audit.md): EsmLevelFromRes 147324 maps RS+16D8 (2048 at SHADOW HIGH) to the directional-ESM
    // array index 3 (2048^2); the 3-byte patch at 14733E (setge al -> xor eax,eax) makes it index 2 (1024^2) for any size >= 1024.
    // The ESM only feeds volumetric fog in-scattering and LPV indirect. Next launch only; no menu item.
    expGroup("ESM1", cfg.esm1 != 0, nullptr);
    // GL1 (1.7.14, notes\gamelogic.md): software prefetch of the task 3 entries ahead in the two mission task-list walks
    // (E266D1 proximity pass, E26690 task-state pass). No architectural effect. Next launch only; no menu item.
    if (expGroup("GL1", cfg.gl1 != 0, [&]() {
            uint8_t* a = findItem("GL1a")->addr;  // cmp rbx,[rbp] (4) / je rel32 (6)
            uint8_t* b = findItem("GL1b")->addr;  // cmp rbx,[rdi] (3) / je rel8 (2)
            gl1_resA = (uint64_t)(uintptr_t)(a + 10);
            gl1_endA = (uint64_t)(uintptr_t)(a + 10 + *(int32_t*)(a + 6));
            gl1_resB = (uint64_t)(uintptr_t)(b + 5);
            gl1_endB = (uint64_t)(uintptr_t)(b + 5 + *(int8_t*)(b + 4));
            if (gl1_endA != (uint64_t)(uintptr_t)(g_base + 0xE2686F) || gl1_endB != (uint64_t)(uintptr_t)(g_base + 0xE266A4)) {
                Log("GL1: loop exits are not where the cave expects them");
                return false;
            }
            return true;
        }))
        Log("GL1: task-list prefetch installed (E266D1 proximity pass, E26690 task-state pass)");
    // LC (1.7.14, notes\loading.md 3 row 4): resource-load managers keep up to 16 requests in flight instead of 10
    // (the boot-flag value 1 stays 1). Same requests and queue order, only more of them started at once.
    // 1.7.30: LC = 1 -> 16 (as before), LC = 11..64 -> that cap (Ally G8 = manager A draining ~1000 queued requests at cap 10,
    // notes\loading-ally-fixed.md 8.9); 2..10 or > 64 -> refused.
    const bool lcValid = cfg.lc == 1 || (cfg.lc >= 11 && cfg.lc <= 64);
    if (cfg.lc != 0 && !lcValid) Log("LC: %d is not 1 or 11..64 - not applied", cfg.lc);
    g_lcCapWanted = cfg.lc == 1 ? 16 : cfg.lc;
    if (expGroup("LC", cfg.lc != 0 && lcValid, nullptr)) {
        g_loaderCap = g_lcCapWanted;
        Log("LC: resource-load managers A/B in-flight cap 10 -> %d (ini LC = %d)", g_lcCapWanted, cfg.lc);
    }
    // LW (1.7.14, notes\loading.md 3 row 5): the managers' Sleep(1) polls wait on an event that finished ResourceStepJobs
    // set, still with the 1 ms timeout as the upper bound.
    if (expGroup("LW", cfg.lw != 0, [&]() {
            HMODULE k = GetModuleHandleW(L"kernel32.dll");
            auto pw = k ? GetProcAddress(k, "WaitForSingleObject") : nullptr;
            auto ps = k ? GetProcAddress(k, "Sleep") : nullptr;
            auto pe = k ? GetProcAddress(k, "SetEvent") : nullptr;
            HANDLE ev = (pw && ps && pe) ? CreateEventW(nullptr, FALSE, FALSE, nullptr) : nullptr;
            if (!ev) { Log("LW: kernel32 entry points / event not available"); return false; }
            lw_event = (uint64_t)(uintptr_t)ev;
            lw_pWait = (uint64_t)(uintptr_t)pw;
            lw_pSleep = (uint64_t)(uintptr_t)ps;
            lw_pSet = (uint64_t)(uintptr_t)pe;
            lw_resSig = (uint64_t)(uintptr_t)(findItem("LWs")->addr + 8);  // 1343D3D
            return true;
        }))
        Log("LW: loader managers wait on an event (1 ms timeout) instead of Sleep(1); ResourceStepJob completion signals it");
    // SPR (notes\stutter.md): count/time runtime graphics-PSO creates and upload-heap fallbacks. Measurement only.
    static bool s_sprDone = false;
    s_sprDone = expGroup("SPR", cfg.spr != 0, [&]() {
        Item* g = findItem("SPg");
        Item* u = findItem("SPu");
        const uintptr_t gf = calleeOf(g->addr), uf = calleeOf(u->addr);
        if (gf != (uintptr_t)(g_base + g->s->expectRva) || uf != (uintptr_t)(g_base + u->s->expectRva)) return false;
        LARGE_INTEGER q0, q1;  // rdtsc ticks per ms
        QueryPerformanceCounter(&q0);
        const uint64_t t0 = __rdtsc();
        Sleep(20);
        QueryPerformanceCounter(&q1);
        const uint64_t t1 = __rdtsc();
        const double us = (double)(q1.QuadPart - q0.QuadPart) * 1e6 / (double)qf.QuadPart;
        const double perUs = us > 0 ? (double)(t1 - t0) / us : 0.0;
        if (perUs < 100.0 || perUs > 10000.0) { Log("SPR: rdtsc calibration looks wrong (%.1f ticks/us)", perUs); return false; }
        g_sprTicksPerUs = perUs;
        spr_slow = (uint64_t)(perUs * 1000.0);
        spr_gfxFn = (uint64_t)gf;
        spr_ufFn = (uint64_t)uf;
        return true;
    });

    // ---- input latency (notes\input-latency.md): IK late input kick (sites K 5AB6CF + C 5ACAAA) and TS timestamp
    // probes (L/W/B/P; K and C stamp too). The K/C caves are installed for either key; with IK off they behave stock.
    static bool s_ikDone = false, s_tsDone = false;
    s_ikDone = expGroup("IK", cfg.ik != 0 || cfg.ts != 0, [&]() {
        Item* k = findItem("IK");
        Item* c = findItem("IKc");
        const uintptr_t kick = calleeOf(k->addr), wait1 = calleeOf(c->addr);
        if (kick != (uintptr_t)(g_base + k->s->expectRva) || wait1 != (uintptr_t)(g_base + c->s->expectRva)) return false;
        uint8_t* w = reinterpret_cast<uint8_t*>(wait1);  // 86C018: lea rcx,[padStageEvent] @+17h; call KtglEventWaitFor @+1Eh
        const uintptr_t ev = (uintptr_t)(w + 0x1E + *(int32_t*)(w + 0x1A));
        const uintptr_t waitN = calleeOf(w + 0x1E);
        if (!InModule(ev) || !InModule(waitN)) { Log("IK: pad-stage event / wait function out of the module"); return false; }
        lat_inputKick = kick;
        lat_padWait1 = wait1;
        lat_padEvent = ev;
        lat_padWaitN = waitN;
        lat_qpcNow = (uint64_t)(uintptr_t)(g_base + 0x205530);  // QpcNow (deps-checked)
        lat_cap = (uint64_t)cfg.ikCapMs;
        lat_ik = cfg.ik ? 1u : 0u;
        return true;
    });
    if (cfg.ts && known && !s_ikDone) Log("TS: needs the IK sites (the K/C stamps) - skipped");
    if (s_ikDone)
        s_tsDone = expGroup("TS", cfg.ts != 0, [&]() {
            Item* b = findItem("TSb");
            Item* p = findItem("TSp");
            const uintptr_t marker = calleeOf(b->addr);
            if (marker != (uintptr_t)(g_base + b->s->expectRva)) return false;
            uint8_t* slot = p->addr + 6 + *(int32_t*)(p->addr + 2);  // FF 15 disp32 -> SetEvent IAT slot
            if (!InModule((uintptr_t)slot) || !*(void**)slot) { Log("TS: SetEvent IAT slot looks wrong"); return false; }
            lat_latMarker = marker;
            lat_iatSetEvent = (uint64_t)(uintptr_t)slot;
            lat_ts = 1;
            return true;
        });
    if (s_ikDone) Log("IK: input kick %s (pad-stage wait cap %d ms)%s%s", cfg.ik ? "at the logic-frame start" : "stock (switch off)",
                      cfg.ikCapMs, s_tsDone ? "; TS probes on" : "", s_tsDone && cfg.tsForPad ? " (for [Debug] PadLatency)" : "");
    exDone[menu::kExpIK] = s_ikDone;
    exDone[menu::kExpTS] = s_tsDone;

    // ---- worker wakes (notes\worker-power.md): WS counters (measurement only) and ED EffectDisp empty-list skip
    static bool s_wsDone = false;
    s_wsDone = expGroup("WS", cfg.wakeStats != 0 || cfg.wg != 0, [&]() {
        auto slotOf = [&](const char* id) -> uint8_t* {  // FF 15 disp32 at the site
            uint8_t* a = findItem(id)->addr;
            uint8_t* s = a + 6 + *(int32_t*)(a + 2);
            return InModule((uintptr_t)s) && *(void**)s ? s : nullptr;
        };
        uint8_t* se = slotOf("WS0");
        uint8_t* wt = slotOf("WSs");
        if (!se || !wt) { Log("WS: IAT slot looks wrong"); return false; }
        for (const char* id : {"WS1", "WS2", "WS3", "WS4", "WS5"})
            if (slotOf(id) != se) { Log("WS: %s does not call the same SetEvent slot", id); return false; }
        ws_iatSetEvent = (uint64_t)(uintptr_t)se;
        ws_iatWait = (uint64_t)(uintptr_t)wt;
        ws_ret[0] = (uint64_t)(uintptr_t)(g_base + 0x3FF5E);  // the 40800 calls in 3FE14 / 403A0 / 207200 (deps-checked)
        ws_ret[1] = (uint64_t)(uintptr_t)(g_base + 0x403D7);
        ws_ret[2] = (uint64_t)(uintptr_t)(g_base + 0x20723D);
        ws_enable = cfg.wakeStats ? 1u : 0u;  // installed only for WG: counting off (live switch in the menu)
        if (cfg.wg) {
            // rdtsc ticks per microsecond for WG_WindowUs (the window only ever makes the gate more stock-like)
            LARGE_INTEGER q0, q1;
            QueryPerformanceCounter(&q0);
            const uint64_t t0 = __rdtsc();
            Sleep(20);
            QueryPerformanceCounter(&q1);
            const uint64_t t1 = __rdtsc();
            const double us = (double)(q1.QuadPart - q0.QuadPart) * 1e6 / (double)qf.QuadPart;
            const double perUs = us > 0 ? (double)(t1 - t0) / us : 0.0;
            if (perUs < 100.0 || perUs > 10000.0) { Log("WG: rdtsc calibration looks wrong (%.1f ticks/us) - WG not installed", perUs); }
            else {
                g_wgWindowTsc = (uint64_t)(perUs * cfg.wgWindowUs);
                g_wgMax = cfg.wgMaxPending;
                g_wgEnable = cfg.wg == 1 ? 1u : 0u;  // 2 = installed, switched off (live A/B)
                wg_installed = 1;
            }
        }
        return true;
    });
    if (s_wsDone && !wkDone) Log("WS: the wake-check caller split needs the WK site (40A4C) - not installed, split stays 0");
    if (s_wsDone && wg_installed)
        Log("WG: wake gate on the wake-one sites 48C6B / 48D06 installed (switch %s, MaxPending %ld, window %d us); worker "
            "sleep 1A6BC35 wrapped", g_wgEnable ? "ON" : "off", g_wgMax, cfg.wgWindowUs);
    if (cfg.wg && known && !s_wsDone) Log("WG: needs the WS sites - not installed");
    exDone[menu::kExpWS] = s_wsDone;
    exDone[menu::kExpWG] = s_wsDone && wg_installed;
    exDone[menu::kExpED] = expGroup("ED", cfg.ed != 0, [&]() {
        Item* k = findItem("EDk");
        Item* j = findItem("EDj");
        const uintptr_t kick = calleeOf(k->addr), join = calleeOf(j->addr);
        if (kick != (uintptr_t)(g_base + k->s->expectRva) || join != (uintptr_t)(g_base + j->s->expectRva)) return false;
        ed_kickFn = kick;
        ed_joinFn = join;
        ed_enable = cfg.ed == 1 ? 1u : 0u;  // 2 = installed with the switch off (live A/B from the menu)
        return true;
    });
    if (exDone[menu::kExpED]) Log("ED: EffectDisp empty-list skip installed, switch %s", ed_enable ? "ON" : "off");

    // ---- GF: GPU-fence Sleep(0) spins (7D9E66 main-thread FenceWait, 117D9F worker fence poll) -> event wait
    static bool s_gfDone = false;
    s_gfDone = expGroup("GF", cfg.gf != 0, [&]() {
        g_gfTls = TlsAlloc();
        if (g_gfTls == TLS_OUT_OF_INDEXES) { Log("GF: TlsAlloc failed"); return false; }
        g_gfTimeoutMs = (DWORD)cfg.gfTimeoutMs;
        g_gfEnable = cfg.gfUser ? 1u : 0u;  // LoadOnlyCpuFixes: off outside loads unless GF is set in the ini
        return true;
    });
    exDone[menu::kExpGF] = s_gfDone;
    if (s_gfDone) Log("GF: GPU-fence waits use SetEventOnCompletion + an event wait (%lu ms rounds) instead of Sleep(0)", g_gfTimeoutMs);

    // ---- WL: close the previous frame's frame-latency waitable handle (4A5338; notes\memory-leaks.md 5.3)
    static bool s_wlDone = false;
    s_wlDone = expGroup("WL", cfg.wl != 0, [&]() {
        HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
        auto ch = k32 ? GetProcAddress(k32, "CloseHandle") : nullptr;
        if (!ch) { Log("WL: CloseHandle not found"); return false; }
        wl_closeHandle = (uint64_t)(uintptr_t)ch;
        wl_enable = 1;
        return true;
    });
    exDone[menu::kExpWL] = s_wlDone;

    // ---- 1.6.0 opt-in items ---------------------------------------------------------------------------------------
    // DS: the main loop's 1 ms retry spin sleeps on its own high-resolution timer (needs L1: the timer is proven usable
    // on this system only when L1 is installed - under Proton that means L1OnWine = 1)
    static bool s_dsDone = false;
    if (cfg.ds && known && l1State != osd::State::On)
        Log("DS: needs L1 installed (high-resolution timer; under Proton set L1OnWine = 1) - skipped");
    s_dsDone = expGroup("DS", cfg.ds != 0 && l1State == osd::State::On, [&]() {
        g_dsTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!g_dsTimer) { Log("DS: high-resolution waitable timer not available"); return false; }
        g_dsMarginUs = cfg.dsMarginUs;
        g_qpcFreqDs = qf.QuadPart;
        g_dsEnable = cfg.ds == 1 ? 1u : 0u;
        return true;
    });
    if (s_dsDone) Log("DS: 1 ms retry spin of the main loop (1A74BDA) sleeps on a timer, margin %d us (switch %s)", cfg.dsMarginUs,
                      g_dsEnable ? "ON" : "off");
    exDone[menu::kExpDS] = s_dsDone;
    // LK: 18 locked "atomic loads" -> plain loads (code bytes, next launch)
    exDone[menu::kExpLK] = expGroup("LK", cfg.lk != 0, nullptr);
    // MA (1.7.39): the same idiom on the main thread ([App]Game): meshlet bookkeeping 4AA34 / 4B104 / 419A8 and the per-draw
    // residency read in DrawEmit 1E835B. 25 in-place byte sites, all-or-nothing, code bytes (next launch), no menu row.
    if ((exDone[menu::kExpMA] = expGroup("MA", cfg.ma != 0, nullptr)))
        Log("MA: 25 main-thread locked atomic loads -> plain loads (4AA34 x5, 4B104 x3, 419A8 x16, 1E835B x1)");
    // BPO (1.7.40): BoundsFromPartObbs 71C60 on the logic thread (notes\bounds-partobbs.md). Live switch; 2 = installed off.
    exDone[menu::kExpBPO] = expGroup("BPO", cfg.bpo != 0, [&]() {
        if (!IsProcessorFeaturePresent(37 /*PF_SSE4_1_INSTRUCTIONS_AVAILABLE*/)) { Log("BPO: needs SSE4.1"); return false; }
        // V: the six 71C60 call sites must call Vec4CopyW0 174610 (the cave falls back to it)
        const uintptr_t v = calleeOf(findItem("BPOV1")->addr);
        if (v != (uintptr_t)(g_base + findItem("BPOV1")->s->expectRva)) { Log("BPO: 72310 does not call 174610"); return false; }
        bpo_vStock = v;
        // quaternion memo: 2^BPO_TableBits x 32-byte seqlock entries, zero = empty (default 16384 = 512 KB, never freed)
        const uint64_t entries = 1ull << cfg.bpoTableBits;
        void* tbl = VirtualAlloc(nullptr, (SIZE_T)(entries * 32), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!tbl) { Log("BPO: table allocation failed"); return false; }
        bpo_tbl = (uint64_t)(uintptr_t)tbl;
        bpo_imask = entries - 1;
        bpo_resQS = (uint64_t)(uintptr_t)(findItem("BPOQA")->addr + 6);                // 71F04
        bpo_resQH = (uint64_t)(uintptr_t)(findItem("BPOQA")->addr + (0x7206F - 0x71EFE));  // 7206F
        bpo_resQB = (uint64_t)(uintptr_t)(findItem("BPOQB")->addr + (0x7206F - 0x72053));  // 7206F
        bpo_resL1 = (uint64_t)(uintptr_t)(findItem("BPOL1")->addr + 5);                // 71DB7
        bpo_resL2 = (uint64_t)(uintptr_t)(findItem("BPOL2")->addr + 6);                // 1A7AE6
        bpo_mask = (uint32_t)cfg.bpoMask;
        bpo_enable = cfg.bpo == 1 ? 1u : 0u;
        return true;
    });
    if (exDone[menu::kExpBPO])
        Log("BPO: BoundsFromPartObbs caves installed (quaternion memo %s with %llu entries, plain ready loads %s, Vec4CopyW0 copy %s), "
            "switch %s", (bpo_mask & 1) ? "yes" : "no", (unsigned long long)(bpo_imask + 1), (bpo_mask & 2) ? "yes" : "no",
            (bpo_mask & 4) ? "yes" : "no", bpo_enable ? "ON" : "off");
    // CP / SPF: worker loop prefetch caves (live switches; 2 = installed, switched off)
    exDone[menu::kExpCP] = expGroup("CP", cfg.cp != 0 || cfg.dl != 0, [&]() {
        cp_resume = (uint64_t)(uintptr_t)(findItem("CP")->addr + 7);  // D8187
        cp_enable = cfg.cp == 1 ? 1u : 0u;
        // DL (same group, second site D83F4 `call D8740`): the callee must be the one the cave tail-calls
        Item* dl = findItem("DL");
        if (calleeOf(dl->addr) != (uintptr_t)(g_base + dl->s->expectRva)) { Log("DL: D83F4 does not call D8740"); return false; }
        dl_fn = (uint64_t)calleeOf(dl->addr);
        dl_resume = (uint64_t)(uintptr_t)(g_base + 0xD8400);
        dl_enable = cfg.dl == 1 ? 1u : 0u;
        dl_count = cfg.cpuStats ? 1u : 0u;
        return true;
    });
    static bool s_dlDone = false, s_mqDone = false, s_ptDone = false, s_cpuStats = false;
    static bool s_mpcsDone = false;
    s_cpuStats = cfg.cpuStats != 0;
    s_dlDone = exDone[menu::kExpDL] = exDone[menu::kExpCP];
    exDone[menu::kExpSPF] = expGroup("SPF", cfg.spf != 0, [&]() {
        uint8_t* a = findItem("SPF")->addr;  // 48 8B 05 rel32: mov rax,[rip+rel] = g_CollisionWorld
        const uintptr_t gw = (uintptr_t)(a + 7 + *(int32_t*)(a + 3));
        if (gw != (uintptr_t)(g_base + 0x45B9E58)) { Log("SPF: g_CollisionWorld reference differs"); return false; }
        spf_gworld = gw;
        spf_resume = (uint64_t)(uintptr_t)(a + 7);  // DB8C6
        spf_enable = cfg.spf == 1 ? 1u : 0u;
        return true;
    });
    if (exDone[menu::kExpCP] || exDone[menu::kExpSPF])
        Log("CP/SPF: worker prefetch caves installed: CP %s, SPF %s", !exDone[menu::kExpCP] ? "-" : cp_enable ? "ON" : "off",
            !exDone[menu::kExpSPF] ? "-" : spf_enable ? "ON" : "off");
    if (exDone[menu::kExpDL]) Log("DL: dead first part-map lookup (D83F4, CP group) switch %s", dl_enable ? "ON" : "off");
    // MQ (1.7.12): bit-exact SSE MatrixFromScaleQuatTrans (88780); the cave falls back to the stock body when off
    s_mqDone = exDone[menu::kExpMQ] = expGroup("MQ", cfg.mq != 0, [&]() {
        mq_resume = (uint64_t)(uintptr_t)(findItem("MQ")->addr + 10);  // 8878A
        if (!IsProcessorFeaturePresent(37 /*PF_SSE4_1_INSTRUCTIONS_AVAILABLE*/)) { Log("MQ: needs SSE4.1"); return false; }
        mq_enable = cfg.mq == 1 ? 1u : 0u;
        mq_count = cfg.cpuStats ? 1u : 0u;
        return true;
    });
    if (exDone[menu::kExpMQ]) Log("MQ: MatrixFromScaleQuatTrans SSE cave installed, switch %s", mq_enable ? "ON" : "off");
    // MPCS (1.7.47, notes\meshlet-frontend.md 3): with the `mpc` build of 0x29ee0902.file MeshletPreculling 5301FC records MPC #28-#31
    // (1 <= cnt <= 1024) and #26 (#23 dispatched) only as no-ops; the caves drop those dispatches + their UAV barrier tokens via the
    // engine's own bind-failed paths. Refused without that exact file (sha256). Next launch only; no menu item. 2 = installed, switch off.
    if (cfg.mpcs) {
        char why[200];
        const bool fileOk = mlprobe::MpcFileOk(why, sizeof why);
        if (!fileOk) Log("MPCS: NOT installed - %s (install the mpc item first: gpu-shaders\\apply_sweep.ps1 -Item mpc)", why);
        s_mpcsDone = expGroup("MPCS", fileOk, [&]() {
            uint8_t* a = findItem("MPCSa")->addr;
            uint8_t* b = findItem("MPCSb")->addr;
            uint8_t* c = findItem("MPCSc")->addr;
            if (a + 0x11 != b || c - b != 0x53C) { Log("MPCS: sites are not where the caves expect them"); return false; }
            mpcs_resB = (uint64_t)(uintptr_t)(b + 8);                // 530FEC: #28 PSO bind
            mpcs_skipB = (uint64_t)(uintptr_t)(g_base + 0x5313C5);  // deps-checked: the #24 section
            mpcs_resC = (uint64_t)(uintptr_t)(c + 8);                // 531528: #26 PSO bind
            mpcs_skipC = (uint64_t)(uintptr_t)(g_base + 0x5315C5);  // deps-checked: the #26 bind-failed path
            mpcs_enable = cfg.mpcs == 1 ? 1u : 0u;
            return true;
        });
        if (s_mpcsDone)
            Log("MPCS: MPC tail skip installed (%s), switch %s: MeshletPreculling drops #28-#31 when 1 <= cnt <= 1024 and #26 when #23 was dispatched",
                why, mpcs_enable ? "ON" : "off");
    }
    // 1.7.49 VSMA (samelook.cpp, notes\same-look-package.md): VSM batch-loop draws of waiting tiles past the marked + front tiles
    // on 1 visit out of VSMA_K. Live row; 2 = installed, switch off. Original call targets verified against the expected RVAs.
    if (samelook::VsmaIni()) {
        const bool ok = expGroup("VSMA", true, [&]() {
            Item* a = findItem("VSMA1");
            Item* b = findItem("VSMA2");
            Item* c = findItem("VSMA3");
            const uintptr_t ta = calleeOf(a->addr), tb = calleeOf(b->addr), tc = calleeOf(c->addr);
            if (ta != (uintptr_t)(g_base + a->s->expectRva) || tb != (uintptr_t)(g_base + b->s->expectRva) ||
                tc != (uintptr_t)(g_base + c->s->expectRva)) {
                Log("VSMA: call targets differ from v2.02");
                return false;
            }
            samelook::SetVsmaTargets(ta, tb, tc, (uintptr_t)(g_base + 0x8F041C));  // 8F041C deps-checked
            return true;
        });
        if (!ok) Log("VSMA: NOT installed");
    }
    // 1.7.49 LPVR / SKYR / FOGT / CLDR (samelook.cpp): render-config overrides after ApplyGraphicsSettings. One live row each.
    if (samelook::RcoIni() || extraperf::Wanted()) {
        const bool ok = expGroup("RCO", true, [&]() { return true; });
        if (ok) {
            if (samelook::RcoIni()) samelook::SetRcoInstalled();
            if (extraperf::Wanted()) extraperf::SetRcoInstalled();
        } else {
            Log("RCO: render-config overrides NOT installed (update-rate knobs / Extra performance render-config options)");
        }
    }
    // 1.7.59 Extra performance (extraperf.cpp, notes\extra-performance-settings.md): GI gather scale, meshlet LOD scale, GPU
    // particle share. All levels 0 = pass-through; the menu rows switch them live. Callees verified against the v2.02 RVAs.
    if (extraperf::Wanted()) {
        const bool gi = expGroup("GIS", true, [&]() {
            const char* ids[4] = {"GIS1", "GIS2", "GIS3", "GIS4"};
            for (const char* id : ids) {
                Item* it = findItem(id);
                if (calleeOf(it->addr) != (uintptr_t)(g_base + it->s->expectRva)) {
                    Log("XGI: %s callee differs from v2.02", id);
                    return false;
                }
            }
            extraperf::SetGiInstalled(calleeOf(findItem("GIS1")->addr));
            return true;
        });
        if (!gi) Log("XGI: GI gather scale NOT installed");
        const bool lod = expGroup("EPL", true, [&]() {
            Item* it = findItem("EPL1");
            const uintptr_t t = calleeOf(it->addr);
            if (t != (uintptr_t)(g_base + it->s->expectRva)) {
                Log("XLOD: EPL1 callee differs from v2.02");
                return false;
            }
            extraperf::SetLodInstalled(t);
            return true;
        });
        if (!lod) Log("XLOD: meshlet LOD scale NOT installed");
        const bool fx = expGroup("EPF", true, [&]() {
            // movss dword ptr [rip+disp32], xmmN (F3 0F 11 /r, 8 bytes): the global = site + 8 + disp32
            uint8_t* a = findItem("EPF1")->addr;
            uint8_t* b = findItem("EPF2")->addr;
            void* sim = a + 8 + *(int32_t*)(a + 4);
            void* draw = b + 8 + *(int32_t*)(b + 4);
            if (sim != (void*)(g_base + 0x4566008) || draw != (void*)(g_base + 0x4566000)) {
                Log("XPRT: fraction globals differ from v2.02");
                return false;
            }
            extraperf::SetFxInstalled(sim, draw);
            return true;
        });
        if (!fx) Log("XPRT: GPU particle share NOT installed");
    }
    // PT1 (1.7.12): TextureStreamRequestTiles holds the pool lock once per texture. A live switch only gates new
    // textures (the bytes are never restored mid-loop), so switching off is always safe.
    s_ptDone = exDone[menu::kExpPT1] = expGroup("PT1", cfg.pt1 != 0, [&]() {
        auto slotOf = [&](const char* id) -> uint8_t* {  // FF 15 disp32 at the site
            uint8_t* a = findItem(id)->addr;
            return a + 6 + *(int32_t*)(a + 2);
        };
        uint8_t* sa = slotOf("PT1a");
        uint8_t* sr = slotOf("PT1r");
        if (sa != g_base + 0x38E2628 || sr != g_base + 0x38E2630 || !*(void**)sa || !*(void**)sr) {
            Log("PT1: SRW lock IAT slots look wrong");
            return false;
        }
        pt1_pAcq = (uint64_t)(uintptr_t)*(void**)sa;
        pt1_pRel = (uint64_t)(uintptr_t)*(void**)sr;
        pt1_ret = (uint64_t)(uintptr_t)(g_base + 0x1297FE);
        pt1_resB = (uint64_t)(uintptr_t)(g_base + 0x129658);
        pt1_resC = (uint64_t)(uintptr_t)(g_base + 0x129835);
        pt1_resD = (uint64_t)(uintptr_t)(g_base + 0x1298BD);
        pt1_jeD = (uint64_t)(uintptr_t)(g_base + 0x129943);
        pt1_enable = cfg.pt1 == 1 ? 1u : 0u;
        return true;
    });
    if (exDone[menu::kExpPT1]) Log("PT1: pool lock held once per texture in TextureStreamRequestTiles, switch %s", pt1_enable ? "ON" : "off");
    // TG: no site of its own (caveLim reads tg_cap). Available whenever VRR is installed; the ini only sets the start
    // state. It never engages without L1 (refused + logged by the governor).
    g_tgL1 = l1State == osd::State::On;
    g_tgFloor = cfg.tgFloor;
    g_tgRatioPct = cfg.tgRatioPct;
    g_tgEnable = (vrrDone && cfg.tg == 1) ? 1u : 0u;
    exDone[menu::kExpTG] = vrrDone;
    if (cfg.tg && !vrrDone) Log("TG: needs VRR installed - not available");
    else if (vrrDone)
        Log("TG: tick governor available (switch %s, floor %d ticks/s, ratio %d %%)%s", g_tgEnable ? "ON" : "off", cfg.tgFloor,
            cfg.tgRatioPct, g_tgL1 ? "" : " - L1 is not installed, so TG will refuse to engage (Proton: L1OnWine = 1)");
    // ---- 1.7.0 GP: verify the engine's marker code (check sites, deps, token table) before gpuprof writes the two flags
    static bool s_gpCodeOk = false;
    if ((cfg.gpuProf || cfg.gpuMarkers) && known) {
        auto v = collect("GP");
        bool ok = allFound(v) && DepsOk("GP");
        const uint64_t* tok = reinterpret_cast<const uint64_t*>(g_base + 0x3C02220);  // KtglTokenHandlerTable
        if (ok && (tok[0xA5] != (uint64_t)(uintptr_t)(g_base + 0x114024) || tok[0xA6] != (uint64_t)(uintptr_t)(g_base + 0x113FC8))) {
            Log("GP: the marker token handlers (A5/A6) are not the engine's - GP off");
            ok = false;
        }
        s_gpCodeOk = ok;
        Log("GP: engine marker code %s (%zu check sites, deps, token table)", ok ? "verified" : "NOT as expected - GP off", v.size());
    } else if (cfg.gpuProf || cfg.gpuMarkers) {
        Log("GP: not in this update - off");
    }
    // ---- 1.7.40 JT: job-timing / logic-join probe (jt.cpp, jt.asm; notes\gq-jt-probes.md). Measurement only.
    if (expGroup("JT", cfg.jt != 0, [&]() {
            const std::wstring jtIni = SelfPathWithExt(L".ini");
            jt::StartInfo ji;
            ji.log = &Log;
            ji.iniPath = jtIni.c_str();   // Prepare reads its keys right away
            ji.base = g_base;
            return jt::Prepare(ji, findItem("JTw")->addr, findItem("JTx")->addr, findItem("JTn1")->addr, findItem("JTn2")->addr);
        }))
        jt::Start();
    // ---- 1.7.44 PM: who writes the engine's Present SyncInterval (GfxDevice+424Ch). Measurement only: the two caves do the
    // original store and record it; pmode.cpp reports it with the present-mode change lines (notes\vrr-present.md).
    pmode::SetWriterProbes(expGroup("PM", cfg.pmLog != 0, nullptr));

    static bool s_tgAvail = false;
    s_tgAvail = vrrDone;
    if (vrrDone) {
        osd::SetRateSink(&TgOnRates);  // the governor runs on the Present-hook thread after each stats window
        if (HANDLE h = CreateThread(nullptr, 0, TgWatchdog, nullptr, 0, nullptr)) CloseHandle(h);
        if (cfg.vrrOnline == 0) {
            vrr_enable = 0;
            g_vrrOnlineState = 3;
            Log("VRR: VRROnline = 0 - variable-dt timing forced off (stock timing)");
        } else if (cfg.vrrOnline == 1) {
            g_vrrOnlineState = 2;
            Log("VRR: VRROnline = 1 - forced on, online detection ignored (use at your own risk online)");
        } else {
            Log("VRR: VRROnline = -1 (auto) - suspended while an online session is detected");
            if (HANDLE h = CreateThread(nullptr, 0, VrrOnlineWatch, nullptr, 0, nullptr)) CloseHandle(h);
        }
    }

    // periodic stats so a log from a handheld shows the wake rates and the experimental patches working without a
    // profiler. The same thread finds the [App]Game thread for FPC.
    static bool s_fpcWanted = false, s_anyExp = false;
    for (bool d : exDone) s_anyExp |= d;
    s_fpcWanted = exDone[menu::kExpFPC];
    if (HANDLE h = CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            struct Snap {
                LONG w3, l1, fg, w6w, w6s;
                LONG64 w6t;
                uint32_t ks, kk, ppr, pps, m1r, x1s, x1p, e1p;
                uint64_t fh, fm, fb, e1u, e1s, w6k;
            };
            auto take = []() {
                return Snap{g_w3Count, g_l1Count, g_fgCount, g_w6Waits, g_w6Sleeps, g_w6Ticks, wk_skipped, wk_kept,
                            pp_ran, pp_skipped, m1_refresh, x1_skip, x1_pass, e1_pass, fpc_hits, fpc_misses, fpc_bypass,
                            e1_upd, e1_skip, w6_wakes};
            };
            LARGE_INTEGER f;
            QueryPerformanceFrequency(&f);
            using GetDescFn = HRESULT(WINAPI*)(HANDLE, PWSTR*);
            HMODULE k = GetModuleHandleW(L"kernelbase.dll");
            if (!k) k = GetModuleHandleW(L"kernel32.dll");
            auto getDesc = k ? reinterpret_cast<GetDescFn>(GetProcAddress(k, "GetThreadDescription")) : nullptr;
            uint32_t lastEpoch = 0;
            bool haveEpoch = false;
            DWORD candTid = 0;
            Snap p = take();
            uint64_t ikAgeSum = lat_agesum, ikAgeCnt = lat_agecnt, ikTo = lat_timeouts, ikKs = lat_kstock, ikKi = lat_kik;
            updatecheck::Configure(SelfPathWithExt(L".ini").c_str());
            for (int sec = 1;; ++sec) {
                Sleep(1000);
                updatecheck::Tick(MOD_VERSION);
                if (s_tsDone && sec % 2 == 0) TsAnalyse(sec % 30 == 0, (double)f.QuadPart);
                if (s_ikDone && sec % 30 == 0) {
                    const uint64_t s = lat_agesum - ikAgeSum, n = lat_agecnt - ikAgeCnt;
                    Log("stats latency: IK %s, kicks/s at frame end %llu / at frame start %llu, input age K->C mean %.2f ms "
                        "(max since start %.2f ms), pad-stage wait timeouts %llu",
                        lat_ik ? "ON" : "off", (unsigned long long)((lat_kstock - ikKs) / 30), (unsigned long long)((lat_kik - ikKi) / 30),
                        n ? (double)s * 1000.0 / (double)f.QuadPart / (double)n : 0.0, (double)lat_agemax * 1000.0 / (double)f.QuadPart,
                        (unsigned long long)(lat_timeouts - ikTo));
                    ikAgeSum = lat_agesum; ikAgeCnt = lat_agecnt; ikTo = lat_timeouts; ikKs = lat_kstock; ikKi = lat_kik;
                }
                if (sec % 2 == 0) GpuOptRefresh();
                if (sec % 30 == 0 && gpuopt::GetState() != gpuopt::State::Off) {
                    char b[256];
                    Log("stats/s gpuopt: %s", GpuOptStatus(b, sizeof(b)));
                }
                if (sec % 30 == 0 && gimerge::WantsHooks()) gimerge::LogLine();  // 1.7.29 GIProbe / GIMerge
                if (sec % 30 == 0 && psoswap::HotEnabled()) {  // fast-testing ShaderHot (diagnostic): per-set state + binds
                    char hb[1024];
                    Log("stats/s shaderhot: %s", psoswap::HotStatus(hb, sizeof hb));
                }
                if (sec % 30 == 0) {  // 1.7.32 DQ / P1E7 (drawprep.cpp); empty when neither is installed
                    char b[160];
                    drawprep::StatusLine(b, sizeof(b));
                    if (b[0]) Log("stats/s %s", b);
                }
                if (s_wsDone && ws_enable && sec % 30 == 0) WakeStatsLog();
                if (s_sprDone && sec % 30 == 0) SprLog();
                if (sec % 30 == 0 && (s_mqDone || s_ptDone || s_dlDone)) {
                    static uint64_t pMq = 0, pDl = 0;
                    static uint32_t pCl = 0, pSv = 0;
                    const uint64_t mqc = mq_calls, dls = dl_skips;
                    const uint32_t cl = pt1_claims, sv = pt1_saved;
                    Log("stats cpu: MQ %s, calls %llu (+%llu in 30 s) | PT1 %s, textures claimed %u (+%u), lock pairs saved %u (+%u) | DL %s, "
                        "lookups skipped %llu (+%llu)%s",
                        s_mqDone ? (mq_enable ? "ON" : "off") : "-", (unsigned long long)mqc, (unsigned long long)(mqc - pMq),
                        s_ptDone ? (pt1_enable ? "ON" : "off") : "-", cl, cl - pCl, sv, sv - pSv,
                        s_dlDone ? (dl_enable ? "ON" : "off") : "-", (unsigned long long)dls, (unsigned long long)(dls - pDl),
                        s_cpuStats ? "" : " (MQ/DL counters need [Patches] CpuStats = 1)");
                    pMq = mqc; pDl = dls; pCl = cl; pSv = sv;
                }
                if (wg_installed && sec % 30 == 0) {
                    static LONG64 pS = 0, pC = 0, pT = 0;
                    const LONG64 s = g_wgSkipped, c = g_wgCounted, t = g_wgStale;
                    long pend = 0, peak = 0;
                    for (const WgPoolState& ps : g_wgPool) {
                        pend += ps.pending > 0 ? ps.pending : 0;
                        peak = std::max(peak, (long)ps.pending);
                    }
                    Log("stats/s WG: wake-one wakes skipped %lld, counted (worker on its way) %lld, stale window -> woke %lld | "
                        "pending now %ld (max per pool %ld), switch %s",
                        (s - pS) / 30, (c - pC) / 30, (t - pT) / 30, pend, peak, g_wgEnable ? "ON" : "off");
                    pS = s; pC = c; pT = t;
                }
                if (s_tgAvail && sec % 30 == 0) {
                    static long pL1 = 0;
                    const long l1 = g_l1Count;
                    static const char* const kTgSt[] = {"off", "governing", "REFUSED: L1 not active", "VRR not armed",
                                                        "nothing to govern (rendered near the cap)"};
                    const long st = g_tgState;
                    Log("stats/s TG: switch %s, state %s, logic tick cap %u (0 = stock), game ticks %.1f/s vs rendered %.1f/s, "
                        "L1 timer sleeps %ld/s", g_tgEnable ? "ON" : "off", kTgSt[st >= 0 && st <= 4 ? st : 0], tg_cap,
                        (double)g_tgTicks, (double)g_tgRendered, (l1 - pL1) / 30);
                    pL1 = l1;
                }
                if (s_dsDone && sec % 30 == 0) {
                    static long pW = 0;
                    static long long pT = 0;
                    const long w = g_dsWaits;
                    const long long tk = g_dsTicks;
                    Log("stats/s DS: retry-spin timer sleeps %ld, avg %.0f us (switch %s)", (w - pW) / 30,
                        w > pW ? (double)(tk - pT) * 1e6 / (double)f.QuadPart / (double)(w - pW) : 0.0, g_dsEnable ? "ON" : "off");
                    pW = w; pT = tk;
                }
                if (s_asyncKnown && sec % 30 == 0) {  // async compute queues the engine created (live slot table, .bss 0x45C0460)
                    const uint8_t* tb = g_base + 0x45C0460;
                    int n = 0; char sl[64] = ""; size_t o = 0;
                    for (int i = 0; i < 13; ++i)
                        if (tb[12 * i] && *reinterpret_cast<const uint32_t*>(tb + 12 * i + 8) == (uint32_t)i) { ++n; o += snprintf(sl + o, sizeof sl - o, " %d", i); }
                    Log("stats/s ASYNC: %d async compute queue(s), slots%s (AS0 %s)", n, n ? sl : " -", s_as0Applied ? "applied" : "off");
                    char ab[400];
                    asyncslots::StatusLine(ab, sizeof ab, reinterpret_cast<uintptr_t>(g_base));
                    if (ab[0]) Log("stats/s %s", ab);
                }
                if (sec % 30 == 0) tilestats::LogLine();  // no-op unless TileStats / TileBatch != 0
                if (sec % 30 == 0) memstats::LogLine();  // no-op unless [Profiler] MemStats = 1
                memstats::Tick1s();                      // 1.7.48 VS: no-op unless [Profiler] MemStats = 2
                if (sec % 30 == 0) loadmem::LogLine();  // no-op unless [Memory] LM = 1
                if (sec % 30 == 0) gp::LogLine();        // no-op unless [Profiler] GPUProf = 1
                if (sec % 30 == 0) mlprobe::LogLine();   // no-op unless [Profiler] MeshletProbe = 1 (CPU half)
                if (sec % 30 == 0) scenemirror::LogLine();   // no-op unless SceneBufMirror / SceneBufProbe != 0
                if (s_mpcsDone && sec % 30 == 0) {       // 1.7.47 MPCS
                    static uint64_t p28 = 0, p26 = 0, pSeen = 0;
                    const uint64_t a = mpcs_n28, b = mpcs_n26, c = mpcs_seen;
                    Log("stats/s MPCS: MPC tail decisions %.1f/s, #28-#31 tails skipped %.1f/s (4 dispatches + 8 UAV barriers each), #26 skipped %.1f/s "
                        "(switch %s)", (double)(c - pSeen) / 30.0, (double)(a - p28) / 30.0, (double)(b - p26) / 30.0, mpcs_enable ? "ON" : "off");
                    p28 = a; p26 = b; pSeen = c;
                }
                if (sec % 30 == 0) pj::LogLine();        // no-op unless [Patches] PJ != 0
                if (sec % 30 == 0) {                     // 1.7.49 same-look package: no-op unless VSMA / LPVR / SKYR / FOGT / CLDR != 0
                    char sb[1024];
                    samelook::StatusLine(sb, sizeof sb, 30);
                    if (sb[0]) Log("%s", sb);
                    extraperf::StatusLine(sb, sizeof sb);  // 1.7.59 Extra performance levels + field write / refill counts
                    if (sb[0]) Log("%s", sb);
                }
                if (sec % 30 == 0) {                     // no-op unless [Patches] TR != 0
                    char tb[512];
                    timerres::StatusLine(tb, sizeof tb, 30);
                    if (tb[0]) Log("stats/s %s", tb);
                }
                if (sec % 30 == 0) {                     // 1.7.58 no-op unless [Patches] PCM != 0
                    char pb[512];
                    psomemo::StatusLine(pb, sizeof pb, 30);
                    if (pb[0]) Log("stats/s %s", pb);
                }
                if (sec % 30 == 0) {                     // 1.7.60-cbu1 no-op unless a CBU key is set
                    char cb[256];
                    cbvupload::StatusLine(cb, sizeof cb);
                    if (cb[0]) Log("stats/s %s", cb);
                }
                if (sec % 30 == 0) {                     // 1.7.59-fo1 no-op unless [GpuOpt] FrameOverlap != 0
                    char fb[768];
                    frameoverlap::StatusLine(fb, sizeof fb, 30);
                    if (fb[0]) Log("stats/s %s", fb);
                }
                if (sec % 30 == 0) {                     // 1.7.58 no-op unless [Patches] RTP != 0
                    char rb[768];
                    thrsched::RtpStatusLine(rb, sizeof rb);
                    if (rb[0]) Log("stats %s", rb);
                }
                if (s_wlDone && sec % 30 == 0) {
                    static uint64_t pC = 0;
                    const uint64_t cl = wl_closed;
                    Log("stats/s WL: waitable handles closed %llu (total %llu, switch %s; 0 = no waitable swapchain, e.g. Proton)",
                        (unsigned long long)((cl - pC) / 30), (unsigned long long)cl, wl_enable ? "ON" : "off");
                    pC = cl;
                }
                if (s_gfDone && sec % 30 == 0) {
                    static long pW = 0, pF = 0;
                    static long long pT = 0;
                    const long w = g_gfWaits, fb = g_gfFallbacks;
                    const long long tk = g_gfTicks;
                    Log("stats/s GF: fence waits %ld, avg wait %.0f us, fallbacks to Sleep(0) %ld (switch %s)", (w - pW) / 30,
                        w > pW ? (double)(tk - pT) * 1e6 / (double)f.QuadPart / (double)(w - pW) : 0.0, (fb - pF) / 30,
                        g_gfEnable ? "ON" : "off");
                    pW = w; pF = fb; pT = tk;
                }
                // FPC: cache only on [App]Game, and only once its frames advance (g+3C60h moving), like the script's `on`
                if (s_fpcWanted && !fpc_tid && sec % 2 == 0 && getDesc) {
                    if (!candTid) {
                        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
                        if (snap != INVALID_HANDLE_VALUE) {
                            THREADENTRY32 te{};
                            te.dwSize = sizeof(te);
                            for (BOOL ok = Thread32First(snap, &te); ok && !candTid; ok = Thread32Next(snap, &te)) {
                                if (te.th32OwnerProcessID != GetCurrentProcessId()) continue;
                                if (HANDLE th = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID)) {
                                    PWSTR d = nullptr;
                                    if (SUCCEEDED(getDesc(th, &d)) && d) {
                                        if (!wcscmp(d, L"[App]Game")) candTid = te.th32ThreadID;
                                        LocalFree(d);
                                    }
                                    CloseHandle(th);
                                }
                            }
                            CloseHandle(snap);
                        }
                    }
                    const uint64_t g = *reinterpret_cast<volatile uint64_t*>(fpc_gaddr);
                    const uint32_t ep = g ? *reinterpret_cast<volatile uint32_t*>(g + 0x3C60) : 0;
                    if (candTid && g && haveEpoch && ep != lastEpoch) {
                        fpc_tid = candTid;
                        Log("FPC: [App]Game is TID %lu and frames advance - the FindParam cache is live", candTid);
                    }
                    lastEpoch = ep;
                    haveEpoch = g != 0;
                }
                if (g_fgcOn) {  // 1.7.44: a skipped null texture = a crash the guard prevented; log it within a second
                    static uint32_t s_nullSeen = 0;
                    const uint32_t nn = fgc_srcNull + fgc_dstNull;
                    if (nn != s_nullSeen) {
                        Log("FGC: frame-generation copy job with a NULL texture skipped (source %u, destination %u so far) - the "
                            "stock game reads [0+90h] here and crashes (vkd3d-proton #2845). FG keeps running; if generated "
                            "frames look wrong, turn FG off", fgc_srcNull, fgc_dstNull);
                        s_nullSeen = nn;
                    }
                }
                if (sec % 30) continue;
                if (g_rd.enabled) Log("RD totals: rounds %ld timeouts %ld wakes %ld errors %ld fallback %ld",
                    g_rd.rounds,g_rd.timeouts,g_rd.wakes,g_rd.errors,g_rd.failed);
                if (g_mw.enabled) Log("MW totals: wait rounds %ld timeouts %ld wakes %ld errors %ld",
                    g_mw.rounds, g_mw.timeouts, g_mw.wakes, g_mw.errors);
                if (g_fgcOn) {
                    static uint32_t s_pSrc = 0, s_pHits = 0;
                    const uint32_t src = fgc_src, hits = fgp_hits;
                    Log("stats/s FG: copy jobs %u (= frames interpolated/s = real fps while FSR FG runs), NULL textures skipped "
                        "src %u / dst %u (total) | FGP %s, paused frames %u",
                        (src - s_pSrc) / 30, fgc_srcNull, fgc_dstNull, "not installed",
                        (hits - s_pHits) / 30);
                    s_pSrc = src;
                    s_pHits = hits;
                }
                Snap n = take();
                Log("stats/s: W3 waits %ld (%s), L1 timer sleeps %ld, F1 timer sleeps %ld, WK wakes kept %u / skipped %u",
                    (n.w3 - p.w3) / 30, g_w3UseSleep ? "Sleep" : "MsgWait", (n.l1 - p.l1) / 30, (n.fg - p.fg) / 30,
                    (n.kk - p.kk) / 30, (n.ks - p.ks) / 30);
                const LONG w6n = n.w6w - p.w6w;
                const double w6us = w6n > 0 ? (double)(n.w6t - p.w6t) * 1e6 / (double)f.QuadPart / w6n : 0.0;
                const uint64_t fh = n.fh - p.fh, fm = n.fm - p.fm;
                if (s_anyExp) Log("stats/s exp: W6 waits %ld avg %.0f us (slept %ld, spun %ld, wakes %llu) | PP scans %u skipped %u | "
                    "FPC hits %llu misses %llu (%.0f %%) bypass %llu | M1 refreshes %u | X1 skipped %u passed %u | "
                    "E1 passes %u updated %llu skipped %llu",
                    w6n / 30, w6us, (n.w6s - p.w6s) / 30, (w6n - (n.w6s - p.w6s)) / 30,
                    (unsigned long long)((n.w6k - p.w6k) / 30), (n.ppr - p.ppr) / 30, (n.pps - p.pps) / 30,
                    (unsigned long long)(fh / 30), (unsigned long long)(fm / 30), fh + fm ? 100.0 * fh / (fh + fm) : 0.0,
                    (unsigned long long)((n.fb - p.fb) / 30), (n.m1r - p.m1r) / 30, (n.x1s - p.x1s) / 30,
                    (n.x1p - p.x1p) / 30, (n.e1p - p.e1p) / 30, (unsigned long long)((n.e1u - p.e1u) / 30),
                    (unsigned long long)((n.e1s - p.e1s) / 30));
                p = n;
            }
        }, nullptr, 0, nullptr)) CloseHandle(h);
    {  // in-game overlay: FPS counter + startup popup (osd.cpp) + mod menu (menu.cpp)
        std::wstring iniPath = SelfPathWithExt(L".ini");
        menu::Host mh;
        mh.version = MOD_VERSION;
        mh.tuning = &LiveTuning;
        mh.log = &Log;
        mh.iniPath = iniPath.c_str();
        mh.base = g_base;
        mh.knownBuild = known;
        mh.wine = wine != nullptr;
        if (vrrDone) mh.vrrEnable = reinterpret_cast<volatile uint32_t*>(&vrr_enable);
        if (w3Done) {
            mh.w3Enable = &g_w3Enable;
            mh.w3UseSleep = &g_w3UseSleep;
        }
        if (r1Done) {
            mh.r1Timeout = &reinterpret_cast<NearData*>(g_near)->r1Timeout;
            mh.r1Default = (uint32_t)cfg.r1Ms;
        }
        if (l1State == osd::State::On) mh.l1Enable = &g_l1Enable;
        if (f1State == osd::State::On) mh.fgEnable = &g_fgEnable;
        if (wkDone) mh.wkEnable = reinterpret_cast<volatile uint32_t*>(&wk_enable);
        mh.l1 = l1State == osd::State::On ? menu::FixState::Installed
                : l1State == osd::State::WineSkipped ? menu::FixState::WineSkipped : menu::FixState::NotInstalled;
        mh.f1 = f1State == osd::State::On ? menu::FixState::Installed
                : f1State == osd::State::WineSkipped ? menu::FixState::WineSkipped : menu::FixState::NotInstalled;
        mh.w3Count = &g_w3Count;
        mh.l1Count = &g_l1Count;
        mh.fgCount = &g_fgCount;
        mh.wkKept = reinterpret_cast<volatile uint32_t*>(&wk_kept);
        mh.wkSkipped = reinterpret_cast<volatile uint32_t*>(&wk_skipped);
        mh.workerCap = cfg.workerCap;
        mh.p12 = cfg.p12;
        mh.l1OnWine = cfg.l1OnWine;
        mh.fgOnWine = cfg.fgOnWine;
        // experimental patches: live switch when installed (M2/J1/S1 are code bytes only: next launch)
        auto U = [](uint32_t& v) { return reinterpret_cast<volatile uint32_t*>(&v); };
        volatile uint32_t* sw[menu::kNumExp] = {};
        sw[menu::kExpW6] = &g_w6Enable;
        sw[menu::kExpB] = U(gal_enable);
        sw[menu::kExpPP] = U(pp_enable);
        sw[menu::kExpFPC] = U(fpc_enable);
        sw[menu::kExpAABB] = U(aabb_enable);
        sw[menu::kExpM1] = U(m1_enable);
        sw[menu::kExpX1] = U(x1_enable);
        sw[menu::kExpE1] = U(e1_enable);
        sw[menu::kExpIK] = U(lat_ik);
        sw[menu::kExpTS] = U(lat_ts);
        sw[menu::kExpWS] = U(ws_enable);
        sw[menu::kExpED] = U(ed_enable);
        sw[menu::kExpGF] = &g_gfEnable;
        sw[menu::kExpWL] = U(wl_enable);
        sw[menu::kExpWG] = &g_wgEnable;
        sw[menu::kExpTG] = &g_tgEnable;
        sw[menu::kExpDS] = &g_dsEnable;
        sw[menu::kExpCP] = U(cp_enable);
        sw[menu::kExpSPF] = U(spf_enable);
        sw[menu::kExpMQ] = U(mq_enable);
        sw[menu::kExpPT1] = U(pt1_enable);
        sw[menu::kExpDL] = U(dl_enable);
        sw[menu::kExpBPO] = U(bpo_enable);
        const int iniv[menu::kNumExp] = {cfg.w6User, cfg.b, cfg.pp, cfg.fpc, cfg.aabb, cfg.m1, cfg.x1, cfg.e1, cfg.m2, cfg.j1, cfg.s1,
                                         cfg.ik, cfg.ts, cfg.wakeStats, cfg.ed, cfg.gfUser, cfg.wl,
                                         cfg.wg, cfg.tg, cfg.ds, cfg.lk, cfg.cp, cfg.spf, cfg.mq, cfg.pt1, cfg.dl,
                                         0, 0, cfg.ma, cfg.bpo, 0, 0, 0};  // DQ / P1E7 / W1B / RPA / AS: set after drawprep::Start below
        for (int i = 0; i < menu::kNumExp; ++i) {
            mh.exp[i].ini = iniv[i] ? 1 : 0;
            mh.exp[i].applied = exDone[i];
            if (exDone[i]) mh.exp[i].enable = sw[i];
        }
        mh.ppRan = U(pp_ran);
        mh.ppSkipped = U(pp_skipped);
        mh.m1Refresh = U(m1_refresh);
        mh.x1Skip = U(x1_skip);
        mh.e1Skip = reinterpret_cast<volatile uint64_t*>(&e1_skip);
        mh.e1Upd = reinterpret_cast<volatile uint64_t*>(&e1_upd);
        mh.fpcHits = reinterpret_cast<volatile uint64_t*>(&fpc_hits);
        mh.fpcMisses = reinterpret_cast<volatile uint64_t*>(&fpc_misses);
        mh.w6Waits = &g_w6Waits;
        mh.w6Sleeps = &g_w6Sleeps;
        mh.w6Ticks = &g_w6Ticks;
        mh.tsInDispUs = &g_tsInDispUs;
        mh.gpuStatus = &GpuOptStatus;
        mh.gpStatus = &gp::StatusLine;
        mh.gfWaits = &g_gfWaits;
        mh.gfTicks = &g_gfTicks;
        mh.gfFallbacks = &g_gfFallbacks;
        mh.wgSkipped = &g_wgSkipped;
        mh.tgCap = U(tg_cap);
        mh.tgState = &g_tgState;
        mh.dsWaits = &g_dsWaits;
        {  // skip the post-load "press to continue" tip page (autocont.cpp, [General] AutoContinueLoad = 0 by default)
            autocont::StartInfo ai;
            ai.log = &Log;
            ai.iniPath = iniPath.c_str();
            ai.base = reinterpret_cast<uintptr_t>(g_base);
            ai.knownBuild = known;
            ai.coverPermission = &menu::CoverTipAllowed;
            mh.autoContinue = autocont::Start(ai);
            mh.autoContinueCount = &autocont::Count;
        }
        {  // 1.7.32 DQ main-loop retry quantum / P1E7 texture-loop prefetch (drawprep.cpp, [Patches] DQ = P1E7 = 0).
           // After the DS group: DS checks the loop bytes DQ overwrites as its dependency.
            drawprep::StartInfo di;
            di.log = &Log;
            di.iniPath = iniPath.c_str();
            di.base = reinterpret_cast<uintptr_t>(g_base);
            di.knownBuild = known;
            drawprep::Start(di);
            // 1.7.39: live menu rows for the two drawprep switches (same in-launch A/B as the other experimental rows)
            const struct { int e; const wchar_t* key; volatile uint32_t* sw; } dpRows[] = {
                {menu::kExpDQ, L"DQ", drawprep::DqSwitch()}, {menu::kExpP1E7, L"P1E7", drawprep::P1e7Switch()},
                {menu::kExpW1B, L"W1B", drawprep::W1bSwitch()}, {menu::kExpRPA, L"RPA", drawprep::RpaSwitch()}};  // 1.7.42
            for (const auto& r : dpRows) {
                mh.exp[r.e].ini = GetPrivateProfileIntW(L"Patches", r.key, 0, iniPath.c_str()) ? 1 : 0;
                mh.exp[r.e].applied = r.sw != nullptr;
                mh.exp[r.e].enable = r.sw;
            }
        }
        {  // 1.7.44 AS (asyncslots.cpp, [Patches] AS = 0): live producer switch for the async slots added at boot
            asyncslots::StartInfo ai;
            ai.log = &Log;
            ai.base = reinterpret_cast<uintptr_t>(g_base);
            ai.knownBuild = known;
            ai.boot = &asBoot;
            asyncslots::Start(ai);
            mh.exp[menu::kExpAS].ini = asBoot.mode ? 1 : 0;
            mh.exp[menu::kExpAS].applied = asyncslots::Switch() != nullptr;
            mh.exp[menu::kExpAS].enable = asyncslots::Switch();
        }
        {  // PJH (psojournal.h, [Patches] PJH = 0): live switch of the PSO hand-over (pj::Start ran with psoswap, above)
            mh.exp[menu::kExpPJH].ini = GetPrivateProfileIntW(L"Patches", L"PJH", 0, iniPath.c_str()) == 1 ? 1 : 0;
            mh.exp[menu::kExpPJH].applied = pj::HandoverSwitch() != nullptr;
            mh.exp[menu::kExpPJH].enable = pj::HandoverSwitch();
        }
        {  // 1.7.45 TR (timerres.cpp, [Patches] TR = 0): hold 1 ms once, skip the HID threads' per-loop timeBeginPeriod pairs
            timerres::StartInfo ti;
            ti.log = &Log;
            ti.iniPath = iniPath.c_str();
            ti.base = reinterpret_cast<uintptr_t>(g_base);
            timerres::Start(ti);
            mh.exp[menu::kExpTR].ini = GetPrivateProfileIntW(L"Patches", L"TR", 0, iniPath.c_str()) == 1 ? 1 : 0;
            mh.exp[menu::kExpTR].applied = timerres::Switch() != nullptr;
            mh.exp[menu::kExpTR].enable = timerres::Switch();
        }
        {  // 1.7.49 same-look package rows (samelook.cpp): VSMA + the four update-rate knobs
            const struct { int e; int ini; volatile uint32_t* sw; } rows[] = {
                {menu::kExpVSMA, samelook::VsmaIni(), samelook::VsmaSwitch()},
                {menu::kExpLPVR, (int)GetPrivateProfileIntW(L"Patches", L"LPVR", 0, iniPath.c_str()), samelook::KnobSwitch(samelook::kLpv)},
                {menu::kExpSKYR, (int)GetPrivateProfileIntW(L"Patches", L"SKYR", 0, iniPath.c_str()), samelook::KnobSwitch(samelook::kSky)},
                {menu::kExpFOGT, (int)GetPrivateProfileIntW(L"Patches", L"FOGT", 0, iniPath.c_str()), samelook::KnobSwitch(samelook::kFogT)},
                {menu::kExpCLDR, (int)GetPrivateProfileIntW(L"Patches", L"CLDR", 0, iniPath.c_str()), samelook::KnobSwitch(samelook::kCloud)},
            };
            for (const auto& r : rows) {
                mh.exp[r.e].ini = r.ini == 1 ? 1 : 0;
                mh.exp[r.e].applied = r.sw != nullptr;
                mh.exp[r.e].enable = r.sw;
            }
        }
        {  // 1.7.58 TSTATE census ([Profiler] ThreadState = 0) + RTP render-thread priority ([Patches] RTP = 0, live row);
           // thrsched.cpp, notes\cpu-bound-midrange.md
            thrsched::StartInfo ti;
            ti.log = &Log;
            ti.iniPath = iniPath.c_str();
            ti.base = reinterpret_cast<uintptr_t>(g_base);
            ti.presentTid = &osd::GamePresentTid;
            thrsched::Start(ti);
            psomemo::StartInfo pmi;  // 1.7.58 PCM graphics-PSO lookup memo ([Patches] PCM = 0; psomemo.cpp, live row)
            pmi.log = &Log;
            pmi.iniPath = iniPath.c_str();
            pmi.base = reinterpret_cast<uintptr_t>(g_base);
            pmi.knownBuild = known;
            psomemo::Start(pmi);
            bkstats::StartInfo bki;  // 1.7.58-bk1 BKS back-end census ([Profiler] Backend = 0) + KL ([Tuning] KL_Div = 0, live)
            bki.log = &Log;
            bki.iniPath = iniPath.c_str();
            bki.base = reinterpret_cast<uintptr_t>(g_base);
            bki.knownBuild = known;
            bkstats::Start(bki);
            rnt::StartInfo rni;  // render-node timer ([Profiler] NodeTimer = 0, F11). No code patch.
            rni.log = &Log;
            rni.iniPath = iniPath.c_str();
            rni.base = reinterpret_cast<uintptr_t>(g_base);
            rni.knownBuild = known;
            rnt::Start(rni);
            frameoverlap::StartInfo foi;  // 1.7.59-fo1 FO frame overlap ([GpuOpt] FrameOverlap = 0; frameoverlap.cpp)
            foi.log = &Log;
            foi.iniPath = iniPath.c_str();
            foi.base = reinterpret_cast<uintptr_t>(g_base);
            foi.knownBuild = known;
            frameoverlap::Start(foi);
            replayprof::StartInfo rpi;  // 1.7.59-rpf1 RPF replay-thread profiler ([Profiler] ReplayProf = 0; replayprof.cpp)
            rpi.log = &Log;
            rpi.iniPath = iniPath.c_str();
            rpi.base = reinterpret_cast<uintptr_t>(g_base);
            rpi.knownBuild = known;
            replayprof::Start(rpi);
            cbvupload::StartInfo cbi;  // 1.7.60-cbu1 CBU constant uploads ([Patches] CBS / CBN / CBT = 0; cbvupload.cpp)
            cbi.log = &Log;
            cbi.iniPath = iniPath.c_str();
            cbi.base = reinterpret_cast<uintptr_t>(g_base);
            cbi.knownBuild = known;
            cbvupload::Start(cbi);
            mh.exp[menu::kExpPCM].ini = GetPrivateProfileIntW(L"Patches", L"PCM", 0, iniPath.c_str()) == 1 ? 1 : 0;
            mh.exp[menu::kExpPCM].applied = psomemo::Switch() != nullptr;
            mh.exp[menu::kExpPCM].enable = psomemo::Switch();
            mh.exp[menu::kExpRTP].ini = GetPrivateProfileIntW(L"Patches", L"RTP", 0, iniPath.c_str()) == 1 ? 1 : 0;
            mh.exp[menu::kExpRTP].applied = thrsched::RtpSwitch() != nullptr;
            mh.exp[menu::kExpRTP].enable = thrsched::RtpSwitch();
        }
        menu::Configure(mh);  // copies everything; must run before osd::Start (which asks menu::Enabled)
        {  // GPU barrier batching (gpuopt.cpp, [GpuOpt]); returns at once, does nothing with Barriers = 0. Before osd::Start:
           // the osd install thread calls gpuopt::InstallNow() once the engine renders.
            gp::StartInfo pi;  // GP profiler (gpuprof.cpp): markers + timestamp queries through gpuopt's hooks
            pi.log = &Log;
            pi.iniPath = iniPath.c_str();
            pi.base = g_base;
            pi.codeOk = s_gpCodeOk;
            pi.wine = wine;
            gp::Start(pi);
            skipfx::StartInfo sk;  // [Debug] SkipFx diagnostic (cost-vs-looks skip list; default 0 = nothing installed)
            sk.log = &Log;
            sk.iniPath = iniPath.c_str();
            skipfx::Start(sk);
            gpuopt::StartInfo gi;
            gi.log = &Log;
            gi.iniPath = iniPath.c_str();
            gi.wine = wine;
            gi.gpuProf = gp::WantsHooks();
            gi.giMerge = gimerge::WantsHooks();  // 1.7.29: GIProbe / GIMerge ride on gpuopt's command-list thunks
            gi.skipFx = skipfx::WantsHooks();
            gi.shaderHot = psoswap::HotEnabled();  // fast-testing: twin-PSO rebind (diagnostic, default off)
            if (gi.shaderHot && gimerge::WantsHooks())
                Log("ShaderHot: WARNING GIProbe / GIMerge is on - a Dispatch it holds and replays keeps the game's own PSO");
            gpuopt::Start(gi);
        }
        {  // fast-testing LiveCtl ([Debug] LiveCtl = 0 by default): control-file transport for one-launch A/B windows
            livectl::StartInfo li;
            li.log = &Log;
            li.iniPath = iniPath.c_str();
            li.remote = &menu::Remote;
            li.tuning = &LiveTuning;
            li.hotSet = &psoswap::HotSet;
            li.hotGet = &psoswap::HotGet;
            livectl::Start(li);
        }
        {  // sampling profiler (sp.cpp, [Profiler] SP = 0 by default): measurement only
            sp::StartInfo pi;
            pi.log = &Log;
            pi.iniPath = iniPath.c_str();
            pi.base = reinterpret_cast<uintptr_t>(g_base);
            sp::Start(pi);
        }
        {  // memory line (memstats.cpp, [Profiler] MemStats = 0 by default): measurement only
            memstats::StartInfo mi;
            mi.log = &Log;
            mi.iniPath = iniPath.c_str();
            mi.base = reinterpret_cast<uintptr_t>(g_base);
            mi.knownBuild = known;
            memstats::Start(mi);
        }
        {  // tile-mapping / queue-sync census (tilestats.cpp; [Profiler] TileStats = 0, [GpuOpt] TileBatch = 0)
            tilestats::StartInfo ti;
            ti.log = &Log;
            ti.iniPath = iniPath.c_str();
            tilestats::Start(ti);
        }
        {  // load-memory sweeper (loadmem.cpp, [Memory] LM = 0 by default; notesoad-memory.md)
            loadmem::StartInfo li;
            li.log = &Log;
            li.iniPath = iniPath.c_str();
            li.base = reinterpret_cast<uintptr_t>(g_base);
            li.knownBuild = known;
            loadmem::Start(li);
        }
        {  // per-load measurement (loadstats.cpp, [Profiler] LoadStats = 0 by default; notes\loading.md)
            loadstats::StartInfo ls;
            ls.log = &Log;
            ls.iniPath = iniPath.c_str();
            ls.base = reinterpret_cast<uintptr_t>(g_base);
            ls.knownBuild = known;
            ls.loaderCap = g_loaderCap;
            ls.readsByProbe = known && readprobe::WantsReads(iniPath.c_str());
            const bool lsOn = loadstats::Start(ls);
            // 1.7.23: RP read probe ([Profiler] ReadProbe, needs LoadStats) + EP event pool ([Patches] EP); readprobe.cpp
            readprobe::StartInfo ri;
            ri.log = &Log;
            ri.iniPath = iniPath.c_str();
            ri.base = reinterpret_cast<uintptr_t>(g_base);
            ri.knownBuild = known;
            ri.loadStatsOn = lsOn;
            readprobe::Start(ri);
        }
        Log("LoadOnlyCpuFixes = %d (%s); user GF %d W6 %d RWb %d", cfg.loadOnly, cfg.loadOnly ? (known ? "gate thread starting" : "refused: unknown build") : "off, GF/W6/RWb follow their own keys", cfg.gfUser, cfg.w6User, cfg.rwbUser);
        if (cfg.loadOnly && known) {  // 1.7.23 LO: GF/W6/RWb only inside the 58A load window (loadonly.cpp)
            loadonly::StartInfo oi;
            oi.log = &Log;
            oi.base = reinterpret_cast<uintptr_t>(g_base);
            oi.knownBuild = known;
            oi.gf = exDone[menu::kExpGF] ? &g_gfEnable : nullptr;
            oi.w6 = exDone[menu::kExpW6] ? &g_w6Enable : nullptr;
            oi.rwbUser = (uint32_t)(cfg.rwbUser != 0);
            loadonly::Start(oi);
        }
        {  // 1.7.41: BossLog / RematchLog / InvLog and [Rematch] moved to Nioh3BossRematch.asi (its own ini); say so if the old keys are set
            const wchar_t* moved[][2] = {{L"Debug", L"BossLog"}, {L"Debug", L"RematchLog"}, {L"Debug", L"InvLog"}, {L"Rematch", L"Enable"}};
            for (auto& k : moved)
                if (GetPrivateProfileIntW(k[0], k[1], 0, iniPath.c_str()))
                    Log("[%ls] %ls: moved to Nioh3BossRematch (set it in Nioh3BossRematch.ini; this ASI has no boss hooks since 1.7.41)", k[0], k[1]);
        }
        {  // 1.7.47 MLP CPU half (mlprobe.cpp, [Profiler] MeshletProbe = 0 by default; notes\meshlet-frontend.md): measurement only
            mlprobe::StartInfo mi;
            mi.log = &Log;
            mi.iniPath = iniPath.c_str();
            mi.base = reinterpret_cast<uintptr_t>(g_base);
            mi.knownBuild = known;
            mlprobe::Start(mi);
        }
        {  // 1.7.57 SBM meshlet scene-buffer mirror ([GpuOpt] SceneBufMirror = 0, [Profiler] SceneBufProbe = 0; scenemirror.cpp,
           // notes\rebar-fix-v3.md): installs the 5D417 call-site hook only when one of the keys is on
            scenemirror::StartInfo si2;
            si2.log = &Log;
            si2.iniPath = iniPath.c_str();
            si2.base = reinterpret_cast<uintptr_t>(g_base);
            si2.knownBuild = known;
            scenemirror::Start(si2);
        }
        {  // FnProbe (fnprobe.cpp, [Debug] FnProbe = 0 by default; notes\remaining-work.md 10.1): measurement only
            fnprobe::StartInfo fi;
            fi.log = &Log;
            fi.iniPath = iniPath.c_str();
            fi.base = reinterpret_cast<uintptr_t>(g_base);
            fi.knownBuild = known;
            fnprobe::Start(fi);
        }
        {  // VirtualPad / KeepRunningUnfocused (vpad.cpp, [Debug] both 0 by default; notes\virtual-input.md). Before padlat /
           // osd: our XInputGetState IAT hook then sits innermost, the mod menu and PadLatency see the virtual pad like a real one
            vpad::StartInfo vi;
            vi.log = &Log;
            vi.iniPath = iniPath.c_str();
            vi.base = g_base;
            vi.knownBuild = known;
            vi.version = MOD_VERSION;
            vi.menuRemote = &menu::Remote;
            vi.pauseReason = &menu::GamePauseReason;
            vpad::Start(vi);
            if (vpad::KeysWanted()) menu::SetKeyFilter(&vpad::KeyFilter);
        }
        {  // PadLatency (padlat.cpp, [Debug] PadLatency = 0 by default; notes\pad-latency.md): measurement only
            padlat::StartInfo li;
            li.log = &Log;
            li.iniPath = iniPath.c_str();
            li.base = g_base;
            li.knownBuild = known;
            if (exDone[menu::kExpTS]) {
                li.tsRing = reinterpret_cast<const volatile int64_t*>(lat_ring);
                li.tsRingIdx = &lat_ringidx;
                li.tsOn = &lat_ts;
            }
            padlat::Start(li);  // before osd::Start: osd asks padlat::WantsPresentHook()
        }
        {  // Tour mode (tour.cpp, [Debug] Tour = 0 by default; notes\tour-mode.md): blocks save writes for the session when on
            tour::StartInfo ti;
            ti.log = &Log;
            ti.iniPath = iniPath.c_str();
            ti.base = reinterpret_cast<uintptr_t>(g_base);
            ti.knownBuild = known;
            tour::Start(ti);
        }
        {  // [Debug] CrashReport (crashreport.cpp, default on): crash/abort/fatal-popup report file beside the .asi; CrashTest = forced-crash hook
            crashreport::StartInfo cri;
            cri.log = &Log;
            cri.iniPath = iniPath.c_str();
            cri.selfModule = g_self;
            cri.base = reinterpret_cast<uintptr_t>(g_base);
            cri.knownBuild = known;
            crashreport::Start(cri);
        }
        {  // 1.7.48 SkipBootNotice (bootnotice.cpp, [Debug] SkipBootNotice = -1: on only when Tour or VirtualPad is on):
           // skips the every-boot autosave notice + the "reset settings?" warning, logs the boot sequencer state
            bootnotice::StartInfo bi;
            bi.log = &Log;
            bi.iniPath = iniPath.c_str();
            bi.base = reinterpret_cast<uintptr_t>(g_base);
            bi.knownBuild = known;
            bootnotice::Start(bi);
        }
        {  // [Debug] HitchLog (hitchlog.cpp, default 1, measurement only; before osd::Start: osd asks hitchlog::Enabled())
            hitchlog::StartInfo hi;
            hi.log = &Log;
            hi.iniPath = iniPath.c_str();
            hi.c.w3 = &g_w3Count;  // the existing wait counters (stats line); the line logs their deltas over the gap
            hi.c.l1 = &g_l1Count;
            hi.c.f1 = &g_fgCount;
            hi.c.w6 = &g_w6Waits;
            hi.c.w6Ticks = &g_w6Ticks;
            hi.c.gf = &g_gfWaits;
            hi.c.gfTicks = &g_gfTicks;
            hi.c.ds = &g_dsWaits;
            hi.c.dsTicks = &g_dsTicks;
            hitchlog::Start(hi);
        }
        {  // 1.7.44 present mode log + [Latency] VrrPresent (pmode.cpp; before osd::Start: osd asks pmode::WantsPresentHook())
            pmode::StartInfo pmi;
            pmi.log = &Log;
            pmi.iniPath = iniPath.c_str();
            pmi.base = g_base;
            pmi.knownBuild = known;
            pmi.wine = wine;
            pmode::Start(pmi);
        }
        osd::StartupInfo si;
        si.version = MOD_VERSION;
        si.log = &Log;
        si.iniPath = iniPath.c_str();
        si.base = g_base;
        si.knownBuild = known;
        si.applied = applied;
        si.skipped = skipped;
        si.skippedNames = skippedNames.c_str();
        si.wine = wine;
        si.vrr = vrrDone;
        si.l1 = l1State;
        si.f1 = f1State;
        osd::Start(si);  // copies everything; returns at once
        {  // 1.7.56 RB: Resizable BAR / SAM check + one-time notice ([General] ReBarNotice = 1; rebar.cpp, notes\rebar-fix-v3.md)
            const std::wstring rbMarker = SelfPathWithExt(L".rebarnotice");
            rebar::StartInfo ri;
            ri.log = &Log;
            ri.iniPath = iniPath.c_str();
            ri.markerPath = rbMarker.c_str();
            ri.base = reinterpret_cast<uintptr_t>(g_base);
            ri.knownBuild = known;
            ri.wine = wine;
            rebar::Start(ri);  // copies the strings; returns at once
        }
    }
    Log("done: %d group(s) applied, %d skipped. Patches live until the game exits (nothing is written to disk).", applied, skipped);
    return 0;
}

bool HostIsNioh3() {
    wchar_t p[MAX_PATH * 2] = {};
    DWORD n = GetModuleFileNameW(nullptr, p, (DWORD)(sizeof(p) / sizeof(p[0])));
    std::wstring s(p, n);
    size_t sep = s.find_last_of(L"\\/");
    std::wstring name = sep == std::wstring::npos ? s : s.substr(sep + 1);
    return _wcsicmp(name.c_str(), L"Nioh3.exe") == 0;
}

}  // namespace

// ---- W3: PeekMessageA, and when the queue was empty, block until input or W3_TimeoutMs ---------------------------
extern "C" BOOL WINAPI hookPeek(LPMSG msg, HWND wnd, UINT fmin, UINT fmax, UINT remove) {
    BOOL r = (*g_peekSlot)(msg, wnd, fmin, fmax, remove);
    // mod menu: drop the menu key (F10 would put Windows into system-menu mode) and, while the menu is open, the
    // navigation keys, before the game translates/dispatches them
    if (r && msg && (remove & PM_REMOVE) && msg->message >= WM_KEYDOWN && msg->message <= WM_SYSKEYUP &&
        menu::SwallowMessage(msg))
        msg->message = WM_NULL;
    if (!r && g_w3Enable) {
        const int64_t ht0 = hitchlog::g_on ? hitchlog::Now() : 0;
        if (g_w3UseSleep)
            Sleep(g_w3Ms ? g_w3Ms : 1);  // Wine: ~1 kHz cap; window messages are picked up on the next Peek
        else
            MsgWaitForMultipleObjectsEx(0, nullptr, g_w3Ms, 0x4FF /* QS_ALLINPUT (Win7-era mask, as in the script) */,
                                        MWMO_INPUTAVAILABLE);
        if (ht0) hitchlog::NoteWait(hitchlog::kW3, ht0, hitchlog::Now());
        InterlockedIncrement(&g_w3Count);
    }
    return r;
}

// ---- L1: sleep on a high-resolution waitable timer until (deadline - margin) when enough slack is left ------------
extern "C" int L1Wait(uint64_t now, uint64_t last, uint64_t ticksPerFrame, uint64_t freq) {
    if (!g_l1Enable || freq == 0) return 0;
    const uint64_t elapsed = now - last;  // caller guarantees now > last
    if (ticksPerFrame <= elapsed) return 0;
    const uint64_t remaining = ticksPerFrame - elapsed;
    uint64_t hi;
    uint64_t rem;
    uint64_t lo = _umul128(g_l1MarginUs + g_l1MinUs, freq, &hi);
    const uint64_t thresh = _udiv128(hi, lo, 1000000ull, &rem);
    if (remaining <= thresh) return 0;
    lo = _umul128(g_l1MarginUs, freq, &hi);
    const uint64_t margin = _udiv128(hi, lo, 1000000ull, &rem);
    const uint64_t sleepTicks = remaining - margin;
    lo = _umul128(sleepTicks, 10000000ull, &hi);
    if (hi >= freq) return 0;  // absurd interval, don't sleep
    const uint64_t units100ns = _udiv128(hi, lo, freq, &rem);
    LARGE_INTEGER due;
    due.QuadPart = -(LONGLONG)units100ns;
    if (!SetWaitableTimer(g_l1Timer, &due, 0, nullptr, nullptr, FALSE)) {
        g_l1Enable = 0;  // timer unusable: stock limiter from now on (as the script's cave did)
        return 0;
    }
    const int64_t ht0 = hitchlog::g_on ? hitchlog::Now() : 0;
    WaitForSingleObject(g_l1Timer, 100);
    if (ht0) hitchlog::NoteWait(hitchlog::kL1, ht0, hitchlog::Now());
    InterlockedIncrement(&g_l1Count);
    return 1;
}

// ---- F1: replaces the FSR presenter's waitUntil(t) = `do QueryPerformanceCounter(&now) while (now < t)` -----------
// Sleeps on a high-resolution waitable timer until (t - margin) when enough time is left, then spins the rest
// exactly like the original, so frame-generation pacing accuracy is unchanged.
extern "C" void fgWait(int64_t target) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (g_fgEnable && target - now.QuadPart > g_fgThreshTicks && target - now.QuadPart < g_qpcFreq) {
        const int64_t sleepTicks = target - now.QuadPart - g_fgMarginTicks;
        LARGE_INTEGER due;
        due.QuadPart = -(sleepTicks * 10000000 / g_qpcFreq);  // relative, 100 ns units (sleepTicks < ~1 s here)
        if (SetWaitableTimer(g_fgTimer, &due, 0, nullptr, nullptr, FALSE)) {
            const int64_t ht0 = hitchlog::g_on ? hitchlog::Now() : 0;
            WaitForSingleObject(g_fgTimer, 100);
            if (ht0) hitchlog::NoteWait(hitchlog::kF1, ht0, hitchlog::Now());
            InterlockedIncrement(&g_fgCount);
        } else {
            g_fgEnable = 0;  // timer unusable: plain spin from now on
        }
    }
    do QueryPerformanceCounter(&now);
    while (now.QuadPart < target);
}

// ---- W6: replaces the Sleep(0) of 7D9E90 `while ([ctx+7728h]) Sleep(0)` (the main thread waiting for the Present
// thread to return from Present). Hybrid: pause-spin for W6_SpinUs, then WaitOnAddress on the flag byte with a
// W6_TimeoutMs timeout per round until it clears (the timeout bounds the latency if a wake is ever missed). The waker
// (caveW6Wake at the clear, 1A822D9) calls WakeByAddressAll when w6_waiting > 0. Switched off: one Sleep(0), as stock.
extern "C" void W6Wait(uint8_t* ctx) {
    volatile uint8_t* flag = ctx + 0x7728;
    if (!g_w6Enable) {
        Sleep(0);
        return;
    }
    LARGE_INTEGER t0, now;
    QueryPerformanceCounter(&t0);
    bool slept = false;
    if (g_w6SpinTicks > 0) {
        const int64_t end = t0.QuadPart + g_w6SpinTicks;
        for (unsigned i = 1; *flag; ++i) {
            _mm_pause();
            if ((i & 15) == 0) {
                QueryPerformanceCounter(&now);
                if (now.QuadPart >= end) break;
            }
        }
    }
    for (;;) {
        uint8_t v = *flag;
        if (!v) break;
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(&w6_waiting));  // full barrier before the re-check
        if (*flag) {
            g_waitOnAddress(flag, &v, 1, g_w6TimeoutMs);
            slept = true;
        }
        InterlockedDecrement(reinterpret_cast<volatile LONG*>(&w6_waiting));
    }
    QueryPerformanceCounter(&now);
    InterlockedIncrement(&g_w6Waits);
    if (slept) InterlockedIncrement(&g_w6Sleeps);
    InterlockedAdd64(&g_w6Ticks, now.QuadPart - t0.QuadPart);
    hitchlog::NoteWait(hitchlog::kW6, t0.QuadPart, now.QuadPart);
}

// ---- GF: replaces the Sleep(0) of `while (fence->GetCompletedValue() < value) Sleep(0)` (7D9E1C event == NULL path,
// 117D6C p2 == NULL path). Registers a per-thread auto-reset event with ID3D12Fence::SetEventOnCompletion (vtbl+48h,
// the call the engine itself makes when it has an event) and waits on it. The caller's loop re-checks the fence after
// every return, so a timeout or a stale signal only costs one more round. If SetEventOnCompletion fails, or no event
// can be made, it falls back to the stock Sleep(0) for that round. Switched off: Sleep(0), as stock.
extern "C" void GfWait(void** pf, uint64_t value) {
    if (!g_gfEnable) {
        Sleep(0);
        return;
    }
    void* fence = pf ? *pf : nullptr;
    HANDLE ev = g_gfTls != TLS_OUT_OF_INDEXES ? (HANDLE)TlsGetValue(g_gfTls) : nullptr;
    if (!ev && g_gfTls != TLS_OUT_OF_INDEXES) {
        ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (ev) TlsSetValue(g_gfTls, ev);
    }
    if (!fence || !ev) {
        InterlockedIncrement(&g_gfFallbacks);
        Sleep(0);
        return;
    }
    using SetEvOnCompl = HRESULT(STDMETHODCALLTYPE*)(void* self, UINT64 value, HANDLE ev);
    auto setEv = reinterpret_cast<SetEvOnCompl>((*reinterpret_cast<void***>(fence))[9]);  // ID3D12Fence vtbl+48h
    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);
    const HRESULT hr = setEv(fence, value, ev);
    if (FAILED(hr)) {
        InterlockedIncrement(&g_gfFallbacks);
        Sleep(0);
        return;
    }
    WaitForSingleObject(ev, g_gfTimeoutMs);
    QueryPerformanceCounter(&t1);
    InterlockedIncrement(&g_gfWaits);
    InterlockedAdd64(&g_gfTicks, t1.QuadPart - t0.QuadPart);
    hitchlog::NoteWait(hitchlog::kGF, t0.QuadPart, t1.QuadPart);
}

// ---- WG (1.6.0): wake gate. Worker slot layout (v2.02, deps-checked): slot = pool->workers ([pool+10h]) + i*458h,
// [slot] = WorkerThread (+8 event, +10h bit0 "sleeping, wake me"), [slot+440h] = pool, [pool+34h] = pool id.
namespace {
bool WgIndex(uint8_t* slot, uint8_t* pool, int& p, int& i) {
    if (!slot || !pool || *reinterpret_cast<uint8_t* volatile*>(slot + 0x440) != pool) return false;
    uint8_t* base = *reinterpret_cast<uint8_t* volatile*>(pool + 0x10);
    if (!base || slot < base) return false;
    const uintptr_t d = (uintptr_t)(slot - base);
    if (d % 0x458 || d / 0x458 >= 64) return false;
    const int id = *reinterpret_cast<volatile int*>(pool + 0x34) & 63;
    WgPoolState& ps = g_wgPool[id];
    if (ps.owner != pool) {
        if (ps.owner) return false;  // two pools share an id slot: the second one stays stock (never gated, never counted)
        InterlockedCompareExchangePointer(const_cast<PVOID volatile*>(&ps.owner), pool, nullptr);
        if (ps.owner != pool) return false;
    }
    p = id;
    i = (int)(d / 0x458);
    return true;
}
using SetEvent_t = BOOL(WINAPI*)(HANDLE);
using Wfso_t = DWORD(WINAPI*)(HANDLE, DWORD);
}  // namespace

// Replaces SetEvent at the wake-one sites (the game has just cleared the target's bit0 with a lock cmpxchg). Skip =
// give the target its bit0 back (it is still blocked: no SetEvent was issued) and report success, as the callers expect
// from "a worker is on its way" - which is true: a worker of the same pool was woken and has not yet left its wait.
extern "C" BOOL WgWake(HANDLE ev, uint8_t* slot) {
    const SetEvent_t setEvent = *reinterpret_cast<const SetEvent_t*>(ws_iatSetEvent);  // chained hooks stay in the path
    int p, i;
    uint8_t* pool = slot ? *reinterpret_cast<uint8_t* volatile*>(slot + 0x440) : nullptr;
    if (!WgIndex(slot, pool, p, i)) return setEvent(ev);
    WgPoolState& ps = g_wgPool[p];
    if (g_wgEnable) {
        InterlockedIncrement64(&g_wgCalls);
        // plain load, ordered after the game's lock cmpxchg on the target (and so after the caller's job push)
        if (ps.pending >= g_wgMax) {
            if (__rdtsc() - (uint64_t)ps.lastTsc <= g_wgWindowTsc) {
                if (uint8_t* wt = *reinterpret_cast<uint8_t* volatile*>(slot)) {
                    _InterlockedOr(reinterpret_cast<volatile long*>(wt + 0x10), 1);  // still asleep: "sleeping, wake me" again
                    InterlockedIncrement64(&g_wgSkipped);
                    return TRUE;
                }
            } else {
                InterlockedIncrement64(&g_wgStale);  // the last counted wake is old: behave stock
            }
        }
    }
    // counted wake: only when the target is inside its WgSleep window (so its own exit clears the mark, then it scans)
    volatile LONG* st = &g_wgWorker[p][i].st;
    for (LONG s = *st; (s & kWgWaiting) && !(s & kWgPending); s = *st)
        if (InterlockedCompareExchange(st, s | kWgPending, s) == s) {
            InterlockedIncrement(&ps.pending);
            ps.lastTsc = (LONG64)__rdtsc();
            InterlockedIncrement64(&g_wgCounted);
            break;
        }
    return setEvent(ev);
}

// Replaces WaitForSingleObject in the worker sleep (1A6BC35). Marks the worker "waiting" for the duration of the real
// wait; on return it clears its mark atomically and, if a counted wake had targeted it, takes that unit off the pool's
// pending count - before it goes on to JobWorkerRunUntilIdle (1A6BC48 -> 1A6BC5C).
extern "C" DWORD WgSleep(HANDLE ev, DWORD ms, uint8_t* pool, uint8_t* slot) {
    const Wfso_t wait = *reinterpret_cast<const Wfso_t*>(ws_iatWait);
    int p, i;
    if (!WgIndex(slot, pool, p, i)) return wait(ev, ms);
    volatile LONG* st = &g_wgWorker[p][i].st;
    if (InterlockedExchange(st, kWgWaiting) & kWgPending) InterlockedDecrement(&g_wgPool[p].pending);  // defensive
    const DWORD r = wait(ev, ms);
    if (InterlockedExchange(st, 0) & kWgPending) InterlockedDecrement(&g_wgPool[p].pending);
    return r;
}

// ---- DS (1.6.0): replaces the QueryPerformanceCounter call of the main loop's 1 ms retry spin (1A74BDA). Reads the
// clock as before; when switched on and more than 200 us of the millisecond (minus DS_MarginUs) is left, it first
// sleeps on its own high-resolution timer and reads the clock again. The spin's exit test is the game's, unchanged.
extern "C" BOOL DsQpc(LARGE_INTEGER* now, int64_t start) {
    QueryPerformanceCounter(now);
    if (!g_dsEnable || !g_dsTimer || !g_qpcFreqDs) return TRUE;
    const LONG tid = (LONG)GetCurrentThreadId();
    if (g_dsTid != tid) {  // one thread only ([App]Game): the timer object is not shared
        if (g_dsTid || InterlockedCompareExchange(&g_dsTid, tid, 0) != 0) return TRUE;
    }
    uint8_t* tm = *reinterpret_cast<uint8_t* volatile*>(g_base + kRvaEngineTimer);
    if (!tm) return TRUE;
    const int64_t freq = *reinterpret_cast<volatile int64_t*>(tm + 0x18);  // the spin divides by this
    const int64_t elapsed = now->QuadPart - start;
    if (freq <= 0 || elapsed < 0) return TRUE;
    const int64_t qUs = drawprep::DqQuantumUs();  // 1.7.32: DQ on -> the spin's quantum is DQ_QuantumUs, not 1 ms
    const int64_t left = (qUs ? freq * qUs / 1000000 : freq / 1000) - elapsed - freq * g_dsMarginUs / 1000000;  // engine ticks
    if (left <= freq / 5000) return TRUE;
    const int64_t leftQpc = left * g_qpcFreqDs / freq;
    LARGE_INTEGER due;
    due.QuadPart = -(leftQpc * 10000000 / g_qpcFreqDs);
    if (due.QuadPart >= 0 || !SetWaitableTimer(g_dsTimer, &due, 0, nullptr, nullptr, FALSE)) return TRUE;
    WaitForSingleObject(g_dsTimer, 2);
    const int64_t before = now->QuadPart;
    QueryPerformanceCounter(now);
    InterlockedIncrement(&g_dsWaits);
    InterlockedAdd64(&g_dsTicks, now->QuadPart - before);
    hitchlog::NoteWait(hitchlog::kDS, before, now->QuadPart);
    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_DETACH && g_self) menu::ReleaseFreeze();
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = mod;
        DisableThreadLibraryCalls(mod);
        if (!HostIsNioh3()) return TRUE;  // loaded into something else: do nothing
        if (HANDLE h = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr)) CloseHandle(h);
    }
    return TRUE;
}
