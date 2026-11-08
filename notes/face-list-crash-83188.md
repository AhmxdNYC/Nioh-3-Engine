# Native timed-lerp crash: facing writes corrupt list metadata (#83188)

## Cause

PC crash #83148: Nioh3.exe+468FC reads 0x20, called by +27EA01 from CharacterUpdateMainBody +40489F. Native 404780 obtains the render root from [chr+20]; 27E92C traverses the native list at root+458 and advances each node+14. A null node yields the observed access.

The last bind log at 3:51:55.706 am EST selects `FormFace: 1 yaw quaternion(s) ... [20]+44C(-)`. The heuristic scans arbitrary heap ranges for four floats resembling a yaw quaternion. FormFaceWriteQuats then writes candidate+4 and candidate+12, i.e. render root+450 and +458. Those are the native list header/head, not quaternion storage. The native crash follows about 25 ms later. Thus there is a direct, logged mod writer to the faulting list metadata; this is stronger evidence than a speculative pack-lifetime or overlap diagnosis. Full captured heap memory is unavailable, so the precise corrupted pointer value cannot be recovered from the text report.

## Fix

Delete heuristic quaternion discovery, its cached absolute addresses, and all quaternion writes. Keep the existing known movement-component yaw fields (+A4/+200), transform rows and mirror. Log `typed yaw/rows only` at bind. No guardian slot pin or press throttle is needed to remove this corruption mechanism; a cooldown would leave it reachable on a single press. Camera changes remain those of parent f93bff9c.

## Offline validation

face_list_offline.py compiles the production FormFaceWrite against a memory fixture. A plausible heap-pointer pattern also passes every historical quaternion admission condition; the historical writer corrupts its list head. The fixed writer preserves model-root bytes across 10 reused actor sessions / 600 updates: 3006 checks PASS. This proves write boundaries, not native concurrency or live gameplay stability. Release build in progress at initial checkpoint.

Next: Claude pc/pc2 owns live 80D5F rapid alternating/equipment-swap reproduction and facing check; this companion never touches the game. Baseline installed PC pair at task start: BR c7315ca3, CamKi 617d93cd, EF 9bfe7189, Training f57937d1. Public latest v1.8.10 is a different release pairing.
