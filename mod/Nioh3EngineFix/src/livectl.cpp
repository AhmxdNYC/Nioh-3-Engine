// livectl.cpp - LiveCtl control-file transport for one-launch A/B tests. See livectl.h and notes\fast-testing.md.
// [Debug] LiveCtl = 0 by default: nothing is started.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "livectl.h"

namespace livectl {
namespace {

StartInfo g_si;
std::wstring g_ctl, g_ack;
volatile LONG g_on = 0;

// frame clock (OnFrame, Present-hook thread = the only writer; seqlock for the ack thread's consistent snapshot)
struct Clock {
    volatile LONG seq = 0;
    long long frames = 0, lastQpc = 0, qpc = 0;
    unsigned long long sq = 0;  // sum of (frame time us)^2 / 1000
    long long h20 = 0, h33 = 0, h50 = 0;
} g_clk;
long long g_qpf = 0;

void Snapshot(Clock& out) {
    for (int i = 0; i < 1000; ++i) {
        const LONG s0 = g_clk.seq;
        if (s0 & 1) {
            YieldProcessor();
            continue;
        }
        MemoryBarrier();
        out.frames = g_clk.frames;
        out.qpc = g_clk.qpc;
        out.sq = g_clk.sq;
        out.h20 = g_clk.h20;
        out.h33 = g_clk.h33;
        out.h50 = g_clk.h50;
        MemoryBarrier();
        if (g_clk.seq == s0) return;
    }
}

// last apply (written by the control thread only)
struct Last {
    std::string gen = "-", tag = "-", kv;  // kv: " KEY=readback ..." in file order
    int ok = 0, fail = 0;
    unsigned long long tApplyUnixMs = 0, tApplyTick = 0;
} g_last;

#define LLOG(...)                                   \
    do {                                            \
        if (g_si.log) {                             \
            char _b[900];                           \
            snprintf(_b, sizeof _b, __VA_ARGS__);   \
            g_si.log("LIVE: %s", _b);               \
        }                                           \
    } while (0)

unsigned long long UnixMs() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    const unsigned long long t = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return t / 10000ull - 11644473600000ull;
}

std::string Narrow(const std::wstring& w) {
    std::string s;
    for (wchar_t c : w) s.push_back((c >= 32 && c < 127) ? (char)c : '?');
    return s;
}
std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}
std::string NoSpaces(const std::string& s) {
    std::string o;
    for (char c : s) o.push_back(c == ' ' || c == '\t' ? '_' : c);
    return o.empty() ? "-" : o;
}

// "(value N)" at the end of a menu::Remote reply
bool ReplyValue(const char* r, int& v) {
    const char* p = strstr(r, "(value ");
    if (!p) return false;
    v = atoi(p + 7);
    return true;
}

bool ApplyOne(const std::string& key, const std::string& val, std::string& readback, std::string& why) {
    char reply[256] = {};
    if (!_strnicmp(key.c_str(), "Hot.", 4)) {
        if (!g_si.hotSet) {
            why = "ShaderHot not built in";
            return false;
        }
        const bool ok = g_si.hotSet(key.c_str() + 4, atoi(val.c_str()) != 0 ? 1 : 0, reply, sizeof reply);
        why = reply;
        if (!ok) return false;
        const int now = g_si.hotGet ? g_si.hotGet(key.c_str() + 4) : -1;
        readback = std::to_string(now);
        return now == (atoi(val.c_str()) != 0 ? 1 : 0);
    }
    if (!_strnicmp(key.c_str(), "Tuning.", 7)) {
        if (!g_si.tuning || !g_si.tuning(key.c_str() + 7, val.c_str(), reply, sizeof reply)) {
            why = "unknown or not-live tuning key";
            return false;
        }
        why = reply;
        const char* eq = strchr(reply, '=');
        readback = NoSpaces(Trim(eq ? eq + 1 : val));
        return true;
    }
    if (!g_si.remote) {
        why = "menu not available";
        return false;
    }
    const int want = atoi(val.c_str());
    const int rc = g_si.remote(key.c_str(), 1, want, reply, sizeof reply, 3000);
    why = reply;
    int v = 0;
    if (rc != 0 || !ReplyValue(reply, v)) return false;
    readback = std::to_string(v);
    // bool rows read back 0/1; numeric rows (R1, max latency, WorkerCap) read back the (clamped) value itself
    return v == want || ((v != 0) == (want != 0));
}

void WriteAck() {
    if (g_ack.empty()) return;
    char head[400];
    const int n = _snprintf_s(head, sizeof head, _TRUNCATE, "gen=%s tag=%s t_apply_unix_ms=%llu age_ms=%llu ok=%d fail=%d", g_last.gen.c_str(),
                              g_last.tag.c_str(), g_last.tApplyUnixMs, g_last.tApplyTick ? GetTickCount64() - g_last.tApplyTick : 0ull,
                              g_last.ok, g_last.fail);
    if (n <= 0) return;
    Clock c;
    Snapshot(c);
    char clk[200];
    _snprintf_s(clk, sizeof clk, _TRUNCATE, " frames=%lld qpc_us=%lld ft_sq_us=%llu h20=%lld h33=%lld h50=%lld", c.frames,
                g_qpf ? (long long)((double)c.qpc * 1e6 / (double)g_qpf) : 0ll, c.sq, c.h20, c.h33, c.h50);
    std::string line = head + std::string(clk) + g_last.kv + "\n";
    const std::wstring tmp = g_ack + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    WriteFile(h, line.data(), (DWORD)line.size(), &w, nullptr);
    CloseHandle(h);
    MoveFileExW(tmp.c_str(), g_ack.c_str(), MOVEFILE_REPLACE_EXISTING);
}

