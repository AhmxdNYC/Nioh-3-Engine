// inputlog.cpp - see inputlog.h. Edges only. No player names, no paths, no per-frame samples.
#include "inputlog.h"

#include <windows.h>
#include <cstdio>
#include <cstring>

namespace inputlog {
namespace {

struct Ev { char line[180]; };
Ev g_ev[kRing];
int g_n = 0;
int g_head = 0;
CRITICAL_SECTION g_cs;
INIT_ONCE g_once = INIT_ONCE_STATIC_INIT;

BOOL CALLBACK Once(PINIT_ONCE, PVOID, PVOID*) {
    InitializeCriticalSection(&g_cs);
    return TRUE;
}
void Lock() {
    InitOnceExecuteOnce(&g_once, Once, nullptr, nullptr);
    EnterCriticalSection(&g_cs);
}

void Scrub(const char* in, char* out, int cap) {
    if (!out || cap < 1) return;
    out[0] = 0;
    if (!in || !in[0]) return;
    int run = 0;
    bool bad = strstr(in, ":\\") || strstr(in, "\\\\") || strstr(in, "/Users/") || strstr(in, "/home/") || strstr(in, "Users\\");
    for (const char* p = in; *p; ++p) run = (*p >= '0' && *p <= '9') ? run + 1 : 0;
    if (run >= 17) bad = true;
    if (bad) {
        snprintf(out, (size_t)cap, "<redacted>");
        return;
    }
    int j = 0;
    for (const char* p = in; *p && j + 2 < cap; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 || c >= 0x80) continue;
        if ((c == '"' || c == '\\') && j + 3 < cap) out[j++] = '\\';
        out[j++] = (char)c;
    }
    out[j] = 0;
}

void Push(const char* line) {
    if (!line || !line[0]) return;
    Lock();
    snprintf(g_ev[g_head].line, sizeof g_ev[g_head].line, "%s", line);
    g_head = (g_head + 1) % kRing;
    if (g_n < kRing) ++g_n;
    LeaveCriticalSection(&g_cs);
}

void NoteInput(const char* dev, const char* what, const char* edge) {
    char d[16], w[32], e[16], line[180];
    Scrub(dev, d, sizeof d);
    Scrub(what, w, sizeof w);
    Scrub(edge, e, sizeof e);
    if (!d[0] || !w[0] || !e[0]) return;
    snprintf(line, sizeof line, "{\"event\":\"input\",\"fields\":{\"dev\":\"%s\",\"what\":\"%s\",\"edge\":\"%s\"}}", d, w, e);
    Push(line);
}

struct Stick { int ax, ay; char dir[4]; };
struct Slot { unsigned buttons; int lt, rt; Stick L, R; };
struct Key { char name[24]; int down; };
Slot g_slot[4];
Key g_key[16];
bool g_ready = false;

int Axis(int v, int prev) {
    if (prev > 0) return v > 12000 ? 1 : 0;
    if (prev < 0) return v < -12000 ? -1 : 0;
    if (v > 20000) return 1;
    if (v < -20000) return -1;
    return 0;
}
const char* DirName(int ax, int ay) {
    if (!ax && !ay) return "n";
    if (ay > 0 && ax > 0) return "ur";
    if (ay > 0 && ax < 0) return "ul";
    if (ay < 0 && ax > 0) return "dr";
    if (ay < 0 && ax < 0) return "dl";
    if (ay > 0) return "u";
    if (ay < 0) return "d";
    return ax > 0 ? "r" : "l";
}
void Arm() {
    if (g_ready) return;
    for (Slot& s : g_slot) {
        snprintf(s.L.dir, sizeof s.L.dir, "n");
        snprintf(s.R.dir, sizeof s.R.dir, "n");
    }
    g_ready = true;
}
void StickEdge(Stick* s, int x, int y, const char* what) {
    const int ax = Axis(x, s->ax), ay = Axis(y, s->ay);
    const char* d = DirName(ax, ay);
    s->ax = ax;
    s->ay = ay;
    if (!strcmp(d, s->dir)) return;
    snprintf(s->dir, sizeof s->dir, "%s", d);
    NoteInput("pad", what, d);
}

const struct { unsigned bit; const char* name; } kBtn[] = {
    {0x0001u, "du"}, {0x0002u, "dd"}, {0x0004u, "dl"}, {0x0008u, "dr"},
    {0x0010u, "start"}, {0x0020u, "back"}, {0x0040u, "ls"}, {0x0080u, "rs"},
    {0x0100u, "lb"}, {0x0200u, "rb"}, {0x1000u, "a"}, {0x2000u, "b"}, {0x4000u, "x"}, {0x8000u, "y"},
};

}  // namespace

