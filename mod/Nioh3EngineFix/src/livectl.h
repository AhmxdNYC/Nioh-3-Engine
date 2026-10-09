// livectl.h - LiveCtl: one-launch A/B control file for Nioh3EngineFix (fast testing, notes\fast-testing.md).
// [Debug] LiveCtl = 0 by default (nothing runs). With 1, a thread polls the control file every 100 ms
// ([Debug] LiveCtlFile, default Nioh3EngineFix_live.ini next to the ini) and applies its [Live] section when it changes:
//
//     [Live]
//     Gen = 7                 ; any number; echoed in the ack so the writer knows its file was applied
//     Tag = PP_q1_on          ; free text (echoed)
//     PP = 1                  ; any live menu row by its key (menu::Remote: same code + 'MENU:' log line as a menu press)
//     Hot.ghdm = 1            ; psoswap ShaderHot set (twin PSOs bound / unbound)
//     Tuning.PP_Period = 8    ; a [Tuning] value that is read at use time (PP_Period, VSMA_K, VSMA_Front, LPVR_N, SKYR_V, ...)
//
// Keys are applied in file order. After every apply, and every 500 ms, <file>.ack is rewritten (temp + rename):
//     gen=7 tag=PP_q1_on t_apply_unix_ms=... age_ms=... ok=3 fail=0 PP=1 Hot.ghdm=1 Tuning.PP_Period=8
// (read-back values; a key that failed shows err) plus one log line 'LIVE: apply ...'. A control file that already
// exists when the game starts is stale (an earlier run): it is not applied until it changes.
// Toggling while the game renders is exactly what the menu rows / vpad 'menu set' already do; LiveCtl only adds a
// file transport (no pipe, no VirtualPad needed; works the same under Proton).
#pragma once
#include <cstddef>

namespace livectl {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    // menu::Remote(key, set, value, reply, n, timeoutMs): 0 = OK; reply ends "(value N)"
    int (*remote)(const char* key, int set, int value, char* reply, size_t n, unsigned timeoutMs) = nullptr;
    // live [Tuning] setter: true = known key (reply = new value text)
    bool (*tuning)(const char* key, const char* value, char* reply, size_t n) = nullptr;
    // ShaderHot: set / get by set name (psoswap::HotSet / HotGet)
    bool (*hotSet)(const char* name, int on, char* reply, size_t n) = nullptr;
    int (*hotGet)(const char* name) = nullptr;
};

void Start(const StartInfo& info);  // reads [Debug] LiveCtl; 0 -> returns, nothing runs
bool Enabled();
// osd Present hook, once per frame of the game's swapchain: the mod's own frame clock for the A/B windows. The ack
// carries cumulative 'frames= qpc_us= ft_sq_us= h20= h33= h50=' (frames, QPC microseconds, sum of squared frame times
// in us^2 / 1e3, frames longer than 20 / 33.3 / 50 ms), so a window's fps and hitches come from two acks, independent
// of PresentMon (which loses present events on some PCs). No-op unless LiveCtl is on.
void OnFrame();

}  // namespace livectl
