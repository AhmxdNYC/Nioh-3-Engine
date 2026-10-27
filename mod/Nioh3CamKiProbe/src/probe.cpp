// Nioh3CamKiProbe. F7 is read-only. An ini can set distance, lens, and a right/up camera position offset.
// F7: snapshot A (stick LEFT), B (stick RIGHT), C (stick STILL), then a diff.
// F8: dump the Ki-pulse ring after about 10 R1 pulses.
// Log: Nioh3CamKiProbe.log next to this ASI.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

static const char* kVer = "1.0.21-camki-offset";
static const uintptr_t kCamVt = 0x38ef208;
static const uintptr_t kCamTag = 0x44c9ac0;
static const uintptr_t kGameVt = 0x38ef168;
static const uintptr_t kGameTag = 0x44c9a80;
static const uintptr_t kPlayerTab = 0x4745348;
static const uintptr_t kCamMgr = 0x4751490;
static const uintptr_t kOrbitFn = 0x2b0e4c;
static const uintptr_t kFovFn = 0x8dad94;
static const uintptr_t kBlendFn = 0x9565e4;

enum { kCap = 1600, kBlob = 160, kBlobBytes = 0x180, kRing = 900, kTop = 70 };

static int Fin(float x) { return x == x && x < 1.0e6f && x > -1.0e6f; }
static int Fin3(const float* v) { return Fin(v[0]) && Fin(v[1]) && Fin(v[2]); }

