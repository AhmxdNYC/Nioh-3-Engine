// vpad_core.h - VirtualPad ([Debug] VirtualPad, 1.7.37): command parser + virtual pad / key composer. Pure logic (no Win32
// calls, not thread-safe on its own: vpad.cpp serialises every call under one lock). Offline-tested by tests/vpad_harness.cpp.
//
// Timing model: the game reads the pad on its input worker once per logic frame (several XInputGetState calls back to back,
// notes\input-thread.md). Calls closer than kRoundGapUs belong to one "round" (= one game frame). A timed input (press A 100)
// starts at the first round that sees it, and ends when its time is up AND at least kMinRounds rounds have seen it, so even a
// 1 ms press is seen by the game on two frames. Keys work the same on the DirectInput keyboard read (once per frame).
#pragma once
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace vpad {

constexpr int64_t kRoundGapUs = 2000;  // calls within 2 ms = the same poll round (same frame)
constexpr uint32_t kMinRounds = 2;     // a timed input is visible to at least 2 game frames
constexpr int kMaxTimed = 64;

struct Pad {
    uint16_t buttons = 0;
    uint8_t lt = 0, rt = 0;
    int16_t lx = 0, ly = 0, rx = 0, ry = 0;
    bool operator==(const Pad& o) const {
        return buttons == o.buttons && lt == o.lt && rt == o.rt && lx == o.lx && ly == o.ly && rx == o.rx && ry == o.ry;
    }
    bool operator!=(const Pad& o) const { return !(*this == o); }
};

struct NameMask {
    const char* name;
    uint16_t mask;
};
// XInput wButtons bits (same values as xinput.h); PlayStation names map to the same positions
inline const NameMask kButtons[] = {
    {"UP", 0x0001},    {"DOWN", 0x0002},     {"LEFT", 0x0004},   {"RIGHT", 0x0008},    {"START", 0x0010},   {"OPTIONS", 0x0010},
    {"MENU", 0x0010},  {"BACK", 0x0020},     {"SELECT", 0x0020}, {"VIEW", 0x0020},     {"LS", 0x0040},      {"L3", 0x0040},
    {"RS", 0x0080},    {"R3", 0x0080},       {"LB", 0x0100},     {"L1", 0x0100},       {"RB", 0x0200},      {"R1", 0x0200},
    {"A", 0x1000},     {"CROSS", 0x1000},    {"B", 0x2000},      {"CIRCLE", 0x2000},   {"X", 0x4000},       {"SQUARE", 0x4000},
    {"Y", 0x8000},     {"TRIANGLE", 0x8000},
};
struct NameDik {
    const char* name;
    uint8_t dik;
};
// DirectInput scan codes (DIK_*) for the keys a harness needs; anything else as hex (0x1C)
inline const NameDik kKeys[] = {
    {"ESC", 0x01},   {"ESCAPE", 0x01}, {"ENTER", 0x1C}, {"RETURN", 0x1C}, {"SPACE", 0x39}, {"BACKSPACE", 0x0E}, {"TAB", 0x0F},
    {"UP", 0xC8},    {"DOWN", 0xD0},   {"LEFT", 0xCB},  {"RIGHT", 0xCD},  {"F1", 0x3B},    {"F2", 0x3C},        {"F3", 0x3D},
    {"F4", 0x3E},    {"F5", 0x3F},     {"F6", 0x40},    {"F7", 0x41},     {"F8", 0x42},    {"F9", 0x43},        {"F10", 0x44},
    {"F11", 0x57},   {"F12", 0x58},    {"W", 0x11},     {"A", 0x1E},      {"S", 0x1F},     {"D", 0x20},         {"E", 0x12},
    {"Q", 0x10},     {"R", 0x13},      {"F", 0x21},     {"LSHIFT", 0x2A}, {"LCTRL", 0x1D}, {"LALT", 0x38},
};

inline bool IEq(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b)
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return false;
    return *a == *b;
}

