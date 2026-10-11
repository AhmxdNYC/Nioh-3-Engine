// ui.cpp - the add-on's own small overlay menu: a topmost layered GDI window over the game window (no graphics hook, so a game
// patch cannot break it and it never meets the perf ASI's Present hooks), input from the game's own XInputGetState (IAT slot
// swapped; the previous pointer is always called, so the Steam overlay / the perf ASI's menu stay in the chain) and the keyboard.
//   open / close: [Menu] Hotkey (default F7) or hold View + D-pad Up for 1 s ([Menu] PadGesture = 1)
//   while open: D-pad / arrows move, A / Enter select-confirm, B / Backspace back; the pad is masked from the game while open.
// [Menu] Overlay = 0 disables the window and the input hook (pipe / log only).
#include "addon.h"

#include <xinput.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace addon {
namespace ui {
namespace {

std::atomic<bool> g_open{false};
HWND g_wnd = nullptr, g_game = nullptr;
UINT g_hotkey = VK_F7;
bool g_gesture = true;

// ---- pad ----
using XGetState = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
XGetState g_prevGet = nullptr;
std::atomic<uint32_t> g_padEdges{0};  // buttons pressed since the UI thread last looked
WORD g_lastButtons = 0;
ULONGLONG g_gestSince = 0;

DWORD WINAPI HkGetState(DWORD user, XINPUT_STATE* st) {
    const DWORD r = g_prevGet ? g_prevGet(user, st) : ERROR_DEVICE_NOT_CONNECTED;
    if (r != ERROR_SUCCESS || user != 0 || !st) return r;
    const WORD b = st->Gamepad.wButtons;
    const WORD down = (WORD)(b & ~g_lastButtons);
    g_lastButtons = b;
    if (g_gesture) {
        const WORD g = XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_DPAD_UP;
        if ((b & g) == g) {
            if (!g_gestSince) g_gestSince = GetTickCount64();
            else if (GetTickCount64() - g_gestSince > 1000) {
                g_gestSince = ~0ull;  // once per hold
                g_padEdges.fetch_or(0x10000);
            }
        } else {
            g_gestSince = 0;
        }
    }
    if (g_open.load()) {
        if (down) g_padEdges.fetch_or(down);
        memset(&st->Gamepad, 0, sizeof(st->Gamepad));  // the game sees an idle pad while our menu is open
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

HFONT g_font = nullptr;
constexpr COLORREF kKey = RGB(255, 0, 255);

void Paint(HDC dc, int w, int h) {
    HBRUSH key = CreateSolidBrush(kKey);
    RECT all = {0, 0, w, h};
    FillRect(dc, &all, key);
    DeleteObject(key);
    SelectObject(dc, g_font);
    SetBkMode(dc, TRANSPARENT);
    TEXTMETRICW tm;
    GetTextMetricsW(dc, &tm);
    const int lh = tm.tmHeight + 2;
    if (g_open.load()) {
        rematch::UiRow rows[24];
        char footer[240] = "";
        const int n = rematch::UiFill(rows, 24, footer, sizeof(footer));
        const int pw = w < 1100 ? w - 40 : 1060, ph = (n + 3) * lh + 16;
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
            TextOutA(dc, 30, 28 + i * lh, rows[i].text, (int)strlen(rows[i].text));
        }
        SetTextColor(dc, RGB(220, 220, 140));
        RECT fr = {30, 28 + (n + 1) * lh, 10 + pw, 28 + (n + 3) * lh};
        DrawTextA(dc, footer, -1, &fr, DT_WORDBREAK | DT_NOPREFIX);
        return;
    }
    char t[160];
    uint32_t c = 0;
    if (rematch::Toast(t, sizeof(t), c)) {
        SIZE sz;
        GetTextExtentPoint32A(dc, t, (int)strlen(t), &sz);
        RECT p = {20, 20, 40 + sz.cx, 30 + lh};
        HBRUSH bg = CreateSolidBrush(RGB(16, 18, 24));
        FillRect(dc, &p, bg);
        DeleteObject(bg);
        SetTextColor(dc, c & 0xFFFFFF);
        TextOutA(dc, 30, 25, t, (int)strlen(t));
    }
}

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT r;
        GetClientRect(h, &r);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, r.right, r.bottom);
        HGDIOBJ ob = SelectObject(mem, bmp);
        Paint(mem, r.right, r.bottom);
        BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob);
        DeleteObject(bmp);
        DeleteDC(mem);
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

void Toggle(bool open) {
    if (open == g_open.load()) return;
    if (open) rematch::UiOpen();
    g_open.store(open);
    Log("Menu %s", open ? "opened" : "closed");
}

DWORD WINAPI UiThread(void*) {
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
        return 0;
    }
    SetLayeredWindowAttributes(g_wnd, kKey, 235, LWA_COLORKEY | LWA_ALPHA);
    g_font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
    bool shown = false;
    for (;;) {
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
        if ((fg && KeyEdge((int)g_hotkey)) || (pe & 0x10000)) Toggle(!g_open.load());
        if (g_open.load()) {
            const bool up = (pe & XINPUT_GAMEPAD_DPAD_UP) || (fg && KeyEdge(VK_UP));
            const bool dn = (pe & XINPUT_GAMEPAD_DPAD_DOWN) || (fg && KeyEdge(VK_DOWN));
            const bool pu = (pe & XINPUT_GAMEPAD_LEFT_SHOULDER) || (fg && KeyEdge(VK_PRIOR));
            const bool pd = (pe & XINPUT_GAMEPAD_RIGHT_SHOULDER) || (fg && KeyEdge(VK_NEXT));
            const bool ok = (pe & XINPUT_GAMEPAD_A) || (fg && KeyEdge(VK_RETURN));
            const bool bk = (pe & XINPUT_GAMEPAD_B) || (fg && KeyEdge(VK_BACK));
            if (up) rematch::UiMove(-1);
            if (dn) rematch::UiMove(1);
            if (pu) rematch::UiMove(-10);
            if (pd) rematch::UiMove(10);
            if (ok && rematch::UiAct()) Toggle(false);
            if (bk && rematch::UiBack()) Toggle(false);
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

bool Start() {
    if (!IniInt(L"Menu", L"Overlay", 1)) {
        Log("Menu: off ([Menu] Overlay = 0): use the pipe ([Remote] Pipe = 1)");
        return false;
    }
    wchar_t hk[16] = {};
    GetPrivateProfileStringW(L"Menu", L"Hotkey", L"F7", hk, 16, IniPath());
    g_hotkey = VkFromName(hk);
    g_gesture = IniInt(L"Menu", L"PadGesture", 1) != 0;
    const bool pad = HookPad();
    if (HANDLE th = CreateThread(nullptr, 0, UiThread, nullptr, 0, nullptr)) CloseHandle(th);
    Log("Menu: overlay on (%ls%s; pad %s)", hk, g_gesture ? " or hold View + D-pad Up 1 s" : "",
        pad ? "via the game's XInputGetState (masked while the menu is open)" : "NOT hooked (no xinput import) - keyboard only");
    return true;
}

}  // namespace ui
}  // namespace addon
