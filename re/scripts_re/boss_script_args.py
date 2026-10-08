#!/usr/bin/env python3
"""Per-boss-script BGM/RTM/boss native call args -> notes/boss-script-args.csv (stdlib, uses lfm_disasm).
Boss script = any archive_09 script that calls Mission::BossBattleMode.
Args = arg-ish tokens right before the call (T/F=op15/op16 bools, vN.M = local var, uN = 16-bit imm, fX = float, ints).
fn = user function whose FN_ENTRY marker most recently precedes the call (approximate: library fns are inlined per script)."""
import sys, os, glob, csv
sys.path.insert(0, os.path.dirname(__file__))
import lfm_disasm as L

ROOT = os.path.join(os.path.dirname(__file__), '..', '..')
WANT = ('Sound::PlayBossBGM', 'Sound::StopBossBGM', 'Sound::PlayMissionBGM', 'Sound::PlayMissionBGMAndState',
        'Sound::StopMissionBGM', 'Sound::ActivateBossBattleBGMWithID', 'Sound::SetEnableAutoBossBGM',
        'Sound::RequestBossBattleBgmEndState', 'Sound::SetBgmIdBgmBox', 'Sound::SetBgmBoxBgmTable',
        'RTM::StartID', 'RTM::Play', 'RTM::PlaywithSetFrame', 'RTM::Request', 'RTM::SetPlayed',
        'Mission::GetTheaterModeRTMID', 'Mission::IsTheaterModeExistRTM',
        'Duel::GetBossPlacementID', 'Duel::IsAliveBoss', 'Mission::BossBattleMode', 'Mission::BossBattleModeEnd',
        'UI::ShowBossGauge', 'Function::PlayBossDefeatRTM', 'PlayBossDefeatRTM', 'Function::RtmContinuousPlaySeamless',
        'Function::RtmContinuousPlayLongLoad', 'Function::RtmPlaySubFunction', 'Function::CreateClearGadgetBossPosition',
        'Function::DuelCommonFlowAnyRange', 'Function::DuelCommonFlow', 'Mission::GetMissionID')


def scripts():
    for f in sorted(glob.glob(os.path.join(ROOT, 'data', 'extract', 'lfm', 'archive_09', '*', '*'))):
        s = L.load(f)
        if s.insns:
            yield f, s


def ids_hex(args):
    """32-bit id/hash-looking args as hex: big PUSH_I32 ints and the first word of [h,7c0b0783,0] literals."""
    out = []
    for a in args:
        if a.startswith('['):
            out.append(a[1:].split(',')[0])
        else:
            try:
                v = int(a)
            except ValueError:
                continue
            if abs(v) >= 0x10000:
                out.append('%08x' % (v & 0xFFFFFFFF))
    return ' '.join(out)


def main(out):
    rows = []
    nboss = 0
    for f, s in scripts():
        nc = L.native_calls(s)
        if not any(n == 'Mission::BossBattleMode' for _, n, _, _, _ in nc):
            continue
        nboss += 1
        sid = os.path.basename(f)[:12]
        for k, name, args, fn, native in nc:
            if name in WANT:
                rows.append((sid, name, ' '.join(args), ids_hex(args), fn, '%x' % s.insns[k].off))
    with open(out, 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['script', 'native', 'args', 'ids_hex', 'fn', 'code_off'])
        w.writerows(rows)
    print(nboss, 'boss scripts,', len(rows), 'rows ->', out)


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'notes', 'boss-script-args.csv'))