void Reset() {
    Lock();
    g_n = 0;
    g_head = 0;
    memset(g_slot, 0, sizeof g_slot);
    memset(g_key, 0, sizeof g_key);
    g_ready = false;
    LeaveCriticalSection(&g_cs);
}

void ObservePad(int slot, unsigned buttons, int lt, int rt, int lx, int ly, int rx, int ry) {
    if (slot < 0 || slot > 3) return;
    Arm();
    Slot* s = &g_slot[slot];
    for (const auto& b : kBtn) {
        const unsigned bit = b.bit;
        if ((buttons & bit) && !(s->buttons & bit)) NoteInput("pad", b.name, "down");
        else if (!(buttons & bit) && (s->buttons & bit)) NoteInput("pad", b.name, "up");
    }
    const int ltn = lt > 30, rtn = rt > 30;
    if (ltn != s->lt) NoteInput("pad", "lt", ltn ? "down" : "up");
    if (rtn != s->rt) NoteInput("pad", "rt", rtn ? "down" : "up");
    s->buttons = buttons;
    s->lt = ltn;
    s->rt = rtn;
    StickEdge(&s->L, lx, ly, "lx");
    StickEdge(&s->R, rx, ry, "rx");
}

void ObserveKey(const char* name, int down, int allowed) {
    if (!name || !name[0]) return;
    if (!allowed) return;
    Arm();
    Key* k = nullptr;
    for (Key& slot : g_key)
        if (slot.name[0] && !strcmp(slot.name, name)) { k = &slot; break; }
    if (!k) {
        for (Key& slot : g_key)
            if (!slot.name[0]) { k = &slot; snprintf(k->name, sizeof k->name, "%s", name); k->down = 0; break; }
    }
    if (!k) return;
    const int d = down ? 1 : 0;
    if (d == k->down) return;
    k->down = d;
    NoteInput("key", name, d ? "down" : "up");
}

void NoteTyped() {}

void NoteMenu(const char* what, const char* page, const char* row, const char* oldv, const char* newv) {
    char w[24], p[48], r[48], o[24], n[24], line[180];
    Scrub(what, w, sizeof w);
    Scrub(page, p, sizeof p);
    Scrub(row, r, sizeof r);
    Scrub(oldv, o, sizeof o);
    Scrub(newv, n, sizeof n);
    if (!w[0]) return;
    int i = snprintf(line, sizeof line, "{\"event\":\"menu\",\"fields\":{\"what\":\"%s\"", w);
    if (i < 0 || i >= (int)sizeof line) return;
    if (p[0]) i += snprintf(line + i, sizeof line - (size_t)i, ",\"page\":\"%s\"", p);
    if (r[0]) i += snprintf(line + i, sizeof line - (size_t)i, ",\"row\":\"%s\"", r);
    if (o[0] || n[0]) i += snprintf(line + i, sizeof line - (size_t)i, ",\"old\":\"%s\",\"new\":\"%s\"", o, n);
    if (i < 0 || i + 3 >= (int)sizeof line) return;
    snprintf(line + i, sizeof line - (size_t)i, "}}");
    Push(line);
}

int Count() {
    Lock();
    const int n = g_n;
    LeaveCriticalSection(&g_cs);
    return n;
}

int WriteTail(char* dst, int cap, int maxN) {
    if (!dst || cap < 1) return 0;
    dst[0] = 0;
    if (maxN < 1) return 0;
    Lock();
    int n = g_n < maxN ? g_n : maxN;
    int start = (g_head - n + kRing) % kRing;
    int used = 0;
    for (int i = 0; i < n; ++i) {
        const char* s = g_ev[(start + i) % kRing].line;
        int len = (int)strlen(s);
        int need = len + (used ? 1 : 0);
        if (used + need + 1 > cap) break;
        if (used) dst[used++] = ',';
        memcpy(dst + used, s, (size_t)len);
        used += len;
        dst[used] = 0;
    }
    LeaveCriticalSection(&g_cs);
    return used;
}

}  // namespace inputlog