void Apply() {
    std::vector<wchar_t> buf(32768);
    const DWORD got = GetPrivateProfileSectionW(L"Live", buf.data(), (DWORD)buf.size(), g_ctl.c_str());
    Last L;
    L.tApplyUnixMs = UnixMs();
    L.tApplyTick = GetTickCount64();
    std::string summary;
    for (const wchar_t* p = buf.data(); p < buf.data() + got && *p; p += wcslen(p) + 1) {
        const std::string line = Narrow(p);
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, eq)), val = Trim(line.substr(eq + 1));
        if (key.empty() || key[0] == ';' || key[0] == '#') continue;
        if (!_stricmp(key.c_str(), "Gen")) {
            L.gen = NoSpaces(val);
            continue;
        }
        if (!_stricmp(key.c_str(), "Tag")) {
            L.tag = NoSpaces(val);
            continue;
        }
        std::string rb = "err", why;
        const bool ok = ApplyOne(key, val, rb, why);
        if (!ok) rb = "err";
        ++(ok ? L.ok : L.fail);
        L.kv += " " + NoSpaces(key) + "=" + rb;
        summary += (summary.empty() ? "" : "; ") + key + "=" + val + (ok ? " ok" : " FAILED (" + why + ")");
    }
    g_last = L;
    LLOG("apply gen %s '%s': %d ok, %d failed%s%s", L.gen.c_str(), L.tag.c_str(), L.ok, L.fail, summary.empty() ? "" : " - ",
         summary.c_str());
    WriteAck();
}

DWORD WINAPI CtlThread(void*) {
    FILETIME lastT = {};
    ULONGLONG lastS = ~0ull, lastAck = 0;
    {   // a file left from an earlier run is stale: remember it, do not apply it
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExW(g_ctl.c_str(), GetFileExInfoStandard, &fa)) {
            lastT = fa.ftLastWriteTime;
            lastS = ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
            LLOG("control file already exists at start - ignored until it changes");
        }
    }
    for (;;) {
        Sleep(100);
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExW(g_ctl.c_str(), GetFileExInfoStandard, &fa)) {
            const ULONGLONG sz = ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
            if (CompareFileTime(&fa.ftLastWriteTime, &lastT) != 0 || sz != lastS) {
                lastT = fa.ftLastWriteTime;
                lastS = sz;
                Sleep(20);  // a writer that does not rename: let it finish
                Apply();
                lastAck = GetTickCount64();
            }
        }
        const ULONGLONG now = GetTickCount64();
        if (now - lastAck >= 250) {
            lastAck = now;
            WriteAck();
        }
    }
}

std::wstring DirOf(const std::wstring& p) {
    const size_t k = p.find_last_of(L"\\/");
    return k == std::wstring::npos ? L"." : p.substr(0, k);
}

}  // namespace

void Start(const StartInfo& info) {
    g_si = info;
    const std::wstring ini = info.iniPath ? info.iniPath : L"";
    g_si.iniPath = nullptr;  // not kept
    if (GetPrivateProfileIntW(L"Debug", L"LiveCtl", 0, ini.c_str()) != 1) return;
    wchar_t f[MAX_PATH] = {};
    GetPrivateProfileStringW(L"Debug", L"LiveCtlFile", L"Nioh3EngineFix_live.ini", f, MAX_PATH, ini.c_str());
    std::wstring ctl = f[0] ? f : L"Nioh3EngineFix_live.ini";
    if (ctl.find(L':') == std::wstring::npos && ctl[0] != L'\\') ctl = DirOf(ini) + L"\\" + ctl;
    g_ctl = ctl;
    g_ack = ctl + L".ack";
    DeleteFileW(g_ack.c_str());
    HANDLE h = CreateThread(nullptr, 0, CtlThread, nullptr, 0, nullptr);
    if (!h) {
        LLOG("could not start the control thread - LiveCtl off");
        return;
    }
    CloseHandle(h);
    LARGE_INTEGER qf;
    QueryPerformanceFrequency(&qf);
    g_qpf = qf.QuadPart;
    InterlockedExchange(&g_on, 1);
    LLOG("ON - control file %s (polled every 100 ms, [Live] KEY = value: menu rows, Hot.<set>, Tuning.<key>), ack %s.ack. "
         "Test harness transport; leave it off for play.",
         Narrow(g_ctl).c_str(), Narrow(g_ctl).c_str());
}

bool Enabled() { return g_on != 0; }

void OnFrame() {
    if (!g_on) return;
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    InterlockedIncrement(&g_clk.seq);  // odd: being written
    if (g_clk.lastQpc && g_qpf) {
        const double us = (double)(q.QuadPart - g_clk.lastQpc) * 1e6 / (double)g_qpf;
        g_clk.sq += (unsigned long long)(us * us / 1000.0);
        g_clk.h20 += us > 20000.0;
        g_clk.h33 += us > 33333.0;
        g_clk.h50 += us > 50000.0;
    }
    g_clk.lastQpc = q.QuadPart;
    g_clk.qpc = q.QuadPart;
    ++g_clk.frames;
    InterlockedIncrement(&g_clk.seq);  // even: consistent
}

}  // namespace livectl
