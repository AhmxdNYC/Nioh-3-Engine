// savelib.cpp - see savelib.h. Native port of the Python save helpers (tools/saves/*.py, re/scripts_re/savecrypt.py).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "savelib.h"
#include "savefiles.h"
#pragma comment(lib, "bcrypt.lib")

namespace savelib {
namespace {

struct Refuse : std::runtime_error { using std::runtime_error::runtime_error; };
[[noreturn]] void Fail(const std::string& why) { throw Refuse(why); }

char g_last[1024] = "";
std::atomic<bool> g_cancel{false};
std::atomic<unsigned> g_settleMs{1000};
const char* kMarker = "NIOH3_COPY_SANDBOX_V1";

void SetLast(const std::string& s) { snprintf(g_last, sizeof g_last, "%s", s.c_str()); }

// ---------------------------------------------------------------- strings / paths
std::string U8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
std::wstring Wd(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}
std::wstring Full(const std::wstring& p) {
    wchar_t b[32768];
    DWORD n = GetFullPathNameW(p.c_str(), 32768, b, nullptr);
    std::wstring r = n && n < 32768 ? std::wstring(b, n) : p;
    for (auto& c : r) if (c == L'/') c = L'\\';
    while (r.size() > 3 && r.back() == L'\\') r.pop_back();
    return r;
}
std::wstring Lower(std::wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
bool IsDriveRoot(const std::wstring& p) {
    std::wstring s = p;
    while (!s.empty() && s.back() == L'\\') s.pop_back();
    return s.empty() || s.find(L'\\') == std::wstring::npos || (s.size() >= 2 && s.rfind(L'\\') == 1 && s[0] == L'\\');
}
std::wstring Parent(const std::wstring& p) { auto i = p.find_last_of(L'\\'); return i == std::wstring::npos ? p : p.substr(0, i); }
std::wstring Leaf(const std::wstring& p) { auto i = p.find_last_of(L'\\'); return i == std::wstring::npos ? p : p.substr(i + 1); }
bool Inside(const std::wstring& child, const std::wstring& parent) {  // child strictly below parent (case-insensitive)
    const std::wstring c = Lower(child), p = Lower(parent);
    return c.size() > p.size() && c.compare(0, p.size(), p) == 0 && c[p.size()] == L'\\';
}
bool Same(const std::wstring& a, const std::wstring& b) { return Lower(a) == Lower(b); }
DWORD Attr(const std::wstring& p) { return GetFileAttributesW(p.c_str()); }
bool Exists(const std::wstring& p) { return Attr(p) != INVALID_FILE_ATTRIBUTES; }
bool IsDir(const std::wstring& p) { DWORD a = Attr(p); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY); }
bool IsFile(const std::wstring& p) { DWORD a = Attr(p); return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY); }
// saveslot.plain(): the path itself must not be a link / reparse point. (Ancestors are not walked: Proton prefixes may keep
// the user's folders behind host symlinks; every tree we read or write is still walked link-free below.)
std::wstring Plain(const std::wstring& p) {
    std::wstring f = Full(p);
    DWORD a = Attr(f);
    if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_REPARSE_POINT)) Fail("Links/reparse points forbidden: " + U8(f));
    return f;
}
std::wstring SafeRoot(const std::wstring& p) {
    std::wstring f = Plain(p), n = Lower(f);
    for (auto& c : n) if (c == L'\\') c = L'/';
    const std::wstring k = L"/koeitecmo";
    if (n.find(k + L"/") != std::wstring::npos || (n.size() >= k.size() && n.compare(n.size() - k.size(), k.size(), k) == 0) ||
        n.find(L"/userdata/") != std::wstring::npos)
        Fail("Installed save/cloud paths forbidden; use a new disposable sandbox");
    return f;
}
std::string Hex(const unsigned char* d, size_t n) {
    static const char* x = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; ++i) { s += x[d[i] >> 4]; s += x[d[i] & 15]; }
    return s;
}
std::wstring RandHex(size_t bytes = 16) {
    unsigned char b[32] = {};
    if (bytes > sizeof b) bytes = sizeof b;
    if (BCryptGenRandom(nullptr, b, (ULONG)bytes, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
        LARGE_INTEGER q; QueryPerformanceCounter(&q);
        for (size_t i = 0; i < bytes; ++i) b[i] = (unsigned char)((q.QuadPart >> ((i % 8) * 8)) ^ (GetCurrentThreadId() * 131 + i * 17));
    }
    return Wd(Hex(b, bytes));
}
void MkdirP(const std::wstring& p) {
    if (IsDir(p)) return;
    std::wstring par = Parent(p);
    if (par != p && !par.empty() && !IsDriveRoot(p)) MkdirP(par);
    if (!CreateDirectoryW(p.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) Fail("Cannot create directory " + U8(p));
}
void MkdirNew(const std::wstring& p) {  // exclusive: never reuse an existing directory
    if (!CreateDirectoryW(p.c_str(), nullptr)) Fail("Directory exists or cannot be created: " + U8(p));
}

// ---------------------------------------------------------------- files / hashing
struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    ~Handle() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
};
std::string ReadAll(const std::wstring& p, size_t cap = 64u << 20) {
    Handle f;
    f.h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f.h == INVALID_HANDLE_VALUE) Fail("Cannot read " + U8(p));
    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(f.h, &sz) || (uint64_t)sz.QuadPart > cap) Fail("Unexpected file size: " + U8(p));
    std::string d((size_t)sz.QuadPart, 0);
    size_t got = 0;
    while (got < d.size()) {
        DWORD n = 0, want = (DWORD)std::min<size_t>(d.size() - got, 1u << 20);
        if (!ReadFile(f.h, d.data() + got, want, &n, nullptr) || !n) Fail("Read failed: " + U8(p));
        got += n;
    }
    return d;
}
std::string ReadHead(const std::wstring& p, size_t n) {
    Handle f;
    f.h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f.h == INVALID_HANDLE_VALUE) return {};
    std::string d(n, 0);
    DWORD got = 0;
    if (!ReadFile(f.h, d.data(), (DWORD)n, &got, nullptr)) return {};
    d.resize(got);
    return d;
}
void WriteNew(const std::wstring& p, const std::string& data) {  // exclusive create + flush (open('xb') + fsync)
    Handle f;
    f.h = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f.h == INVALID_HANDLE_VALUE) Fail("Cannot create " + U8(p));
    size_t put = 0;
    while (put < data.size()) {
        DWORD n = 0, want = (DWORD)std::min<size_t>(data.size() - put, 1u << 20);
        if (!WriteFile(f.h, data.data() + put, want, &n, nullptr) || n != want) Fail("Write failed: " + U8(p));
        put += n;
    }
    if (!FlushFileBuffers(f.h)) Fail("Flush failed: " + U8(p));
}
void WriteAtomic(const std::wstring& p, const std::string& data) {  // saveslot.write(): temp file + os.replace
    const std::wstring tmp = p + L"." + RandHex() + L".tmp";
    WriteNew(tmp, data);
    if (!MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        Fail("Cannot replace " + U8(p));
    }
}
struct Sha {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h = nullptr;
    Sha() {
        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 || BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) < 0)
            Fail("SHA-256 unavailable");
    }
    ~Sha() { if (h) BCryptDestroyHash(h); if (alg) BCryptCloseAlgorithmProvider(alg, 0); }
    void Add(const void* d, size_t n) { if (n && BCryptHashData(h, (PUCHAR)d, (ULONG)n, 0) < 0) Fail("SHA-256 failed"); }
    std::string Done() { unsigned char dig[32]; if (BCryptFinishHash(h, dig, 32, 0) < 0) Fail("SHA-256 failed"); return Hex(dig, 32); }
};
std::string ShaOf(const std::string& d) { Sha s; s.Add(d.data(), d.size()); return s.Done(); }