// "A", "A+B", "LB+RB+X" -> mask; 0 = unknown name
inline uint16_t ParseButtons(const char* s) {
    uint16_t m = 0;
    char part[32];
    while (*s) {
        size_t n = 0;
        while (s[n] && s[n] != '+') ++n;
        if (n == 0 || n >= sizeof(part)) return 0;
        memcpy(part, s, n);
        part[n] = 0;
        uint16_t b = 0;
        for (const NameMask& k : kButtons)
            if (IEq(part, k.name)) b = k.mask;
        if (!b) return 0;
        m |= b;
        s += n;
        if (*s == '+' && !*++s) return 0;  // trailing '+'
    }
    return m;
}
// key name or hex scan code -> DIK (0 = unknown)
inline uint8_t ParseKey(const char* s) {
    for (const NameDik& k : kKeys)
        if (IEq(s, k.name)) return k.dik;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        char* e = nullptr;
        const unsigned long v = strtoul(s + 2, &e, 16);
        if (e && *e == 0 && v > 0 && v < 256) return (uint8_t)v;
    }
    return 0;
}
// stick axis: -1..1 (float) -> int16 (clamped)
inline int16_t AxisFrom(double v) {
    if (!(v == v)) v = 0;  // NaN
    if (v > 1) v = 1;
    if (v < -1) v = -1;
    const double s = v * 32767.0;
    return (int16_t)(s >= 0 ? floor(s + 0.5) : -floor(-s + 0.5));
}
// trigger: 0..1 as a fraction, or 2..255 as the raw byte
inline uint8_t TriggerFrom(double v) {
    if (!(v == v) || v <= 0) return 0;
    if (v <= 1.0) return (uint8_t)floor(v * 255.0 + 0.5);
    return v >= 255 ? 255 : (uint8_t)floor(v + 0.5);
}

// ---- commands ------------------------------------------------------------------------------------------------------------
enum class Op : uint8_t { Bad, Ping, Press, Hold, Release, ReleaseAll, Stick, Trigger, Key, Arm, Disarm, State, MenuGet, MenuSet, Log, Wait, Help, RealPads };

struct Cmd {
    Op op = Op::Bad;
    uint16_t mask = 0;   // buttons
    char side = 0;       // 'L' / 'R' (stick, trigger)
    double x = 0, y = 0; // stick -1..1, trigger value in x
    int ms = -1;         // -1 = hold until changed (stick/trigger) / default (press/key)
    uint8_t dik = 0;
    char key[48] = {};   // menu key
    int value = 0;       // menu set value
    char text[200] = {}; // log text / error
};

inline bool ParseInt(const char* s, int& out) {
    char* e = nullptr;
    const long v = strtol(s, &e, 10);
    if (!e || e == s || *e) return false;
    out = (int)v;
    return true;
}
inline bool ParseNum(const char* s, double& out) {
    char* e = nullptr;
    const double v = strtod(s, &e);
    if (!e || e == s || *e) return false;
    out = v;
    return true;
}
inline bool ParseMs(const char* s, int& out) {  // "100" or "100ms"
    char b[24];
    size_t n = strlen(s);
    if (n == 0 || n >= sizeof(b)) return false;
    memcpy(b, s, n + 1);
    if (n > 2 && (b[n - 1] == 's' || b[n - 1] == 'S') && (b[n - 2] == 'm' || b[n - 2] == 'M')) b[n - 2] = 0;
    return ParseInt(b, out) && out >= 0 && out <= 600000;
}
inline Cmd Bad(const char* why) {
    Cmd c;
    snprintf(c.text, sizeof(c.text), "%s", why);
    return c;
}

