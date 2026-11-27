// ui.cpp - single UI worker, hosted through EngineFix's DX12 overlay when available.
// Hosted mode publishes snapshots/actions and relinquishes pad ownership; no Ui* method runs on Present.
// Standalone fallback: a topmost layered GDI window over the game window (no graphics hook, so a game
// patch cannot break it and it never meets the perf ASI's Present hooks), input from the game's own XInputGetState (IAT slot
// swapped; the previous pointer is always called, so the Steam overlay / the perf ASI's menu stay in the chain) and the keyboard.
//   open / close: [Menu] Hotkey (default F7) or hold View + D-pad Up for 1 s ([Menu] PadGesture = 1)
//   while open: D-pad / arrows move, A / Enter select-confirm, B / Backspace back; the pad is masked from the game while open.
// [Menu] Overlay = 0 disables the window and the input hook (pipe / log only).
#include "addon.h"
#include "ui_input.h"
#include "../../shared/gdi_stroke_text.h"
#include "../../rematch_menu_bridge.h"

#include <xinput.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace addon {
namespace ui {
namespace {

std::atomic<bool> g_open{false};
std::atomic<bool> g_hostRequested{false};
HWND g_wnd = nullptr, g_game = nullptr;
UINT g_hotkey = VK_F7;
bool g_gesture = true;

// ---- pad ----
using XGetState = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
XGetState g_prevGet = nullptr;
std::atomic<uint32_t> g_padEdges{0};  // buttons pressed since the UI thread last looked
addon_ui_input::Pad g_pads[4];
std::atomic<int> g_owner{-1}, g_toggleOwner{-1};
std::atomic<unsigned> g_latchEpoch{0};

DWORD WINAPI HkGetState(DWORD user, XINPUT_STATE* st) {
    const DWORD r = g_prevGet ? g_prevGet(user, st) : ERROR_DEVICE_NOT_CONNECTED;
    if (g_hostRequested.load()) return r; // preserve chain; the EngineFix host owns input
    if (user >= 4) return r;
    if (r != ERROR_SUCCESS || !st) {
        int expected = (int)user;
        g_owner.compare_exchange_strong(expected, -1);
        g_pads[user] = {};
        return r;
    }
    const WORD b = st->Gamepad.wButtons;
    const XINPUT_GAMEPAD& pad = st->Gamepad;
    const bool nonNeutral = b || pad.bLeftTrigger > 30 || pad.bRightTrigger > 30 ||
        abs((int)pad.sThumbLX) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || abs((int)pad.sThumbLY) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
        abs((int)pad.sThumbRX) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || abs((int)pad.sThumbRY) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
    uint16_t directions = 0;
    if (pad.sThumbLY > 16000) directions |= XINPUT_GAMEPAD_DPAD_UP;
    if (pad.sThumbLY < -16000) directions |= XINPUT_GAMEPAD_DPAD_DOWN;
    if (pad.sThumbLX > 16000) directions |= XINPUT_GAMEPAD_DPAD_RIGHT;
    if (pad.sThumbLX < -16000) directions |= XINPUT_GAMEPAD_DPAD_LEFT;
    const auto input = g_pads[user].Feed(b, nonNeutral, GetTickCount64(), g_open.load(), g_gesture, g_latchEpoch.load(), directions);
    if (input.toggle && addon_ui_input::OwnsInput(g_owner.load(), user)) {
        int expected = -1;
        if (g_toggleOwner.compare_exchange_strong(expected, (int)user)) g_padEdges.fetch_or(0x10000);
    }
    const uint16_t nav = addon_ui_input::NavigationEdges(input.edges);
    if (nav) {
        int expected = -1;
        g_owner.compare_exchange_strong(expected, (int)user);  // keyboard-opened menu adopts its first active pad
        if (g_owner.load() == (int)user) g_padEdges.fetch_or(nav);
    }
    if (input.blank) {
        st->dwPacketNumber += 0x40000000u;
        memset(&st->Gamepad, 0, sizeof(st->Gamepad));  // all pads stay neutral, including the release after closing
    }
    return r;
}

void** FindXInputSlot() {
    uint8_t* base = (uint8_t*)g_img.base;
    __try {
        auto dos = (IMAGE_DOS_HEADER*)base;
        auto nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
        const IMAGE_DATA_DIRECTORY& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dd.VirtualAddress) return nullptr;
        for (auto d = (IMAGE_IMPORT_DESCRIPTOR*)(base + dd.VirtualAddress); d->Name; ++d) {
            const char* dll = (const char*)(base + d->Name);
            if (_strnicmp(dll, "xinput", 6) != 0 || !d->OriginalFirstThunk || !d->FirstThunk) continue;
            const bool ord2 = _strnicmp(dll, "xinput1_", 8) == 0;
            auto names = (IMAGE_THUNK_DATA64*)(base + d->OriginalFirstThunk);
            auto iat = (void**)(base + d->FirstThunk);
            for (int i = 0; names[i].u1.AddressOfData; ++i) {
                const ULONGLONG e = names[i].u1.AddressOfData;
                if (IMAGE_SNAP_BY_ORDINAL64(e)) {
                    if (ord2 && IMAGE_ORDINAL64(e) == 2) return &iat[i];
                } else if (!strcmp(((IMAGE_IMPORT_BY_NAME*)(base + e))->Name, "XInputGetState")) {
                    return &iat[i];
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return nullptr;
}
bool HookPad() {
    void** slot = FindXInputSlot();
    if (!slot) return false;
    DWORD old = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    g_prevGet = (XGetState)InterlockedExchangePointer(slot, (void*)&HkGetState);
    DWORD t;
    VirtualProtect(slot, sizeof(void*), old, &t);
    return g_prevGet != nullptr;
}

// ---- window ----
BOOL CALLBACK FindGame(HWND h, LPARAM) {
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId() || h == g_wnd || !IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r;
    GetClientRect(h, &r);
    if (r.right - r.left < 320) return TRUE;
    g_game = h;
    return FALSE;
}

constexpr int kFontPx = 20;
constexpr COLORREF kKey = RGB(255, 0, 255);

void Paint(HDC dc, int w, int h) {
    HBRUSH key = CreateSolidBrush(kKey);
    RECT all = {0, 0, w, h};
    FillRect(dc, &all, key);
    DeleteObject(key);
    const int lh = kFontPx + 2;
    if (g_open.load()) {
        rematch::UiRow rows[24];
        char footer[240] = "";
        const int n = rematch::UiFill(rows, 24, footer, sizeof(footer));
        const int pw = w < 1100 ? w - 40 : 1060, ph = (n + 5) * lh + 16;
        RECT p = {20, 20, 20 + pw, 20 + ph};
        HBRUSH bg = CreateSolidBrush(RGB(16, 18, 24));
        FillRect(dc, &p, bg);
        DeleteObject(bg);
        for (int i = 0; i < n; ++i) {
            if (rows[i].sel) {
                RECT s = {24, 28 + i * lh, 16 + pw, 28 + (i + 1) * lh};
                HBRUSH sb = CreateSolidBrush(RGB(48, 56, 80));
                FillRect(dc, &s, sb);
                DeleteObject(sb);
            }
            SetTextColor(dc, rows[i].color & 0xFFFFFF);
            // user #22412: a row wider than the panel lost its tail (Repeat / Used state). Cut the middle ("...") and keep the end.
            char fit[256];
            snprintf(fit, sizeof fit, "%s", rows[i].text);
            for (char* t = strchr(fit, '	'); t; t = strchr(t, '	')) *t = ' ';  // human #44361: right-column TAB (EF host aligns it)
            const int maxW = pw - 24;
            int rowPx=kFontPx;
            SIZE rs=N3StrokeMeasure((int)strlen(fit),rowPx,N3StrokeWidth(rowPx));
            // user #23924: first shrink the font for a long row (down to 70 %) so nothing is lost; only what still does not fit is cut in the middle
            while(rs.cx>maxW && rowPx>kFontPx*7/10) {
                --rowPx;
                rs=N3StrokeMeasure((int)strlen(fit),rowPx,N3StrokeWidth(rowPx));
            }
            if (rs.cx > maxW) {
                const int len = (int)strlen(fit), tail = len > 30 ? 30 : len / 2;
                char cut[256]; snprintf(cut, sizeof cut, "%s", fit);
                for (int head = len - tail - 1; head > 4; head -= 2) {
                    snprintf(cut, sizeof cut, "%.*s...%s", head, fit, fit + len - tail);
                    rs=N3StrokeMeasure((int)strlen(cut),rowPx,N3StrokeWidth(rowPx));
                    if (rs.cx <= maxW) break;
                }
                snprintf(fit, sizeof fit, "%s", cut);
            }
            const int saved=SaveDC(dc);
            IntersectClipRect(dc,30,28+i*lh,30+maxW,28+(i+1)*lh);
            N3StrokeText(dc,30,28+i*lh,fit,(int)strlen(fit),rowPx,N3StrokeWidth(rowPx));
            RestoreDC(dc,saved);
        }
        SetTextColor(dc, RGB(220, 220, 140));
        RECT fr = {30, 28 + (n + 1) * lh, 10 + pw, 28 + (n + 3) * lh};
        N3StrokeWrapped(dc,footer,fr,kFontPx,N3StrokeWidth(kFontPx));
        SetTextColor(dc, RGB(180, 185, 200));
        const char* controls = "D-pad / arrows: move/change   A / Enter: select   B / Esc / Backspace: back   LB / RB: page";
        N3StrokeText(dc, 30, 28 + (n + 3) * lh, controls, (int)strlen(controls), kFontPx, N3StrokeWidth(kFontPx));
        char openHint[128];
        snprintf(openHint, sizeof(openHint), "Open / close: F%u%s", g_hotkey - VK_F1 + 1,
                 g_gesture ? " or hold View + D-pad Up for 1 second" : "");
        N3StrokeText(dc, 30, 28 + (n + 4) * lh, openHint, (int)strlen(openHint), kFontPx, N3StrokeWidth(kFontPx));
        return;
    }
    char t[160];
    uint32_t c = 0;
    if (rematch::Toast(t, sizeof(t), c)) {
        SIZE sz=N3StrokeMeasure((int)strlen(t),kFontPx,N3StrokeWidth(kFontPx));
        RECT p = {20, 20, 40 + sz.cx, 30 + lh};
        HBRUSH bg = CreateSolidBrush(RGB(16, 18, 24));
        FillRect(dc, &p, bg);
        DeleteObject(bg);
        SetTextColor(dc, c & 0xFFFFFF);
        N3StrokeText(dc, 30, 25, t, (int)strlen(t), kFontPx, N3StrokeWidth(kFontPx));
    }
}

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        (void)dc; N3StrokeWindow(h,Paint);
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_NCHITTEST) return HTTRANSPARENT;
    return DefWindowProcW(h, m, w, l);
}

bool KeyEdge(int vk) {
    static bool prev[256];
    const bool d = (GetAsyncKeyState(vk) & 0x8000) != 0;
    const bool e = d && !prev[vk & 0xFF];
    prev[vk & 0xFF] = d;
    return e;
}

std::atomic<bool> g_gateReq{false};
void Toggle(bool open) {
    if (open == g_open.load()) return;
    if (open) rematch::UiOpen();
    g_padEdges.store(0);
    g_latchEpoch.fetch_add(1);  // publish isolation before publishing a close
    if (!open) g_owner.store(-1);
    g_open.store(open);
    Log("Menu %s", open ? "opened" : "closed");
}

void GateReqTick() {  // UI worker only. user #6789: Return gate used in the arena -> 2-option page
    rematch::TitleWatchdog();
    if (!g_gateReq.exchange(false)) return;
    if (!g_open.load()) { g_owner.store(-1); Toggle(true); }
    rematch::UiGatePage();
    Log("Menu: Return gate page opened");
}
#if BR_SOULCORE
void ScOverlayKick();  // TASK #137007
#endif
// human #135018: soul core pair keys (F2 next, Shift+F2 previous, F6 flag; F9 is Nioh3Training's debug freeze key) with the menu closed; both the standalone and the
// Engine Fix hosted loops call this every ~33 ms. Only while the game window is in front.
void SoulPairKeys() {
    const HWND fgw = GetForegroundWindow(); DWORD fpid = 0; if (fgw) GetWindowThreadProcessId(fgw, &fpid);
    const bool mine = fpid == GetCurrentProcessId();
    const bool f9 = KeyEdge(VK_F2), f6 = KeyEdge(VK_F6), sh = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool f3 = KeyEdge(VK_F3), f4 = KeyEdge(VK_F4);  // TASK #137007 (human #137004): Yang 1 / Yang 2 summon <-> Transform
    rematch::SoulPairTick(mine && f9 && !sh, mine && f9 && sh, mine && f6);
#if BR_SOULCORE
    // TASK #137543 (regression #137316 gap 4): Alt+F4 quits the game; it must not also flip Yang 2 (and save it) on the way out.
    const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    rematch::SoulCallKeyTick(mine && f3 && !alt, mine && f4 && !alt, sh);  // TASK #141276: F3 / F4 step Summon -> Transform -> Fast summon, Shift = back
    if (mine && (f9 || ((f3 || f4) && !alt))) ScOverlayKick();  // the overlay redraws now, not on its next 250 ms poll
#endif
}
#include "menu_bridge_impl.h"

#if BR_SOULCORE
// TASK #137000 (human #136998 / #137004): soul core overlay, bottom left of the game window. Its own small layered topmost window and thread,
// so it shows with the EngineFix-hosted menu and the standalone one alike. Hidden while the mod menu is open, the game is not in front or minimized,
// or there is nothing to show. Text is the menu's stroke font on the toast panel colour; redrawn only when a line changes.
namespace scov {
HWND g_wnd = nullptr, g_gameWnd = nullptr;
HANDLE g_kick = CreateEventW(nullptr, FALSE, FALSE, nullptr);  // TASK #137007: F2 / F3 / F4 wake the thread at once
char g_lines[2][64] = {};
uint32_t g_colors[2] = {};
int g_n = 0, g_px = 20, g_adv = 10, g_box = 1, g_bold = 0;
uint32_t g_panel = 0x181210;
BOOL CALLBACK FindGameOv(HWND h, LPARAM) {
    DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId() || h == g_wnd || h == addon::ui::g_wnd || !IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r; GetClientRect(h, &r);
    if (r.right - r.left < 320) return TRUE;
    g_gameWnd = h; return FALSE;
}
void PaintOv(HDC dc, int w, int h) {
    HBRUSH key = CreateSolidBrush(RGB(255, 0, 255)); RECT all = {0, 0, w, h}; FillRect(dc, &all, key); DeleteObject(key);
    if (g_box) { HBRUSH bg = CreateSolidBrush(g_panel & 0xFFFFFF); FillRect(dc, &all, bg); DeleteObject(bg); }
    const int lh = g_px + g_px / 5, pad = g_px / 3, o = g_px / 14 > 1 ? g_px / 14 : 1;
    for (int i = 0; i < g_n; ++i) {
        const int x = pad, y = pad + i * lh, len = (int)strlen(g_lines[i]);
        if (!g_box) {  // TASK #137007: no panel -> a dark outline keeps the text readable on any background (the style meter's look)
            SetTextColor(dc, RGB(0, 0, 0));
            static const int d[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, 1}, {-1, 1}, {1, -1}};
            for (const auto& k : d) N3StrokeText(dc, x + k[0] * o + (g_bold ? o : 0) * (k[0] > 0), y + k[1] * o, g_lines[i], len, g_px, g_adv);
        }
        SetTextColor(dc, g_colors[i] & 0xFFFFFF);
        N3StrokeText(dc, x, y, g_lines[i], len, g_px, g_adv);
        if (g_bold) N3StrokeText(dc, x + o, y, g_lines[i], len, g_px, g_adv);  // Bold: the strokes once more, one step right
    }
}
// TASK #138501 (human #138497): 'Game' font = the style meter's in-game letters (Nioh3Training style_meter_draw.h SmGameLetter: Cambria,
// cream 0xECE6D6, soft black shadow at 70%), drawn as per-pixel alpha (premultiplied) so the anti-aliased edges never blend with a colour key.
// The yokai name is cream; the ' | S / FS / T / TM' tag keeps the line's theme colour.
constexpr uint32_t kGameCream = 0xD6E6EC;  // COLORREF of the style meter's SM_GAME_TEXT_RGB 0xECE6D6
HFONT GameFont(int px) { return CreateFontW(-px, 0, 0, 0, FW_NORMAL, FALSE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Cambria"); }
int GameMeasure(const char* s, int px) {
    HDC dc = CreateCompatibleDC(nullptr); HFONT f = GameFont(px); int cx = 0;
    if (dc && f) { HGDIOBJ of = SelectObject(dc, f); SIZE sz = {}; GetTextExtentPoint32A(dc, s, (int)strlen(s), &sz); cx = sz.cx; SelectObject(dc, of); }
    if (f) DeleteObject(f);
    if (dc) DeleteDC(dc);
    return cx;
}
inline void PmOver(uint32_t& d, uint32_t rgb, float a) {  // premultiplied src-over into BGRA; rgb = COLORREF
    if (a <= 0.f) return;
    const float r = (float)(rgb & 255), g = (float)((rgb >> 8) & 255), b = (float)((rgb >> 16) & 255), k = 1.f - a;
    const float db = (float)(d & 255), dg = (float)((d >> 8) & 255), dr = (float)((d >> 16) & 255), da = (float)(d >> 24);
    d = ((uint32_t)(da * k + 255.f * a + .5f) << 24) | ((uint32_t)(dr * k + r * a + .5f) << 16) | ((uint32_t)(dg * k + g * a + .5f) << 8) |
        (uint32_t)(db * k + b * a + .5f);
}
void PaintGameOv(HWND wnd, BYTE alpha) {
    RECT r; GetClientRect(wnd, &r); const int w = r.right, h = r.bottom; if (w <= 0 || h <= 0) return;
    HDC scr = GetDC(nullptr), dc = CreateCompatibleDC(scr), mdc = CreateCompatibleDC(scr);
    BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    uint32_t *bits = nullptr, *mask = nullptr;
    HBITMAP bm = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0), mb = CreateDIBSection(mdc, &bi, DIB_RGB_COLORS, (void**)&mask, nullptr, 0);
    HFONT f = GameFont(g_px);
    if (bm && mb && f && bits && mask) {
        HGDIOBJ ob = SelectObject(dc, bm), omb = SelectObject(mdc, mb), of = SelectObject(mdc, f);
        memset(bits, 0, (size_t)w * h * 4);
        if (g_box) for (size_t i = 0; i < (size_t)w * h; ++i) PmOver(bits[i], g_panel & 0xFFFFFF, 0.62f);
        SetBkMode(mdc, TRANSPARENT); SetTextColor(mdc, RGB(255, 255, 255));
        const int lh = g_px + g_px / 4, pad = g_px / 3, so = g_px / 16 > 1 ? g_px / 16 : 1;
        for (int i = 0; i < g_n; ++i) {
            memset(mask, 0, (size_t)w * h * 4);
            const char* line = g_lines[i]; const char* bar = strstr(line, " | ");
            SIZE all = {}, nm = {}; GetTextExtentPoint32A(mdc, line, (int)strlen(line), &all);
            if (bar) GetTextExtentPoint32A(mdc, line, (int)(bar - line), &nm); else nm = all;
            const int x0 = (w - all.cx) / 2, y0 = pad + i * lh;  // each line centred
            TextOutA(mdc, x0, y0, line, (int)strlen(line)); GdiFlush();
            const int split = x0 + nm.cx;
            for (int pass = 0; pass < 2; ++pass)  // 0 = soft shadow (+so, +so) and a faint outline, 1 = the letters
                for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
                    const uint32_t m = mask[(size_t)y * w + x];
                    const unsigned c8 = (m & 255) > ((m >> 8) & 255) ? (m & 255) : ((m >> 8) & 255);
                    if (!c8) continue;
                    const float c = c8 / 255.f;
                    if (pass == 0) {
                        if (x + so < w && y + so < h) PmOver(bits[(size_t)(y + so) * w + x + so], 0, 0.7f * c);
                        static const int d[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
                        for (const auto& k : d) {
                            const int xx = x + k[0], yy = y + k[1];
                            if (xx >= 0 && yy >= 0 && xx < w && yy < h) PmOver(bits[(size_t)yy * w + xx], 0, 0.35f * c);
                        }
                    } else {
                        PmOver(bits[(size_t)y * w + x], x < split ? kGameCream : (g_colors[i] & 0xFFFFFF), c);
                    }
                }
        }
        POINT origin = {0, 0}; SIZE size = {w, h}; BLENDFUNCTION blend = {AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA};
        UpdateLayeredWindow(wnd, scr, nullptr, &size, dc, &origin, 0, &blend, ULW_ALPHA);
        SelectObject(mdc, of); SelectObject(mdc, omb); SelectObject(dc, ob);
    }
    if (f) DeleteObject(f);
    if (mb) DeleteObject(mb);
    if (bm) DeleteObject(bm);
    DeleteDC(mdc); DeleteDC(dc); ReleaseDC(nullptr, scr);
}
LRESULT CALLBACK OvProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCHITTEST) return HTTRANSPARENT;
    return DefWindowProcW(h, m, w, l);
}
DWORD WINAPI OvThread(void*) {
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = OvProc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"Nioh3BossRematchSoulCoreOverlay";
    RegisterClassExW(&wc);
    g_wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP,
                            0, 0, 10, 10, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_wnd) { Log("SoulCoreOverlay: window could not be created (%lu)", GetLastError()); return 0; }
    Log("SoulCoreOverlay: ready ([Menu] SoulCoreOverlay, TASK #137000)");
    bool shown = false; RECT last = {};
    char was[2][64] = {}; uint32_t wasC[2] = {}; int wasN = -1, wasPx = 0;
    int wasLook[8] = {};
    for (;;) {
        WaitForSingleObject(g_kick, 250);  // TASK #137007: F2 / F3 / F4 set it
        MSG msg; while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        if (!g_gameWnd || !IsWindow(g_gameWnd)) { g_gameWnd = nullptr; EnumWindows(FindGameOv, 0); }
        char lines[2][64] = {}; uint32_t colors[2] = {};
        const bool fg = g_gameWnd && !IsIconic(g_gameWnd) && GetForegroundWindow() == g_gameWnd;
        const int n = fg && !addon::ui::g_open.load() ? addon::rematch::ScOverlayLines(lines, colors, 2) : 0;
        if (n <= 0) { if (shown) { ShowWindow(g_wnd, SW_HIDE); shown = false; } continue; }
        RECT cr; GetClientRect(g_gameWnd, &cr); POINT o = {0, 0}; ClientToScreen(g_gameWnd, &o);
        if (cr.right <= 0 || cr.bottom <= 0) continue;
        int look[8] = {}; addon::rematch::ScOverlayLook(look);  // TASK #137007: size %, x / y per-mille, opacity %, box, colors, font, panel
        const int base = cr.bottom / 54 < 14 ? 14 : cr.bottom / 54;  // 20 px at 1080p, the menu's size
        const int px = base * look[0] / 100 < 8 ? 8 : base * look[0] / 100;
        const bool game = look[6] == 3;  // TASK #138501: the style meter's letters
        const int adv = look[6] == 1 ? N3StrokeWidth(px) * 6 / 5 : N3StrokeWidth(px), bold = look[6] == 2;
        const int ol = game ? 2 + 2 * (px / 16 > 1 ? px / 16 : 1) : look[4] ? 0 : 2 * (px / 14 > 1 ? px / 14 : 1);  // room for the outline / shadow
        int wmax = 0;
        for (int i = 0; i < n; ++i) { const int cx = game ? GameMeasure(lines[i], px) : N3StrokeMeasure((int)strlen(lines[i]), px, adv).cx; if (cx > wmax) wmax = cx; }
        const int lh = game ? px + px / 4 : px + px / 5, pad = px / 3, w = wmax + 2 * pad + ol + (bold ? px / 14 + 1 : 0), h = n * lh + 2 * pad + (game ? ol : 0);
        // TASK #138501: x = the overlay's centre (under the game's left item HUD by default), y = its bottom edge
        int x = o.x + (int)((long long)cr.right * look[1] / 1000) - w / 2, y = o.y + (int)((long long)cr.bottom * look[2] / 1000) - h;
        if (x + w > o.x + cr.right) x = o.x + cr.right - w;
        if (x < o.x) x = o.x;
        if (y < o.y) y = o.y;
        const RECT want = {x, y, x + w, y + h};
        const bool changed = n != wasN || px != wasPx || memcmp(lines, was, sizeof was) || memcmp(colors, wasC, sizeof wasC) || memcmp(look, wasLook, sizeof look);
        if (!shown || memcmp(&want, &last, sizeof last)) {
            SetWindowPos(g_wnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            last = want; shown = true;
        }
        if (changed || wasN < 0) {
            memcpy(g_lines, lines, sizeof g_lines); memcpy(g_colors, colors, sizeof g_colors); g_n = n; g_px = px;
            g_adv = adv; g_box = look[4]; g_bold = bold; g_panel = (uint32_t)look[7];
            const int op = look[3] < 20 ? 20 : look[3] > 100 ? 100 : look[3];
            if (game) PaintGameOv(g_wnd, (BYTE)((op * 255 + 50) / 100));  // TASK #138501
            else N3StrokeWindowAlpha(g_wnd, PaintOv, (BYTE)((op * 255 + 50) / 100));  // TASK #137543: rounded, 92% = 235 like #137000
            memcpy(was, lines, sizeof was); memcpy(wasC, colors, sizeof wasC); wasN = n; wasPx = px; memcpy(wasLook, look, sizeof look);
        }
    }
}
}  // namespace scov
void ScOverlayKick() { if (scov::g_kick) SetEvent(scov::g_kick); }
#endif

UINT VkFromName(const wchar_t* s);
bool EngineHostReady() {
    const HMODULE engine = GetModuleHandleW(L"Nioh3EngineFix.asi");
    const auto hostVersion = engine ? reinterpret_cast<uint32_t (*)()>(GetProcAddress(engine, "Nioh3EngineMenuHostVersion")) : nullptr;
    return hostVersion && hostVersion() == rematch_menu_bridge::kVersion;
}

DWORD WINAPI UiThread(void*) {
    // The host export is 0 until EngineFix has presented, so a check at load always missed it.
    // Poll here, before any standalone window or pad hook. Attach can also set g_hostRequested.
    const ULONGLONG t0 = GetTickCount64();
    for (;;) {
        if (g_hostRequested.load() || EngineHostReady()) {
            g_hostRequested.store(true);
            Log("Menu: EngineFix in-game host attached; standalone window/pad hook skipped");
            return HostedThread(nullptr);
        }
        const ULONGLONG waited = GetTickCount64() - t0;
        const bool engine = GetModuleHandleW(L"Nioh3EngineFix.asi") != nullptr;
        if (waited >= (engine ? 180000ULL : 20000ULL)) break;
        Sleep(200);
    }
    Log("Menu: EngineFix host did not answer; standalone overlay starting");
    wchar_t hk[16] = {};
    GetPrivateProfileStringW(L"Menu", L"Hotkey", L"F7", hk, 16, IniPath());
    g_hotkey = VkFromName(hk);
    g_gesture = IniInt(L"Menu", L"PadGesture", 1) != 0;
    const bool pad = HookPad();
    Log("Menu: overlay on (%ls%s; pad %s)", hk, g_gesture ? " or hold View + D-pad Up 1 s" : "",
        pad ? "via the game's XInputGetState (masked while the menu is open)" : "NOT hooked (no xinput import) - keyboard only");
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"Nioh3BossRematchOverlay";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);
    g_wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP, 0,
                            0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_wnd) {
        Log("Menu: overlay window could not be created (%lu) - pipe / log only", GetLastError());
        g_hostRequested.store(true);
        return HostedThread(nullptr); // still allow a later compatible in-game host
    }
    // Per-pixel alpha is supplied by N3StrokeWindow.
    bool shown = false;
    for (;;) {
        GateReqTick();
        SoulPairKeys();  // human #135018
        if (g_hostRequested.load()) { HostedTick(); Sleep(33); continue; }
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!g_game || !IsWindow(g_game)) {
            g_game = nullptr;
            EnumWindows(FindGame, 0);
        }
        const bool fg = g_game && GetForegroundWindow() == g_game;
        // input
        const uint32_t pe = g_padEdges.exchange(0);
        const bool hotkey = KeyEdge((int)g_hotkey), upKey = KeyEdge(VK_UP), dnKey = KeyEdge(VK_DOWN);
        const bool puKey = KeyEdge(VK_PRIOR), pdKey = KeyEdge(VK_NEXT), okKey = KeyEdge(VK_RETURN);
        const bool bkKey = KeyEdge(VK_BACK), escKey = KeyEdge(VK_ESCAPE);
        const bool leftKey = KeyEdge(VK_LEFT), rightKey = KeyEdge(VK_RIGHT);
        // Sample every key every tick: opening cannot replay an old Enter/arrow edge.
        const int toggleOwner = pe & 0x10000 ? g_toggleOwner.exchange(-1) : -1;
        if ((fg && hotkey) || (pe & 0x10000)) {
            g_owner.store(pe & 0x10000 ? toggleOwner : -1);
            Toggle(!g_open.load());
        }
        else if (g_open.load()) {
            const bool up = (pe & XINPUT_GAMEPAD_DPAD_UP) || (fg && upKey);
            const bool dn = (pe & XINPUT_GAMEPAD_DPAD_DOWN) || (fg && dnKey);
            const bool pu = (pe & XINPUT_GAMEPAD_LEFT_SHOULDER) || (fg && puKey);
            const bool pd = (pe & XINPUT_GAMEPAD_RIGHT_SHOULDER) || (fg && pdKey);
            const bool left = (pe & XINPUT_GAMEPAD_DPAD_LEFT) || (fg && leftKey);
            const bool right = (pe & XINPUT_GAMEPAD_DPAD_RIGHT) || (fg && rightKey);
            const bool ok = (pe & XINPUT_GAMEPAD_A) || (fg && okKey);
            const bool bk = (pe & XINPUT_GAMEPAD_B) || (fg && (bkKey || escKey));
            if (bk) { if (rematch::UiBack()) Toggle(false); }
            else if (pu || pd) { if (pu != pd) rematch::UiMove(pu ? -10 : 10); }
            else if (up || dn) { if (up != dn) rematch::UiMove(up ? -1 : 1); }
            else if (left || right) { if (left != right) rematch::UiChange(left ? -1 : 1); }
            else if (ok && rematch::UiAct()) Toggle(false);
        }
        // draw
        char t[8];
        uint32_t c;
        const bool want = g_game && (g_open.load() || rematch::Toast(t, sizeof(t), c)) && !IsIconic(g_game);
        if (want) {
            RECT r;
            GetClientRect(g_game, &r);
            POINT o = {0, 0};
            ClientToScreen(g_game, &o);
            SetWindowPos(g_wnd, HWND_TOPMOST, o.x, o.y, r.right, r.bottom, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            InvalidateRect(g_wnd, nullptr, FALSE);
            shown = true;
        } else if (shown) {
            ShowWindow(g_wnd, SW_HIDE);
            shown = false;
        }
        Sleep(33);
    }
}

