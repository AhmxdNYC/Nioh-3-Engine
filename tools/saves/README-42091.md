# Live title import helper (42091)

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