// ---------------------------------------------------------------- minimal JSON (manifest / sandbox / receipts)
struct J {
    enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false;
    long long n = 0;
    std::string s;
    std::vector<J> a;
    std::vector<std::pair<std::string, J>> o;
    const J* Get(const char* k) const { for (auto& kv : o) if (kv.first == k) return &kv.second; return nullptr; }
};
struct Parser {
    const char* p; const char* e;
    void Ws() { while (p < e && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p; }
    [[noreturn]] void Bad() { Fail("Malformed JSON"); }
    static void Put(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 63)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 63)); out += (char)(0x80 | (cp & 63)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 63)); out += (char)(0x80 | ((cp >> 6) & 63)); out += (char)(0x80 | (cp & 63)); }
    }
    unsigned Hex4() {
        if (e - p < 4) Bad();
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = *p++; v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0'; else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10; else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10; else Bad();
        }
        return v;
    }
    std::string Str() {
        if (p >= e || *p != '"') Bad();
        ++p;
        std::string out;
        while (p < e && *p != '"') {
            char c = *p++;
            if (c != '\\') { out += c; continue; }
            if (p >= e) Bad();
            char x = *p++;
            switch (x) {
                case '"': out += '"'; break; case '\\': out += '\\'; break; case '/': out += '/'; break;
                case 'b': out += '\b'; break; case 'f': out += '\f'; break; case 'n': out += '\n'; break;
                case 'r': out += '\r'; break; case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = Hex4();
                    if (cp >= 0xD800 && cp < 0xDC00 && e - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                        p += 2; unsigned lo = Hex4();
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    Put(out, cp); break;
                }
                default: Bad();
            }
        }
        if (p >= e) Bad();
        ++p;
        return out;
    }
    J Val(int depth = 0) {
        if (depth > 32) Bad();
        Ws();
        if (p >= e) Bad();
        J v;
        if (*p == '{') {
            v.t = J::Obj; ++p; Ws();
            if (p < e && *p == '}') { ++p; return v; }
            for (;;) {
                Ws(); std::string k = Str(); Ws();
                if (p >= e || *p != ':') Bad();
                ++p;
                v.o.emplace_back(std::move(k), Val(depth + 1));
                Ws();
                if (p < e && *p == ',') { ++p; continue; }
                if (p < e && *p == '}') { ++p; return v; }
                Bad();
            }
        }
        if (*p == '[') {
            v.t = J::Arr; ++p; Ws();
            if (p < e && *p == ']') { ++p; return v; }
            for (;;) {
                v.a.push_back(Val(depth + 1)); Ws();
                if (p < e && *p == ',') { ++p; continue; }
                if (p < e && *p == ']') { ++p; return v; }
                Bad();
            }
        }
        if (*p == '"') { v.t = J::Str; v.s = Str(); return v; }
        if (e - p >= 4 && !strncmp(p, "true", 4)) { p += 4; v.t = J::Bool; v.b = true; return v; }
        if (e - p >= 5 && !strncmp(p, "false", 5)) { p += 5; v.t = J::Bool; return v; }
        if (e - p >= 4 && !strncmp(p, "null", 4)) { p += 4; return v; }
        if (*p == '-' || (*p >= '0' && *p <= '9')) {
            v.t = J::Num; bool neg = *p == '-'; if (neg) ++p;
            if (p >= e || *p < '0' || *p > '9') Bad();
            while (p < e && *p >= '0' && *p <= '9') v.n = v.n * 10 + (*p++ - '0');
            while (p < e && (*p == '.' || *p == 'e' || *p == 'E' || *p == '+' || *p == '-' || (*p >= '0' && *p <= '9'))) ++p;
            if (neg) v.n = -v.n;
            return v;
        }
        Bad();
    }
};
J ParseJson(const std::string& text) {
    size_t off = text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF ? 3 : 0;
    Parser ps{text.data() + off, text.data() + text.size()};
    J v = ps.Val();
    ps.Ws();
    if (ps.p != ps.e) Fail("Malformed JSON (trailing data)");
    return v;
}
J ReadJson(const std::wstring& p) { Plain(p); return ParseJson(ReadAll(p, 256u << 20)); }
std::string Esc(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if (c == '"') o += "\\\""; else if (c == '\\') o += "\\\\";
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += (char)c;
    }
    return o + "\"";
}
struct Obj {  // ordered JSON object builder (raw values)
    std::vector<std::pair<std::string, std::string>> kv;
    Obj& S(const std::string& k, const std::string& v) { return Raw(k, Esc(v)); }
    Obj& N(const std::string& k, unsigned long long v) { return Raw(k, std::to_string(v)); }
    Obj& B(const std::string& k, bool v) { return Raw(k, v ? "true" : "false"); }
    Obj& Raw(const std::string& k, const std::string& v) {
        for (auto& x : kv) if (x.first == k) { x.second = v; return *this; }
        kv.emplace_back(k, v); return *this;
    }
    std::string Str() const {
        std::string o = "{";
        for (size_t i = 0; i < kv.size(); ++i) o += (i ? ", " : "") + Esc(kv[i].first) + ": " + kv[i].second;
        return o + "}";
    }
};