UINT VkFromName(const wchar_t* s) {
    if ((s[0] == L'F' || s[0] == L'f') && s[1]) {
        const int n = _wtoi(s + 1);
        if (n >= 1 && n <= 24) return (UINT)(VK_F1 + n - 1);
    }
    return VK_F7;
}

}  // namespace

void HostedFightCommand(const char* row,char* reply,size_t n){BridgeTestFight(row,reply,n);}

bool GateChoiceRequest() { if (!g_bridgeAvailable.load()) return false; g_gateReq.store(true); return true; }

bool Start() {
    g_hostedFightTest=IniInt(L"Remote",L"HostedFightTest",0)!=0;
    if (!IniInt(L"Menu", L"Overlay", 1)) {
        Log("Menu: off ([Menu] Overlay = 0): use the pipe ([Remote] Pipe = 1)");
        return false;
    }
    rematch::UiSettingsPrepare();
    // One worker. It retries the host API until the in-game menu has presented,
    // then attaches. Standalone F7 is only if EngineFix never shows up.
#if BR_SOULCORE
    if (HANDLE ov = CreateThread(nullptr, 0, scov::OvThread, nullptr, 0, nullptr)) CloseHandle(ov);  // TASK #137000
#endif
    g_bridgeAvailable.store(true);
    if (HANDLE th = CreateThread(nullptr, 0, UiThread, nullptr, 0, nullptr)) {
        CloseHandle(th);
        Log("Menu: retrying EngineFix host API before standalone overlay");
        return true;
    }
    g_bridgeAvailable.store(false);
    Log("Menu: UI worker unavailable");
    return false;
}

}  // namespace ui
}  // namespace addon

// user #6789: Nioh3Training's arena Return gate asks for the 2-option page (1 = the menu worker will open it, 0 = no menu: caller returns directly)
extern "C" __declspec(dllexport) int Nioh3BossRematch_WavesGate() { return addon::rematch::WavesGateLive() ? 1 : 0; }
extern "C" __declspec(dllexport) int Nioh3BossRematch_GateChoice() { return addon::ui::GateChoiceRequest() ? 1 : 0; }

extern "C" __declspec(dllexport) int Nioh3RematchMenuGetApi(uint32_t version, uint32_t bytes, rematch_menu_bridge::Api* out) {
    namespace mb = rematch_menu_bridge;
    if (!out || version != mb::kVersion || bytes != sizeof(*out)) return mb::BadVersion;
    *out = {sizeof(*out), mb::kVersion, mb::kRequiredCapabilities, 0,
        &addon::ui::BridgeAttach, &addon::ui::BridgeDetach, &addon::ui::BridgePost, &addon::ui::BridgeSnapshot, &addon::ui::BridgePresentCover};
    return mb::Accepted;
}
