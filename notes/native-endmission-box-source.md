# Native 'This will end the mission. Proceed?' box: real source (static, in progress)

Offline/static only (re/Nioh3.dump.exe, RVAs; data/extract). Game not launched.

## Checkpoint 1
- Text id: 65061424 = 0x3E0C230 (g15) 'This will end the mission. Proceed?'. In the EXE it is an imm32 only in 12A1F80 / 1DAE4E0 / 1DB58B0
  (our hooks, never fired). In game data it appears in two archive_16 (gadget script) files:
  - lfm/archive_16/2A/319B314E61D7... (G_CLEAR_GADGET script, CLEAR_GADGET_WAIT_DECIDE/FINISH, Mission::IsTrainingMission) @0x2f5c
  - lfm/archive_16/3E/E97932260A44... (variant with is_from_training_and_play_end etc.) @0x3114 (+ 65591390 'Do you wish to end the mission?')
- So the box is opened by the clear gadget's own SCRIPT through the script native UI::OpenCommonWindowTwoButton (CommonWindow
  system), not by the pause-menu / ChoiceBox path. That is why 12A1F80 / 1DAE4E0 / 1DB58B0 / 348274 never fired.