// One command line (no newline). Tokens are separated by spaces/tabs; case-insensitive keywords.
inline Cmd Parse(const char* line) {
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", line ? line : "");
    const char* tok[12] = {};
    int n = 0;
    for (char* p = buf; *p && n < 12;) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') *p++ = 0;
        if (!*p) break;
        tok[n++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') ++p;
    }
    if (n == 0) return Bad("empty command");
    const char* v = tok[0];
    Cmd c;
    if (IEq(v, "ping")) { c.op = Op::Ping; return c; }
    if (IEq(v, "help")) { c.op = Op::Help; return c; }
    if (IEq(v, "state")) { c.op = Op::State; return c; }
    if (IEq(v, "arm")) { c.op = Op::Arm; return c; }
    if (IEq(v, "disarm")) { c.op = Op::Disarm; return c; }
    if (IEq(v, "realpads")) {  // the user's real controllers reach the game (on) or not (off); test popup's block toggle
        if (n != 2 || !(IEq(tok[1], "on") || IEq(tok[1], "off"))) return Bad("usage: realpads on|off");
        c.op = Op::RealPads;
        c.value = IEq(tok[1], "on") ? 1 : 0;
        return c;
    }
    if (IEq(v, "log")) {
        // the rest of the original line, verbatim
        const char* r = line;
        while (*r == ' ' || *r == '\t') ++r;
        r += 3;
        while (*r == ' ' || *r == '\t') ++r;
        c.op = Op::Log;
        snprintf(c.text, sizeof(c.text), "%s", r);
        for (char* q = c.text; *q; ++q)
            if (*q == '\r' || *q == '\n') *q = ' ';
        return c;
    }
    if (IEq(v, "wait")) {
        if (n != 2 || !ParseMs(tok[1], c.ms) || c.ms > 60000) return Bad("usage: wait <ms> (<= 60000)");
        c.op = Op::Wait;
        return c;
    }
    if (IEq(v, "press") || IEq(v, "hold") || IEq(v, "release")) {
        if (n < 2) return Bad("usage: press <btn[+btn]> [ms] | hold <btn[+btn]> | release <btn[+btn]|all>");
        if (IEq(v, "release") && IEq(tok[1], "all")) {
            if (n != 2) return Bad("usage: release all");
            c.op = Op::ReleaseAll;
            return c;
        }
        c.mask = ParseButtons(tok[1]);
        if (!c.mask) return Bad("unknown button (A B X Y LB RB LS RS START BACK UP DOWN LEFT RIGHT; PS names too; join with +)");
        if (IEq(v, "press")) {
            c.op = Op::Press;
            c.ms = 100;
            if (n == 3 && !ParseMs(tok[2], c.ms)) return Bad("bad ms");
            if (n > 3) return Bad("usage: press <btn[+btn]> [ms]");
        } else {
            if (n != 2) return Bad("usage: hold|release <btn[+btn]>");
            c.op = IEq(v, "hold") ? Op::Hold : Op::Release;
        }
        return c;
    }
    if (IEq(v, "stick")) {
        if (n < 4 || n > 5) return Bad("usage: stick L|R <x -1..1> <y -1..1> [ms]");
        if (IEq(tok[1], "L") || IEq(tok[1], "left")) c.side = 'L';
        else if (IEq(tok[1], "R") || IEq(tok[1], "right")) c.side = 'R';
        else return Bad("stick side must be L or R");
        if (!ParseNum(tok[2], c.x) || !ParseNum(tok[3], c.y) || fabs(c.x) > 1.0 || fabs(c.y) > 1.0) return Bad("stick x / y must be -1..1");
        if (n == 5 && !ParseMs(tok[4], c.ms)) return Bad("bad ms");
        c.op = Op::Stick;
        return c;
    }
    if (IEq(v, "trigger")) {
        if (n < 3 || n > 4) return Bad("usage: trigger L|R <0..1 | 0..255> [ms]");
        if (IEq(tok[1], "L") || IEq(tok[1], "LT")) c.side = 'L';
        else if (IEq(tok[1], "R") || IEq(tok[1], "RT")) c.side = 'R';
        else return Bad("trigger side must be L or R");
        if (!ParseNum(tok[2], c.x) || c.x < 0 || c.x > 255) return Bad("trigger value must be 0..1 or 0..255");
        if (n == 4 && !ParseMs(tok[3], c.ms)) return Bad("bad ms");
        c.op = Op::Trigger;
        return c;
    }
    if (IEq(v, "key")) {
        if (n < 2 || n > 3) return Bad("usage: key <ESC|ENTER|SPACE|UP|...|0xDIK> [ms]");
        c.dik = ParseKey(tok[1]);
        if (!c.dik) return Bad("unknown key (names: ESC ENTER SPACE BACKSPACE TAB UP DOWN LEFT RIGHT F1-F12 W A S D E Q R F; or 0x<DIK>)");
        c.ms = 100;
        if (n == 3 && !ParseMs(tok[2], c.ms)) return Bad("bad ms");
        c.op = Op::Key;
        return c;
    }
    if (IEq(v, "menu")) {
        if (n >= 3 && IEq(tok[1], "get") && n == 3) {
            c.op = Op::MenuGet;
            snprintf(c.key, sizeof(c.key), "%s", tok[2]);
            return c;
        }
        if (n == 4 && IEq(tok[1], "set")) {
            c.op = Op::MenuSet;
            snprintf(c.key, sizeof(c.key), "%s", tok[2]);
            if (IEq(tok[3], "on") || IEq(tok[3], "true")) c.value = 1;
            else if (IEq(tok[3], "off") || IEq(tok[3], "false")) c.value = 0;
            else if (!ParseInt(tok[3], c.value)) return Bad("menu set value must be an integer or on/off");
            return c;
        }
        return Bad("usage: menu get <KEY> | menu set <KEY> <value|on|off>");
    }
    return Bad("unknown command (help lists them)");
}