// ---------------------------------------------------------------- inventory (saveslot.inventory / copy_verified / identity)
struct Entry { unsigned long long size = 0; std::string sha; bool operator==(const Entry& o) const { return size == o.size && sha == o.sha; } };
struct Tree {
    std::map<std::string, Entry> files;
    std::vector<std::string> dirs;  // sorted
    bool operator==(const Tree& o) const { return files == o.files && dirs == o.dirs; }
    bool operator!=(const Tree& o) const { return !(*this == o); }
    std::string Json() const {
        std::string f = "{";
        bool first = true;
        for (auto& kv : files) { f += (first ? "" : ", ") + Esc(kv.first) + ": {\"size\": " + std::to_string(kv.second.size) + ", \"sha256\": " + Esc(kv.second.sha) + "}"; first = false; }
        f += "}";
        std::string d = "[";
        for (size_t i = 0; i < dirs.size(); ++i) d += (i ? ", " : "") + Esc(dirs[i]);
        return "{\"files\": " + f + ", \"dirs\": " + d + "]}";
    }
};
Tree TreeFrom(const J* j) {
    Tree t;
    const J* f = j ? j->Get("files") : nullptr;
    const J* d = j ? j->Get("dirs") : nullptr;
    if (!f || f->t != J::Obj || !d || d->t != J::Arr) Fail("Manifest tree malformed");
    for (auto& kv : f->o) {
        const J* sz = kv.second.Get("size"); const J* sh = kv.second.Get("sha256");
        if (!sz || sz->t != J::Num || !sh || sh->t != J::Str) Fail("Manifest entry malformed");
        t.files[kv.first] = Entry{(unsigned long long)sz->n, sh->s};
    }
    for (auto& x : d->a) { if (x.t != J::Str) Fail("Manifest dir malformed"); t.dirs.push_back(x.s); }
    std::sort(t.dirs.begin(), t.dirs.end());
    return t;
}
std::string RelU8(const std::wstring& rel) { std::string s = U8(rel); for (auto& c : s) if (c == '\\') c = '/'; return s; }
std::wstring RelW(const std::string& rel) { std::wstring w = Wd(rel); for (auto& c : w) if (c == L'/') c = L'\\'; return w; }

void Walk(const std::wstring& root, const std::wstring& rel, Tree* t, int depth) {
    if (depth > 64) Fail("Directory tree too deep");
    const std::wstring dir = rel.empty() ? root : root + L"\\" + rel;
    WIN32_FIND_DATAW d{};
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &d);
    if (h == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) return;  // empty directory
        Fail("Cannot list " + U8(dir));
    }
    struct Closer { HANDLE h; ~Closer() { FindClose(h); } } closer{h};
    std::vector<std::pair<std::wstring, bool>> kids;
    do {
        if (!wcscmp(d.cFileName, L".") || !wcscmp(d.cFileName, L"..")) continue;
        const std::wstring childRel = rel.empty() ? std::wstring(d.cFileName) : rel + L"\\" + d.cFileName;
        if (d.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) Fail("Links/reparse points forbidden: " + U8(root + L"\\" + childRel));
        kids.emplace_back(childRel, (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
    } while (FindNextFileW(h, &d));
    if (GetLastError() != ERROR_NO_MORE_FILES) Fail("Directory read error: " + U8(dir));
    for (auto& k : kids) {
        if (k.second) {
            if (t) t->dirs.push_back(RelU8(k.first));
            Walk(root, k.first, t, depth + 1);
            continue;
        }
        if (!t) continue;
        const std::wstring path = root + L"\\" + k.first;
        Handle f;
        f.h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (f.h == INVALID_HANDLE_VALUE) Fail("Cannot open " + U8(path));
        BY_HANDLE_FILE_INFORMATION bi{};
        if (!GetFileInformationByHandle(f.h, &bi) || bi.nNumberOfLinks != 1) Fail("Only ordinary single-link files allowed");
        Sha s;
        static thread_local std::unique_ptr<char[]> buf(new char[1 << 20]);
        unsigned long long total = 0;
        for (;;) {
            DWORD n = 0;
            if (!ReadFile(f.h, buf.get(), 1 << 20, &n, nullptr)) Fail("Read failed: " + U8(path));
            if (!n) break;
            s.Add(buf.get(), n);
            total += n;
        }
        t->files[RelU8(k.first)] = Entry{total, s.Done()};
    }
}
Tree Inventory(const std::wstring& root) {
    Plain(root);
    if (!IsDir(root)) Fail("Missing directory: " + U8(root));
    Tree t;
    Walk(root, L"", &t, 0);
    std::sort(t.dirs.begin(), t.dirs.end());
    return t;
}
void ScanLinks(const std::wstring& root) { Plain(root); if (IsDir(root)) Walk(root, L"", nullptr, 0); }

void CopyFileNew(const std::wstring& src, const std::wstring& dst) {
    Handle a, b;
    a.h = CreateFileW(src.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (a.h == INVALID_HANDLE_VALUE) Fail("Cannot open " + U8(src));
    b.h = CreateFileW(dst.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (b.h == INVALID_HANDLE_VALUE) Fail("Cannot create " + U8(dst));
    static thread_local std::unique_ptr<char[]> buf(new char[1 << 20]);
    for (;;) {
        DWORD n = 0, w = 0;
        if (!ReadFile(a.h, buf.get(), 1 << 20, &n, nullptr)) Fail("Read failed: " + U8(src));
        if (!n) break;
        if (!WriteFile(b.h, buf.get(), n, &w, nullptr) || w != n) Fail("Write failed: " + U8(dst));
    }
    if (!FlushFileBuffers(b.h)) Fail("Flush failed: " + U8(dst));
}
void CopyTreeInto(const std::wstring& src, const std::wstring& dst, const Tree& t) {
    for (auto& d : t.dirs) MkdirP(dst + L"\\" + RelW(d));
    for (auto& f : t.files) {
        const std::wstring out = dst + L"\\" + RelW(f.first);
        MkdirP(Parent(out));
        CopyFileNew(src + L"\\" + RelW(f.first), out);
    }
}
Tree CopyVerified(const std::wstring& src, const std::wstring& dst) {
    Tree expected = Inventory(src);
    if (expected.files.empty()) Fail("Empty save copy");
    MkdirNew(dst);  // exclusive destination, never overwrite
    CopyTreeInto(src, dst, expected);
    if (Inventory(src) != expected || Inventory(dst) != expected) Fail("Copy changed during transfer");
    return expected;
}
bool Digits17(const std::wstring& s) { if (s.size() != 17) return false; for (wchar_t c : s) if (c < L'0' || c > L'9') return false; return true; }
std::vector<std::wstring> SubDirs(const std::wstring& dir) {
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW d{};
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &d);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (!wcscmp(d.cFileName, L".") || !wcscmp(d.cFileName, L"..")) continue;
        if (d.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { FindClose(h); Fail("Links/reparse points forbidden: " + U8(dir + L"\\" + d.cFileName)); }
        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) out.push_back(d.cFileName);
    } while (FindNextFileW(h, &d));
    FindClose(h);
    std::sort(out.begin(), out.end());
    return out;
}
std::string Identity(const std::wstring& root) {
    const std::wstring data = Plain(root + L"\\Savedata");
    std::vector<std::wstring> all = IsDir(data) ? SubDirs(data) : std::vector<std::wstring>{};
    std::vector<std::wstring> ids;
    for (auto& n : all) if (Digits17(n)) ids.push_back(n);
    if (ids.size() != 1) Fail("Require exactly one 17-digit account under Savedata");
    for (auto& n : all) if (n != ids[0]) Fail("Unexpected account directory");
    const std::wstring acct = data + L"\\" + ids[0];
    for (const wchar_t* rel : {L"SAVEDATA00\\SAVEDATA.BIN", L"SAVEDATA00\\BACKUP.BIN", L"SYSTEMSAVEDATA00\\SAVEDATA.BIN"}) {
        const std::wstring p = Plain(acct + L"\\" + rel);
        WIN32_FILE_ATTRIBUTE_DATA fa{};
        if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fa) || (fa.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (!fa.nFileSizeHigh && !fa.nFileSizeLow))
            Fail("Incomplete save copy: " + RelU8(rel));
    }
    return U8(ids[0]);
}
void NameOk(const std::string& n) { if (!savefiles::ValidSlotName(n.c_str())) Fail("Invalid slot/transaction name"); }

