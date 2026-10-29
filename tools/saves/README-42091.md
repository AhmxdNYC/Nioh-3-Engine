# Live title import helper (42091)

## Updated acceptance requirement (human via pc 42175)

The title screen must display the selected save's character after switching,
inside the same running process. Importing a slot and asking the player to use
Load is not sufficient acceptance. This helper is only the file-import component;
it does not refresh the title model or complete the requested live switch.

Remaining integration needs a verified native load of the selected profile and
a verified rebuild of the title/profile display. Do not assume an op3 request
selects the imported profile: it chooses the newest savedAt. Do not call the
deserializer directly or assume completion alone rebuilds the title model.
Updated static evidence from pc 42229, branch `pc/title-refresh-trace-42179`
commit `857dc322`, corrects the earlier 1572AA0 candidate: that was a VA/RVA
conversion error. The real call, 2162AA0, strips unowned DLC items and must not
be treated as a display-refresh API. Title character 100 is created once per
TitleMode by 1C2EE50, after the newest-profile load. Continue/Load do not rebuild
it. The trace proposes the native soft-reset request 1C205A4 on the game thread,
which schedules a new TitleMode, load and model rebuild without a process exit.
Stack teardown and native acceptance remain unproven; pc2 owns integration.
The chosen import must already be strictly newest or require a separately
validated header timestamp change. This helper does not restamp saves or issue
soft-reset calls. pc owns runtime proof: selected character appears at title
without exiting, followed by correct Continue/Load and autosave with no model
duplication across repeated resets. See `notes/title-refresh-trace.md` on pc's
trace branch for the exact call chain and native test plan.

Branch `cx-pc/live-title-42091`, based on named-save picker `b014da5c`.
Uses Plan A from `pc/save-reload-trace-42007` (`4a2eb159`). Offline only;
pc owns native acceptance and installed saves. This is a separate helper, not
a replacement for the after-exit helper or a claim that a profile is loaded.

Build with `py tools/saves/build_live_package.py`. Integration command:

```text
python.exe SaveSlots-live-title.pyz --sandbox LIBRARY --save-root COPIED_ROOT --slot "Yoritomo Practice" --token UNIQUE_NONCE --pid ORIGIN_PID --created ORIGIN_CREATION_FILETIME
```

For a snapshot containing multiple user profiles, add `--source-slot SAVEDATA00`
(or the desired SAVEDATAnn). Ambiguous source selection is refused. The supplied
root contains `Savedata/<17-digit-account>/...` and must be disjoint from LIBRARY.
Source snapshots use the existing verified immutable slot manifest. Library
and target identities must agree, as must the encrypted header account.

The helper checks the original executable name/lifetime and supported top-mode
signature through read-only Windows process handles. It requires the established
state-3 title gate (state 2 remains denied), empty mode queue, no loading/system
message, nonpending save completion, worker not busy and null backend. Samples
are doubled and must have a stable identity for at least one second. Gates are
rechecked after expensive validation/backup and directly before publication.

It decrypts/checks the selected SAVEDATA.BIN (size, RNNUSR, account, body version,
body checksum). It backs up the entire root, including system data, settings and
Savedata_backup, with hashes. The imported pair contains two identical copies
of that validated file; an obsolete snapshot BACKUP.BIN is not propagated.

The single mutation is a Windows no-replace directory move into the first free
SAVEDATA00..49. Staging is a unique sibling of the save root, invisible to the
game's slot enumerator. Existing user slots, SYSTEMSAVEDATA00, timestamps and
Savedata_backup are unchanged. Full libraries are refused. Every import consumes
one free slot. The player must use **Load Game** and select the imported profile;
Continue selects the newest timestamp and is not promised to select this import.

Receipts: `LIBRARY/request-TOKEN/receipt.json` and retained transaction directory.
Success says `state=imported`, `loaded=false`, target, backup and Load instruction.
Cancellation: create `LIBRARY/cancel-TOKEN` before publication. Never terminate a
helper to undo a completed import. It never calls game code, closes the game or
modifies memory. Native load/character/autosave confirmation is a separate step.

