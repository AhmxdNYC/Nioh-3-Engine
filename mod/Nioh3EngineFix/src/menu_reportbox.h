// menu_reportbox.h - the menu Report box (bridge #64882, human #64860 / #64870). Included by menu.cpp inside its anonymous
// namespace, after Open/Close/DoReport. Sending, scrubbing and the local copy live in reportsend.cpp.
//
// A / Enter on the Report row opens the box ("What just went wrong?", at most 1000 characters). Rows under it: Send, Cancel,
// and "Website + folder" (the old Report row action).
//   Keyboard: typed characters come from WM_CHAR through the PeekMessage hook (SwallowMessage), so the keyboard layout, Shift and
//     Proton all work. Backspace deletes, Enter is a new line in the box (activates the row elsewhere), Ctrl+Enter sends, Up / Down
//     move between the box and the rows, Esc cancels. While the box is open the game sees no keys at all.
//   Controller: A on the box opens Steam's gamepad keyboard ([Report] SteamKeyboard = 1, steam_api64 export, only when Steam
//     accepts) or else the built-in key grid drawn here: D-pad / stick move, A types, B deletes, X space, Y caps, RB or View = done.
//     B on a row cancels the box.
// Nothing typed here is recorded in the input ring; only "box open / send / cancel" are.
namespace rbox {
constexpr int kMax = 1000;
SRWLOCK g_lock = SRWLOCK_INIT;   // text (message thread <-> Present thread)
volatile LONG g_on = 0;          // box open (read by the input threads)
volatile LONG g_keyMsgEver = 0;  // the PeekMessage hook delivered a key message this session (keyboard text works)
volatile LONG g_act = 0;         // keyboard actions queued by the message thread for the Present thread
enum : LONG { KA_UP = 1, KA_DOWN = 2, KA_OK = 4, KA_CANCEL = 8, KA_SEND = 16 };
char g_text[kMax + 1] = "";
int g_len = 0;
enum Focus : int { fText = 0, fSend, fCancel, fWeb, fGrid };
volatile LONG g_focus = fText;
bool g_grid = false, g_caps = false;
int g_gr = 0, g_gc = 0;
int g_steam = 0;                 // 1 = Steam's keyboard is up, waiting for its callback
const char* g_via = "keyboard";
char g_hint[160] = "";
ULONGLONG g_rep[4] = {};
LONG g_lastSendState = 0;

constexpr int kGridRows = 5, kGridCols = 10;
const char* const kGridLower[4] = {"1234567890", "qwertyuiop", "asdfghjkl'", "zxcvbnm,.?"};
const char* const kGridUpper[4] = {"!@#$%&*()-", "QWERTYUIOP", "ASDFGHJKL\"", "ZXCVBNM;:/"};
const int kSpecialStart[4] = {0, 2, 7, 9};  // bottom row: Caps (0-1), Space (2-6), Del (7-8), Done (9)
const char* const kSpecialName[4] = {"Caps", "Space", "Del", "Done"};
int SpecialCell(int col) { return col <= 1 ? 0 : col <= 6 ? 1 : col <= 8 ? 2 : 3; }

void InsertLocked(char c) {
    if (g_len >= kMax) return;
    g_text[g_len++] = c;
    g_text[g_len] = 0;
}
void Insert(char c) { AcquireSRWLockExclusive(&g_lock); InsertLocked(c); ReleaseSRWLockExclusive(&g_lock); }
void Backspace() {
    AcquireSRWLockExclusive(&g_lock);
    if (g_len > 0) g_text[--g_len] = 0;
    ReleaseSRWLockExclusive(&g_lock);
}
void SetText(const char* s) {
    AcquireSRWLockExclusive(&g_lock);
    g_len = 0;
    for (const char* p = s; *p && g_len < kMax; ++p) {
        const unsigned char c = (unsigned char)*p;
        if (c == '\n' || (c >= 32 && c < 127)) g_text[g_len++] = (char)c;
        else if (c >= 0x80 && (c & 0xC0) != 0x80) g_text[g_len++] = '?';  // non-ASCII (the menu font is ASCII): one '?' per character
    }
    g_text[g_len] = 0;
    ReleaseSRWLockExclusive(&g_lock);
}

void OpenBox(bool fromPad) {
    InterlockedExchange(&g_act, 0);
    g_grid = false;
    g_steam = 0;
    g_gr = g_gc = 0;
    g_hint[0] = 0;
    for (auto& r : g_rep) r = 0;
    if (reportsend::Status(nullptr, 0) != reportsend::State::Sending) reportsend::ClearStatus();
    g_lastSendState = (LONG)reportsend::Status(nullptr, 0);
    g_via = fromPad ? "pad" : "keyboard";
    g_focus = fText;
    InterlockedExchange(&g_on, 1);
    reportsend::NoteInput(fromPad ? "pad" : "key", "report_box");
    MLOG("report box opened (%s)", fromPad ? "controller" : "keyboard");
}
void CloseBox(const char* why) {
    if (!g_on) return;
    InterlockedExchange(&g_on, 0);
    g_grid = false;
    g_steam = 0;
    reportsend::NoteInput("menu", "report_close");
    MLOG("report box closed (%s)", why);
}

// Pad A on the text box: Steam's keyboard when it takes the request, else the built-in grid.
void StartPadTyping() {
    g_via = "pad";
    if (reportsend::SteamEnabled()) {
        char cur[kMax + 1];
        AcquireSRWLockShared(&g_lock);
        memcpy(cur, g_text, sizeof cur);
        ReleaseSRWLockShared(&g_lock);
        if (reportsend::SteamShow(cur, kMax)) {
            g_steam = 1;
            g_via = "steam";
            snprintf(g_hint, sizeof g_hint, "Steam keyboard open. No keyboard on screen? Press B for the built-in keys.");
            return;
        }
    }
    g_grid = true;
    g_focus = fGrid;
    snprintf(g_hint, sizeof g_hint, "%s", "");
}

void DoSend() {
    char text[kMax + 1];
    AcquireSRWLockShared(&g_lock);
    memcpy(text, g_text, sizeof text);
    ReleaseSRWLockShared(&g_lock);
    char msg[160];
    if (reportsend::Send(text, g_via, msg, sizeof msg)) {
        g_hint[0] = 0;
        reportsend::NoteInput("menu", "report_send");
        MLOG("report box: send started (%d chars, via %s)", (int)strlen(text), g_via);
    } else {
        snprintf(g_hint, sizeof g_hint, "%s", msg);
    }
}

void Activate(int focus, bool fromPad) {
    switch (focus) {
        case fText:
            if (fromPad) StartPadTyping();
            else snprintf(g_hint, sizeof g_hint, "Type on the keyboard. Ctrl+Enter sends.");
            break;
        case fSend: DoSend(); break;
        case fCancel: CloseBox("Cancel row"); break;
        case fWeb: DoReport(); CloseBox("website row"); break;
        default: break;
    }
}

void MoveFocus(int dir) {
    int f = (int)g_focus;
    if (f == fGrid) f = fText;
    f = (f + dir + 4) % 4;
    g_focus = f;
}

void GridPress() {
    if (g_gr < 4) {
        const char c = (g_caps ? kGridUpper : kGridLower)[g_gr][g_gc];
        Insert(c);
        return;
    }
    switch (SpecialCell(g_gc)) {
        case 0: g_caps = !g_caps; break;
        case 1: Insert(' '); break;
        case 2: Backspace(); break;
        default: g_grid = false; g_focus = fSend; break;
    }
}
void GridMove(int d) {  // 0 up, 1 down, 2 left, 3 right
    if (d == 0) g_gr = (g_gr + kGridRows - 1) % kGridRows;
    else if (d == 1) g_gr = (g_gr + 1) % kGridRows;
    else if (g_gr == 4) {  // bottom row: jump between its four keys
        int cell = SpecialCell(g_gc);
        cell = (cell + (d == 2 ? 3 : 1)) % 4;
        g_gc = kSpecialStart[cell];
    } else g_gc = (g_gc + (d == 2 ? kGridCols - 1 : 1)) % kGridCols;
}

// Present thread, while the box is open. pressed = N_* edges this frame, pad = the part that came from a controller.
// held = N_* bits held now (for D-pad repeat in the grid).
void Handle(uint32_t pressed, uint32_t pad, uint32_t held, ULONGLONG t) {
    // keyboard actions from the message thread
    const LONG act = InterlockedExchange(&g_act, 0);
    if (act & KA_CANCEL) { CloseBox("Esc"); return; }
    if (act & KA_SEND) DoSend();
    if (act & KA_UP) MoveFocus(-1);
    if (act & KA_DOWN) MoveFocus(+1);
    if (act & KA_OK) Activate((int)g_focus, false);
    // a finished send: clear the box once
    const LONG ss = (LONG)reportsend::Status(nullptr, 0);
    if (ss != g_lastSendState) {
        if (ss == (LONG)reportsend::State::Sent) { SetText(""); g_focus = fCancel; g_grid = false; }
        g_lastSendState = ss;
    }
    if (g_steam) {
        char buf[kMax + 1];
        const int r = reportsend::SteamPoll(buf, sizeof buf);
        if (r == 1) { SetText(buf); g_steam = 0; g_focus = fSend; g_hint[0] = 0; }
        else if (r == 2) { g_steam = 0; g_hint[0] = 0; }
        else if (pressed & N_BACK) { g_steam = 0; g_grid = true; g_focus = fGrid; g_hint[0] = 0; }  // no Steam keyboard showed up
        return;
    }
    const bool fromPad = (pressed & pad) != 0;
    if (g_grid && g_focus == fGrid) {
        static const uint32_t dirs[4] = {N_UP, N_DOWN, N_LEFT, N_RIGHT};
        for (int d = 0; d < 4; ++d) {
            bool go = false;
            if (pressed & dirs[d]) { go = true; g_rep[d] = t + 350; }
            else if ((held & dirs[d]) && g_rep[d] && t >= g_rep[d]) { go = true; g_rep[d] = t + 110; }
            else if (!(held & dirs[d])) g_rep[d] = 0;
            if (go) GridMove(d);
        }
        if (pressed & N_OK) GridPress();
        if (pressed & N_BACK) Backspace();
        if (pressed & N_X) Insert(' ');
        if (pressed & N_CLOSE) g_caps = !g_caps;
        if (pressed & (N_TAB_NEXT | N_HOME)) { g_grid = false; g_focus = fSend; }
        if (pressed & N_TAB_PREV) Backspace();
        return;
    }
    if (pressed & N_BACK) { CloseBox(fromPad ? "B" : "Esc"); return; }
    if (pressed & N_CLOSE) { CloseBox("Y"); Close("Y in the report box"); return; }
    if (pressed & N_UP) MoveFocus(-1);
    if (pressed & N_DOWN) MoveFocus(+1);
    if (pressed & N_OK) Activate((int)g_focus, fromPad);
}

// Message thread (PeekMessage hook). Returns true when the message belongs to the box (the game never sees it).
bool OnMessage(const tagMSG* m) {
    const UINT msg = m->message;
    if (msg == WM_CHAR) {
        const unsigned c = (unsigned)m->wParam;
        if (c >= 32 && c < 127) Insert((char)c);
        else if (c >= 0x80) Insert('?');
        return true;
    }
    if (msg != WM_KEYDOWN && msg != WM_SYSKEYDOWN && msg != WM_KEYUP && msg != WM_SYSKEYUP) return false;
    if (msg == WM_KEYUP || msg == WM_SYSKEYUP) return true;
    const UINT vk = (UINT)m->wParam;
    const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    switch (vk) {
        case VK_ESCAPE: InterlockedOr(&g_act, KA_CANCEL); return true;
        case VK_BACK: if (g_focus == fText || g_focus == fGrid) Backspace(); return true;
        case VK_RETURN:
            if (ctrl) InterlockedOr(&g_act, KA_SEND);
            else if (g_focus == fText) Insert('\n');
            else InterlockedOr(&g_act, KA_OK);
            return true;
        case VK_UP: InterlockedOr(&g_act, KA_UP); return true;
        case VK_DOWN: case VK_TAB: InterlockedOr(&g_act, KA_DOWN); return true;
        default: break;
    }
    if (g_focus == fText || g_focus == fGrid) TranslateMessage(reinterpret_cast<const MSG*>(m));  // queues the WM_CHAR we read next
    return true;
}

// word-wrap the text into lines of at most cols characters (newlines kept)
int Wrap(const char* s, int cols, char (*out)[64], int cap) {
    int n = 0, col = 0;
    char cur[64] = "";
    auto flush = [&]() { if (n < cap) snprintf(out[n], 64, "%s", cur); ++n; cur[0] = 0; col = 0; };
    for (const char* p = s; *p; ++p) {
        if (*p == '\n') { flush(); continue; }
        if (col >= cols) {  // break at the last space when there is one
            char* sp = strrchr(cur, ' ');
            if (sp && sp != cur) {
                char rest[64];
                snprintf(rest, sizeof rest, "%s", sp + 1);
                *sp = 0;
                flush();
                snprintf(cur, sizeof cur, "%s", rest);
                col = (int)strlen(cur);
            } else flush();
        }
        cur[col++] = *p;
        cur[col] = 0;
    }
    flush();
    return n;
}
}  // namespace rbox