// ---------------------------------------------------------------- library (saveslot.Slots)
struct Manifest { std::string account; Tree tree; };
struct Lib {
    std::wstring root;
    explicit Lib(const std::wstring& ws) {
        root = SafeRoot(ws);
        J m = ReadJson(root + L"\\sandbox.json");
        const J* f = m.Get("format");
        if (m.t != J::Obj || m.o.size() != 1 || !f || f->t != J::Str || f->s != kMarker) Fail("Not a copy sandbox");
        ScanLinks(root);  // refuse redirected descendants before taking a lock or writing anything
    }
    void Idle() const { if (Exists(root + L"\\pending.json")) Fail("Unfinished transaction; run recover first"); }
    Manifest Verify(const std::string& name) const {
        NameOk(name);
        const std::wstring p = root + L"\\slots\\" + Wd(name);
        J m = ReadJson(p + L"\\manifest.json");
        const J* a = m.Get("account");
        if (!a || a->t != J::Str) Fail("Manifest malformed");
        Manifest out{a->s, TreeFrom(m.Get("tree"))};
        if (Inventory(p + L"\\data") != out.tree || Identity(p + L"\\data") != out.account) Fail("Slot checksum/account mismatch");
        return out;
    }
};
struct Lock {
    std::wstring path;
    explicit Lock(const Lib& l) : path(l.root + L"\\operation.lock") {
        Handle f;
        f.h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f.h == INVALID_HANDLE_VALUE) { path.clear(); Fail("Library is locked by another operation"); }
        char b[32]; int n = snprintf(b, sizeof b, "%lu", GetCurrentProcessId()); DWORD w = 0;
        WriteFile(f.h, b, (DWORD)n, &w, nullptr);
    }
    ~Lock() { if (!path.empty()) DeleteFileW(path.c_str()); }
};
void InitLib(const std::wstring& r) {
    const std::wstring root = SafeRoot(r);
    MkdirNew(root);  // must be NEW; cannot designate existing real saves
    for (const wchar_t* n : {L"current", L"slots", L"transactions"}) MkdirNew(root + L"\\" + n);
    WriteAtomic(root + L"\\sandbox.json", Obj().S("format", kMarker).Str());
}
std::string ManifestJson(const std::string& account, const Tree& t) { return "{\"account\": " + Esc(account) + ", \"tree\": " + t.Json() + "}"; }

// ---------------------------------------------------------------- save crypto (savecrypt.py)
const char* kSboxHex =
    "1c2f0353a30149daa6cde08a19a704d4061ada4908e2f6b29ee12249ce7b7e5e"
    "a0092a63af49ce707b3c2380fa1747f262626c5910cc299cb54658c74413e738"
    "d5af2783d4d5a09ee3763b8504d9d6986066d47853eaca0e8d565344e2efbda9"
    "9b100aa11393f0430b7c398a47dfd3c50e3431a6ae5ab8e7e63143c0aa0fe082"
    "124cd1df8ba5ac70c53d1b8e93174d794ece63c4330e1457f0d8195b9b6171f2"
    "2b337efd2c0bb62320b9d491199404a430138af1d005ec5eac4ad4d6a5177ff9"
    "e5f60029d7932d5e2cf181a3b7633957c233872da83f02cc08677460d8f0da67"
    "40648755bb7ff210c90314b58066cb91f61f795888bc95c2065fe90932ed9b85";
