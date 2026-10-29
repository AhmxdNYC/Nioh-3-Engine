# Training meter on the all-day pack (#34127)

Base is gx-pc/allday-33749 @ 73e7a325. The 26 Training files that differ from cx-pc/meter-center-25713 @ c3c55e8d were taken from that meter tip, plus mod/move_catalog.h so archive hit and damage facts load. EngineFix and BossRematch were not rebuilt.

Shipped [Patches] VRROnline is already -1 on this base, and the code default is -1. The live game ini already has VRROnline = -1.

Controller display is EngineFix, not Training. Menu group "Mod menu controls", row "Controller display". Ini [Menu] ControllerDisplay = 0 (off). The live game ini has no such key, so the code default 0 is what runs. Offline padviz harness on this base: ALL PASSED.

Meter ini: [Practice] FrameMeter, and FrameMeterY = -1 means anchored above the player HUD.
