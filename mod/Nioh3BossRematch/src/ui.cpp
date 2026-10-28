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
    if (!g_gateReq.exchange(false)) return;
    if (!g_open.load()) { g_owner.store(-1); Toggle(true); }
    rematch::UiGatePage();
    Log("Menu: Return gate page opened");
}
#include "menu_bridge_impl.h"

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
extern "C" __declspec(dllexport) int Nioh3BossRematch_GateChoice() { return addon::ui::GateChoiceRequest() ? 1 : 0; }

extern "C" __declspec(dllexport) int Nioh3RematchMenuGetApi(uint32_t version, uint32_t bytes, rematch_menu_bridge::Api* out) {
    namespace mb = rematch_menu_bridge;
    if (!out || version != mb::kVersion || bytes != sizeof(*out)) return mb::BadVersion;
    *out = {sizeof(*out), mb::kVersion, mb::kRequiredCapabilities, 0,
        &addon::ui::BridgeAttach, &addon::ui::BridgeDetach, &addon::ui::BridgePost, &addon::ui::BridgeSnapshot, &addon::ui::BridgePresentCover};
    return mb::Accepted;
}