struct Key16 { uint8_t b[16]; };
Key16 K(const char* hex) { Key16 k{}; for (int i = 0; i < 16; ++i) { unsigned v = 0; sscanf_s(hex + i * 2, "%2x", &v); k.b[i] = (uint8_t)v; } return k; }
constexpr size_t kHdr = 0x158, kBody = 0x900058;
struct Tables {
    uint8_t sb[256];
    uint32_t t0[256], t1[256], t2[256], t3[256];
    static uint32_t Xt(uint32_t a) { a <<= 1; return a & 0x100 ? (a ^ 0x11b) & 0xff : a; }
    Tables() {
        for (int i = 0; i < 256; ++i) { unsigned v = 0; sscanf_s(kSboxHex + i * 2, "%2x", &v); sb[i] = (uint8_t)v; }
        for (int x = 0; x < 256; ++x) {
            uint32_t s = sb[x], s2 = Xt(s), s3 = s2 ^ s;
            t0[x] = (s2 << 24) | (s << 16) | (s << 8) | s3;
            t1[x] = (s3 << 24) | (s2 << 16) | (s << 8) | s;
            t2[x] = (s << 24) | (s3 << 16) | (s2 << 8) | s;
            t3[x] = (s << 24) | (s << 16) | (s3 << 8) | s2;
        }
    }
};
const Tables& T() { static Tables t; return t; }
uint32_t Be(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
void PutBe(uint8_t* p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
// CTR with 32-bit BE counter at iv[12:16]; data ^= keystream (enc == dec).
void Ctr(const uint8_t key[16], const uint8_t iv[16], uint8_t* data, size_t n) {
    const Tables& tb = T();
    uint32_t k[44];
    for (int i = 0; i < 4; ++i) k[i] = Be(key + 4 * i);
    uint32_t rc = 1;
    for (int i = 4; i < 44; ++i) {
        uint32_t t = k[i - 1];
        if (i % 4 == 0) {
            t = (t << 8) | (t >> 24);
            t = ((uint32_t)tb.sb[t >> 24] << 24) | ((uint32_t)tb.sb[(t >> 16) & 255] << 16) | ((uint32_t)tb.sb[(t >> 8) & 255] << 8) | tb.sb[t & 255];
            t ^= rc << 24;
            rc = Tables::Xt(rc);
        }
        k[i] = k[i - 4] ^ t;
    }
    const uint32_t s0i = Be(iv), s1i = Be(iv + 4), s2i = Be(iv + 8);
    uint32_t c = Be(iv + 12);
    const uint8_t* sb = tb.sb;
    for (size_t off = 0; off < n; off += 16) {
        uint32_t s0 = s0i ^ k[0], s1 = s1i ^ k[1], s2 = s2i ^ k[2], s3 = c ^ k[3];
        for (int r = 1; r < 10; ++r) {
            const int b = 4 * r;
            uint32_t a0 = tb.t0[s0 >> 24] ^ tb.t1[(s1 >> 16) & 255] ^ tb.t2[(s2 >> 8) & 255] ^ tb.t3[s3 & 255] ^ k[b];
            uint32_t a1 = tb.t0[s1 >> 24] ^ tb.t1[(s2 >> 16) & 255] ^ tb.t2[(s3 >> 8) & 255] ^ tb.t3[s0 & 255] ^ k[b + 1];
            uint32_t a2 = tb.t0[s2 >> 24] ^ tb.t1[(s3 >> 16) & 255] ^ tb.t2[(s0 >> 8) & 255] ^ tb.t3[s1 & 255] ^ k[b + 2];
            uint32_t a3 = tb.t0[s3 >> 24] ^ tb.t1[(s0 >> 16) & 255] ^ tb.t2[(s1 >> 8) & 255] ^ tb.t3[s2 & 255] ^ k[b + 3];
            s0 = a0; s1 = a1; s2 = a2; s3 = a3;
        }
        uint8_t ks[16];
        PutBe(ks, (((uint32_t)sb[s0 >> 24] << 24) | ((uint32_t)sb[(s1 >> 16) & 255] << 16) | ((uint32_t)sb[(s2 >> 8) & 255] << 8) | sb[s3 & 255]) ^ k[40]);
        PutBe(ks + 4, (((uint32_t)sb[s1 >> 24] << 24) | ((uint32_t)sb[(s2 >> 16) & 255] << 16) | ((uint32_t)sb[(s3 >> 8) & 255] << 8) | sb[s0 & 255]) ^ k[41]);
        PutBe(ks + 8, (((uint32_t)sb[s2 >> 24] << 24) | ((uint32_t)sb[(s3 >> 16) & 255] << 16) | ((uint32_t)sb[(s0 >> 8) & 255] << 8) | sb[s1 & 255]) ^ k[42]);
        PutBe(ks + 12, (((uint32_t)sb[s3 >> 24] << 24) | ((uint32_t)sb[(s0 >> 16) & 255] << 16) | ((uint32_t)sb[(s1 >> 8) & 255] << 8) | sb[s2 & 255]) ^ k[43]);
        const size_t m = std::min<size_t>(16, n - off);
        for (size_t i = 0; i < m; ++i) data[off + i] ^= ks[i];
        ++c;
    }
}
const std::string& HdrStream() {
    static const std::string s = [] {
        std::string a(kHdr, 0), b(kHdr, 0);
        const Key16 k1 = K("35311fcdf8524ea279cc51924b5848fe"), i1 = K("1bdfdd5727cbce873aeac29ee05b2925");
        const Key16 k2 = K("0e8c95cd6329f6c253ecfa34d5b47f5e"), i2 = K("fd4c40a28ece198b7701bc0c14b757bd");
        Ctr(k1.b, i1.b, (uint8_t*)a.data(), kHdr);
        Ctr(k2.b, i2.b, (uint8_t*)b.data(), kHdr);
        for (size_t i = 0; i < kHdr; ++i) a[i] ^= b[i];
        return a;
    }();
    return s;
}
std::string HeaderPlain(const std::string& blob) {
    std::string h = blob.substr(0, std::min(blob.size(), kHdr));
    const std::string& s = HdrStream();
    for (size_t i = 0; i < h.size(); ++i) h[i] ^= s[i];
    return h;
}
template <class V> V Rd(const std::string& d, size_t off) { V v{}; if (off + sizeof v <= d.size()) memcpy(&v, d.data() + off, sizeof v); return v; }
std::string Decrypt(const std::string& blob) {
    std::string hdr = HeaderPlain(blob);
    if (hdr.size() < kHdr || (hdr.compare(0, 6, "RNNUSR") != 0 && hdr.compare(0, 6, "RNNSYS") != 0)) Fail("Bad magic after header decode");
    const Key16 ke = K("545b5ebdfd7843557c7d7c7274c74491"), ivd = K("57077871dfece8e48762c77e69f9045d");
    uint8_t c8[16], b8[16], a8[16], k98[16];
    memcpy(c8, hdr.data() + 0x49, 16); Ctr(ke.b, ivd.b, c8, 16);
    memcpy(b8, hdr.data() + 0x59, 16); Ctr(ke.b, ivd.b, b8, 16);
    memcpy(a8, hdr.data() + 0x69, 16); Ctr(c8, b8, a8, 16);
    memcpy(k98, hdr.data() + 0x79, 16); Ctr(c8, b8, k98, 16);
    std::string out = hdr + blob.substr(kHdr);
    uint8_t* body = (uint8_t*)out.data() + kHdr;
    Ctr(a8, k98, body, out.size() - kHdr);
    Ctr(c8, b8, body, out.size() - kHdr);
    return out;
}
uint32_t Checksum(const std::string& p) {
    const uint32_t seed = Rd<uint32_t>(p, 0x900190);
    uint64_t acc = 0;
    for (size_t blk = 0; blk < 9216; ++blk) {
        const size_t base = 0x190 + blk * 1024;
        uint64_t a = 0, b = 0;
        for (int k = 0; k < 32; ++k) {
            b += Rd<uint64_t>(p, base + (4 * k) * 8) + Rd<uint64_t>(p, base + (4 * k + 2) * 8);
            a += Rd<uint64_t>(p, base + (4 * k + 1) * 8) + Rd<uint64_t>(p, base + (4 * k + 3) * 8);
        }
        acc = (b + acc + a) ^ seed;
    }
    return (uint32_t)((acc / 0xFFFFFFFFull) + acc);
}
uint64_t NewestStamp(const std::wstring& accountDir) {
    uint64_t best = 0;
    for (int i = 0; i < 50; ++i) {
        wchar_t n[32]; swprintf(n, 32, L"\\SAVEDATA%02d\\SAVEDATA.BIN", i);
        const std::wstring f = accountDir + n;
        if (!IsFile(f)) continue;
        std::string h = HeaderPlain(ReadHead(f, kHdr));
        if (h.size() >= kHdr && h.compare(0, 6, "RNNUSR") == 0) best = std::max(best, Rd<uint64_t>(h, 0x20));
    }
    return best;
}

// ---------------------------------------------------------------- gate / cancel (live_title guard)
struct Guard {
    GateFn gate; std::wstring cancel, root;
    void Check() const {
        if (g_cancel.load() || (!cancel.empty() && Exists(cancel))) Fail("Cancelled");
        Plain(root);
        if (gate && !gate()) Fail("Title/save state changed");
    }
    void Settle() const {
        Check();
        const ULONGLONG t0 = GetTickCount64();
        while (GetTickCount64() - t0 < g_settleMs.load()) {
            Sleep(100);
            if (gate && !gate()) Fail("Title changed during 1-second admission");
        }
        Check();
    }
};
bool Disjoint(const std::wstring& a, const std::wstring& b) { return !Same(a, b) && !Inside(a, b) && !Inside(b, a); }

// ---------------------------------------------------------------- operations
struct ProfilePick { std::wstring name; std::string blob; uint64_t savedAt = 0; };

bool Bootstrap(const std::wstring& sandbox, const std::wstring& saveRoot) {
    const std::wstring root = SafeRoot(sandbox), source = Plain(saveRoot);
    if (IsDriveRoot(root) || !Disjoint(root, source)) Fail("Library and source must be separate non-root directories");
    if (Exists(root)) {  // existing_library()
        Lib lib(root);
        lib.Idle();
        for (const wchar_t* n : {L"current", L"slots", L"transactions"}) if (!IsDir(root + L"\\" + n)) Fail("Incomplete library");
        std::vector<std::string> names;
        for (auto& s : SubDirs(root + L"\\slots")) {
            if (!s.empty() && s[0] == L'.') Fail("Unfinished library snapshot");
            lib.Verify(U8(s));
            names.push_back(U8(s));
        }
        SetLast("{\"state\": \"existing\", \"slots\": " + std::to_string(names.size()) + ", \"changed\": false}");
        return true;
    }
    const Tree before = Inventory(source);
    const std::wstring accounts = source + L"\\Savedata";
    std::vector<std::wstring> ids = IsDir(accounts) ? SubDirs(accounts) : std::vector<std::wstring>{};
    if (ids.size() != 1 || !Digits17(ids[0])) Fail("Require exactly one Steam account");
    const std::wstring accountW = ids[0];
    const std::string account = U8(accountW);
    std::vector<std::wstring> profiles;
    for (auto& p : SubDirs(accounts + L"\\" + accountW))
        if (p.size() == 10 && p.compare(0, 8, L"SAVEDATA") == 0 && iswdigit(p[8]) && iswdigit(p[9])) profiles.push_back(p);
    if (profiles.empty()) Fail("No user saves to copy");
    if (!IsFile(accounts + L"\\" + accountW + L"\\SYSTEMSAVEDATA00\\SAVEDATA.BIN")) Fail("Missing system save");
    MkdirP(Parent(root));
    Plain(Parent(root));
    const std::wstring stage = root + L".bootstrap-" + RandHex();
    InitLib(stage);
    const std::wstring backup = stage + L"\\backups";
    MkdirNew(backup);
    if (CopyVerified(source, backup + L"\\initial") != before) Fail("Source changed before backup");
    const std::wstring snap = backup + L"\\initial\\Savedata\\" + accountW;
    std::vector<ProfilePick> picks;
    std::string meta = "{";
    for (auto& p : profiles) {
        ProfilePick pk{p, ReadAll(snap + L"\\" + p + L"\\SAVEDATA.BIN"), 0};
        std::string err;
        if (!ValidateSave(pk.blob, account, &pk.savedAt, &err)) Fail(U8(p) + ": " + err);
        meta += (picks.empty() ? "" : ", ") + Esc(U8(p)) + ": " + Obj().N("savedAt", pk.savedAt).S("sha256", ShaOf(pk.blob)).Str();
        picks.push_back(std::move(pk));
    }
    meta += "}";
    std::vector<std::string> names;
    Lib stageLib(stage);
    for (auto& pk : picks) {
        const std::string name = picks.size() == 1 ? "My current save" : "My save " + U8(pk.name.substr(8));
        const std::wstring slot = stage + L"\\slots\\" + Wd(name), data = slot + L"\\data", dest = data + L"\\Savedata\\" + accountW;
        MkdirNew(slot);
        MkdirP(dest);
        MkdirNew(dest + L"\\SAVEDATA00");  // normalize the profile to slot 00 so each snapshot is unambiguous
        for (const wchar_t* fn : {L"SAVEDATA.BIN", L"BACKUP.BIN"}) WriteNew(dest + L"\\SAVEDATA00\\" + fn, pk.blob);
        CopyVerified(snap + L"\\SYSTEMSAVEDATA00", dest + L"\\SYSTEMSAVEDATA00");
        WriteAtomic(slot + L"\\manifest.json", ManifestJson(account, Inventory(data)));
        stageLib.Verify(name);
        names.push_back(name);
    }
    const auto newest = std::max_element(picks.begin(), picks.end(), [](const ProfilePick& a, const ProfilePick& b) { return a.savedAt < b.savedAt; });
    const std::string selected = picks.size() == 1 ? "My current save" : "My save " + U8(newest->name.substr(8));
    const std::wstring selData = stage + L"\\slots\\" + Wd(selected) + L"\\data";
    CopyTreeInto(selData, stage + L"\\current", Inventory(selData));  // current is a separate normalized copy
    Identity(stage + L"\\current");
    std::string slotsJson = "[";
    for (size_t i = 0; i < names.size(); ++i) slotsJson += (i ? ", " : "") + Esc(names[i]);
    slotsJson += "]";
    WriteAtomic(stage + L"\\bootstrap.json", Obj().S("source", U8(source)).S("backup", "backups/initial").Raw("tree", before.Json())
                                                 .Raw("profiles", meta).Raw("slots", slotsJson).Str());
    if (Inventory(source) != before || Inventory(backup + L"\\initial") != before) Fail("Source or backup changed; refusing publication");
    if (Exists(root)) Fail("Library appeared during preparation; nothing overwritten");
    if (!MoveFileW(stage.c_str(), root.c_str())) Fail("Library publication failed (" + std::to_string(GetLastError()) + ")");
    SetLast(Obj().S("state", "created").Raw("slots", slotsJson).S("backup", U8(root + L"\\backups\\initial"))
                .N("source_files", before.files.size()).B("changed", true).Str());
    return true;
}

bool LiveImport(const std::wstring& sandbox, const std::wstring& saveRoot, const std::string& slot, const std::wstring& token,
                bool makeNewest, GateFn gate) {
    Lib lib(sandbox);
    NameOk(slot);
    NameOk(U8(token));
    const std::wstring root = Plain(saveRoot);
    if (IsDriveRoot(root) || !Disjoint(root, lib.root)) Fail("Save root and library must be disjoint, non-root directories");
    const std::wstring request = lib.root + L"\\request-" + token;
    MkdirNew(request);  // nonce consumed even on failure; the receipt says imported, never loaded
    Obj receipt;
    receipt.S("state", "preparing").S("slot", slot).B("loaded", false);
    auto record = [&] { WriteAtomic(request + L"\\receipt.json", receipt.Str()); };
    record();
    Guard g{gate, lib.root + L"\\cancel-" + token, root};
    std::wstring txDir;
    try {
        Lock lock(lib);
        lib.Idle();
        g.Settle();
        const Manifest manifest = lib.Verify(slot);
        const std::string account = Identity(root);
        if (manifest.account != account) Fail("Cross-account import refused");
        const std::wstring accountW = Wd(account);
        const std::wstring source = lib.root + L"\\slots\\" + Wd(slot) + L"\\data\\Savedata\\" + accountW;
        std::vector<std::wstring> cands;
        for (auto& p : SubDirs(source))
            if (p.size() == 10 && p.compare(0, 8, L"SAVEDATA") == 0 && p[8] >= L'0' && p[8] <= L'4' && iswdigit(p[9])) cands.push_back(p);
        if (cands.empty()) Fail("No user save in the snapshot");
        // A full-root snapshot (menu backup) holds several profiles: import the newest valid one (the Python helper refused).
        ProfilePick pick;
        std::string lastErr = "No valid user save in the snapshot";
        for (auto& c : cands) {
            ProfilePick pk{c, ReadAll(source + L"\\" + c + L"\\SAVEDATA.BIN"), 0};
            std::string err;
            if (!ValidateSave(pk.blob, account, &pk.savedAt, &err)) { lastErr = U8(c) + ": " + err; continue; }
            if (pick.blob.empty() || pk.savedAt > pick.savedAt) pick = std::move(pk);
        }
        if (pick.blob.empty()) Fail(lastErr);
        Obj meta;
        meta.N("savedAt", pick.savedAt).S("sha256", ShaOf(pick.blob)).S("source_slot", U8(pick.name));
        g.Check();
        std::string blob = pick.blob;
        if (makeNewest) { blob = Restamp(blob, NewestStamp(root + L"\\Savedata\\" + accountW)); meta.B("restamped", true); }
        if (lib.Verify(slot).tree != manifest.tree) Fail("Source snapshot changed");
        const Tree before = Inventory(root);
        txDir = lib.root + L"\\transactions\\live-" + RandHex();
        MkdirNew(txDir);
        if (CopyVerified(root, txDir + L"\\backup") != before) Fail("Root changed during mandatory backup");
        WriteAtomic(txDir + L"\\backup.json", Obj().S("root", U8(root)).Raw("tree", before.Json()).B("verified", true).Str());
        g.Check();
        const std::wstring accountDir = root + L"\\Savedata\\" + accountW;
        std::wstring target;
        for (int i = 0; i < 50 && target.empty(); ++i) {
            wchar_t n[16]; swprintf(n, 16, L"\\SAVEDATA%02d", i);
            if (!Exists(accountDir + n)) target = accountDir + n;
        }
        if (target.empty()) Fail("All 50 game slots occupied; nothing overwritten");
        // Stage beside root, outside the game enumerator, on the same volume.
        const std::wstring stage = Parent(root) + L"\\" + Leaf(root) + L".live-incoming-" + RandHex();
        MkdirNew(stage);
        for (const wchar_t* fn : {L"SAVEDATA.BIN", L"BACKUP.BIN"}) WriteNew(stage + L"\\" + fn, blob);
        const Tree expected = Inventory(stage);
        const std::string want = ShaOf(blob);
        for (auto& kv : expected.files) if (kv.second.sha != want) Fail("Staged profile verification failed");
        receipt.S("state", "prepared").S("target", U8(target)).S("backup", U8(txDir + L"\\backup")).S("stage", U8(stage)).Raw("metadata", meta.Str());
        record();
        WriteAtomic(txDir + L"\\receipt.json", receipt.Str());
        if (Inventory(root) != before || Inventory(txDir + L"\\backup") != before) Fail("Save root or backup changed before import");
        if (Inventory(stage) != expected) Fail("Stage changed before import");
        g.Check();
        Plain(target);
        if (Exists(target)) Fail("Target slot appeared; nothing overwritten");
        if (!MoveFileW(stage.c_str(), target.c_str())) Fail("Import publication failed (" + std::to_string(GetLastError()) + ")");  // no replace
        wchar_t ins[200];
        swprintf(ins, 200, L"Choose Load Game and select %s; Continue selects newest timestamp.", Leaf(target).c_str());
        receipt.S("state", "imported").S("instruction", U8(ins));
        try { g.Check(); receipt.S("gate_after", "idle"); } catch (const std::exception& e) { receipt.S("gate_after", std::string("changed: ") + e.what()); }
        // Never remove a published slot after gate loss: the native loader may already be reading it.
        record();
        WriteAtomic(txDir + L"\\receipt.json", receipt.Str());
        SetLast(receipt.Str());
        return true;
    } catch (const std::exception& e) {
        receipt.S("state", "refused").S("error", e.what());
        try { record(); } catch (...) {}
        throw;
    }
}

bool Backup(const std::wstring& sandbox, const std::wstring& saveRoot, const std::string& name, const std::wstring& token, GateFn gate) {
    Lib lib(sandbox);
    NameOk(name);
    const std::wstring root = Plain(saveRoot);
    if (Leaf(root).empty() || IsDriveRoot(root)) Fail("Filesystem root forbidden");
    if (!Disjoint(root, lib.root)) Fail("Save root and workspace must be disjoint");
    Guard g{gate, token.empty() ? L"" : lib.root + L"\\cancel-" + token, root};
    Lock lock(lib);
    lib.Idle();
    g.Settle();
    const std::string account = Identity(root);
    const std::wstring target = lib.root + L"\\slots\\" + Wd(name);
    if (Exists(target)) Fail("Slot exists; never overwrite");
    const std::wstring stage = lib.root + L"\\slots\\.stage-" + RandHex();
    MkdirNew(stage);
    const Tree tree = CopyVerified(root, stage + L"\\data");
    g.Check();
    WriteAtomic(stage + L"\\manifest.json", ManifestJson(account, tree));
    if (!MoveFileW(stage.c_str(), target.c_str())) Fail("Snapshot publication failed (" + std::to_string(GetLastError()) + ")");
    SetLast(Obj().S("saved", name).N("files", tree.files.size()).B("real_root", true).Str());
    return true;
}

template <class F> bool Run(F&& f) {
    g_last[0] = 0;
    try { return f(); }
    catch (const std::exception& e) { SetLast(e.what()); }
    catch (...) { SetLast("unexpected error"); }
    return false;
}

struct Job {
    int kind = 0;  // 0 bootstrap, 1 live import, 2 backup
    std::wstring sandbox, saveRoot, token;
    std::string slot;
    bool makeNewest = false;
    GateFn gate = nullptr;
};
DWORD WINAPI Worker(LPVOID p) {
    std::unique_ptr<Job> j((Job*)p);
    bool ok = false;
    switch (j->kind) {
        case 0: ok = RunBootstrap(j->sandbox, j->saveRoot); break;
        case 1: ok = RunLiveImport(j->sandbox, j->saveRoot, j->slot, j->token, j->makeNewest, j->gate); break;
        case 2: ok = RunBackup(j->sandbox, j->saveRoot, j->slot, j->token, j->gate); break;
    }
    return ok ? 0 : 1;
}
void* Start(Job* j) {
    g_cancel.store(false);
    g_last[0] = 0;
    HMODULE self = nullptr;  // keep the .asi mapped while a worker runs
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&Worker), &self);
    HANDLE h = CreateThread(nullptr, 1 << 20, Worker, j, 0, nullptr);
    if (!h) { SetLast("worker thread could not start (" + std::to_string(GetLastError()) + ")"); delete j; }
    return h;
}

}  // namespace