static float Dist3(const float* a, const float* b) {
    float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// 1 pos copy, 2 height above/below the player, 3 nearby point, 0 ignore.
static int KindOf(const float* v, const float* p, float* dist) {
    if (!Fin3(v) || !Fin3(p)) return 0;
    float dx = v[0] - p[0], dy = v[1] - p[1], dz = v[2] - p[2];
    float xz = sqrtf(dx * dx + dz * dz);
    *dist = sqrtf(dx * dx + dy * dy + dz * dz);
    if (xz < 0.25f && dy > -0.15f && dy < 0.15f) return 1;
    if (xz < 0.35f && ((dy >= 0.25f && dy <= 4.0f) || (dy <= -0.25f && dy >= -4.0f))) return 2;
    if (*dist >= 0.8f && *dist <= 12.0f) return 3;
    return 0;
}

static int IsUnit(const float* v) {
    if (!Fin3(v)) return 0;
    float L = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    return L > 0.81f && L < 1.21f;
}

static void Basis(const float* m, int colMajor, int axis, float* o) {
    if (colMajor) {
        o[0] = m[axis];
        o[1] = m[4 + axis];
        o[2] = m[8 + axis];
    } else {
        o[0] = m[axis * 4];
        o[1] = m[axis * 4 + 1];
        o[2] = m[axis * 4 + 2];
    }
}

static int Ortho(const float* m, int colMajor) {
    float c[3][3];
    int i, k;
    for (i = 0; i < 3; i++) Basis(m, colMajor, i, c[i]);
    for (i = 0; i < 3; i++) {
        float len = sqrtf(c[i][0] * c[i][0] + c[i][1] * c[i][1] + c[i][2] * c[i][2]);
        if (len < 0.85f || len > 1.15f) return 0;
    }
    for (i = 0; i < 3; i++) {
        float d;
        k = (i + 1) % 3;
        d = c[i][0] * c[k][0] + c[i][1] * c[k][1] + c[i][2] * c[k][2];
        if (d > 0.2f || d < -0.2f) return 0;
    }
    return 1;
}

// Writes the translation that sits a camera-like distance from the player. 0 if not a matrix.
static int MatTrans(const float* m, float* out, const float* player, float* distOut) {
    float t[2][3];
    int mode;
    if (!Fin(m[15]) || fabsf(m[15] - 1.0f) > 0.05f) return 0;
    t[0][0] = m[12]; t[0][1] = m[13]; t[0][2] = m[14];
    t[1][0] = m[3]; t[1][1] = m[7]; t[1][2] = m[11];
    for (mode = 0; mode < 2; mode++) {
        float d;
        if (!Fin3(t[mode]) || !Ortho(m, mode)) continue;
        d = Dist3(t[mode], player);
        if (d >= 0.4f && d <= 15.0f) {
            out[0] = t[mode][0];
            out[1] = t[mode][1];
            out[2] = t[mode][2];
            *distOut = d;
            return 1;
        }
    }
    return 0;
}

#ifdef PROBE_TEST
int main(void) {
    float p[3] = {10.f, 20.f, 30.f};
    float eye[3] = {10.f, 22.f, 30.f};
    float self[3] = {10.f, 20.f, 30.f};
    float farp[3] = {10.f, 20.f, 36.f};
    float d = 0.f, dist = 0.f, tr[3];
    float ident[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 12,22,30,1};
    if (KindOf(eye, p, &d) != 2) return 1;
    if (KindOf(self, p, &d) != 1) return 2;
    if (KindOf(farp, p, &d) != 3) return 3;
    if (!MatTrans(ident, tr, p, &dist)) return 4;
    ident[0] = 5.f;
    if (MatTrans(ident, tr, p, &dist)) return 5;
    eye[0] = 0.f; eye[1] = 1.f; eye[2] = 0.f;
    if (!IsUnit(eye)) return 6;
    printf("probe_test ok\n");
    return 0;
}
#else

static FILE* g_log;
static CRITICAL_SECTION g_logCs;
static volatile LONG g_logCsReady;
static HMODULE g_mod;
static volatile LONG g_run = 1;
static uint8_t g_chunk[1 << 16];

static int Rd(uintptr_t a, void* dst, size_t n) {
    __try {
        memcpy(dst, (const void*)a, n);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static int Rd64(uintptr_t a, uint64_t* o) { return Rd(a, o, 8); }

static int Wr(uintptr_t a, const void* src, size_t n) {
    __try {
        memcpy((void*)a, src, n);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static void Log(const char* fmt, ...) {
    va_list ap;
    if (!g_log) return;
    if (g_logCsReady) EnterCriticalSection(&g_logCs);
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
    if (g_logCsReady) LeaveCriticalSection(&g_logCs);
}

static void OpenLog(void) {
    wchar_t path[MAX_PATH];
    if (!g_logCsReady) {
        InitializeCriticalSection(&g_logCs);
        g_logCsReady = 1;
    }
    if (g_log) return;
    DWORD n;
    n = GetModuleFileNameW(g_mod, path, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        wchar_t* slash = wcsrchr(path, L'\\');
        if (slash) {
            wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - path), L"Nioh3CamKiProbe.log");
            g_log = _wfopen(path, L"a");
        }
    }
    if (!g_log) g_log = fopen("Nioh3CamKiProbe.log", "a");
}

struct Slot {
    uint64_t addr;
    float v[3][16];
    int nfloat;
    int kind;
    int walk;
    int good[3];
    float dist;
};
struct Blob {
    uint64_t addr;
    int len;
    int good[3];
    uint8_t b[3][kBlobBytes];
};
struct KiSample {
    uint32_t ms;
    uint32_t act;
    uint16_t fileAct, motion;
    float ki, kiMax, rate;
    float pos[3];
    float kt[6];
    uint8_t k83, k85;
    uint32_t bits[4];
    uint32_t actW[16];
    uint32_t motW[8];
    uint64_t actObj;
};
struct Top {
    float mag;
    char line[220];
};

static Slot g_slot[kCap];
static int g_n;
static int g_kindN[6];
static uint64_t g_cam;
static float g_camSnap[3][16];
static int g_camHave[3];
static int g_hash[4096];
static uint64_t g_haddr[4096];
static Blob g_blob[kBlob];
static int g_bn;
static KiSample g_ring[kRing];
static int g_ri, g_rn;
static Top g_top[kTop];
static int g_topN;
static int g_phase;
static char g_snapName[3] = {'A', 'B', 'C'};

static const char* KindName(int k) {
    if (k == 1) return "pos";
    if (k == 2) return "height";
    if (k == 3) return "pair";
    if (k == 4) return "matrix";
    if (k == 5) return "near";
    return "?";
}

static void ClearCands(void) {
    g_n = 0;
    g_bn = 0;
    memset(g_kindN, 0, sizeof g_kindN);
    memset(g_hash, 0, sizeof g_hash);
    memset(g_haddr, 0, sizeof g_haddr);
}

static int HashFind(uint64_t addr, int* empty) {
    uint64_t h = (addr >> 2) * 11400714819323198485ull;
    int n;
    *empty = -1;
    for (n = 0; n < 48; n++) {
        int i = (int)((h + (uint64_t)n) & 4095u);
        if (g_hash[i] == 0) {
            if (*empty < 0) *empty = i;
            return -1;
        }
        if (g_haddr[i] == addr) return g_hash[i] - 1;
    }
    return -1;
}

static int KindCap(int kind) {
    if (kind == 1) return 80;
    if (kind == 2) return 300;
    if (kind == 3) return 800;
    if (kind == 4) return 400;
    if (kind == 5) return 200;
    return 0;
}

static void Store(uint64_t addr, const float* src, int nfloat, int kind, int walk, float dist, int phase) {
    int empty = -1, idx;
    if (kind < 1 || kind > 5 || phase != 0) return;
    idx = HashFind(addr, &empty);
    if (idx >= 0) {
        if (walk) g_slot[idx].walk = 1;
        return;
    }
    if (g_kindN[kind] >= KindCap(kind) || g_n >= kCap || empty < 0) return;
    idx = g_n++;
    g_kindN[kind]++;
    g_hash[empty] = idx + 1;
    g_haddr[empty] = addr;
    memset(&g_slot[idx], 0, sizeof(Slot));
    g_slot[idx].addr = addr;
    g_slot[idx].nfloat = nfloat;
    g_slot[idx].kind = kind;
    g_slot[idx].walk = walk;
    g_slot[idx].dist = dist;
    g_slot[idx].good[0] = 1;
    memcpy(g_slot[idx].v[0], src, (size_t)nfloat * sizeof(float));
}

static void Consider(const uint8_t* buf, size_t n, uint64_t base, int walk, const float* player, uint64_t skipPos) {
    size_t i;
    for (i = 0; i + 12 <= n; i += 4) {
        float v[3], d = 0.f;
        int kind;
        uint64_t addr = base + (uint64_t)i;
        memcpy(v, buf + i, 12);
        if (addr == skipPos) continue;
        kind = KindOf(v, player, &d);
        if (kind == 1 || kind == 2) Store(addr, v, 3, kind, walk, d, 0);
        else if (kind == 3) {
            int paired = 0;
            if (i + 24 <= n) {
                float u[3];
                memcpy(u, buf + i + 12, 12);
                if (IsUnit(u)) {
                    Store(addr, v, 3, 3, walk, d, 0);
                    paired = 1;
                }
            }
            if (!paired) Store(addr, v, 3, 5, walk, d, 0);
        }
        if ((i % 16) == 0 && i + 64 <= n) {
            float m[16], tr[3], md = 0.f;
            memcpy(m, buf + i, 64);
            if (MatTrans(m, tr, player, &md)) Store(addr, m, 16, 4, walk, md, 0);
        }
    }
}

static int PtrOk(uint64_t p) {
    if (p < 0x10000ull) return 0;
    if (p > 0x00007fffffffffffull) return 0;
    if (p & 7ull) return 0;
    return 1;
}

static int LooksSpatial(uint64_t p, const float* player) {
    uint8_t b[0x80];
    int i;
    if (!Rd((uintptr_t)p, b, sizeof b)) return 0;
    for (i = 0; i + 12 <= (int)sizeof b; i += 4) {
        float v[3], d = 0.f;
        memcpy(v, b + i, 12);
        if (KindOf(v, player, &d)) return 1;
        if (IsUnit(v)) return 1;
    }
    return 0;
}

static int Seen(uint64_t* s, int* sn, int cap, uint64_t p) {
    int i;
    for (i = 0; i < *sn; i++) if (s[i] == p) return 1;
    if (*sn >= cap) return 1;
    s[(*sn)++] = p;
    return 0;
}

static void Walk(uint64_t actor, const float* player) {
    uint64_t q[kBlob], seen[kBlob];
    int depth[kBlob];
    int qn = 0, sn = 0, qi;
    q[qn] = actor;
    depth[qn] = 0;
    qn++;
    seen[sn++] = actor;
    for (qi = 0; qi < qn; qi++) {
        uint8_t buf[kBlobBytes];
        int off;
        if (!Rd((uintptr_t)q[qi], buf, kBlobBytes)) continue;
        if (g_bn < kBlob) {
            Blob* bl = &g_blob[g_bn++];
            memset(bl, 0, sizeof *bl);
            bl->addr = q[qi];
            bl->len = kBlobBytes;
            bl->good[0] = 1;
            memcpy(bl->b[0], buf, kBlobBytes);
        }
        Consider(buf, kBlobBytes, q[qi], 1, player, actor + 0x110);
        for (off = 0; off + 8 <= kBlobBytes && qn < kBlob; off += 8) {
            uint64_t p;
            memcpy(&p, buf + off, 8);
            if (!PtrOk(p) || Seen(seen, &sn, kBlob, p)) continue;
            if (depth[qi] >= 1 && !LooksSpatial(p, player)) continue;
            q[qn] = p;
            depth[qn] = depth[qi] + 1;
            qn++;
        }
    }
}

static int PageOk(DWORD prot) {
    if (prot & (PAGE_GUARD | PAGE_NOACCESS)) return 0;
    prot &= 0xFF;
    return prot == PAGE_READONLY || prot == PAGE_READWRITE || prot == PAGE_WRITECOPY
        || prot == PAGE_EXECUTE_READ || prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY;
}

static void ScanRange(uintptr_t base, size_t size, const float* player, uint64_t skipPos, size_t* scanned) {
    size_t off = 0;
    while (off < size) {
        size_t n = size - off;
        if (n > sizeof g_chunk) n = sizeof g_chunk;
        if (Rd(base + off, g_chunk, n)) {
            Consider(g_chunk, n, base + off, 0, player, skipPos);
            *scanned += n;
        }
        off += n;
    }
}

static size_t ScanHeap(const float* player, uint64_t skipPos, DWORD wantType) {
    uintptr_t addr = 0;
    size_t scanned = 0;
    const size_t budget = (size_t)256 << 20;
    while (scanned < budget) {
        MEMORY_BASIC_INFORMATION m;
        uintptr_t base, next;
        size_t take;
        if (!VirtualQuery((void*)addr, &m, sizeof m)) break;
        base = (uintptr_t)m.BaseAddress;
        next = base + m.RegionSize;
        if (next <= addr) break;
        if (m.State == MEM_COMMIT && m.Type == wantType && PageOk(m.Protect) && m.RegionSize >= 0x1000) {
            take = m.RegionSize;
            if (take > 16u << 20) take = 16u << 20;
            if (take > budget - scanned) take = budget - scanned;
            ScanRange(base, take, player, skipPos, &scanned);
            if (m.RegionSize > (16u << 20) && scanned + (4u << 20) < budget) {
                size_t mid = m.RegionSize / 2;
                if (mid > 4u << 20) ScanRange(base + mid, 4u << 20, player, skipPos, &scanned);
            }
        }
        addr = next;
    }
    return scanned;
}

static int Player(uint64_t base, uint64_t* actor, float pos[4]) {
    uint64_t tab = 0, pl = 0;
    *actor = 0;
    if (!base || !Rd64(base + kPlayerTab, &tab) || !tab) return 0;
    if (!Rd64(tab + (uint64_t)0xCD * 0x18, &pl) || !pl) return 0;
    if (!Rd(pl + 0x110, pos, 16)) return 0;
    *actor = pl;
    return 1;
}

static void ReadAction(uint64_t actor, KiSample* s) {
    uint64_t ai = 0, ctrl = 0, obj = 0, row = 0, mot = 0, kiObj = 0;
    memset(s->actW, 0, sizeof s->actW);
    memset(s->motW, 0, sizeof s->motW);
    s->act = 0;
    s->fileAct = 0;
    s->motion = 0;
    s->ki = 0;
    s->kiMax = 0;
    s->rate = 0;
    s->actObj = 0;
    if (!actor) return;
    if (Rd64(actor + 0x3B0, &ai) && ai && Rd64(ai + 8, &ctrl) && ctrl && Rd64(ctrl + 0xE8, &obj) && obj) {
        uint32_t id = 0;
        s->actObj = obj;
        if (Rd(obj, &id, 4)) s->act = id;
        if (Rd(obj, s->actW, sizeof s->actW)) { /* first 64 bytes */ }
        if (Rd64(obj + 0x20, &row) && row) {
            Rd(row + 0x0C, &s->fileAct, 2);
            Rd(row + 0x20, &s->motion, 2);
        }
    }
    if (Rd64(actor + 0x40, &mot) && mot) {
        if (!Rd(mot + 0x300, s->motW, sizeof s->motW)) memset(s->motW, 0, sizeof s->motW);
        Rd(mot + 0x324, &s->rate, 4);
    }
    if (Rd64(actor + 0x3A0, &kiObj) && kiObj) {
        static const int kOff[6] = {0x54, 0x58, 0x5C, 0x60, 0x74, 0x7C};
        int i;
        Rd(kiObj + 0xAC, &s->ki, 4);
        Rd(kiObj + 0xB0, &s->kiMax, 4);
        Rd(kiObj + 0x48, &s->bits[2], 4);
        for (i = 0; i < 6; i++) Rd((kiObj + 0x98) + kOff[i], &s->kt[i], 4);
        Rd(kiObj + 0x98 + 0x83, &s->k83, 1);
        Rd(kiObj + 0x98 + 0x85, &s->k85, 1);
        Rd(kiObj + 0x98 + 0x80, &s->bits[3], 4);
    }
    if (Rd64(actor + 0x3B0, &ai) && ai && Rd64(ai + 8, &ctrl) && ctrl) {
        Rd(ctrl + 0xA8, &s->bits[0], 4);
        Rd(ctrl + 0x1060, &s->bits[1], 4);
    }
}

static int CamOk(uint64_t base) {
    uint64_t vt = 0, tag = 0;
    if (!g_cam || !base) return 0;
    if (!Rd(g_cam, &vt, 8) || !Rd(g_cam + 0x58, &tag, 8)) { g_cam = 0; return 0; }
    if (vt != base + kCamVt || tag != base + kCamTag) { g_cam = 0; return 0; }
    return 1;
}

static int g_vtHits, g_tagHits, g_otherHits, g_gameHits, g_gameTagHits;
static uint64_t g_sib[8];
static int g_sibN;
static uint64_t g_game[8];
static int g_gameN;

static int GameLive(uint64_t addr) {
    float f[4];
    memset(f, 0, sizeof f);
    if (!Rd((uintptr_t)(addr + 0x10), f, 16)) return 0;
    if (!Fin(f[0]) || !Fin(f[1]) || !Fin(f[2])) return 0;
    return fabsf(f[0]) > 1.f || fabsf(f[1]) > 1.f || fabsf(f[2]) > 1.f;
}

static void NoteGame(uint64_t addr) {
    int k, dup = 0, slot = -1;
    for (k = 0; k < g_gameN; k++) if (g_game[k] == addr) dup = 1;
    if (dup) return;
    if (g_gameN < 8) {
        g_game[g_gameN++] = addr;
        return;
    }
    if (!GameLive(addr)) return;
    for (k = 0; k < g_gameN; k++) if (!GameLive(g_game[k])) { slot = k; break; }
    if (slot >= 0) g_game[slot] = addr;
}

static void NoteCamQword(uint64_t addr, uint64_t q, uint64_t base) {
    uint64_t other = 0;
    if (q == base + kGameVt) {
        g_gameHits++;
        Rd(addr + 0x58, &other, 8);
        if (other == base + kGameTag) g_gameTagHits++;
        NoteGame(addr);
        if (g_gameHits <= 3)
            Log("GAME vtable addr=%llx tag-rva=%llx", (unsigned long long)addr,
                (unsigned long long)(base && other >= base ? other - base : 0));
    } else if (q == base + kCamVt) {
        g_vtHits++;
        Rd(addr + 0x58, &other, 8);
        if (other == base + kCamTag) g_cam = addr;
        else if (g_vtHits <= 3)
            Log("CAM vtable-only addr=%llx tag=%llx", (unsigned long long)addr, (unsigned long long)other);
    } else if (q == base + 0x38ef258 || q == base + 0x38ef2a8) {
        g_otherHits++;
        if (q == base + 0x38ef2a8 && g_sibN < 8) {
            int k, dup = 0;
            for (k = 0; k < g_sibN; k++) if (g_sib[k] == addr) dup = 1;
            if (!dup) g_sib[g_sibN++] = addr;
        }
        if (g_otherHits <= 3)
            Log("CAM other-vtable addr=%llx rva=%llx", (unsigned long long)addr,
                (unsigned long long)(q - base));
    } else if (q == base + kCamTag && addr > 0x80) {
        uint64_t obj = addr - 0x58;
        g_tagHits++;
        Rd(obj, &other, 8);
        if (other == base + kCamVt) g_cam = obj;
        else if (g_tagHits <= 3)
            Log("CAM tag-only obj=%llx vt=%llx", (unsigned long long)obj, (unsigned long long)other);
    }
}

static void ScanWindow(uintptr_t start, size_t size, uint64_t base, size_t* scanned, size_t budget) {
    size_t off = 0;
    while (off < size && *scanned < budget && !g_cam) {
        size_t n = size - off;
        size_t i;
        if (n > sizeof g_chunk) n = sizeof g_chunk;
        if (*scanned + n > budget) n = budget - *scanned;
        if (!n) break;
        if (Rd(start + off, g_chunk, n)) {
            for (i = 0; i + 8 <= n; i += 8) {
                uint64_t q = 0;
                memcpy(&q, g_chunk + i, 8);
                NoteCamQword(start + off + i, q, base);
                if (g_cam) break;
            }
            *scanned += n;
        }
        off += n;
    }
}

static uintptr_t g_scanAt;
static size_t g_scanned, g_mapScanned;
static int g_scanStarted, g_scanDone, g_scanMark, g_scanPhase;

static void ScanCamSlice(uint64_t base, size_t step) {
    size_t got = 0;
    if (!base || g_scanDone || CamOk(base)) return;
    if (!g_scanStarted) {
        uint64_t slot0 = 0;
        g_vtHits = 0;
        g_tagHits = 0;
        g_otherHits = 0;
        g_sibN = 0;
        g_gameHits = 0;
        g_gameTagHits = 0;
        g_gameN = 0;
        Rd(base + kCamVt, &slot0, 8);
        Log("CAM module slot0=%llx", (unsigned long long)slot0);
        g_scanStarted = 1;
        g_scanAt = 0;
        g_scanPhase = 0;
    }
    while (got < step && !g_cam && !g_scanDone) {
        MEMORY_BASIC_INFORMATION m;
        uintptr_t region, next, from;
        size_t left, take, local;
        DWORD want;
        if (!VirtualQuery((void*)g_scanAt, &m, sizeof m)) {
            if (g_scanPhase == 0) {
                Log("CAM private done scanned-MB=%u vtable-hits=%d tag-hits=%d other-hits=%d obj=%llx",
                    (unsigned)(g_scanned >> 20), g_vtHits, g_tagHits, g_otherHits, (unsigned long long)g_cam);
                g_scanPhase = 1;
                g_scanAt = 0;
                g_scanMark = 0;
                continue;
            }
            g_scanDone = 1;
            break;
        }
        region = (uintptr_t)m.BaseAddress;
        next = region + m.RegionSize;
        if (next <= g_scanAt) {
            if (g_scanPhase == 0) {
                Log("CAM private done scanned-MB=%u vtable-hits=%d tag-hits=%d other-hits=%d obj=%llx",
                    (unsigned)(g_scanned >> 20), g_vtHits, g_tagHits, g_otherHits, (unsigned long long)g_cam);
                g_scanPhase = 1;
                g_scanAt = 0;
                g_scanMark = 0;
                continue;
            }
            g_scanDone = 1;
            break;
        }
        want = g_scanPhase == 0 ? MEM_PRIVATE : MEM_MAPPED;
        if (m.State == MEM_COMMIT && m.Type == want && PageOk(m.Protect) && m.RegionSize >= 0x1000) {
            from = g_scanAt > region ? g_scanAt : region;
            left = next - from;
            take = left;
            if (take > step - got) take = step - got;
            local = 0;
            ScanWindow(from, take, base, &local, take);
            g_scanAt = from + take;
            if (g_scanPhase == 0) g_scanned += take;
            else g_mapScanned += take;
            got += take;
        } else {
            g_scanAt = next;
        }
    }
    if (g_cam || g_scanDone) {
        if (g_scanMark >= 0) {
            if (g_scanPhase == 0)
                Log("CAM private done scanned-MB=%u vtable-hits=%d tag-hits=%d other-hits=%d obj=%llx",
                    (unsigned)(g_scanned >> 20), g_vtHits, g_tagHits, g_otherHits, (unsigned long long)g_cam);
            else
                Log("CAM mapped done scanned-MB=%u vtable-hits=%d tag-hits=%d other-hits=%d obj=%llx",
                    (unsigned)(g_mapScanned >> 20), g_vtHits, g_tagHits, g_otherHits, (unsigned long long)g_cam);
            g_scanMark = -1;
        }
    } else if (g_scanPhase == 0 && (int)(g_scanned >> 30) > g_scanMark) {
        g_scanMark = (int)(g_scanned >> 30);
        Log("CAM search so-far GB=%d", g_scanMark);
    } else if (g_scanPhase == 1 && (int)(g_mapScanned >> 30) > g_scanMark) {
        g_scanMark = (int)(g_mapScanned >> 30);
        Log("CAM mapped so-far GB=%d", g_scanMark);
    }
}

static void LogCam(uint64_t base, const char* why, const float* player, int phase) {
    float v10[4], v20[4], v80[4], va0[4];
    float f160 = 0, f164 = 0, f168 = 0, f174 = 0;
    float pack[16];
    float d10 = -1.f, d20 = -1.f;
    memset(v10, 0, sizeof v10);
    memset(v20, 0, sizeof v20);
    memset(v80, 0, sizeof v80);
    memset(va0, 0, sizeof va0);
    if (!CamOk(base)) {
        if (phase >= 0 && phase < 3) g_camHave[phase] = 0;
        Log("CAM %s: CameraModeTps not found (vtable rva 38ef208)", why);
        return;
    }
    Rd(g_cam + 0x10, v10, 16);
    Rd(g_cam + 0x20, v20, 16);
    Rd(g_cam + 0x80, v80, 16);
    Rd(g_cam + 0xA0, va0, 16);
    Rd(g_cam + 0x160, &f160, 4);
    Rd(g_cam + 0x164, &f164, 4);
    Rd(g_cam + 0x168, &f168, 4);
    Rd(g_cam + 0x174, &f174, 4);
    if (player && Fin3(v10)) d10 = Dist3(v10, player);
    if (player && Fin3(v20)) d20 = Dist3(v20, player);
    Log("CAM %s obj=%llx v10 %.3f %.3f %.3f %.3f dist %.2f v20 %.3f %.3f %.3f %.3f dist %.2f v80 %.3f %.3f %.3f va0 %.3f %.3f %.3f f160 %.3f f164 %.3f f168 %.3f f174 %.3f",
        why, (unsigned long long)g_cam,
        v10[0], v10[1], v10[2], v10[3], d10, v20[0], v20[1], v20[2], v20[3], d20,
        v80[0], v80[1], v80[2], va0[0], va0[1], va0[2], f160, f164, f168, f174);
    pack[0] = v10[0]; pack[1] = v10[1]; pack[2] = v10[2];
    pack[3] = v20[0]; pack[4] = v20[1]; pack[5] = v20[2];
    pack[6] = v80[0]; pack[7] = v80[1]; pack[8] = v80[2];
    pack[9] = va0[0]; pack[10] = va0[1]; pack[11] = va0[2];
    pack[12] = f160; pack[13] = f164; pack[14] = f168; pack[15] = f174;
    if (phase >= 0 && phase < 3) {
        memcpy(g_camSnap[phase], pack, sizeof pack);
        g_camHave[phase] = 1;
    }
    if (IsUnit(v10)) Log("CAM %s v10 is a direction, yaw %.1f deg", why, atan2f(v10[0], v10[2]) * 57.29578f);
    if (IsUnit(v20)) Log("CAM %s v20 is a direction, yaw %.1f deg", why, atan2f(v20[0], v20[2]) * 57.29578f);
    if (IsUnit(v80)) Log("CAM %s v80 is a direction, yaw %.1f deg", why, atan2f(v80[0], v80[2]) * 57.29578f);
    if (IsUnit(va0)) Log("CAM %s va0 is a direction, yaw %.1f deg", why, atan2f(va0[0], va0[2]) * 57.29578f);
}

static void Reread(int phase, const float* player) {
    int i;
    (void)player;
    for (i = 0; i < g_n; i++) {
        float tmp[16];
        memset(tmp, 0, sizeof tmp);
        g_slot[i].good[phase] = Rd(g_slot[i].addr, tmp, (size_t)g_slot[i].nfloat * sizeof(float));
        if (g_slot[i].good[phase]) memcpy(g_slot[i].v[phase], tmp, (size_t)g_slot[i].nfloat * sizeof(float));
    }
    for (i = 0; i < g_bn; i++) {
        g_blob[i].good[phase] = Rd((uintptr_t)g_blob[i].addr, g_blob[i].b[phase], (size_t)g_blob[i].len);
    }
}

static void TopAdd(float mag, const char* line) {
    int i, worst = 0;
    if (g_topN < kTop) {
        g_top[g_topN].mag = mag;
        strncpy(g_top[g_topN].line, line, sizeof g_top[0].line - 1);
        g_top[g_topN].line[sizeof g_top[0].line - 1] = 0;
        g_topN++;
        return;
    }
    for (i = 1; i < kTop; i++) if (g_top[i].mag < g_top[worst].mag) worst = i;
    if (mag <= g_top[worst].mag) return;
    g_top[worst].mag = mag;
    strncpy(g_top[worst].line, line, sizeof g_top[0].line - 1);
    g_top[worst].line[sizeof g_top[0].line - 1] = 0;
}

static void TopFlush(void) {
    int i, j;
    for (i = 0; i < g_topN; i++) {
        int best = i;
        for (j = i + 1; j < g_topN; j++) if (g_top[j].mag > g_top[best].mag) best = j;
        if (best != i) {
            Top t = g_top[i];
            g_top[i] = g_top[best];
            g_top[best] = t;
        }
        Log("%s", g_top[i].line);
    }
}

static float DeltaN(const float* a, const float* b, int n) {
    float s = 0.f;
    int i;
    for (i = 0; i < n; i++) s += fabsf(a[i] - b[i]);
    return s;
}

// Stick left, then right, then still often swings a value one way and back. Rank that above one-way drift.
static float Swing(const float* a, const float* b, const float* c, int n) {
    float mag = 0.f;
    int i, rev = 0;
    for (i = 0; i < n; i++) {
        float ab = b[i] - a[i], bc = c[i] - b[i], m = fabsf(ab) > fabsf(bc) ? fabsf(ab) : fabsf(bc);
        if (m > mag) mag = m;
        if (ab * bc < 0.f && fabsf(ab) > 0.02f && fabsf(bc) > 0.02f) rev = 1;
    }
    if (rev) mag *= 4.f;
    return mag;
}

static void Report(const float pos[3][4], int have[3]) {
    int i, changed = 0, stableH = 0;
    char line[220];
    g_topN = 0;
    static const char* camName[16] = {
        "v10x", "v10y", "v10z", "v20x", "v20y", "v20z", "v80x", "v80y", "v80z",
        "va0x", "va0y", "va0z", "f160", "f164", "f168", "f174"
    };
    Log("DIFF stand still except the camera stick. A=left B=right C=still.");
    if (g_camHave[0] && g_camHave[1] && g_camHave[2]) {
        int moved = 0;
        for (i = 0; i < 16; i++) {
            float fv[3], mag;
            fv[0] = g_camSnap[0][i];
            fv[1] = g_camSnap[1][i];
            fv[2] = g_camSnap[2][i];
            if (!Fin(fv[0]) || !Fin(fv[1]) || !Fin(fv[2])) continue;
            mag = Swing(fv, fv + 1, fv + 2, 1);
            if (mag < 0.01f) continue;
            moved++;
            Log("CAMCHG %s A=%.4f B=%.4f C=%.4f", camName[i], fv[0], fv[1], fv[2]);
        }
        if (!moved) Log("CAM fields did not move across the three shots.");
    } else {
        Log("CAM diff skipped: a shot had no CameraModeTps object.");
    }
    for (i = 0; i < 3; i++) {
        if (!have[i]) Log("shot %c: no player", g_snapName[i]);
        else Log("shot %c player pos %.3f %.3f %.3f", g_snapName[i], pos[i][0], pos[i][1], pos[i][2]);
    }
    for (i = 0; i < g_n; i++) {
        float dAB, dBC, mag;
        int nf = g_slot[i].kind == 4 ? 16 : 3;
        if (!g_slot[i].good[0] || !g_slot[i].good[1] || !g_slot[i].good[2]) continue;
        if (!Fin(g_slot[i].v[0][0]) || !Fin(g_slot[i].v[1][0]) || !Fin(g_slot[i].v[2][0])) continue;
        if (nf > 1 && (!Fin(g_slot[i].v[0][1]) || !Fin(g_slot[i].v[1][1]) || !Fin(g_slot[i].v[2][1]))) continue;
        if (nf > 2 && (!Fin(g_slot[i].v[0][2]) || !Fin(g_slot[i].v[1][2]) || !Fin(g_slot[i].v[2][2]))) continue;
        dAB = DeltaN(g_slot[i].v[0], g_slot[i].v[1], nf);
        dBC = DeltaN(g_slot[i].v[1], g_slot[i].v[2], nf);
        mag = Swing(g_slot[i].v[0], g_slot[i].v[1], g_slot[i].v[2], nf);
        if (mag < 0.02f) {
            if (g_slot[i].kind == 2 && stableH < 25) {
                stableH++;
                Log("HEIGHT stable addr=%llx d=%.2f y=%.3f (player y shot A %.3f) walk=%d",
                    (unsigned long long)g_slot[i].addr, g_slot[i].dist, g_slot[i].v[0][1], pos[0][1], g_slot[i].walk);
            }
            continue;
        }
        changed++;
        if (g_slot[i].kind == 4) {
            sprintf_s(line, sizeof line,
                "CHG matrix addr=%llx walk=%d dAB=%.3f dBC=%.3f A=(%.3f %.3f %.3f) B=(%.3f %.3f %.3f) C=(%.3f %.3f %.3f)",
                (unsigned long long)g_slot[i].addr, g_slot[i].walk, dAB, dBC,
                g_slot[i].v[0][12], g_slot[i].v[0][13], g_slot[i].v[0][14],
                g_slot[i].v[1][12], g_slot[i].v[1][13], g_slot[i].v[1][14],
                g_slot[i].v[2][12], g_slot[i].v[2][13], g_slot[i].v[2][14]);
        } else {
            sprintf_s(line, sizeof line,
                "CHG %s addr=%llx walk=%d d=%.2f dAB=%.3f dBC=%.3f A=(%.3f %.3f %.3f) B=(%.3f %.3f %.3f) C=(%.3f %.3f %.3f)",
                KindName(g_slot[i].kind), (unsigned long long)g_slot[i].addr, g_slot[i].walk, g_slot[i].dist, dAB, dBC,
                g_slot[i].v[0][0], g_slot[i].v[0][1], g_slot[i].v[0][2],
                g_slot[i].v[1][0], g_slot[i].v[1][1], g_slot[i].v[1][2],
                g_slot[i].v[2][0], g_slot[i].v[2][1], g_slot[i].v[2][2]);
        }
        TopAdd(mag, line);
    }
    Log("heap/walk slots=%d changed=%d (top %d below) stable-height-shown=%d", g_n, changed, g_topN, stableH);
    TopFlush();
    g_topN = 0;
    changed = 0;
    for (i = 0; i < g_bn; i++) {
        int off;
        if (!g_blob[i].good[0] || !g_blob[i].good[1] || !g_blob[i].good[2]) continue;
        for (off = 0; off + 4 <= g_blob[i].len; off += 4) {
            float a, b, c, dAB, dBC, mag, fv[3];
            memcpy(&a, g_blob[i].b[0] + off, 4);
            memcpy(&b, g_blob[i].b[1] + off, 4);
            memcpy(&c, g_blob[i].b[2] + off, 4);
            if (!Fin(a) || !Fin(b) || !Fin(c)) continue;
            if (fabsf(a) > 1.0e5f && fabsf(b) > 1.0e5f && fabsf(a - b) < 2.f && fabsf(b - c) < 2.f) continue;
            dAB = fabsf(a - b);
            dBC = fabsf(b - c);
            fv[0] = a; fv[1] = b; fv[2] = c;
            mag = Swing(fv, fv + 1, fv + 2, 1);
            if (mag < 0.02f) continue;
            changed++;
            sprintf_s(line, sizeof line, "BLOB %llx+%03X A=%.4f B=%.4f C=%.4f dAB=%.4f dBC=%.4f",
                (unsigned long long)g_blob[i].addr, off, a, b, c, dAB, dBC);
            TopAdd(mag, line);
        }
    }
    Log("nearby objects=%d float-changes=%d (top %d below)", g_bn, changed, g_topN);
    TopFlush();
    Log("Next F7 starts again. Hold the camera stick LEFT, stand still, then press F7.");
}

static void NoteCtor(uint64_t base);
static void LogView(void);
static void ViewObjShot(int phase);
static void MgrShot(uint64_t base, int phase, const float* player);

static void LogSibPtr(int i, const char* name, uint64_t q, uint64_t base) {
    uint64_t v = 0;
    if (q < 0x10000) return;
    if (base && q >= base && q - base < 0x8000000) {
        Log("SIB %d %s rva=%llx", i, name, (unsigned long long)(q - base));
        return;
    }
    if (Rd((uintptr_t)q, &v, 8) && base && v >= base && v - base < 0x8000000)
        Log("SIB %d %s -> vt rva=%llx", i, name, (unsigned long long)(v - base));
}

static void LogSib(uint64_t base) {
    int i;
    Log("SIB saved=%d other-hits=%d scan-done=%d", g_sibN, g_otherHits, g_scanDone);
    for (i = 0; i < g_sibN; i++) {
        uint64_t a = g_sib[i];
        uint64_t vt = 0, q8 = 0, q10 = 0, q18 = 0;
        float f20[4], f30 = 0, f38 = 0, f40 = 0, f48 = 0;
        unsigned char b34 = 0;
        memset(f20, 0, sizeof f20);
        Rd((uintptr_t)a, &vt, 8);
        Rd((uintptr_t)(a + 8), &q8, 8);
        Rd((uintptr_t)(a + 0x10), &q10, 8);
        Rd((uintptr_t)(a + 0x18), &q18, 8);
        Rd((uintptr_t)(a + 0x20), f20, 16);
        Rd((uintptr_t)(a + 0x30), &f30, 4);
        Rd((uintptr_t)(a + 0x34), &b34, 1);
        Rd((uintptr_t)(a + 0x38), &f38, 4);
        Rd((uintptr_t)(a + 0x40), &f40, 4);
        Rd((uintptr_t)(a + 0x48), &f48, 4);
        Log("SIB %d addr=%llx vt-rva=%llx q8=%llx q10=%llx q18=%llx b34=%02x",
            i, (unsigned long long)a,
            (unsigned long long)(base && vt >= base ? vt - base : 0),
            (unsigned long long)q8, (unsigned long long)q10, (unsigned long long)q18, b34);
        Log("SIB %d f20=%.4f f24=%.4f f28=%.4f f2c=%.4f f30=%.4f f38=%.4f f40=%.4f f48=%.4f",
            i, f20[0], f20[1], f20[2], f20[3], f30, f38, f40, f48);
        LogSibPtr(i, "q8", q8, base);
        LogSibPtr(i, "q10", q10, base);
        LogSibPtr(i, "q18", q18, base);
    }
}

static void LogGame(uint64_t base) {
    int i;
    Log("GAME hits=%d game-tag=%d saved=%d", g_gameHits, g_gameTagHits, g_gameN);
    for (i = 0; i < g_gameN; i++) {
        float f10[4], f20[4], f30[4], f40[4];
        uint64_t tag = 0;
        memset(f10, 0, sizeof f10);
        memset(f20, 0, sizeof f20);
        memset(f30, 0, sizeof f30);
        memset(f40, 0, sizeof f40);
        Rd((uintptr_t)(g_game[i] + 0x10), f10, 16);
        Rd((uintptr_t)(g_game[i] + 0x20), f20, 16);
        Rd((uintptr_t)(g_game[i] + 0x30), f30, 16);
        Rd((uintptr_t)(g_game[i] + 0x40), f40, 16);
        Rd((uintptr_t)(g_game[i] + 0x58), &tag, 8);
        Log("GAME %d addr=%llx tag-rva=%llx", i, (unsigned long long)g_game[i],
            (unsigned long long)(base && tag >= base ? tag - base : 0));
        Log("GAME %d f10=%.4f %.4f %.4f %.4f", i, f10[0], f10[1], f10[2], f10[3]);
        Log("GAME %d f20=%.4f %.4f %.4f %.4f", i, f20[0], f20[1], f20[2], f20[3]);
        Log("GAME %d f30=%.4f %.4f %.4f %.4f", i, f30[0], f30[1], f30[2], f30[3]);
        Log("GAME %d f40=%.4f %.4f %.4f %.4f", i, f40[0], f40[1], f40[2], f40[3]);
    }
}

static void Shot(uint64_t base) {
    uint64_t actor = 0;
    float pos[4] = {0, 0, 0, 0};
    int have;
    static float saved[3][4];
    static int haveS[3];
    const char* hint = g_phase == 0 ? "LEFT" : g_phase == 1 ? "RIGHT" : "STILL";
    have = Player(base, &actor, pos);
    Log("F7 shot %c (%s) player=%d actor=%llx pos=%.3f %.3f %.3f",
        g_snapName[g_phase], hint, have, (unsigned long long)actor, pos[0], pos[1], pos[2]);
    memcpy(saved[g_phase], pos, sizeof pos);
    haveS[g_phase] = have;
    if (!have) {
        Log("No player yet. Load into a mission and stand still, then try F7 again.");
        return;
    }
    if (pos[0] == 0.f && pos[1] == 0.f && pos[2] == 0.f)
        Log("Player pos is 0,0,0. The scan still runs, but a real mission position is better.");
    NoteCtor(base);
    if (!CamOk(base) && !g_scanDone)
        Log("CAM search still running, scanned-MB=%u", (unsigned)(g_scanned >> 20));
    LogSib(base);
    LogGame(base);
    LogView();
    ViewObjShot(g_phase);
    MgrShot(base, g_phase, pos);
    LogCam(base, hint, pos, g_phase);
    if (g_phase == 0) {
        size_t priv, mapped;
        ClearCands();
        Walk(actor, pos);
        priv = ScanHeap(pos, actor + 0x110, MEM_PRIVATE);
        mapped = 0;
        if (priv < (64u << 20)) mapped = ScanHeap(pos, actor + 0x110, MEM_MAPPED);
        Log("shot A stored slots=%d (pos=%d height=%d pair=%d matrix=%d near=%d) nearby-objects=%d scanned-private-MB=%u mapped-MB=%u",
            g_n, g_kindN[1], g_kindN[2], g_kindN[3], g_kindN[4], g_kindN[5], g_bn,
            (unsigned)(priv >> 20), (unsigned)(mapped >> 20));
        Log("Now hold the stick RIGHT and press F7.");
        g_phase = 1;
        return;
    }
    Reread(g_phase, pos);
    if (g_phase == 1) {
        Log("shot B reread. Now let the stick go STILL and press F7.");
        g_phase = 2;
        return;
    }
    Report(saved, haveS);
    g_phase = 0;
}

static int RingOldest(void) { return (g_rn < kRing) ? 0 : g_ri; }

static void Sample(uint64_t base) {
    KiSample* s = &g_ring[g_ri];
    uint64_t actor = 0;
    float pos[4] = {0, 0, 0, 0};
    memset(s, 0, sizeof *s);
    s->ms = GetTickCount();
    if (Player(base, &actor, pos)) {
        s->pos[0] = pos[0];
        s->pos[1] = pos[1];
        s->pos[2] = pos[2];
        ReadAction(actor, s);
    }
    g_ri = (g_ri + 1) % kRing;
    if (g_rn < kRing) g_rn++;
}

static void DumpKi(void) {
    int n = g_rn, i, actC = 0, fileC = 0, motC = 0, kiC = 0, lines = 0;
    int wC[16], mC[8], ktC[6], bitC[4], c83 = 0, c85 = 0;
    memset(wC, 0, sizeof wC);
    memset(mC, 0, sizeof mC);
    memset(ktC, 0, sizeof ktC);
    memset(bitC, 0, sizeof bitC);
    Log("F8 Ki ring samples=%d (about the last %d s). Press R1 pulse about 10 times, then F8.",
        n, n > 0 ? (int)((g_ring[(g_ri + kRing - 1) % kRing].ms - g_ring[RingOldest()].ms) / 1000) : 0);
    if (n < 2) {
        Log("Not enough samples yet. Wait a few seconds in a mission.");
        return;
    }
    for (i = 1; i < n; i++) {
        const KiSample* a = &g_ring[(RingOldest() + i - 1) % kRing];
        const KiSample* b = &g_ring[(RingOldest() + i) % kRing];
        int k;
        if (a->act != b->act) actC++;
        if (a->fileAct != b->fileAct) fileC++;
        if (a->motion != b->motion) motC++;
        if (fabsf(a->ki - b->ki) > 1.0f) kiC++;
        for (k = 0; k < 16; k++) if (a->actW[k] != b->actW[k]) wC[k]++;
        for (k = 0; k < 8; k++) if (a->motW[k] != b->motW[k]) mC[k]++;
        for (k = 0; k < 6; k++) if (fabsf(a->kt[k] - b->kt[k]) > 0.05f) ktC[k]++;
        for (k = 0; k < 4; k++) if (a->bits[k] != b->bits[k]) bitC[k]++;
        if (a->k83 != b->k83) c83++;
        if (a->k85 != b->k85) c85++;
    }
    Log("change counts: act=%d file=%d motion=%d ki=%d", actC, fileC, motC, kiC);
    Log("action-object dword changes [0..15]= %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
        wC[0], wC[1], wC[2], wC[3], wC[4], wC[5], wC[6], wC[7],
        wC[8], wC[9], wC[10], wC[11], wC[12], wC[13], wC[14], wC[15]);
    Log("motion+0x300 dword changes [0..7]= %d %d %d %d %d %d %d %d",
        mC[0], mC[1], mC[2], mC[3], mC[4], mC[5], mC[6], mC[7]);
    Log("KiTick +54 +58 +5c +60 +74 +7c changes= %d %d %d %d %d %d  b83=%d b85=%d",
        ktC[0], ktC[1], ktC[2], ktC[3], ktC[4], ktC[5], c83, c85);
    Log("bits ctrl+A8 ctrl+1060 status+48 tick+80 changes= %d %d %d %d", bitC[0], bitC[1], bitC[2], bitC[3]);
    for (i = 1; i < n && lines < 160; i++) {
        const KiSample* a = &g_ring[(RingOldest() + i - 1) % kRing];
        const KiSample* b = &g_ring[(RingOldest() + i) % kRing];
        int interesting = a->act != b->act || a->fileAct != b->fileAct || a->motion != b->motion
            || fabsf(a->ki - b->ki) > 1.0f || a->k83 != b->k83 || a->k85 != b->k85;
        int k;
        if (!interesting) {
            for (k = 0; k < 16; k++) if (wC[k] >= 4 && wC[k] <= 40 && a->actW[k] != b->actW[k]) interesting = 1;
            for (k = 0; k < 8; k++) if (mC[k] >= 4 && mC[k] <= 40 && a->motW[k] != b->motW[k]) interesting = 1;
            for (k = 0; k < 6; k++) if (ktC[k] >= 2 && ktC[k] <= 80 && fabsf(a->kt[k] - b->kt[k]) > 0.05f) interesting = 1;
            for (k = 0; k < 4; k++) if (bitC[k] >= 2 && bitC[k] <= 80 && a->bits[k] != b->bits[k]) interesting = 1;
        }
        if (!interesting) continue;
        lines++;
        Log("t+%ums act %08X->%08X mot %u->%u ki %.1f->%.1f kt54 %.2f->%.2f kt58 %.2f->%.2f kt5c %.2f->%.2f kt60 %.2f->%.2f kt74 %.2f->%.2f kt7c %.2f->%.2f b83 %u->%u b85 %u->%u bits %08X %08X %08X %08X",
            b->ms - g_ring[RingOldest()].ms, a->act, b->act, a->motion, b->motion, a->ki, b->ki,
            a->kt[0], b->kt[0], a->kt[1], b->kt[1], a->kt[2], b->kt[2], a->kt[3], b->kt[3], a->kt[4], b->kt[4], a->kt[5], b->kt[5],
            a->k83, b->k83, a->k85, b->k85, b->bits[0], b->bits[1], b->bits[2], b->bits[3]);
    }
    Log("Ki lines written=%d. A field that changes about 20 times across 10 pulses is the one to keep.", lines);
}

static uint8_t* g_ctorStub;
static int g_ctorTries;
static uint8_t* g_viewStub;
static int g_viewTries;
static uint8_t* g_orbitStub;
static int g_orbitTries;
static volatile int g_distOn;
static volatile int g_yawOn;
static volatile int g_pitchOn;
static volatile int g_fovOn;
static volatile float g_distSet;
static volatile float g_yawDeg;
static volatile float g_pitchDeg;
static volatile float g_fovDeg;
static uint8_t* g_fovStub;
static int g_fovTries;
static volatile LONG g_orbitCalls;
static volatile LONG g_orbitWrites;
static volatile LONG g_fovCalls;
static volatile LONG g_fovWrites;
static volatile uint64_t g_orbitCamSeen;
static volatile uint64_t g_fovCamSeen;
static volatile float g_orbitDistSeen;
static volatile float g_fovSeen;
static volatile int g_orbitDistSt;
static volatile int g_fovSt;

static uint8_t g_mgrSnap[3][0x800];
static int g_mgrLen[3];
static uint64_t g_mgrPtr[3];
static float g_mgrPlayer[3][3];

static void EyeLine(int phase) {
    float e[3], dx, dy, dz, horiz, dist;
    const float* p = g_mgrPlayer[phase];
    char name = g_snapName[phase];
    if (g_mgrLen[phase] < 0x4C) {
        Log("EYE %c missing", name);
        return;
    }
    memcpy(e, g_mgrSnap[phase] + 0x40, 12);
    if (!Fin3(e) || !Fin3(p)) {
        Log("EYE %c not finite", name);
        return;
    }
    dx = e[0] - p[0];
    dy = e[1] - p[1];
    dz = e[2] - p[2];
    horiz = sqrtf(dx * dx + dz * dz);
    dist = sqrtf(dx * dx + dy * dy + dz * dz);
    Log("EYE %c xyz=%.4f %.4f %.4f height=%.2f horiz=%.2f dist=%.2f",
        name, e[0], e[1], e[2], dy, horiz, dist);
}

static void BasisLine(const char* tag, int off, int lim) {
    float a[4], b[4], c[4];
    if (off + 16 > lim) {
        Log("BASIS %s missing", tag);
        return;
    }
    memcpy(a, g_mgrSnap[0] + off, 16);
    memcpy(b, g_mgrSnap[1] + off, 16);
    memcpy(c, g_mgrSnap[2] + off, 16);
    Log("BASIS %s +%03X A=%.4f %.4f %.4f %.4f B=%.4f %.4f %.4f %.4f C=%.4f %.4f %.4f %.4f",
        tag, off, a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3], c[0], c[1], c[2], c[3]);
}

static int SameFloat(int earlier, int off) {
    return memcmp(g_mgrSnap[0] + earlier, g_mgrSnap[0] + off, 4) == 0
        && memcmp(g_mgrSnap[1] + earlier, g_mgrSnap[1] + off, 4) == 0
        && memcmp(g_mgrSnap[2] + earlier, g_mgrSnap[2] + off, 4) == 0;
}

static int FovBand(float x) {
    if (x >= 20.f && x <= 110.f) return 1;
    if (x >= 0.40f && x <= 1.50f) {
        if (fabsf(x - 0.5f) < 0.02f || fabsf(x - 1.0f) < 0.02f) return 0;
        return 2;
    }
    return 0;
}

static void MgrFov(int lim) {
    int off, n = 0;
    for (off = 0; off + 4 <= lim && n < 16; off += 4) {
        float a, b, c;
        int ba, bb, bc;
        memcpy(&a, g_mgrSnap[0] + off, 4);
        memcpy(&b, g_mgrSnap[1] + off, 4);
        memcpy(&c, g_mgrSnap[2] + off, 4);
        if (!Fin(a) || !Fin(b) || !Fin(c)) continue;
        if (fabsf(a - b) > 0.05f || fabsf(b - c) > 0.05f) continue;
        ba = FovBand(a);
        bb = FovBand(b);
        bc = FovBand(c);
        if (!ba || ba != bb || ba != bc) continue;
        Log("FOV +%03X A=%.4f B=%.4f C=%.4f band=%s", off, a, b, c, ba == 1 ? "deg" : "rad");
        n++;
    }
    Log("FOV stable=%d", n);
}

static void MgrDiff(void) {
    int off, n = 0, skipped = 0;
    int lim = g_mgrLen[0];
    Log("MGR diff ptr A=%llx B=%llx C=%llx",
        (unsigned long long)g_mgrPtr[0], (unsigned long long)g_mgrPtr[1], (unsigned long long)g_mgrPtr[2]);
    if (g_mgrLen[1] < lim) lim = g_mgrLen[1];
    if (g_mgrLen[2] < lim) lim = g_mgrLen[2];
    EyeLine(0);
    EyeLine(1);
    EyeLine(2);
    if (lim < 16) {
        Log("MGR diff skipped, a shot had no manager");
        return;
    }
    BasisLine("axis70", 0x70, lim);
    BasisLine("axis90", 0x90, lim);
    MgrFov(lim);
    for (off = 0; off + 4 <= lim; off += 4) {
        float a, b, c, dAB, dBC;
        int e, copy = 0;
        if (off == 0x40 || off == 0x44 || off == 0x48) continue;
        if (off >= 0x70 && off < 0xA0) continue;
        memcpy(&a, g_mgrSnap[0] + off, 4);
        memcpy(&b, g_mgrSnap[1] + off, 4);
        memcpy(&c, g_mgrSnap[2] + off, 4);
        if (!Fin(a) || !Fin(b) || !Fin(c)) continue;
        dAB = fabsf(a - b);
        dBC = fabsf(b - c);
        if (dAB < 0.05f && dBC < 0.05f) continue;
        if (dAB > 100000.f || dBC > 100000.f) continue;
        for (e = 0; e + 4 <= off; e += 4) {
            if (SameFloat(e, off)) { copy = 1; break; }
        }
        if (copy) { skipped++; continue; }
        Log("MGR +%03X A=%.4f B=%.4f C=%.4f dAB=%.4f dBC=%.4f", off, a, b, c, dAB, dBC);
        if (++n >= 48) {
            Log("MGR diff truncated");
            Log("MGR movers=%d copies-skipped=%d", n, skipped);
            return;
        }
    }
    Log("MGR movers=%d copies-skipped=%d", n, skipped);
}

static void LogOrbit(uint64_t base) {
    uint64_t mgr = 0, begin = 0, end = 0, cam = 0, tag = 0;
    float yaw = 0.f, pitch = 0.f, dist = 0.f, eye[3] = {0.f, 0.f, 0.f};
    if (!Rd(base + kCamMgr, &mgr, 8) || mgr < 0x10000) {
        Log("ORBIT mgr null");
        return;
    }
    if (!Rd((uintptr_t)(mgr + 8), &begin, 8) || !Rd((uintptr_t)(mgr + 0x10), &end, 8)
        || end < begin + 8 || (end - begin) > 0x2000
        || !Rd((uintptr_t)(end - 8), &cam, 8) || cam < 0x10000) {
        Log("ORBIT no camera list mgr=%llx", (unsigned long long)mgr);
        return;
    }
    Rd((uintptr_t)(cam + 0x58), &tag, 8);
    Rd((uintptr_t)(cam + 0x290), &yaw, 4);
    Rd((uintptr_t)(cam + 0x294), &pitch, 4);
    Rd((uintptr_t)(cam + 0x2f8), &dist, 4);
    Rd((uintptr_t)(cam + 0x310), eye, 12);
    Log("ORBIT cam=%llx tag-rva=%llx yaw=%.4f pitch=%.4f dist=%.2f eye=%.1f %.1f %.1f",
        (unsigned long long)cam, (unsigned long long)(tag >= base ? tag - base : 0),
        yaw, pitch, dist, eye[0], eye[1], eye[2]);
}

static void MgrShot(uint64_t base, int phase, const float* player) {
    uint64_t mgr = 0, vt = 0;
    if (phase < 0 || phase > 2) return;
    g_mgrLen[phase] = 0;
    g_mgrPtr[phase] = 0;
    memset(g_mgrSnap[phase], 0, 0x800);
    memset(g_mgrPlayer[phase], 0, sizeof g_mgrPlayer[phase]);
    if (player) memcpy(g_mgrPlayer[phase], player, 12);
    if (!Rd(base + kCamMgr, &mgr, 8) || mgr < 0x10000) {
        Log("MGR null");
        return;
    }
    g_mgrPtr[phase] = mgr;
    Rd((uintptr_t)mgr, &vt, 8);
    Log("MGR addr=%llx vt-rva=%llx", (unsigned long long)mgr,
        (unsigned long long)(vt >= base ? vt - base : 0));
    for (g_mgrLen[phase] = 0; g_mgrLen[phase] + 0x40 <= 0x800; g_mgrLen[phase] += 0x40) {
        if (!Rd((uintptr_t)(mgr + g_mgrLen[phase]), g_mgrSnap[phase] + g_mgrLen[phase], 0x40)) break;
    }
    Log("MGR bytes=%d", g_mgrLen[phase]);
    EyeLine(phase);
    if (g_mgrLen[phase] >= 0x180) {
        float fov = 0.f, aspect = 0.f;
        memcpy(&fov, g_mgrSnap[phase] + 0x178, 4);
        memcpy(&aspect, g_mgrSnap[phase] + 0x17c, 4);
        Log("LENS %c fov=%.4f rad aspect=%.4f", g_snapName[phase], fov, aspect);
    }
    LogOrbit(base);
    if (phase == 2) MgrDiff();
}

static void LogView(void) {
    uint64_t calls = 0, goodN = 0, th = 0;
    uint64_t q10 = 0, q18 = 0, q20 = 0, q28 = 0;
    float f10[2], f18[2], f20[2], f28[2];
    if (!g_viewStub) {
        Log("VIEW hook off");
        return;
    }
    memcpy(&calls, g_viewStub + 200, 8);
    memcpy(&goodN, g_viewStub + 208, 8);
    memcpy(&th, g_viewStub + 160, 8);
    memcpy(&q10, g_viewStub + 168, 8);
    memcpy(&q18, g_viewStub + 176, 8);
    memcpy(&q20, g_viewStub + 184, 8);
    memcpy(&q28, g_viewStub + 192, 8);
    memcpy(f10, &q10, 8);
    memcpy(f18, &q18, 8);
    memcpy(f20, &q20, 8);
    memcpy(f28, &q28, 8);
    Log("VIEW calls=%llu nonzero=%llu this=%llx",
        (unsigned long long)calls, (unsigned long long)goodN, (unsigned long long)th);
    Log("VIEW f10=%.4f %.4f f18=%.4f %.4f f20=%.4f %.4f f28=%.4f %.4f",
        f10[0], f10[1], f18[0], f18[1], f20[0], f20[1], f28[0], f28[1]);
    memset(g_viewStub + 200, 0, 16);
}

static uint8_t g_viewObj[3][0x300];
static int g_viewObjLen[3];
static uint64_t g_viewObjPtr[3];

static void ViewObjDiff(void) {
    int off, n = 0;
    int lim = g_viewObjLen[0];
    if (g_viewObjLen[1] < lim) lim = g_viewObjLen[1];
    if (g_viewObjLen[2] < lim) lim = g_viewObjLen[2];
    Log("VIEW obj ptr A=%llx B=%llx C=%llx",
        (unsigned long long)g_viewObjPtr[0], (unsigned long long)g_viewObjPtr[1],
        (unsigned long long)g_viewObjPtr[2]);
    if (lim < 16) {
        Log("VIEW obj movers=0");
        return;
    }
    for (off = 0; off + 4 <= lim; off += 4) {
        float a, b, c, dAB, dBC;
        memcpy(&a, g_viewObj[0] + off, 4);
        memcpy(&b, g_viewObj[1] + off, 4);
        memcpy(&c, g_viewObj[2] + off, 4);
        if (!Fin(a) || !Fin(b) || !Fin(c)) continue;
        dAB = fabsf(a - b);
        dBC = fabsf(b - c);
        if (dAB < 0.05f && dBC < 0.05f) continue;
        if (dAB > 100000.f || dBC > 100000.f) continue;
        Log("VIEW obj +%03X A=%.4f B=%.4f C=%.4f dAB=%.4f dBC=%.4f", off, a, b, c, dAB, dBC);
        if (++n >= 16) {
            Log("VIEW obj truncated");
            Log("VIEW obj movers=%d", n);
            return;
        }
    }
    Log("VIEW obj movers=%d", n);
}

static void ViewObjShot(int phase) {
    uint64_t th = 0;
    int n;
    if (phase < 0 || phase > 2) return;
    g_viewObjLen[phase] = 0;
    g_viewObjPtr[phase] = 0;
    memset(g_viewObj[phase], 0, 0x300);
    if (!g_viewStub) {
        Log("VIEW obj hook off");
        return;
    }
    memcpy(&th, g_viewStub + 160, 8);
    g_viewObjPtr[phase] = th;
    if (th < 0x10000) {
        Log("VIEW obj this=0");
        return;
    }
    for (n = 0; n + 0x40 <= 0x300; n += 0x40) {
        if (!Rd((uintptr_t)(th + n), g_viewObj[phase] + n, 0x40)) break;
        g_viewObjLen[phase] = n + 0x40;
    }
    Log("VIEW obj this=%llx bytes=%d", (unsigned long long)th, g_viewObjLen[phase]);
    if (phase == 2) ViewObjDiff();
}

static uint64_t CtorSlot(void) {
    uint64_t v = 0;
    if (!g_ctorStub) return 0;
    memcpy(&v, g_ctorStub + 40, 8);
    return v;
}

static void NoteCtor(uint64_t base) {
    static uint64_t last = 0;
    static int n = 0;
    uint64_t c = CtorSlot();
    uint64_t prev;
    if (!c || c == last) return;
    last = c;
    n++;
    Log("CAM ctor this=%llx n=%d", (unsigned long long)c, n);
    prev = g_cam;
    g_cam = c;
    if (!CamOk(base)) g_cam = prev;
}

static int HookCtor(uint64_t base) {
    static const uint8_t kHead[15] = {
        0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x68, 0x10, 0x48, 0x89, 0x70, 0x18
    };
    uint8_t* site;
    uint8_t* stub;
    uint8_t have[15];
    DWORD old = 0;
    int32_t disp;
    size_t n = 0;
    uint64_t back;
    uint64_t stubAddr;
    if (!base || g_ctorStub || g_ctorTries) return 0;
    g_ctorTries = 1;
    site = (uint8_t*)(base + 0x3b4288);
    memset(have, 0, sizeof have);
    if (!Rd((uintptr_t)site, have, 15) || memcmp(have, kHead, 15) != 0) {
        Log("CAM ctor bytes differ. first=%02X%02X%02X%02X hook skipped", have[0], have[1], have[2], have[3]);
        return 0;
    }
    stub = (uint8_t*)VirtualAlloc(0, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) {
        Log("CAM ctor stub alloc failed");
        return 0;
    }
    stub[n++] = 0x48;
    stub[n++] = 0x89;
    stub[n++] = 0x0D;
    disp = 40 - 7;
    memcpy(stub + n, &disp, 4);
    n += 4;
    memcpy(stub + n, kHead, 15);
    n += 15;
    stub[n++] = 0xFF;
    stub[n++] = 0x25;
    stub[n++] = 0;
    stub[n++] = 0;
    stub[n++] = 0;
    stub[n++] = 0;
    back = base + 0x3b4288 + 15;
    memcpy(stub + n, &back, 8);
    n += 8;
    if (!VirtualProtect(site, 16, PAGE_EXECUTE_READWRITE, &old)) {
        Log("CAM ctor protect failed %lu", GetLastError());
        return 0;
    }
    site[0] = 0xFF;
    site[1] = 0x25;
    site[2] = site[3] = site[4] = site[5] = 0;
    stubAddr = (uint64_t)stub;
    memcpy(site + 6, &stubAddr, 8);
    site[14] = 0x90;
    VirtualProtect(site, 16, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 16);
    FlushInstructionCache(GetCurrentProcess(), stub, 64);
    g_ctorStub = stub;
    Log("CAM ctor hook on. rva 3b4288");
    return 1;
}

static void EmitRipStore(uint8_t* stub, int* n, uint8_t modrm, int slot) {
    int at = *n;
    int32_t disp;
    stub[at] = 0x48;
    stub[at + 1] = 0x89;
    stub[at + 2] = modrm;
    disp = slot - (at + 7);
    memcpy(stub + at + 3, &disp, 4);
    *n = at + 7;
}

static int HookView(uint64_t base) {
    static const uint8_t kHead[15] = {
        0xF3, 0x0F, 0x10, 0x41, 0x10, 0xF3, 0x0F, 0x10, 0x49, 0x18, 0xF3, 0x0F, 0x5C, 0x41, 0x20
    };
    uint8_t* site;
    uint8_t* stub;
    uint8_t have[15];
    DWORD old = 0;
    int n = 0;
    int32_t rel;
    uint64_t back;
    uint64_t stubAddr;
    if (!base || g_viewStub || g_viewTries) return 0;
    g_viewTries = 1;
    site = (uint8_t*)(base + 0x9fd530);
    memset(have, 0, sizeof have);
    if (!Rd((uintptr_t)site, have, 15) || memcmp(have, kHead, 15) != 0) {
        Log("VIEW bytes differ. first=%02X%02X%02X%02X hook skipped", have[0], have[1], have[2], have[3]);
        return 0;
    }
    stub = (uint8_t*)VirtualAlloc(0, 256, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) {
        Log("VIEW stub alloc failed");
        return 0;
    }
    memset(stub, 0, 256);
    stub[n++] = 0x48; stub[n++] = 0x8B; stub[n++] = 0x41; stub[n++] = 0x10;
    EmitRipStore(stub, &n, 0x05, 168);
    stub[n++] = 0x48; stub[n++] = 0x85; stub[n++] = 0xC0;
    stub[n++] = 0x74;
    stub[n++] = 14;
    EmitRipStore(stub, &n, 0x0D, 160);
    stub[n++] = 0xF0; stub[n++] = 0x48; stub[n++] = 0xFF; stub[n++] = 0x05;
    rel = 208 - (n + 4);
    memcpy(stub + n, &rel, 4);
    n += 4;
    stub[n++] = 0x48; stub[n++] = 0x8B; stub[n++] = 0x41; stub[n++] = 0x18;
    EmitRipStore(stub, &n, 0x05, 176);
    stub[n++] = 0x48; stub[n++] = 0x8B; stub[n++] = 0x41; stub[n++] = 0x20;
    EmitRipStore(stub, &n, 0x05, 184);
    stub[n++] = 0x48; stub[n++] = 0x8B; stub[n++] = 0x41; stub[n++] = 0x28;
    EmitRipStore(stub, &n, 0x05, 192);
    stub[n++] = 0xF0; stub[n++] = 0x48; stub[n++] = 0xFF; stub[n++] = 0x05;
    rel = 200 - (n + 4);
    memcpy(stub + n, &rel, 4);
    n += 4;
    if (n > 140) {
        Log("VIEW stub too big %d", n);
        return 0;
    }
    memcpy(stub + n, kHead, 15);
    n += 15;
    stub[n++] = 0xFF; stub[n++] = 0x25;
    stub[n++] = 0; stub[n++] = 0; stub[n++] = 0; stub[n++] = 0;
    back = base + 0x9fd530 + 15;
    memcpy(stub + n, &back, 8);
    if (!VirtualProtect(site, 16, PAGE_EXECUTE_READWRITE, &old)) {
        Log("VIEW protect failed %lu", GetLastError());
        return 0;
    }
    site[0] = 0xFF;
    site[1] = 0x25;
    site[2] = site[3] = site[4] = site[5] = 0;
    stubAddr = (uint64_t)stub;
    memcpy(site + 6, &stubAddr, 8);
    site[14] = 0x90;
    VirtualProtect(site, 16, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 16);
    FlushInstructionCache(GetCurrentProcess(), stub, 256);
    g_viewStub = stub;
    Log("VIEW hook on. rva 9fd530");
    return 1;
}

static const char* TouchWord(int st) {
    if (st == 4) return "read";
    if (st == 3) return "wrote";
    if (st == 2) return "out-of-range";
    if (st == 1) return "unreadable";
    return "ini-off";
}

/* 0 ini off, 1 unreadable, 2 live float outside the expected band, 3 wrote. */
static int TouchFloat(uint8_t* cam, int off, int on, float lo, float hi, float set, float* curOut) {
    float cur = 0.f;
    *curOut = 0.f;
    if (!on) return 0;
    if (!cam || !Rd((uintptr_t)(cam + off), &cur, 4)) return 1;
    *curOut = cur;
    if (cur < lo || cur > hi) return 2;
    if (!Wr((uintptr_t)(cam + off), &set, 4)) return 1;
    return 3;
}

static volatile float g_b0Seen;
static volatile int g_b0St;
static volatile float g_camFovSeen;
static volatile int g_camFovSt;
static volatile float g_copyFovSeen;
static volatile int g_copyFovSt;

/*
 * Live fields, from the decompile of the third-person camera update 2aeaa8 (rcx = camera, same object as 2b0e4c):
 *   cam+0xB0  boom length. 2aeaa8 sets it from 2aafc4 each frame, blends it (450 pull, 953730, 9565e4), then after
 *             2b0e4c it reads it again: eye = pivot + normalize(cam+0x310 - pivot) * [cam+0xB0].
 *   cam+0x40  lens in radians. Set from 2aae2c each frame, pulled toward 0.8290314 (47.5 deg) by the pitch blend, then
 *             after 2b0e4c copied into the output camera +0x148 (manager +0x178 is that copy).
 *   2b0e4c (the hooked entry) reads neither field and writes neither, so a store at its entry is what the eye and the
 *   lens use this frame. +2F8 is a timer (2aeaa8 adds dt up to 1000; 900.5 in the 1.0.16 log was 900 + dt), and
 *   +290/+294 are one-shot stick deltas that force 2b0e4c's early exit, so none of them is written any more.
 */
static int ApplyOrbit(uint8_t* cam, float* b0, float* fov40, int* bs, int* fs) {
    *bs = TouchFloat(cam, 0x0b0, g_distOn, 50.f, 4000.f, g_distSet, b0);
    *fs = TouchFloat(cam, 0x40, g_fovOn, 0.3f, 1.6f, g_fovDeg * 0.017453292f, fov40);
    return *bs == 3 || *fs == 3;
}

/* 8dad94 is a 16:9 helper fed from the manager camera copy. Read only: +178 and +458 show whether the source write reached the copies. */
static void ReadFov(uint8_t* cam, float* cur, float* copy, int* st, int* cst) {
    *cur = 0.f;
    *copy = 0.f;
    *st = Rd((uintptr_t)(cam + 0x178), cur, 4) ? 4 : 1;
    *cst = Rd((uintptr_t)(cam + 0x458), copy, 4) ? 4 : 1;
}

static void OrbitTouch(uint8_t* cam, uint64_t* retSlot) {
    float b0 = 0.f, fov40 = 0.f, timer = 0.f;
    int bs, fs;
    LONG n;
    (void)retSlot;
    if (ApplyOrbit(cam, &b0, &fov40, &bs, &fs)) InterlockedIncrement(&g_orbitWrites);
    n = InterlockedIncrement(&g_orbitCalls);
    Rd((uintptr_t)(cam + 0x2f8), &timer, 4);
    g_orbitCamSeen = (uint64_t)(uintptr_t)cam;
    g_orbitDistSeen = timer;
    g_orbitDistSt = 4;
    g_b0Seen = b0;
    g_b0St = bs;
    g_camFovSeen = fov40;
    g_camFovSt = fs;
    if (n <= 4 || n == 120) {
        Log("ORBIT fire n=%ld cam=%llx +B0 %s %.1f +40 %s %.4f (+2F8 timer %.1f read only)",
            (long)n, (unsigned long long)(uintptr_t)cam,
            TouchWord(bs), b0, TouchWord(fs), fov40, timer);
    }
}

static int HookOrbit(uint64_t base) {
    static const uint8_t kHead[15] = {
        0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x70, 0x10, 0x48, 0x89, 0x78, 0x18
    };
    uint8_t* site;
    uint8_t* stub;
    uint8_t have[15];
    DWORD old = 0;
    int n = 0;
    uint64_t back;
    uint64_t stubAddr;
    uint64_t fn;
    if (!base || g_orbitStub || g_orbitTries) return 0;
    g_orbitTries = 1;
    site = (uint8_t*)(base + kOrbitFn);
    memset(have, 0, sizeof have);
    if (!Rd((uintptr_t)site, have, 15) || memcmp(have, kHead, 15) != 0) {
        Log("ORBIT bytes differ. first=%02X%02X%02X%02X hook skipped", have[0], have[1], have[2], have[3]);
        return 0;
    }
    stub = (uint8_t*)VirtualAlloc(0, 256, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) {
        Log("ORBIT stub alloc failed");
        return 0;
    }
    memset(stub, 0, 256);
    stub[n++] = 0x51;
    stub[n++] = 0x52;
    stub[n++] = 0x41; stub[n++] = 0x50;
    stub[n++] = 0x41; stub[n++] = 0x51;
    stub[n++] = 0x48; stub[n++] = 0x81; stub[n++] = 0xEC; stub[n++] = 0x88; stub[n++] = 0x00; stub[n++] = 0x00; stub[n++] = 0x00;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x44; stub[n++] = 0x24; stub[n++] = 0x20;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x4C; stub[n++] = 0x24; stub[n++] = 0x30;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x54; stub[n++] = 0x24; stub[n++] = 0x40;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x5C; stub[n++] = 0x24; stub[n++] = 0x50;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x64; stub[n++] = 0x24; stub[n++] = 0x60;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x6C; stub[n++] = 0x24; stub[n++] = 0x70;
    stub[n++] = 0x48; stub[n++] = 0x8D; stub[n++] = 0x94; stub[n++] = 0x24; stub[n++] = 0xA8; stub[n++] = 0x00; stub[n++] = 0x00; stub[n++] = 0x00;
    stub[n++] = 0x48; stub[n++] = 0x8B; stub[n++] = 0x8C; stub[n++] = 0x24; stub[n++] = 0xA0; stub[n++] = 0x00; stub[n++] = 0x00; stub[n++] = 0x00;
    stub[n++] = 0x48; stub[n++] = 0xB8;
    fn = (uint64_t)&OrbitTouch;
    memcpy(stub + n, &fn, 8);
    n += 8;
    stub[n++] = 0xFF; stub[n++] = 0xD0;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x44; stub[n++] = 0x24; stub[n++] = 0x20;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x4C; stub[n++] = 0x24; stub[n++] = 0x30;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x54; stub[n++] = 0x24; stub[n++] = 0x40;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x5C; stub[n++] = 0x24; stub[n++] = 0x50;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x64; stub[n++] = 0x24; stub[n++] = 0x60;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x6C; stub[n++] = 0x24; stub[n++] = 0x70;
    stub[n++] = 0x48; stub[n++] = 0x81; stub[n++] = 0xC4; stub[n++] = 0x88; stub[n++] = 0x00; stub[n++] = 0x00; stub[n++] = 0x00;
    stub[n++] = 0x41; stub[n++] = 0x59;
    stub[n++] = 0x41; stub[n++] = 0x58;
    stub[n++] = 0x5A;
    stub[n++] = 0x59;
    if (n > 180) {
        Log("ORBIT stub too big %d", n);
        return 0;
    }
    memcpy(stub + n, kHead, 15);
    n += 15;
    stub[n++] = 0xFF; stub[n++] = 0x25;
    stub[n++] = 0; stub[n++] = 0; stub[n++] = 0; stub[n++] = 0;
    back = base + kOrbitFn + 15;
    memcpy(stub + n, &back, 8);
    if (!VirtualProtect(site, 16, PAGE_EXECUTE_READWRITE, &old)) {
        Log("ORBIT protect failed %lu", GetLastError());
        return 0;
    }
    site[0] = 0xFF;
    site[1] = 0x25;
    site[2] = site[3] = site[4] = site[5] = 0;
    stubAddr = (uint64_t)stub;
    memcpy(site + 6, &stubAddr, 8);
    site[14] = 0x90;
    VirtualProtect(site, 16, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 16);
    FlushInstructionCache(GetCurrentProcess(), stub, 256);
    g_orbitStub = stub;
    Log("ORBIT hook installed. rva 2b0e4c. An ORBIT fire line is the proof it ran. ctor is 3b4288, not this.");
    return 1;
}

static uint8_t* g_blendStub;
static int g_blendTries;
static volatile LONG g_blendCalls;
static volatile LONG g_blendWrites;
static volatile float g_blendB0Seen;

/*
 * 9565e4(cam, pivot*, dist* = cam+B0, lens* = cam+40, ...) is the last blend 2aeaa8 runs on +B0/+40 before
 * 2af0d2 reads +B0 into the eye offset that 2b0e4c chases into cam+310, and cam+310 becomes the eye (2aeaa8, after
 * collision: eye = [cam+310]). 1.0.18 wrote +B0 at 2b0e4c entry, after that read, so only collision saw 900.
 * Writing here, at entry, also lets the game's own scripted blend in 9565e4 (+1F0 timer) still run on top.
 */
static void BlendTouch(uint8_t* cam, uint64_t* retSlot) {
    float b0 = 0.f, fov40 = 0.f;
    int bs, fs;
    LONG n;
    (void)retSlot;
    bs = TouchFloat(cam, 0x0b0, g_distOn, 50.f, 4000.f, g_distSet, &b0);
    fs = TouchFloat(cam, 0x40, g_fovOn, 0.3f, 1.6f, g_fovDeg * 0.017453292f, &fov40);
    if (bs == 3 || fs == 3) InterlockedIncrement(&g_blendWrites);
    n = InterlockedIncrement(&g_blendCalls);
    g_blendB0Seen = b0;
    if (n <= 4 || n == 120) {
        Log("BLEND fire n=%ld cam=%llx +B0 %s %.1f +40 %s %.4f",
            (long)n, (unsigned long long)(uintptr_t)cam, TouchWord(bs), b0, TouchWord(fs), fov40);
    }
}

static int HookBlend(uint64_t base) {
    static const uint8_t kHead[15] = {
        0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x68, 0x10, 0x48, 0x89, 0x70, 0x18
    };
    uint8_t* site;
    uint8_t* stub;
    uint8_t have[15];
    DWORD old = 0;
    int n = 0;
    uint64_t back;
    uint64_t stubAddr;
    uint64_t fn;
    if (!base || g_blendStub || g_blendTries) return 0;
    g_blendTries = 1;
    site = (uint8_t*)(base + kBlendFn);
    memset(have, 0, sizeof have);
    if (!Rd((uintptr_t)site, have, 15) || memcmp(have, kHead, 15) != 0) {
        Log("BLEND bytes differ. first=%02X%02X%02X%02X hook skipped", have[0], have[1], have[2], have[3]);
        return 0;
    }
    stub = (uint8_t*)VirtualAlloc(0, 256, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) {
        Log("BLEND stub alloc failed");
        return 0;
    }
    memset(stub, 0, 256);
    stub[n++] = 0x51;
    stub[n++] = 0x52;
    stub[n++] = 0x41; stub[n++] = 0x50;
    stub[n++] = 0x41; stub[n++] = 0x51;
    stub[n++] = 0x48; stub[n++] = 0x81; stub[n++] = 0xEC; stub[n++] = 0x88; stub[n++] = 0x00; stub[n++] = 0x00; stub[n++] = 0x00;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x44; stub[n++] = 0x24; stub[n++] = 0x20;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x4C; stub[n++] = 0x24; stub[n++] = 0x30;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x54; stub[n++] = 0x24; stub[n++] = 0x40;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x5C; stub[n++] = 0x24; stub[n++] = 0x50;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x64; stub[n++] = 0x24; stub[n++] = 0x60;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x6C; stub[n++] = 0x24; stub[n++] = 0x70;
    stub[n++] = 0x48; stub[n++] = 0x8D; stub[n++] = 0x94; stub[n++] = 0x24; stub[n++] = 0xA8; stub[n++] = 0x00; stub[n++] = 0x00; stub[n++] = 0x00;
    stub[n++] = 0x48; stub[n++] = 0x8B; stub[n++] = 0x8C; stub[n++] = 0x24; stub[n++] = 0xA0; stub[n++] = 0x00; stub[n++] = 0x00; stub[n++] = 0x00;
    stub[n++] = 0x48; stub[n++] = 0xB8;
    fn = (uint64_t)&BlendTouch;
    memcpy(stub + n, &fn, 8);
    n += 8;
    stub[n++] = 0xFF; stub[n++] = 0xD0;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x44; stub[n++] = 0x24; stub[n++] = 0x20;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x4C; stub[n++] = 0x24; stub[n++] = 0x30;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x54; stub[n++] = 0x24; stub[n++] = 0x40;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x5C; stub[n++] = 0x24; stub[n++] = 0x50;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x64; stub[n++] = 0x24; stub[n++] = 0x60;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x6C; stub[n++] = 0x24; stub[n++] = 0x70;
    stub[n++] = 0x48; stub[n++] = 0x81; stub[n++] = 0xC4; stub[n++] = 0x88; stub[n++] = 0x00; stub[n++] = 0x00; stub[n++] = 0x00;
    stub[n++] = 0x41; stub[n++] = 0x59;
    stub[n++] = 0x41; stub[n++] = 0x58;
    stub[n++] = 0x5A;
    stub[n++] = 0x59;
    if (n > 180) {
        Log("BLEND stub too big %d", n);
        return 0;
    }
    memcpy(stub + n, kHead, 15);
    n += 15;
    stub[n++] = 0xFF; stub[n++] = 0x25;
    stub[n++] = 0; stub[n++] = 0; stub[n++] = 0; stub[n++] = 0;
    back = base + kBlendFn + 15;
    memcpy(stub + n, &back, 8);
    if (!VirtualProtect(site, 16, PAGE_EXECUTE_READWRITE, &old)) {
        Log("BLEND protect failed %lu", GetLastError());
        return 0;
    }
    site[0] = 0xFF;
    site[1] = 0x25;
    site[2] = site[3] = site[4] = site[5] = 0;
    stubAddr = (uint64_t)stub;
    memcpy(site + 6, &stubAddr, 8);
    site[14] = 0x90;
    VirtualProtect(site, 16, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 16);
    FlushInstructionCache(GetCurrentProcess(), stub, 256);
    g_blendStub = stub;
    Log("BLEND hook installed. rva 9565e4 (last distance/lens blend before 2aeaa8 builds the eye offset at 2af0d2). A BLEND fire line is the proof it ran.");
    return 1;
}

static void FovTouch(uint8_t* cam, uint64_t* retSlot) {
    float cur = 0.f, copy = 0.f;
    int st, cst;
    LONG n;
    (void)retSlot;
    ReadFov(cam, &cur, &copy, &st, &cst);
    n = InterlockedIncrement(&g_fovCalls);
    g_fovCamSeen = (uint64_t)(uintptr_t)cam;
    g_fovSeen = cur;
    g_fovSt = st;
    g_copyFovSeen = copy;
    g_copyFovSt = cst;
    if (n <= 4 || n == 120) {
        Log("FOV fire n=%ld cam=%llx +178 %s %.4f +458 %s %.4f",
            (long)n, (unsigned long long)(uintptr_t)cam,
            TouchWord(st), cur, TouchWord(cst), copy);
    }
}

static int HookFov(uint64_t base) {
    static const uint8_t kHead[14] = {
        0x48, 0x89, 0x5C, 0x24, 0x10, 0x55, 0x48, 0x8D, 0xAC, 0x24, 0x20, 0xFF, 0xFF, 0xFF
    };
    uint8_t* site;
    uint8_t* stub;
    uint8_t have[14];
    DWORD old = 0;
    int n = 0;
    uint64_t back, stubAddr, fn;
    if (!base || g_fovStub || g_fovTries) return 0;
    g_fovTries = 1;
    site = (uint8_t*)(base + kFovFn);
    memset(have, 0, sizeof have);
    if (!Rd((uintptr_t)site, have, 14) || memcmp(have, kHead, 14) != 0) {
        Log("FOV bytes differ. first=%02X%02X%02X%02X hook skipped", have[0], have[1], have[2], have[3]);
        return 0;
    }
    stub = (uint8_t*)VirtualAlloc(0, 256, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) return 0;
    memset(stub, 0, 256);
    stub[n++] = 0x51; stub[n++] = 0x52;
    stub[n++] = 0x41; stub[n++] = 0x50;
    stub[n++] = 0x41; stub[n++] = 0x51;
    stub[n++] = 0x48; stub[n++] = 0x81; stub[n++] = 0xEC; stub[n++] = 0x88; stub[n++] = 0; stub[n++] = 0; stub[n++] = 0;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x44; stub[n++] = 0x24; stub[n++] = 0x20;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x4C; stub[n++] = 0x24; stub[n++] = 0x30;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x54; stub[n++] = 0x24; stub[n++] = 0x40;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x5C; stub[n++] = 0x24; stub[n++] = 0x50;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x64; stub[n++] = 0x24; stub[n++] = 0x60;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x6C; stub[n++] = 0x24; stub[n++] = 0x70;
    stub[n++] = 0x48; stub[n++] = 0x8D; stub[n++] = 0x94; stub[n++] = 0x24; stub[n++] = 0xA8; stub[n++] = 0; stub[n++] = 0; stub[n++] = 0;
    stub[n++] = 0x48; stub[n++] = 0x8B; stub[n++] = 0x8C; stub[n++] = 0x24; stub[n++] = 0xA0; stub[n++] = 0; stub[n++] = 0; stub[n++] = 0;
    stub[n++] = 0x48; stub[n++] = 0xB8;
    fn = (uint64_t)&FovTouch;
    memcpy(stub + n, &fn, 8);
    n += 8;
    stub[n++] = 0xFF; stub[n++] = 0xD0;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x44; stub[n++] = 0x24; stub[n++] = 0x20;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x4C; stub[n++] = 0x24; stub[n++] = 0x30;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x54; stub[n++] = 0x24; stub[n++] = 0x40;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x5C; stub[n++] = 0x24; stub[n++] = 0x50;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x64; stub[n++] = 0x24; stub[n++] = 0x60;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x6C; stub[n++] = 0x24; stub[n++] = 0x70;
    stub[n++] = 0x48; stub[n++] = 0x81; stub[n++] = 0xC4; stub[n++] = 0x88; stub[n++] = 0; stub[n++] = 0; stub[n++] = 0;
    stub[n++] = 0x41; stub[n++] = 0x59;
    stub[n++] = 0x41; stub[n++] = 0x58;
    stub[n++] = 0x5A; stub[n++] = 0x59;
    memcpy(stub + n, kHead, 14);
    n += 14;
    stub[n++] = 0xFF; stub[n++] = 0x25; stub[n++] = 0; stub[n++] = 0; stub[n++] = 0; stub[n++] = 0;
    back = base + kFovFn + 14;
    memcpy(stub + n, &back, 8);
    if (!VirtualProtect(site, 16, PAGE_EXECUTE_READWRITE, &old)) {
        Log("FOV protect failed %lu", GetLastError());
        return 0;
    }
    site[0] = 0xFF; site[1] = 0x25;
    site[2] = site[3] = site[4] = site[5] = 0;
    stubAddr = (uint64_t)stub;
    memcpy(site + 6, &stubAddr, 8);
    VirtualProtect(site, 16, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 14);
    FlushInstructionCache(GetCurrentProcess(), stub, 256);
    g_fovStub = stub;
    Log("FOV hook installed. rva 8dad94. A FOV fire line is the proof it ran.");
    return 1;
}

/*
 * Camera position offset (CamOffsetRight / CamOffsetUp).
 * 2aeaa8 ends by filling its output camera (rsi = param_2, the manager camera at mgr+30): +10 eye, +20 target,
 * +30 up, +140..+150 lens, then calls 243ed0(out, 1, 1), which builds the view matrices from those fields
 * (call at rva 2afc54, return 2afc59). 243ed0 is shared by many cameras, so the hook only acts when its return
 * address is 2afc59. Eye and target both move by the same vector, so the view direction (yaw/pitch) is unchanged.
 * Nothing in the camera's own state (cam+10/+20/+270/+310, 2ab9f8 does not read the output) is touched, so there
 * is no feedback: with both offsets at 0 the hook writes nothing and the game camera is the native one.
 */
static const uintptr_t kOutFn = 0x243ed0;
static const uintptr_t kOutRet = 0x2afc59;
static const float kOffMax = 400.f;
static uint8_t* g_outStub;
static int g_outTries;
static uint64_t g_outRetAbs;
static volatile LONG g_outCalls;
static volatile LONG g_outWrites;
static volatile float g_offRightSet;
static volatile float g_offUpSet;
static float g_offRight;
static float g_offUp;
static volatile float g_offEyeSeen[3];
static volatile float g_offRightSeen;
static volatile float g_offUpSeen;

static float Ease(float cur, float want) {
    float d = want - cur;
    if (d < 0.5f && d > -0.5f) return want;
    return cur + d * 0.2f;
}

static void OutTouch(uint8_t* out, uint64_t* retSlot) {
    float eye[4], tgt[4], f[3], r[3], u[3], len, d[3];
    LONG n;
    if (!retSlot || !g_outRetAbs || *retSlot != g_outRetAbs) return;
    InterlockedIncrement(&g_outCalls);
    g_offRight = Ease(g_offRight, g_offRightSet);
    g_offUp = Ease(g_offUp, g_offUpSet);
    g_offRightSeen = g_offRight;
    g_offUpSeen = g_offUp;
    if (g_offRight == 0.f && g_offUp == 0.f) return;
    if (!Rd((uintptr_t)(out + 0x10), eye, 16) || !Rd((uintptr_t)(out + 0x20), tgt, 16)) return;
    if (!Fin3(eye) || !Fin3(tgt)) return;
    f[0] = tgt[0] - eye[0];
    f[1] = tgt[1] - eye[1];
    f[2] = tgt[2] - eye[2];
    len = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (!(len > 1.0f)) return;
    f[0] /= len; f[1] /= len; f[2] /= len;
    /* right = forward x worldUp (horizontal), up = worldUp with the forward part removed. */
    r[0] = -f[2]; r[1] = 0.f; r[2] = f[0];
    len = sqrtf(r[0] * r[0] + r[2] * r[2]);
    if (len < 0.01f) return;
    r[0] /= len; r[2] /= len;
    u[0] = -f[0] * f[1]; u[1] = 1.f - f[1] * f[1]; u[2] = -f[2] * f[1];
    len = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    if (len < 0.01f) return;
    u[0] /= len; u[1] /= len; u[2] /= len;
    d[0] = r[0] * g_offRight + u[0] * g_offUp;
    d[1] = r[1] * g_offRight + u[1] * g_offUp;
    d[2] = r[2] * g_offRight + u[2] * g_offUp;
    eye[0] += d[0]; eye[1] += d[1]; eye[2] += d[2];
    tgt[0] += d[0]; tgt[1] += d[1]; tgt[2] += d[2];
    if (!Wr((uintptr_t)(out + 0x10), eye, 12) || !Wr((uintptr_t)(out + 0x20), tgt, 12)) return;
    g_offEyeSeen[0] = eye[0];
    g_offEyeSeen[1] = eye[1];
    g_offEyeSeen[2] = eye[2];
    n = InterlockedIncrement(&g_outWrites);
    if (n <= 3 || n == 600) {
        Log("OFFSET fire n=%ld out=%llx right %.1f up %.1f fwd %.3f %.3f %.3f rightv %.3f %.3f eye now %.1f %.1f %.1f",
            (long)n, (unsigned long long)(uintptr_t)out, g_offRight, g_offUp, f[0], f[1], f[2], r[0], r[2],
            eye[0], eye[1], eye[2]);
    }
}

static int HookOut(uint64_t base) {
    static const uint8_t kHead[15] = {
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x32, 0xC0, 0x41, 0x8A, 0xF8
    };
    static const uint8_t kCall[5] = { 0xE8, 0x77, 0x42, 0xF9, 0xFF };
    uint8_t* site;
    uint8_t* stub;
    uint8_t have[15], call[5];
    DWORD old = 0;
    int n = 0;
    uint64_t back, stubAddr, fn;
    if (!base || g_outStub || g_outTries) return 0;
    g_outTries = 1;
    site = (uint8_t*)(base + kOutFn);
    memset(have, 0, sizeof have);
    memset(call, 0, sizeof call);
    if (!Rd((uintptr_t)site, have, 15) || memcmp(have, kHead, 15) != 0) {
        Log("OFFSET bytes differ at 243ed0. first=%02X%02X%02X%02X hook skipped", have[0], have[1], have[2], have[3]);
        return 0;
    }
    if (!Rd((uintptr_t)(base + kOutRet - 5), call, 5) || memcmp(call, kCall, 5) != 0) {
        Log("OFFSET call site 2afc54 differs. %02X%02X%02X%02X%02X hook skipped", call[0], call[1], call[2], call[3], call[4]);
        return 0;
    }
    stub = (uint8_t*)VirtualAlloc(0, 256, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) return 0;
    memset(stub, 0, 256);
    stub[n++] = 0x51; stub[n++] = 0x52;
    stub[n++] = 0x41; stub[n++] = 0x50;
    stub[n++] = 0x41; stub[n++] = 0x51;
    stub[n++] = 0x48; stub[n++] = 0x81; stub[n++] = 0xEC; stub[n++] = 0x88; stub[n++] = 0; stub[n++] = 0; stub[n++] = 0;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x44; stub[n++] = 0x24; stub[n++] = 0x20;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x4C; stub[n++] = 0x24; stub[n++] = 0x30;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x54; stub[n++] = 0x24; stub[n++] = 0x40;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x5C; stub[n++] = 0x24; stub[n++] = 0x50;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x64; stub[n++] = 0x24; stub[n++] = 0x60;
    stub[n++] = 0x0F; stub[n++] = 0x29; stub[n++] = 0x6C; stub[n++] = 0x24; stub[n++] = 0x70;
    stub[n++] = 0x48; stub[n++] = 0x8D; stub[n++] = 0x94; stub[n++] = 0x24; stub[n++] = 0xA8; stub[n++] = 0; stub[n++] = 0; stub[n++] = 0;
    stub[n++] = 0x48; stub[n++] = 0x8B; stub[n++] = 0x8C; stub[n++] = 0x24; stub[n++] = 0xA0; stub[n++] = 0; stub[n++] = 0; stub[n++] = 0;
    stub[n++] = 0x48; stub[n++] = 0xB8;
    fn = (uint64_t)&OutTouch;
    memcpy(stub + n, &fn, 8);
    n += 8;
    stub[n++] = 0xFF; stub[n++] = 0xD0;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x44; stub[n++] = 0x24; stub[n++] = 0x20;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x4C; stub[n++] = 0x24; stub[n++] = 0x30;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x54; stub[n++] = 0x24; stub[n++] = 0x40;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x5C; stub[n++] = 0x24; stub[n++] = 0x50;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x64; stub[n++] = 0x24; stub[n++] = 0x60;
    stub[n++] = 0x0F; stub[n++] = 0x28; stub[n++] = 0x6C; stub[n++] = 0x24; stub[n++] = 0x70;
    stub[n++] = 0x48; stub[n++] = 0x81; stub[n++] = 0xC4; stub[n++] = 0x88; stub[n++] = 0; stub[n++] = 0; stub[n++] = 0;
    stub[n++] = 0x41; stub[n++] = 0x59;
    stub[n++] = 0x41; stub[n++] = 0x58;
    stub[n++] = 0x5A; stub[n++] = 0x59;
    memcpy(stub + n, kHead, 15);
    n += 15;
    stub[n++] = 0xFF; stub[n++] = 0x25; stub[n++] = 0; stub[n++] = 0; stub[n++] = 0; stub[n++] = 0;
    back = base + kOutFn + 15;
    memcpy(stub + n, &back, 8);
    g_outRetAbs = base + kOutRet;
    if (!VirtualProtect(site, 16, PAGE_EXECUTE_READWRITE, &old)) {
        Log("OFFSET protect failed %lu", GetLastError());
        return 0;
    }
    site[0] = 0xFF; site[1] = 0x25;
    site[2] = site[3] = site[4] = site[5] = 0;
    stubAddr = (uint64_t)stub;
    memcpy(site + 6, &stubAddr, 8);
    site[14] = 0x90;
    VirtualProtect(site, 16, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 16);
    FlushInstructionCache(GetCurrentProcess(), stub, 256);
    g_outStub = stub;
    Log("OFFSET hook installed. rva 243ed0, acts only for the return 2afc59 (third-person output camera). An OFFSET fire line is the proof it moved the eye.");
    return 1;
}

static int IniFloat(const char* s, const char* key, float* out) {
    size_t n = strlen(key);
    float sign = 1.f, v = 0.f, div = 1.f;
    int frac = 0, any = 0;
    if (strncmp(s, key, n) != 0 || s[n] != '=') return 0;
    s += n + 1;
    if (*s == '+') s++;
    else if (*s == '-') { sign = -1.f; s++; }
    for (; *s; s++) {
        if (*s >= '0' && *s <= '9') {
            any = 1;
            if (!frac) v = v * 10.f + (float)(*s - '0');
            else { div *= 10.f; v += (float)(*s - '0') / div; }
        } else if (*s == '.' && !frac) {
            frac = 1;
        } else break;
    }
    if (!any) return 0;
    *out = sign * v;
    return 1;
}

static void LoadCamIni(void) {
    wchar_t path[MAX_PATH];
    FILE* f;
    char line[160];
    int sawDist = 0, sawYaw = 0, sawPitch = 0, sawFov = 0;
    float dist = 0.f, yaw = 0.f, pitch = 0.f, fov = 0.f, offR = 0.f, offU = 0.f;
    DWORD n;
    int distOn, yawOn, pitchOn, fovOn;
    n = GetModuleFileNameW(g_mod, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    {
        wchar_t* slash = wcsrchr(path, L'\\');
        if (!slash) return;
        wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - path), L"Nioh3CamKiProbe.ini");
    }
    f = _wfopen(path, L"rb");
    if (!f) {
        distOn = yawOn = pitchOn = fovOn = 0;
    } else {
        while (fgets(line, (int)sizeof line, f)) {
            if (line[0] == ';' || line[0] == '#') continue;
            if (IniFloat(line, "CamDist", &dist)) sawDist = 1;
            if (IniFloat(line, "CamYawDeg", &yaw)) sawYaw = 1;
            if (IniFloat(line, "CamPitchDeg", &pitch)) sawPitch = 1;
            if (IniFloat(line, "CamFovDeg", &fov)) sawFov = 1;
            IniFloat(line, "CamOffsetRight", &offR);
            IniFloat(line, "CamOffsetUp", &offU);
        }
        fclose(f);
        distOn = sawDist && dist >= 50.f && dist <= 4000.f;
        yawOn = sawYaw && yaw >= -180.f && yaw <= 180.f;
        pitchOn = sawPitch && pitch >= -80.f && pitch <= 80.f;
        fovOn = sawFov && fov >= 20.f && fov <= 90.f;
    }
    if (distOn != g_distOn || yawOn != g_yawOn || pitchOn != g_pitchOn || fovOn != g_fovOn
        || (distOn && dist != g_distSet) || (yawOn && yaw != g_yawDeg)
        || (pitchOn && pitch != g_pitchDeg) || (fovOn && fov != g_fovDeg)) {
        Log("CAM ini dist=%s%.1f yaw=%s%.1f pitch=%s%.1f fov=%s%.1f",
            distOn ? "" : "off ", distOn ? dist : 0.f,
            yawOn ? "" : "off ", yawOn ? yaw : 0.f,
            pitchOn ? "" : "off ", pitchOn ? pitch : 0.f,
            fovOn ? "" : "off ", fovOn ? fov : 0.f);
    }
    if (!(offR >= -kOffMax && offR <= kOffMax)) offR = offR > 0.f ? kOffMax : -kOffMax;
    if (!(offU >= -kOffMax && offU <= kOffMax)) offU = offU > 0.f ? kOffMax : -kOffMax;
    if (offR != g_offRightSet || offU != g_offUpSet)
        Log("CAM ini offset right=%.1f up=%.1f%s", offR, offU, (offR == 0.f && offU == 0.f) ? " (released, native camera)" : "");
    g_offRightSet = offR;
    g_offUpSet = offU;
    g_distSet = distOn ? dist : 0.f;
    g_yawDeg = yawOn ? yaw : 0.f;
    g_pitchDeg = pitchOn ? pitch : 0.f;
    g_fovDeg = fovOn ? fov : 0.f;
    g_distOn = distOn;
    g_yawOn = yawOn;
    g_pitchOn = pitchOn;
    g_fovOn = fovOn;
}

static void LogBoom(uint64_t base) {
    uint64_t actor = 0, mgr = 0, begin = 0, end = 0, cam = 0;
    float pos[4] = {0, 0, 0, 0};
    float eye[3] = {0, 0, 0};
    float b0 = 0.f, f40 = 0.f, m458 = 0.f, m178 = 0.f, ob0 = 0.f, of40 = 0.f;
    uint64_t ocam = g_orbitCamSeen;
    float dx, dy, dz, boom, horiz;
    if (!Player(base, &actor, pos)) {
        Log("BOOM no player");
        return;
    }
    if (!Rd(base + kCamMgr, &mgr, 8) || mgr < 0x10000) {
        Log("BOOM no manager");
        return;
    }
    Rd((uintptr_t)(mgr + 0x40), eye, 12);
    Rd((uintptr_t)(mgr + 0x178), &m178, 4);
    Rd((uintptr_t)(mgr + 0x458), &m458, 4);
    if (Rd((uintptr_t)(mgr + 8), &begin, 8) && Rd((uintptr_t)(mgr + 0x10), &end, 8)
        && end >= begin + 8 && (end - begin) <= 0x2000)
        Rd((uintptr_t)(end - 8), &cam, 8);
    if (cam > 0x10000) {
        Rd((uintptr_t)(cam + 0x0b0), &b0, 4);
        Rd((uintptr_t)(cam + 0x40), &f40, 4);
    }
    if (ocam > 0x10000) {
        Rd((uintptr_t)(ocam + 0x0b0), &ob0, 4);
        Rd((uintptr_t)(ocam + 0x40), &of40, 4);
    }
    dx = eye[0] - pos[0];
    dy = eye[1] - pos[1];
    dz = eye[2] - pos[2];
    boom = sqrtf(dx * dx + dy * dy + dz * dz);
    horiz = sqrtf(dx * dx + dz * dz);
    Log("BOOM eye %.1f horiz %.1f height %.1f | list cam=%llx +B0 %.1f +40 %.4f | orbit cam=%llx +B0 %.1f +40 %.4f (%.1f deg) | mgr+178 %.4f mgr+458 %.4f",
        boom, horiz, dy, (unsigned long long)cam, b0, f40,
        (unsigned long long)ocam, ob0, of40, of40 * 57.29578f, m178, m458);
}

/*
 * Read only. Where along 2aeaa8 the eye loses the +B0 length:
 *   d310 = |cam+310 - cam+280| smoothed direction point (2b0e4c) to pivot
 *   d270 = |cam+270 - cam+280| eye after collision 2acdc4 and framing 2afd70 (stored at 2aeaa8 end of that block)
 *   d10  = |cam+10 - cam+20|   eye after the per-frame limiter 8b4598
 *   dOut = |mgr+40 - mgr+50|   output camera eye to target
 * Collision state: +344 smoothed hit length, +340 hit byte, +341 any-hit byte, +324 hold byte.
 * Modes: +610 framing (2afd70), +238 lock blend, +23C limiter lerp, +364/+36C lock state, +231 snap.
 */
static void LogChain(uint64_t base) {
    uint64_t mgr = 0, c = g_orbitCamSeen;
    float e10[4] = {0}, t20[4] = {0}, e270[4] = {0}, p280[4] = {0}, s310[4] = {0}, me[4] = {0}, mt[4] = {0};
    float b0 = 0.f, c344 = 0.f, f238 = 0.f, f23c = 0.f, f364 = 0.f;
    uint8_t b324 = 0, b340 = 0, b341 = 0, b231 = 0, b610 = 0;
    int i36c = 0;
    if (c < 0x10000) {
        Log("CHAIN no orbit camera yet");
        return;
    }
    Rd((uintptr_t)(c + 0x10), e10, 12);
    Rd((uintptr_t)(c + 0x20), t20, 12);
    Rd((uintptr_t)(c + 0x270), e270, 12);
    Rd((uintptr_t)(c + 0x280), p280, 12);
    Rd((uintptr_t)(c + 0x310), s310, 12);
    Rd((uintptr_t)(c + 0xb0), &b0, 4);
    Rd((uintptr_t)(c + 0x344), &c344, 4);
    Rd((uintptr_t)(c + 0x238), &f238, 4);
    Rd((uintptr_t)(c + 0x23c), &f23c, 4);
    Rd((uintptr_t)(c + 0x364), &f364, 4);
    Rd((uintptr_t)(c + 0x36c), &i36c, 4);
    Rd((uintptr_t)(c + 0x324), &b324, 1);
    Rd((uintptr_t)(c + 0x340), &b340, 1);
    Rd((uintptr_t)(c + 0x341), &b341, 1);
    Rd((uintptr_t)(c + 0x231), &b231, 1);
    Rd((uintptr_t)(c + 0x610), &b610, 1);
    if (Rd(base + kCamMgr, &mgr, 8) && mgr > 0x10000) {
        Rd((uintptr_t)(mgr + 0x40), me, 12);
        Rd((uintptr_t)(mgr + 0x50), mt, 12);
    }
    Log("CHAIN blend calls=%ld writes=%ld pre+B0 %.1f", (long)g_blendCalls, (long)g_blendWrites, g_blendB0Seen);
    Log("CHAIN +B0 %.1f d310 %.1f d270 %.1f d10 %.1f dOut %.1f | +344 %.1f +340 %u +341 %u +324 %u | +610 %u +238 %.3f +23C %.3f +364 %.3f +36C %d +231 %u",
        b0, Dist3(s310, p280), Dist3(e270, p280), Dist3(e10, t20), Dist3(me, mt),
        c344, b340, b341, b324, b610, f238, f23c, f364, i36c, b231);
}

static DWORD WINAPI Worker(LPVOID) {
    int was7 = 0, was8 = 0;
    uint32_t beat = 0, iniBeat = 0;
    OpenLog();
    Log("==== %s ====", kVer);
    Log("Stand still. F7 with the camera stick LEFT, then RIGHT, then STILL.");
    Log("The camera search walks every private page in small slices after the player appears. F7 does not hitch.");
    Log("Then attack, tap R1 at short gaps, and press F8. KiTick floats are status+0x98.");
    Log("Nioh3CamKiProbe.ini beside this ASI: CamDist 50..4000 (camera +B0), CamFovDeg 20..90 (camera +40, radians). A missing key stays on the game.");
    Log("CamYawDeg and CamPitchDeg are ignored in this build. +290/+294 are stick deltas, not angles.");
    Log("CamOffsetRight / CamOffsetUp (world units, same as CamDist, -400..400): move the eye and target together along the camera right and up vectors at 243ed0 (return 2afc59). 0 or missing = native camera.");
    Log("Writes happen at the entry of 9565e4 (before the eye offset reads +B0) and again at the entry of 2b0e4c.");
    while (g_run) {
        uint64_t base = (uint64_t)GetModuleHandleW(L"Nioh3.exe");
        if (base) HookCtor(base);
        if (base) HookView(base);
        if (base) HookOrbit(base);
        if (base) HookBlend(base);
        if (base) HookFov(base);
        if (base) HookOut(base);
        if (GetTickCount() - iniBeat > 2000) {
            LoadCamIni();
            iniBeat = GetTickCount();
        }
        if (base) NoteCtor(base);
        if (base) {
            uint64_t actor = 0;
            float pos[4] = {0, 0, 0, 0};
            if (Player(base, &actor, pos)) ScanCamSlice(base, (size_t)128 << 20);
        }
        int f7 = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
        int f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        if (f7 && !was7) Shot(base);
        if (f8 && !was8) DumpKi();
        was7 = f7;
        was8 = f8;
        Sample(base);
        if (GetTickCount() - beat > 10000) {
            uint64_t actor = 0;
            float pos[4] = {0, 0, 0, 0};
            int have = Player(base, &actor, pos);
            KiSample tmp;
            LONG oc, fc, ow, fw;
            static LONG prevOrbit, prevFov;
            memset(&tmp, 0, sizeof tmp);
            if (have) ReadAction(actor, &tmp);
            beat = GetTickCount();
            oc = g_orbitCalls;
            fc = g_fovCalls;
            ow = g_orbitWrites;
            fw = g_fovWrites;
            Log("alive shot-next=%c samples=%d player=%d pos=%.2f %.2f %.2f act=%08X ki=%.1f cam=%llx",
                g_snapName[g_phase], g_rn, have, pos[0], pos[1], pos[2], tmp.act, tmp.ki, (unsigned long long)g_cam);
            Log("HOOK orbit calls=%ld (+%ld/10s) writes=%ld cam=%llx +2F8timer %s %.1f +B0 %s %.1f +40 %s %.4f | fov calls=%ld (+%ld/10s) writes=%ld cam=%llx +178 %s %.4f +458 %s %.4f",
                (long)oc, (long)(oc - prevOrbit), (long)ow,
                (unsigned long long)g_orbitCamSeen, TouchWord(g_orbitDistSt), g_orbitDistSeen,
                TouchWord(g_b0St), g_b0Seen, TouchWord(g_camFovSt), g_camFovSeen,
                (long)fc, (long)(fc - prevFov), (long)fw,
                (unsigned long long)g_fovCamSeen, TouchWord(g_fovSt), g_fovSeen,
                TouchWord(g_copyFovSt), g_copyFovSeen);
            Log("OFFSET calls=%ld writes=%ld set right %.1f up %.1f now right %.1f up %.1f eye %.1f %.1f %.1f",
                (long)g_outCalls, (long)g_outWrites, g_offRightSet, g_offUpSet, g_offRightSeen, g_offUpSeen,
                g_offEyeSeen[0], g_offEyeSeen[1], g_offEyeSeen[2]);
            LogBoom(base);
            LogChain(base);
            prevOrbit = oc;
            prevFov = fc;
        }
        Sleep(12);
    }
    if (g_log) fclose(g_log);
    g_log = 0;
    return 0;
}

extern "C" __declspec(dllexport) const char* Nioh3CamKiProbeVersion(void) { return kVer; }

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_mod = (HMODULE)h;
        DisableThreadLibraryCalls((HMODULE)h);
        OpenLog();
        {
            uint64_t base = (uint64_t)GetModuleHandleW(L"Nioh3.exe");
            if (base) HookCtor(base);
            if (base) HookView(base);
            if (base) HookOrbit(base);
            if (base) HookBlend(base);
            if (base) HookFov(base);
            if (base) HookOut(base);
        }
        CreateThread(0, 0, Worker, 0, 0, 0);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_run = 0;
    }
    return TRUE;
}

#endif
