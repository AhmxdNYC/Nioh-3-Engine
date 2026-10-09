// bootnotice.h - SkipBootNotice ([Debug] SkipBootNotice, 1.7.48; notes\autosave-notice.md).
// Hands-free boots: skips the boot-time "This game has an autosave feature ... [Confirm]" notice and the "Current graphics
// settings may lead to unstable gameplay - reset all settings to the defaults? Yes/No" warning, and logs the boot
// sequencer's state changes so a stalled boot says where it stopped.
//
// The notice is NOT a first-run notice. The boot sequencer 1C35188 (Game::Mode boot / title sequence, state = [obj+14h])
// shows it on EVERY boot in state 4, after PreloadPSOMode (state 3), unless saving is disabled (283950 = GameMain+3Eh ||
// +3Fh, the same test SaveRequest 5B63A8 uses). Nothing records that it was confirmed: no save flag, no marker file, no
// registry key. State 4:
//   if (!SaveDisabled 283950) {
//     [demo builds only (GameMain+3Ch): two demo notices]
//     SystemMessageAdd E6DA30(text 0x3C4ABF1 "This game has an autosave feature...", confirm = 1)   <- site BN1 1C35393
//     if (![obj+55h] && GfxSettingsTooHeavy 1FE3D38())                                              <- site BN2 1C353A2
//       queue the Yes/No dialog 0x3E7F0F1 / 0x1B80845 (Yes = reset every setting to the defaults)
//   }
//   state = 5
// State 5 waits until no system message is queued ([45BBF80]+C0h == 0, 21AB54C) and no UI layer is open (F1B7C), then
// state 7: SaveRequest(saveMgr 45C8480, 5, 31h, 1) = load the save data (SaveDataLoadBin also rewrites BACKUP.BIN), state 8/9
// wait for [saveMgr]+10h, then the title. Tour mode's SB2/SB3 skip that BACKUP.BIN write ("written OK"), so tour mode
// cannot hold this flow (seen live on the PC 2026-10-09 12:32: notice -> 2 skipped BACKUP.BIN writes -> title -> load).
//
// Sites (patches.json group BN, check-only; verified by tests\test_patterns.py), both E8 rel32 retargeted to a near thunk:
//   BN1 1C35393  call SystemMessageAdd E6DA30 (rdx = the text string, r8b = 1). Skip: rcx = rdx; jmp StringReset 310C18
//                (E6DA30 frees its string argument the same way at its end), so nothing is queued and nothing leaks.
//   BN2 1C353A2  call GfxSettingsTooHeavy 1FE3D38. Skip: call it, count a "true", answer 0 (= the player's "No": nothing
//                is reset; the setting values stay exactly as configured, which is what a benchmark needs).
// Both thunks record the sequencer object (rdi) so a watcher thread can log its state; with the switch off they jump to
// the originals (stock behaviour).
//
// Ini: [Debug] SkipBootNotice = -1 (default: on only when [Debug] Tour != 0 or [Debug] VirtualPad != 0, i.e. hands-free
// test sessions; normal players have both at 0 and nothing is installed), 0 = off (nothing installed), 1 = on,
// 2 = installed but stock (state log only).
#pragma once
#include <cstdint>

namespace bootnotice {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;       // Nioh3.exe image base
    bool knownBuild = false;  // v2.02 only
};

// Verifies the bytes and installs both call-site thunks when the ini asks for it. Call early (dllmain worker): the
// notice comes ~10-30 s after launch, after the boot PSO list.
void Start(const StartInfo& info);
long NoticesSkipped();
long WarningsSkipped();

}  // namespace bootnotice