inline const char* HelpText() {
    return "commands: ping | state | arm | disarm | press <btn[+btn]> [ms=100] | hold <btn[+btn]> | release <btn[+btn]|all> | "
           "stick L|R <x> <y> [ms] | trigger L|R <v> [ms] | key <name|0xDIK> [ms=100] | menu get <KEY> | menu set <KEY> <v|on|off> | "
           "wait <ms> | log <text> | realpads on|off";
}

// ---- composer ------------------------------------------------------------------------------------------------------------
// Timed inputs + held state -> what the game reads. All times in microseconds (caller's clock).
struct Timed {
    uint32_t id = 0;
    uint8_t kind = 0;  // 0 free, 1 buttons, 2 stick L, 3 stick R, 4 trigger L, 5 trigger R, 6 key
    uint16_t mask = 0;
    int16_t x = 0, y = 0;
    uint8_t v = 0;     // trigger / key dik
    int64_t durUs = 0;
    int64_t startUs = -1;  // -1 = not seen by a round yet
    uint32_t rounds = 0;   // distinct rounds that saw it
    uint32_t lastRound = 0;
};

class Composer {
public:
    // ---- control (pipe thread) ----
    void Hold(uint16_t m) { held_.buttons |= m; }
    void Release(uint16_t m) {
        held_.buttons &= (uint16_t)~m;
        for (Timed& t : t_)
            if (t.kind == 1) t.mask &= (uint16_t)~m;  // a running press of that button ends now (rounds rule still applies)
    }
    void ReleaseAll() {
        held_ = Pad{};
        for (Timed& t : t_)
            if (t.kind >= 1 && t.kind <= 5) t.durUs = 0;  // pad entries end after their minimum rounds
        memset(keyHeld_, 0, sizeof(keyHeld_));
    }
    void SetStick(char side, int16_t x, int16_t y) {
        if (side == 'L') held_.lx = x, held_.ly = y;
        else held_.rx = x, held_.ry = y;
    }
    void SetTrigger(char side, uint8_t v) { (side == 'L' ? held_.lt : held_.rt) = v; }
    // timed entries; returns the id (0 = table full)
    uint32_t AddButtons(uint16_t m, int ms) { return Add(1, m, 0, 0, 0, ms); }
    uint32_t AddStick(char side, int16_t x, int16_t y, int ms) { return Add(side == 'L' ? 2 : 3, 0, x, y, 0, ms); }
    uint32_t AddTrigger(char side, uint8_t v, int ms) { return Add(side == 'L' ? 4 : 5, 0, 0, 0, v, ms); }
    uint32_t AddKey(uint8_t dik, int ms) { return Add(6, 0, 0, 0, dik, ms); }
    // state of a timed entry: 0 = gone (done), 1 = waiting for its first round, 2 = running
    int Status(uint32_t id) const {
        for (const Timed& t : t_)
            if (t.kind && t.id == id) return t.startUs < 0 ? 1 : 2;
        return 0;
    }
    void Cancel(uint32_t id) {
        for (Timed& t : t_)
            if (t.kind && t.id == id) t.kind = 0;
    }
    uint32_t DoneRound(uint32_t id) const { return id == lastDoneId_ ? lastDoneRound_ : 0; }
    uint32_t PadRound() const { return padRound_; }
    uint32_t KeyRound() const { return keyRound_; }
    uint64_t PadReads() const { return padReads_; }
    int64_t LastPadReadUs() const { return lastPadUs_; }
    int64_t LastKeyReadUs() const { return lastKeyUs_; }
    const Pad& Held() const { return held_; }
    bool AnyActive() const {
        if (held_ != Pad{}) return true;
        for (const Timed& t : t_)
            if (t.kind) return true;
        for (uint8_t k : keyHeld_)
            if (k) return true;
        return false;
    }