float DrawReportBox(unsigned W, unsigned H) {
    namespace ms = menu_style;
    const ULONGLONG t = Now();
    const float sc = PanelScale(W, H);
    constexpr int cols = 58, textCols = 54, textLines = 7;
    const float lineH = (osd::kGlyphH + 2) * sc, pad = 6 * sc, cw = osd::kGlyphW * sc;
    const bool grid = rbox::g_grid;
    const int rows = 1 + 1 + textLines + 2 + 1 + (grid ? rbox::kGridRows + 1 : 0) + 3 + 1 + 2 + 2;
    const float w = (float)cols * cw + 2 * pad + cw, h = rows * lineH + 2 * pad;
    const float x0 = std::floor((float)W - w - 12.0f), y0 = 12.0f;
    const float fa = ms::OpenFade(t);
    ms::Panel(x0, y0, w, h, sc, ms::kPanelBg, fa);
    ms::PanelBand(x0, y0, w, h, sc, 2 * sc, pad + lineH - 2 * sc, sc, ms::Fade(ms::kTitleBg, fa));
    int line = 0;
    auto Y = [&](int l) { return y0 + pad + (float)l * lineH; };
    auto text = [&](float x, int l, const char* s, uint32_t c) { return osd::DrawText(x, Y(l), s, ms::Fade(c, fa), sc); };
    {
        char ver[40];
        snprintf(ver, sizeof ver, "EngineFix v%s", g_host.version);
        text(x0 + pad, line, "Report a problem", ms::kTitleText);
        text(x0 + w - pad - (float)strlen(ver) * cw, line, ver, cDim);
        ++line;
    }
    text(x0 + pad, line++, "What just went wrong?", cCyan);
    // the text box
    char copy[rbox::kMax + 1];
    AcquireSRWLockShared(&rbox::g_lock);
    memcpy(copy, rbox::g_text, sizeof copy);
    const int len = rbox::g_len;
    ReleaseSRWLockShared(&rbox::g_lock);
    const bool boxSel = rbox::g_focus == rbox::fText || rbox::g_focus == rbox::fGrid;
    const float bx = x0 + pad, by = Y(line) - 2 * sc, bw = w - 2 * pad, bh = textLines * lineH + 4 * sc;
    osd::DrawRect(bx, by, bw, bh, ms::Fade(0xC0101010, fa));
    ms::Divider(bx, by, bw, sc, fa);
    ms::Divider(bx, by + bh - sc, bw, sc, fa);
    if (boxSel) { osd::DrawRect(bx, by, 2 * sc, bh, ms::Fade(cGold, fa)); }
    static char wrapped[64][64];
    const int nl = rbox::Wrap(copy, textCols, wrapped, 64);
    const int firstL = nl > textLines ? nl - textLines : 0;
    const bool caretOn = boxSel && ((t / 500) % 2) == 0 && !rbox::g_steam;
    if (len == 0) {
        text(bx + 2 * cw, line, rbox::g_steam ? "(Steam keyboard is open)" : rbox::g_via[0] == 'p' ? "A: type with the controller" : "Type here", cDim);
        if (caretOn) text(bx + cw, line, "_", cWhite);
    } else {
        for (int i = firstL; i < nl && i < 64; ++i) {
            const float ex = text(bx + cw, line + (i - firstL), wrapped[i], cWhite);
            if (i == nl - 1 && caretOn) text(ex, line + (i - firstL), "_", cGold);
        }
    }
    line += textLines;
    {
        char cnt[64];
        snprintf(cnt, sizeof cnt, "%d / %d characters", len, rbox::kMax);
        text(x0 + w - pad - (float)strlen(cnt) * cw, line, cnt, len >= rbox::kMax ? cOrange : cDim);
        ++line;
    }
    ++line;
    if (grid) {  // the built-in key grid
        const float kw = 4 * cw, kx0 = x0 + pad + 3 * cw;
        for (int r = 0; r < rbox::kGridRows; ++r) {
            if (r < 4) {
                const char* keys = (rbox::g_caps ? rbox::kGridUpper : rbox::kGridLower)[r];
                for (int c = 0; c < rbox::kGridCols; ++c) {
                    const bool sel = rbox::g_focus == rbox::fGrid && rbox::g_gr == r && rbox::g_gc == c;
                    const float kx = kx0 + (float)c * (kw + cw);
                    osd::DrawRect(kx, Y(line) - sc, kw, lineH - sc, ms::Fade(sel ? cGold : 0xFF303030, fa));
                    char k[2] = {keys[c], 0};
                    text(kx + 1.5f * cw, line, k, sel ? 0xFF000000 : cWhite);
                }
            } else {
                for (int cell = 0; cell < 4; ++cell) {
                    const int c0 = rbox::kSpecialStart[cell], c1 = cell < 3 ? rbox::kSpecialStart[cell + 1] : rbox::kGridCols;
                    const bool sel = rbox::g_focus == rbox::fGrid && rbox::g_gr == 4 && rbox::SpecialCell(rbox::g_gc) == cell;
                    const float kx = kx0 + (float)c0 * (kw + cw), kwid = (float)(c1 - c0) * (kw + cw) - cw;
                    const bool capsOn = cell == 0 && rbox::g_caps;
                    osd::DrawRect(kx, Y(line) - sc, kwid, lineH - sc, ms::Fade(sel ? cGold : capsOn ? 0xFF606060 : 0xFF303030, fa));
                    const char* nm = rbox::kSpecialName[cell];
                    text(kx + (kwid - (float)strlen(nm) * cw) / 2, line, nm, sel ? 0xFF000000 : cWhite);
                }
            }
            ++line;
        }
        ++line;
    }
    // rows
    char st[300] = "";
    const reportsend::State ss = reportsend::Status(st, sizeof st);
    const bool sent = ss == reportsend::State::Sent;
    const char* rowsTxt[3] = {ss == reportsend::State::Sending ? "Sending..." : "Send", sent ? "Close" : "Cancel", "Website + folder (old way)"};
    for (int i = 0; i < 3; ++i) {
        const bool sel = rbox::g_focus == rbox::fSend + i;
        if (sel) ms::SelectBar(x0 + sc, Y(line) - sc, w - 2 * sc, lineH, sc, fa);
        text(x0 + pad + 2 * cw, line, rowsTxt[i], sel ? cWhite : cGray);
        if (sel) ms::Hint(x0 + pad + 34 * cw, Y(line), "A", i == 0 ? "send" : i == 1 ? (sent ? "close" : "cancel") : "open", cw, lineH, sc, fa);
        ++line;
    }
    // status / hint (two lines, clipped)
    const char* status = rbox::g_hint[0] ? rbox::g_hint : st;
    const uint32_t scol = rbox::g_hint[0] ? cYellow : ss == reportsend::State::Sent ? cGreen : ss == reportsend::State::Failed ? cOrange : cGray;
    char l1[cols + 1] = "", l2[cols + 1] = "";
    {
        const size_t sl = strlen(status);
        size_t cut = sl <= (size_t)cols ? sl : (size_t)cols;
        if (sl > (size_t)cols) { for (size_t k = cut; k > 20; --k) if (status[k] == ' ') { cut = k; break; } }
        memcpy(l1, status, cut);
        l1[cut] = 0;
        snprintf(l2, sizeof l2, "%s", status + cut + (status[cut] == ' ' ? 1 : 0));
    }
    text(x0 + pad, line++, l1[0] ? l1 : "Sends your text, the mod report and the recent logs.", l1[0] ? scol : cDim);
    text(x0 + pad, line++, l2[0] ? l2 : "No name or Steam id: user folders become <user>.", l2[0] ? scol : cDim);
    ++line;
    // footer: pad + keys
    const float fy = Y(line) - 2 * sc;
    ms::PanelBand(x0, y0, w, h, sc, fy - y0, y0 + h - fy - sc, sc, ms::Fade(ms::kFooterBg, fa));
    if (grid && rbox::g_focus == rbox::fGrid) {
        const ms::HintItem kPad[] = {{"A", "Type"}, {"B", "Delete"}, {"X", "Space"}, {"Y", "Caps"}, {"RB", "Done"}};
        ms::HintRow(x0 + pad, Y(line), x0 + w - pad, kPad, 5, cw, lineH, sc, fa);
    } else {
        const ms::HintItem kPad[] = {{"A", "Select"}, {"B", "Cancel"}, {"UD", "Move"}};
        ms::HintRow(x0 + pad, Y(line), x0 + w - pad, kPad, 3, cw, lineH, sc, fa);
    }
    ++line;
    {
        static const ms::HintItem kBoxKeys[] = {{"Enter", "New line"}, {"Ctrl+Enter", "Send"}, {"Up Down", "Rows"}, {"Esc", "Cancel"}};
        const float kx = osd::DrawText(x0 + pad, Y(line), "Keys", ms::Fade(cDim, fa), sc) + cw;
        ms::HintRow(kx, Y(line), x0 + w - pad, kBoxKeys, 4, cw, lineH, sc, fa);
    }
    return sc;
}
