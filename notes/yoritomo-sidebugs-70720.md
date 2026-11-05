# Yoritomo side bugs — 70720

Working base cdae6b9e preserves passing infinite-health TR11e796c0. Shipping base requested from lead; RC integration is owned elsewhere. Offline only.

Evidence: 70453, full archived log file5287 (original SHA256 02d6769ecc745d9c90b58075dd0eb15bbb6b007fc45d67b6bb01ef653fcae439). File5252 contains genuine-kill evidence, not the cliff episode. Lines612784..612988:18 blocked lethal calls, AFF22 and786EDD. Lines613159..613174: boss61847->0, death animation4E3B, then win. No boss damage origin is logged. The v1 guard watched a downstream death animation/AI selection, not native HP/death processing; v2 already removed it. Do not restore a move-ID guard.

Static dump trace (tools/n3dis.py, re/Nioh3.dump.exe):
- 403388 movement routine writes actor+89B from native world-volume test31DDF0 and physics fall distance ([actor+3B0]+30)->188 minus18C compared with runtime parameter45B1 (4042EB..404337). This is native hazard intent, not a guessed Y threshold.
- AF910 consumes +89B nonzero/+89A zero. The local-player hazard branch AFF1D subtracts all current HP, then2A2050; the shared INFHP guard cuts it and does not reset hazard/camera state.
- Native protected-player branch AFE64..AFEAA obtains a recovery position via2169D30(global[4751860]), calls669AB8(actor,pos,0), resets controller via vfunc18(FF), clears word+89A and sets+ADEB=1. Getter2169D30 chooses the player-data position at6950/69A0 through native2FEB58; no mod table.
- Player::RecoverFull204E038 calls6028A4, whose602A08 clears the89A/89B word, then resets character/controller. Existing PlayerRecoverFull wrapper already uses it.
- Camera::Reset VM238AB44 calls no-argument native3B3B1C. Existing Warp syncs actor/stream center but does not invoke this reset. Reset fight uses Warp in practice_stats_game.h:393; automatic retry and loss use it at training.c:1665,1677.
- Native death/drop1FFFEDC(comp,type,drop) gates loot54C9E4 and amritaA1AFA8 on drop; 786C84 calls it at787061 only after native death checks. Its prologue has three whole stack-store instructions (15 bytes), no relative operand. Changing a death animation is too late to prevent these rewards.

Remaining: establish non-player cause at the reward boundary without conflating poison, player-owned projectiles or a valid finisher with a self-death. Actor hazard flags can identify environmental death; proximity cannot. No executable change or gameplay fix claimed at this checkpoint.

## Recovery implementation checkpoint
Player native hazard intent queues recovery through the game recovery-position getter (verified 9-byte prologue), RecoverFull, existing physics/stream Warp and Camera::Reset. Manual Reset fight, retry and loss warps reset the camera too. Failed warp retains intent for retry; OFF/loading/player replacement clear intent; dead players are not resurrected. Native point preferred; active round start is fallback. No guessed altitude or boss/move list.
Build exit0; 16 available harnesses PASS including recovery native/fallback/failure/OFF/loading/dead/stale cases. Existing missing arena generator and 3 raw-action fixture skips remain. No live efficacy claim. Boss self-death changes not yet implemented; incomplete earlier draft excluded.