    // ---- game side: one XInputGetState call for the virtual slot ----
    Pad ReadPad(int64_t nowUs) {
        ++padReads_;
        if (padReads_ == 1 || nowUs - lastPadUs_ > kRoundGapUs) ++padRound_;
        lastPadUs_ = nowUs;
        Pad p = held_;
        for (Timed& t : t_) {
            if (t.kind < 1 || t.kind > 5) continue;
            if (!Advance(t, nowUs, padRound_)) continue;
            switch (t.kind) {
                case 1: p.buttons |= t.mask; break;
                case 2: p.lx = t.x, p.ly = t.y; break;
                case 3: p.rx = t.x, p.ry = t.y; break;
                case 4: p.lt = t.v > p.lt ? t.v : p.lt; break;
                case 5: p.rt = t.v > p.rt ? t.v : p.rt; break;
            }
        }
        return p;
    }
    // ---- game side: one 256-byte DirectInput keyboard read; ORs the virtual keys in ----
    void ReadKeys(int64_t nowUs, uint8_t* keys) {
        if (keyReads_++ == 0 || nowUs - lastKeyUs_ > kRoundGapUs) ++keyRound_;
        lastKeyUs_ = nowUs;
        for (int i = 0; i < 256; ++i)
            if (keyHeld_[i]) keys[i] = 0x80;
        for (Timed& t : t_)
            if (t.kind == 6 && Advance(t, nowUs, keyRound_)) keys[t.v] = 0x80;
    }

private:
    uint32_t Add(uint8_t kind, uint16_t m, int16_t x, int16_t y, uint8_t v, int ms) {
        for (Timed& t : t_) {
            if (t.kind) continue;
            t = Timed{};
            t.id = ++nextId_;
            t.kind = kind;
            t.mask = m;
            t.x = x, t.y = y;
            t.v = v;
            t.durUs = (int64_t)(ms < 0 ? 0 : ms) * 1000;
            return t.id;
        }
        return 0;
    }
    // true = contributes to this read; frees the entry once its time is up and kMinRounds rounds saw it
    bool Advance(Timed& t, int64_t nowUs, uint32_t round) {
        if (t.startUs < 0) t.startUs = nowUs;
        if (t.lastRound != round) {
            t.lastRound = round;
            ++t.rounds;
        }
        if (t.rounds > kMinRounds && nowUs - t.startUs >= t.durUs) {  // a new round past the end: not shown any more
            lastDoneId_ = t.id;
            lastDoneRound_ = round;
            t.kind = 0;
            return false;
        }
        return true;
    }

    Pad held_;
    Timed t_[kMaxTimed];
    uint8_t keyHeld_[256] = {};
    uint32_t nextId_ = 0, padRound_ = 0, keyRound_ = 0, lastDoneId_ = 0, lastDoneRound_ = 0;
    uint64_t padReads_ = 0, keyReads_ = 0;
    int64_t lastPadUs_ = 0, lastKeyUs_ = 0;
};

}  // namespace vpad