const char* LastResult() { return g_last; }
void RequestCancel() { g_cancel.store(true); }
void SetSettleMs(unsigned ms) { g_settleMs.store(ms); }

bool ValidateSave(const std::string& blob, const std::string& account, uint64_t* savedAt, std::string* error) {
    auto no = [&](const char* why) { if (error) *error = why; return false; };
    if (blob.size() != kBody + kHdr) return no("Unexpected user-save file size");
    const std::string h = HeaderPlain(blob);
    if (h.compare(0, 6, "RNNUSR") != 0) return no("Not a user save");
    if (Rd<uint64_t>(h, 0x10) != _strtoui64(account.c_str(), nullptr, 10)) return no("Header Steam account mismatch");
    if (Rd<uint32_t>(h, 0x1C) != kBody) return no("Invalid body size");
    std::string plain;
    try { plain = Decrypt(blob); } catch (const std::exception&) { return no("Decrypt failed"); }
    if (Rd<uint32_t>(plain, 0x158) != 0x25111400u) return no("Unsupported user-save version");
    if (Checksum(plain) != Rd<uint32_t>(plain, 0x900194)) return no("User-save checksum mismatch");
    if (savedAt) *savedAt = Rd<uint64_t>(h, 0x20);
    return true;
}
uint64_t SavedAt(const std::string& blob) { return Rd<uint64_t>(HeaderPlain(blob), 0x20); }
std::string Restamp(const std::string& blob, uint64_t floor) {
    std::string h = HeaderPlain(blob);
    if (h.size() < kHdr || Rd<uint64_t>(h, 0x20) > floor) return blob;
    const uint64_t v = floor + 1;
    memcpy(h.data() + 0x20, &v, 8);
    const std::string& s = HdrStream();
    for (size_t i = 0; i < kHdr; ++i) h[i] ^= s[i];
    return h + blob.substr(kHdr);
}
std::string DecryptForTest(const std::string& blob) { try { return Decrypt(blob); } catch (...) { return {}; } }
uint32_t ChecksumForTest(const std::string& plain) { return Checksum(plain); }

