// reportsend.cpp - player report upload for the menu Report box. See reportsend.h.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <winhttp.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "reportsend.h"
#include "crashupload.h"
#include "inputlog.h"
#ifndef REPORTSEND_NO_CRASHREPORT
#include "crashreport.h"
#endif

namespace reportsend {
namespace {

constexpr size_t kBodyCap = 256 * 1024;  // site cap (api/report.py MAX_BODY)
constexpr size_t kTextCap = 1000;
constexpr int kInputCap = 400;

void (*g_log)(const char*, ...) = nullptr;
#define RLOG(...)                                 \
    do {                                          \
        if (g_log) g_log("REPORT: " __VA_ARGS__); \
    } while (0)

wchar_t g_dir[MAX_PATH * 2];       // folder of the .asi, trailing backslash
wchar_t g_url[512];                // POST target
char g_urlA[512];
wchar_t g_bypass[128];
char g_id[40];
char g_ver[32];
bool g_steamWanted = true;
bool g_started = false;

std::string g_user, g_pc, g_profile;  // scrub names (UTF-8)

SRWLOCK g_lock = SRWLOCK_INIT;     // status + input ring
std::atomic<int> g_state{0};
char g_status[300] = "";

struct InputEv { ULONGLONG t; SYSTEMTIME st; char dev[4]; char what[12]; };
InputEv g_in[kInputCap];
int g_inHead = 0, g_inCount = 0;

void SetStatus(State s, const char* fmt, ...) {
    AcquireSRWLockExclusive(&g_lock);
    va_list a;
    va_start(a, fmt);
    vsnprintf(g_status, sizeof g_status, fmt, a);
    va_end(a);
    g_state = (int)s;
    ReleaseSRWLockExclusive(&g_lock);
}

std::string Utf8(const wchar_t* w) {
    if (!w || !*w) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    return s;
}

bool ReadAll(const wchar_t* path, std::string& out, size_t cap) {
    out.clear();
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    GetFileSizeEx(h, &sz);
    if (sz.QuadPart > (LONGLONG)cap) {  // keep the tail
        LARGE_INTEGER off;
        off.QuadPart = sz.QuadPart - (LONGLONG)cap;
        SetFilePointerEx(h, off, nullptr, FILE_BEGIN);
    }
    out.resize((size_t)std::min<LONGLONG>(sz.QuadPart, (LONGLONG)cap));
    DWORD got = 0;
    BOOL ok = out.empty() || ReadFile(h, &out[0], (DWORD)out.size(), &got, nullptr);
    CloseHandle(h);
    out.resize(got);
    return ok != FALSE;
}

bool WriteAll(const wchar_t* path, const std::string& data, bool append) {
    HANDLE h = CreateFileW(path, append ? FILE_APPEND_DATA : GENERIC_WRITE, FILE_SHARE_READ, nullptr, append ? OPEN_ALWAYS : CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD put = 0;
    BOOL ok = data.empty() || WriteFile(h, data.data(), (DWORD)data.size(), &put, nullptr);
    CloseHandle(h);
    return ok && put == data.size();
}

// Crash and typed reports share the same ini-backed identity, including a
// read-only ini's in-memory fallback. Never generate a second ID/file here.
void LoadInstallId(const wchar_t* ini) {
    snprintf(g_id, sizeof g_id, "%s", crashupload::EnsureInstallId(ini));
}

std::string Lower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

void ReplaceAllCI(std::string& s, const std::string& what, const char* with) {
    if (what.size() < 2) return;
    const std::string lw = Lower(what);
    std::string ls = Lower(s);
    size_t pos = 0;
    const size_t wl = strlen(with);
    while ((pos = ls.find(lw, pos)) != std::string::npos) {
        s.replace(pos, what.size(), with);
        ls.replace(pos, what.size(), with);
        pos += wl;
    }
}

// ---- tails of the mod logs ------------------------------------------------------------------------------------------------
void TailLines(const wchar_t* name, const char* mod, int maxLines, std::vector<std::string>& out) {
    std::string data;
    if (!ReadAll((std::wstring(g_dir) + name).c_str(), data, 64 * 1024)) return;
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < data.size()) {
        size_t e = data.find('\n', start);
        if (e == std::string::npos) e = data.size();
        std::string ln = data.substr(start, e - start);
        while (!ln.empty() && (ln.back() == '\r' || ln.back() == ' ')) ln.pop_back();
        if (!ln.empty()) lines.push_back(ln.size() > 300 ? ln.substr(0, 300) : ln);
        start = e + 1;
    }
    const size_t from = lines.size() > (size_t)maxLines ? lines.size() - (size_t)maxLines : 0;
    for (size_t i = from; i < lines.size(); ++i)
        out.push_back(std::string("{\"mod\":") + JsonString(mod) + ",\"event\":\"log\",\"fields\":{\"line\":" + JsonString(Scrub(lines[i])) + "}}");
}

// ---- HTTP -----------------------------------------------------------------------------------------------------------------
struct HInt {
    HINTERNET h;
    explicit HInt(HINTERNET v) : h(v) {}
    ~HInt() { if (h) WinHttpCloseHandle(h); }
};

int Post(const std::string& body, std::string& reply) {
    reply.clear();
    URL_COMPONENTS u{};
    u.dwStructSize = sizeof(u);
    wchar_t host[256] = {}, path[1024] = {};
    u.lpszHostName = host;
    u.dwHostNameLength = 256;
    u.lpszUrlPath = path;
    u.dwUrlPathLength = 1024;
    if (!g_url[0] || !WinHttpCrackUrl(g_url, 0, 0, &u)) return -2;
    HInt s(WinHttpOpen(L"Nioh3EngineFix/report", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!s.h) return -1;
    WinHttpSetTimeouts(s.h, 10000, 10000, 20000, 20000);
    HInt c(WinHttpConnect(s.h, host, u.nPort, 0));
    if (!c.h) return -1;
    HInt r(WinHttpOpenRequest(c.h, L"POST", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                              u.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0));
    if (!r.h) return -1;
    wchar_t hdr[512];
    swprintf(hdr, 512, L"Content-Type: application/json\r\nX-Nioh3-Install: %hs\r\n%s%s%s", g_id, g_bypass[0] ? L"x-vercel-protection-bypass: " : L"",
             g_bypass, g_bypass[0] ? L"\r\n" : L"");
    if (!WinHttpSendRequest(r.h, hdr, (DWORD)-1L, (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0) ||
        !WinHttpReceiveResponse(r.h, nullptr))
        return -1;
    DWORD status = 0, len = sizeof(status);
    if (!WinHttpQueryHeaders(r.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &len,
                             WINHTTP_NO_HEADER_INDEX))
        return -1;
    char sink[1024];
    DWORD got = 0;
    for (int i = 0; i < 8 && WinHttpReadData(r.h, sink, sizeof sink, &got) && got; ++i)
        if (reply.size() < 4096) reply.append(sink, got);
    return (int)status;
}

// tiny reader for {"key":"value"} / {"key":123} in the site's reply
std::string JsonField(const std::string& j, const char* key) {
    const std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k);
    if (p == std::string::npos) return {};
    p = j.find(':', p + k.size());
    if (p == std::string::npos) return {};
    ++p;
    while (p < j.size() && j[p] == ' ') ++p;
    std::string v;
    if (p < j.size() && j[p] == '"') {
        for (++p; p < j.size() && j[p] != '"' && v.size() < 160; ++p) {
            if (j[p] == '\\' && p + 1 < j.size()) ++p;
            v += j[p];
        }
    } else {
        while (p < j.size() && j[p] != ',' && j[p] != '}' && v.size() < 40) v += j[p++];
    }
    return v;
}

// ---- the send job ---------------------------------------------------------------------------------------------------------
struct Job {
    std::string text, via;
};

std::string Stamp(const SYSTEMTIME& st, bool file) {
    char b[40];
    if (file) snprintf(b, sizeof b, "%04u%02u%02u-%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    else snprintf(b, sizeof b, "%04u-%02u-%02uT%02u:%02u:%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return b;
}

DWORD WINAPI SendThread(LPVOID p) {
    Job* job = static_cast<Job*>(p);
    SYSTEMTIME st;
    GetLocalTime(&st);
    // 1. the existing report text (same writer as the old Report row and the crash report)
    std::string snapshot;
#ifndef REPORTSEND_NO_CRASHREPORT
    char snapPath[MAX_PATH * 2] = {};
    if (crashreport::Snapshot(snapPath, sizeof snapPath))
        ReadAll((std::wstring(g_dir) + L"Nioh3EngineFix-report-latest.txt").c_str(), snapshot, 200 * 1024);
#endif
    if (snapshot.empty()) snapshot = "(no snapshot: [Debug] CrashReport is off or the writer failed)";
    // 2. recent event log: mod log tails, newest last; then the menu input ring
    std::vector<std::string> events;
    TailLines(L"Nioh3ModEvents.jsonl", "Events", 60, events);
    TailLines(L"Nioh3Training.log", "Training", 40, events);
    TailLines(L"Nioh3BossRematch.log", "BossRematch", 40, events);
    TailLines(L"Nioh3CamKiProbe.log", "CamKiProbe", 20, events);
    TailLines(L"Nioh3EngineFix.log", "EngineFix", 80, events);
    std::vector<std::string> input;
    AcquireSRWLockShared(&g_lock);
    for (int i = 0; i < g_inCount; ++i) {
        const InputEv& e = g_in[(g_inHead - g_inCount + i + kInputCap) % kInputCap];
        input.push_back(std::string("{\"t\":\"") + Stamp(e.st, false) + "\",\"event\":\"menu\",\"fields\":{\"dev\":\"" + e.dev +
                        "\",\"what\":\"" + e.what + "\",\"edge\":\"down\"}}");
    }
    ReleaseSRWLockShared(&g_lock);
    // join with caps: BuildBody trims snapshot first, then the oldest events
    std::string evAll, inAll;
    for (size_t i = 0; i < events.size(); ++i) evAll += (i ? "," : "") + events[i];
    for (size_t i = 0; i < input.size(); ++i) inAll += (i ? "," : "") + input[i];
    // #64888: prefer the full input ring (pad edges, hotkeys, menu choices); last 400, oldest first
    int inN = (int)input.size();
    {
        static char ring[160 * 1024];
        const int bytes = inputlog::WriteTail(ring, (int)sizeof ring, 400);
        const int n = inputlog::Count() < 400 ? inputlog::Count() : 400;
        if (bytes > 0 && n > 0) { inAll = ring; inN = n; }
    }
    std::string full = BuildBody(g_id, g_ver, job->via, Scrub(job->text), Scrub(snapshot), evAll, (int)events.size(), inAll, inN, kBodyCap);
    int drop = 0;
    while (full.empty() && !events.empty()) {  // still over the cap with an empty snapshot: drop the oldest events
        const size_t n = std::max<size_t>(1, events.size() / 4);
        events.erase(events.begin(), events.begin() + (std::ptrdiff_t)n);
        drop += (int)n;
        evAll.clear();
        for (size_t i = 0; i < events.size(); ++i) evAll += (i ? "," : "") + events[i];
        full = BuildBody(g_id, g_ver, job->via, Scrub(job->text), "", evAll, (int)events.size(), inAll, inN, kBodyCap);
    }
    // 3. local copy (every report, sent or not)
    std::wstring dir = std::wstring(g_dir) + L"Nioh3Reports\\";
    CreateDirectoryW(dir.c_str(), nullptr);
    const std::string stamp = Stamp(st, true);
    std::wstring wname = L"report-" + std::wstring(stamp.begin(), stamp.end()) + L".json";
    const bool saved = WriteAll((dir + wname).c_str(), full, false);
    const std::string name = "Nioh3Reports\\report-" + stamp + ".json";
    // 4. send
    std::string reply;
    const int code = full.empty() ? -3 : Post(full, reply);
    std::string ref = JsonField(reply, "ref"), err = JsonField(reply, "error");
    if (code >= 200 && code < 300) {
        SetStatus(State::Sent, "Sent. Thank you!%s%s%s", ref.empty() ? "" : " Reference ", ref.c_str(), saved ? "" : " (local copy failed)");
    } else {
        const char* why = code == -1 ? "no connection to the report site" : code == -2 ? "bad [Report] ReportUrl" : code == -3 ? "report too large"
                          : code == 429 ? "too many reports, try again in a few minutes" : nullptr;
        char tmp[200];
        if (!why) { snprintf(tmp, sizeof tmp, "site answered %d%s%s", code, err.empty() ? "" : ": ", err.c_str()); why = tmp; }
        SetStatus(State::Failed, "Not sent: %s. %s %s", why, saved ? "Saved a copy:" : "Copy not saved", saved ? name.c_str() : "");
    }
    char line[512];
    snprintf(line, sizeof line, "%s %s via=%s bytes=%zu http=%d ref=%s err=%s dropped=%d\r\n", Stamp(st, false).c_str(), name.c_str(),
             job->via.c_str(), full.size(), code, ref.c_str(), err.c_str(), drop);
    WriteAll((dir + L"reports.log").c_str(), line, true);
    RLOG("%s", line);
    delete job;
    return 0;
}

// ---- Steam gamepad text input (flat API exports of the steam_api64.dll the game already loaded) --------------------------
using SteamUtilsFn = void* (*)();
using ShowFn = bool (*)(void*, int, int, const char*, uint32_t, const char*);
using LenFn = uint32_t (*)(void*);
using GetFn = bool (*)(void*, char*, uint32_t);
class SteamCb;
using RegFn = void (*)(SteamCb*, int);

struct DismissedMsg { bool submitted; uint32_t length; uint32_t app; };
std::atomic<int> g_steamResult{0};  // 0 waiting, 1 submitted, 2 dismissed
std::atomic<uint32_t> g_steamLen{0};

// Same declaration order as Steam's CCallbackBase, so MSVC lays the vtable out the way steam_api64.dll calls it.
class SteamCb {
public:
    virtual void Run(void* p) {
        const DismissedMsg* m = static_cast<const DismissedMsg*>(p);
        g_steamLen = m ? m->length : 0;
        g_steamResult = m && m->submitted ? 1 : 2;
    }
    virtual void Run(void* p, bool, uint64_t) { Run(p); }
    virtual int GetCallbackSizeBytes() { return (int)sizeof(DismissedMsg); }
    uint8_t flags = 0;
    int id = 714;  // k_iSteamUtilsCallbacks + 14 = GamepadTextInputDismissed_t
};
SteamCb g_steamCb;
bool g_steamLooked = false, g_steamRegistered = false;
void* g_utils = nullptr;
ShowFn g_show = nullptr;
LenFn g_len = nullptr;
GetFn g_get = nullptr;

void LookSteam() {
    if (g_steamLooked) return;
    g_steamLooked = true;
    HMODULE m = GetModuleHandleW(L"steam_api64.dll");
    if (!m) { RLOG("steam_api64.dll is not loaded: built-in key grid only"); return; }
    static const char* kUtils[] = {"SteamAPI_SteamUtils_v010", "SteamAPI_SteamUtils_v009", "SteamAPI_SteamUtils_v008", "SteamUtils"};
    const char* used = "";
    for (const char* n : kUtils)
        if (auto f = reinterpret_cast<SteamUtilsFn>(GetProcAddress(m, n))) {
            __try { g_utils = f(); } __except (EXCEPTION_EXECUTE_HANDLER) { g_utils = nullptr; }
            if (g_utils) { used = n; break; }
        }
    g_show = reinterpret_cast<ShowFn>(GetProcAddress(m, "SteamAPI_ISteamUtils_ShowGamepadTextInput"));
    g_len = reinterpret_cast<LenFn>(GetProcAddress(m, "SteamAPI_ISteamUtils_GetEnteredGamepadTextLength"));
    g_get = reinterpret_cast<GetFn>(GetProcAddress(m, "SteamAPI_ISteamUtils_GetEnteredGamepadTextInput"));
    auto reg = reinterpret_cast<RegFn>(GetProcAddress(m, "SteamAPI_RegisterCallback"));
    if (g_utils && g_show && g_get && reg) {
        __try { reg(&g_steamCb, 714); g_steamRegistered = true; } __except (EXCEPTION_EXECUTE_HANDLER) { g_steamRegistered = false; }
    }
    RLOG("steam: utils %s %p, show %p, get %p, callback %s", used[0] ? used : "(none)", g_utils, (void*)g_show, (void*)g_get,
         g_steamRegistered ? "registered" : "not registered");
}

}  // namespace

// ---- public -------------------------------------------------------------------------------------------------------------
void SetScrubNamesForTest(const char* user, const char* pc, const char* profile) {
    g_user = user ? user : "";
    g_pc = pc ? pc : "";
    g_profile = profile ? profile : "";
}

std::string Scrub(const std::string& in) {
    std::string s = in;
    // path parts first: <drive>:\Users\<name>\..., \home\<name>, /home/<name>, /Users/<name>
    static const char* kMarks[] = {"\\users\\", "/users/", "/home/", "\\home\\"};
    std::string ls = Lower(s);
    for (const char* mk : kMarks) {
        const size_t ml = strlen(mk);
        size_t pos = 0;
        while ((pos = ls.find(mk, pos)) != std::string::npos) {
            const size_t a = pos + ml;
            size_t e = a;
            while (e < s.size() && !strchr("\\/\"'<>|\r\n\t", s[e])) ++e;
            if (e > a && s.compare(a, e - a, "<user>") != 0) {
                s.replace(a, e - a, "<user>");
                ls.replace(a, e - a, "<user>");
            }
            pos = a + 1;
        }
    }
    if (!g_profile.empty()) ReplaceAllCI(s, g_profile, "<user>");
    if (g_user.size() >= 3) ReplaceAllCI(s, g_user, "<user>");
    if (g_pc.size() >= 3) ReplaceAllCI(s, g_pc, "<pc>");
    // 17-digit Steam ids (7656119...)
    for (size_t i = 0; i + 17 <= s.size(); ++i) {
        if (s.compare(i, 7, "7656119") != 0 || (i && isdigit((unsigned char)s[i - 1]))) continue;
        size_t e = i;
        while (e < s.size() && isdigit((unsigned char)s[e])) ++e;
        if (e - i == 17) s.replace(i, 17, "<steamid>");
    }
    return s;
}

std::string JsonString(const std::string& s) {
    std::string o = "\"";
    o.reserve(s.size() + 2);
    for (unsigned char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else if (c >= 0x80) o += '?';
        else o += (char)c;
    }
    return o + "\"";
}

std::string BuildBody(const std::string& id, const std::string& ver, const std::string& via, const std::string& text,
                      const std::string& snapshot, const std::string& eventsJson, int eventCount, const std::string& inputJson,
                      int inputCount, size_t cap) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    const std::string t = text.size() > kTextCap ? text.substr(0, kTextCap) : text;
    auto make = [&](const std::string& snap) {
        return std::string("{\"kind\":\"player\",\"id\":") + JsonString(id) + ",\"ver\":" + JsonString(ver) + ",\"via\":" + JsonString(via) +
               ",\"t\":" + JsonString(Stamp(st, false)) + ",\"player_text\":" + JsonString(t) + ",\"snapshot\":" + JsonString(snap) +
               ",\"event_count\":" + std::to_string(eventCount) + ",\"events\":[" + eventsJson + "],\"input_count\":" +
               std::to_string(inputCount) + ",\"input\":[" + inputJson + "]}";
    };
    std::string body = make(snapshot);
    if (!cap || body.size() <= cap) return body;
    const size_t over = body.size() - cap;  // the escaped snapshot shrinks about 1:1 with its raw size
    if (snapshot.size() > over + 64) {
        body = make(snapshot.substr(0, snapshot.size() - over - 64) + "\n...(cut to fit 256 KB)");
        if (body.size() <= cap) return body;
    }
    body = make("(snapshot dropped to fit 256 KB)");
    return body.size() <= cap ? body : std::string();
}

void Start(const StartInfo& info) {
    g_log = info.log;
    snprintf(g_ver, sizeof g_ver, "%s", info.modVersion ? info.modVersion : "");
    DWORD n = GetModuleFileNameW(static_cast<HMODULE>(info.selfModule), g_dir, (DWORD)(sizeof(g_dir) / sizeof(g_dir[0])));
    while (n && g_dir[n - 1] != L'\\' && g_dir[n - 1] != L'/') --n;
    g_dir[n] = 0;
    wchar_t url[512] = {}, base[300] = {};
    if (info.iniPath) {
        GetPrivateProfileStringW(L"Report", L"ReportUrl", L"", url, 512, info.iniPath);
        GetPrivateProfileStringW(L"Report", L"Url", L"https://nioh3-mod-feedback.vercel.app", base, 300, info.iniPath);
        GetPrivateProfileStringW(L"Report", L"ReportBypass", L"", g_bypass, 128, info.iniPath);
        if (!g_bypass[0]) GetPrivateProfileStringW(L"Report", L"CrashBypass", L"", g_bypass, 128, info.iniPath);
        g_steamWanted = GetPrivateProfileIntW(L"Report", L"SteamKeyboard", 1, info.iniPath) != 0;
    } else {
        wcscpy_s(base, L"https://nioh3-mod-feedback.vercel.app");
    }
    for (wchar_t* p = g_bypass; *p; ++p)
        if (*p < 32 || *p == L':') { g_bypass[0] = 0; break; }  // header injection guard
    if (url[0]) wcscpy_s(g_url, url);
    else {
        size_t bl = wcslen(base);
        while (bl && base[bl - 1] == L'/') base[--bl] = 0;
        swprintf(g_url, 512, L"%s/api/report", base);
    }
    snprintf(g_urlA, sizeof g_urlA, "%s", Utf8(g_url).c_str());
    wchar_t v[MAX_PATH * 2];
    if (GetEnvironmentVariableW(L"USERNAME", v, MAX_PATH * 2)) g_user = Utf8(v);
    if (GetEnvironmentVariableW(L"COMPUTERNAME", v, MAX_PATH * 2)) g_pc = Utf8(v);
    if (GetEnvironmentVariableW(L"USERPROFILE", v, MAX_PATH * 2)) g_profile = Utf8(v);
    LoadInstallId(info.iniPath);
    g_started = true;
    RLOG("ready: endpoint %s, install id %.8s..., bypass %s, steam keyboard %d", g_urlA, g_id, g_bypass[0] ? "set" : "none",
         g_steamWanted ? 1 : 0);
}

bool Send(const char* playerText, const char* via, char* msg, size_t cap) {
    if (msg && cap) msg[0] = 0;
    std::string t = playerText ? playerText : "";
    while (!t.empty() && (t.back() == ' ' || t.back() == '\n')) t.pop_back();
    if (t.empty()) { if (msg) snprintf(msg, cap, "Type what went wrong first."); return false; }
    if (!g_started) { if (msg) snprintf(msg, cap, "Report sender is not running."); return false; }
    int idle = (int)State::Sending;
    for (;;) {  // one send at a time
        int cur = g_state.load();
        if (cur == (int)State::Sending) { if (msg) snprintf(msg, cap, "Still sending the last report."); return false; }
        if (g_state.compare_exchange_weak(cur, idle)) break;
    }
    SetStatus(State::Sending, "Sending report...");
    Job* job = new Job{t, via ? via : "keyboard"};
    HANDLE h = CreateThread(nullptr, 256 * 1024, &SendThread, job, 0, nullptr);
    if (!h) {
        delete job;
        SetStatus(State::Failed, "Not sent: could not start the send thread.");
        return false;
    }
    CloseHandle(h);
    return true;
}

State Status(char* msg, size_t cap) {
    AcquireSRWLockShared(&g_lock);
    if (msg && cap) snprintf(msg, cap, "%s", g_status);
    const State s = (State)g_state.load();
    ReleaseSRWLockShared(&g_lock);
    return s;
}

void ClearStatus() {
    if (g_state.load() == (int)State::Sending) return;
    SetStatus(State::Idle, "");
}

const char* Endpoint() { return g_urlA; }

void NoteInput(const char* dev, const char* what) {
    AcquireSRWLockExclusive(&g_lock);
    InputEv& e = g_in[g_inHead];
    e.t = GetTickCount64();
    GetLocalTime(&e.st);
    snprintf(e.dev, sizeof e.dev, "%s", dev ? dev : "");
    snprintf(e.what, sizeof e.what, "%s", what ? what : "");
    g_inHead = (g_inHead + 1) % kInputCap;
    if (g_inCount < kInputCap) ++g_inCount;
    ReleaseSRWLockExclusive(&g_lock);
}

bool SteamEnabled() {
    if (!g_steamWanted) return false;
    LookSteam();
    return g_utils && g_show && g_get;
}

bool SteamShow(const char* existing, unsigned maxChars) {
    if (!SteamEnabled()) return false;
    g_steamResult = 0;
    bool ok = false;
    __try {
        ok = g_show(g_utils, 0 /*normal*/, 1 /*multiple lines*/, "What just went wrong?", maxChars, existing ? existing : "");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
        g_steamWanted = false;
    }
    RLOG("steam ShowGamepadTextInput -> %d (callback %s)", ok ? 1 : 0, g_steamRegistered ? "registered" : "missing");
    return ok;
}

int SteamPoll(char* out, size_t cap) {
    const int r = g_steamResult.load();
    if (r == 0) return 0;
    g_steamResult = 0;
    if (r == 1 && out && cap) {
        out[0] = 0;
        bool ok = false;
        __try { ok = g_get(g_utils, out, (uint32_t)cap); } __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; }
        if (!ok) out[0] = 0;
        out[cap - 1] = 0;
        RLOG("steam keyboard submitted %u chars", (unsigned)strlen(out));
        return 1;
    }
    RLOG("steam keyboard dismissed");
    return 2;
}

}  // namespace reportsend