There is no game-thread exclusion lease: read-only gate sampling cannot eliminate
the final sample/publication race. Atomic addition protects existing slots and
prevents partial-file reads. A transition racing publication may miss the new
slot in that enumeration; return to idle title and choose Load again. If the game
creates that target first, no-replace publication fails. After successful
publication, gate loss is recorded but the slot is never removed underneath a
possible native load. Receipt write/crash uncertainty is resolved by inspecting
the prepared target/stage journal; do not blindly retry or delete imported saves.

Menu integration is deliberately separate from the concurrent final-build menu
merge. Add an explicit "Import at title, then Load" action and a separate helper
path. Do not point the existing after-exit Helper setting at this pyz: its backup
and quit semantics are different. Pass a fresh nonce and process creation time,
display the receipt target and Load instruction, and keep after-exit backups as
the whole-root/system-file option. Do not label an import as a completed switch.

Native acceptance for pc: disposable root first, then backed-up same-account test
profiles; title-only admission, busy/dialog/cancel refusal, same PID, correct Load
selection/character, autosave targets imported slot, original/system files stay
unchanged. Test multiple source slots, occupied target and all 50 slots full.
Proton publication is not accepted yet. Runtime UI/Windows API behavior remains
unproven by the offline tests.

Offline validation on PC (2026-10-28): 22 new tests PASS (69.6 s), including
full-size encrypted synthetic user-save roundtrip/checksum rejection, gate
failure at every prepublication boundary, post-publication gate loss, target
creation races including an empty directory, mandatory backup failure,
multi-profile selection, cancellation and nonce replay. Existing 83 save tests
PASS (39.6 s). Run from `tools/saves`: `py -m unittest test_live_title -v` and
`py -m unittest test_after_exit test_real_root test_saveslot -q`.
Packaged CLI `--help` smoke PASS. Bridge artifact 3120, SHA256
`ea28d277dbc267ea837836facabe814a4c93cf3de6aa99d8e3fc58d8ee22263a`.
No game, installed mods or actual save files were accessed.

## Native acceptance on pc, 2026-10-28 (branch pc2/live-title-menu)

EF built with MSVC from this branch: SHA256 `5f3b14b53dd33d161ecd870d8fcedb8edde459e4191974eb6cfbd4e76c9246cd`.
Helper pyz `7ce28f774b62e7ee7971511a0da1b9cba6e572875ee63455eff5f6077166d662`.
Test root contained copies only: A = current save in SAVEDATA00 (newest), B = an older second-slot
copy in SAVEDATA01. Library: Char A, Char B, Tampered B (body byte flipped). The original save folder
and installed files were restored byte for byte afterwards.

- Boot: title showed A, `S+0x28=0`.
- Import Char B: the helper exited 0 after about 24 s and published SAVEDATA02, restamped. `1C205A4` was called once.
  Countdown 40 -> 0, then a new TitleMode loaded with `S+0x28=2`. The title showed B (a different character) in the same PID.
  Continue then entered B's game (prologue). **Restamped headers are accepted.**
- Bug fixed here: the EF menu pauses the game, so the reset countdown stayed frozen until the menu closed.
  The menu now closes when the reset is armed (`savefiles::TakeMenuCloseRequest`).
- 3 repeats in one PID: B -> A -> B, slots 2, 3, 4. Handles 3211 -> 3218 -> 3213 -> 3213. Private bytes 14291 -> 14414 -> 14421 -> 14417 MB.
  The reset lands on PRESS ANY BUTTON, then the title shows the new character.
- Tampered library slot: the helper refused it (checksum, exit 1). No slot was published, no reset fired, and the title stayed usable.
- Corrupt newest file on disk at boot: the game showed "Unable to open the saved data because the file is corrupted" (`S+0x41=1`).
  After confirming, the title was usable (player hidden, Continue disabled).
  A live import from that degraded title was refused: the helper reported "Title/save state changed during sample" because the
  gate flickers on the alternate idle scene. The refusal is safe, but recovery there needs Load Game or a restart.
- Fixes in 0551f411: the validator version offset (+0x158) previously refused every real save, and the cancel-path escape.