bool RunBootstrap(const std::wstring& sandbox, const std::wstring& saveRoot) { return Run([&] { return Bootstrap(sandbox, saveRoot); }); }
bool RunLiveImport(const std::wstring& sandbox, const std::wstring& saveRoot, const std::string& slot, const std::wstring& token, bool makeNewest, GateFn gate) {
    return Run([&] { return LiveImport(sandbox, saveRoot, slot, token, makeNewest, gate); });
}
bool RunBackup(const std::wstring& sandbox, const std::wstring& saveRoot, const std::string& slot, const std::wstring& token, GateFn gate) {
    return Run([&] { return Backup(sandbox, saveRoot, slot, token, gate); });
}
bool VerifySlot(const std::wstring& sandbox, const std::string& slot) { return Run([&] { Lib(sandbox).Verify(slot); return true; }); }

void* StartBootstrap(const std::wstring& sandbox, const std::wstring& saveRoot) {
    Job* j = new Job; j->kind = 0; j->sandbox = sandbox; j->saveRoot = saveRoot;
    return Start(j);
}
void* StartLiveImport(const std::wstring& sandbox, const std::wstring& saveRoot, const std::string& slot, const std::wstring& token, bool makeNewest, GateFn gate) {
    Job* j = new Job; j->kind = 1; j->sandbox = sandbox; j->saveRoot = saveRoot; j->slot = slot; j->token = token; j->makeNewest = makeNewest; j->gate = gate;
    return Start(j);
}
void* StartBackup(const std::wstring& sandbox, const std::wstring& saveRoot, const std::string& slot, const std::wstring& token, GateFn gate) {
    Job* j = new Job; j->kind = 2; j->sandbox = sandbox; j->saveRoot = saveRoot; j->slot = slot; j->token = token; j->gate = gate;
    return Start(j);
}

}  // namespace savelib
