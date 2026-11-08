# Crash upload context — #71062 / #71018

Final combined candidate on `cxl-pc/crash-combined-71133`, based on timeline `a7f27be0`, with module inventory `43f16045` cherry-picked before the context change. The earlier standalone branch/artifacts below are superseded. No game, installation, deployment or version bump.

## Payload

- `registers`: all 18 x64 context fields already recorded locally, encoded as hex strings (no JSON integer precision loss). Null without an exception context.
- `stack_top`: 16 eight-byte words starting at RSP; each read has its own SEH guard and unreadable words are null. No pointer chasing or string interpretation.
- `settings`: fixed-buffer, crash-time reads of Training/BossRematch INIs. Only explicit numeric overrides escape: Summon KiBreakLog/NoFinisher/ActFix and related act-fix, MoveBlock/fallback and no-death keys; Training aggro/repeat settings; per-boss Moves.<id> action overrides; Spirit.SoulCoreForms. ANSI/UTF8 BOM and UTF16 LE are supported. No paths or arbitrary keys/values. Missing keys are absent, not invented defaults.
- `settings.source=ini_at_crash_not_live_state`: the on-disk snapshot does not claim to be unsaved live state. Stance changes are runtime-only, available through Training events rather than a nonexistent INI setting.
- `log.Training`: newest 20 lines, chronological, up to 2047 bytes each (the normal Training logger formats into 1024 bytes). Existing 120-character `last[].fields.line` remains compatible; full lines are in log.Training.
- `training_tail` reports captured/included counts and line cap. If escaping/other data exceeds 65536 bytes, discard optional other tails first, then retain as many newest full Training lines as fit. Registers/settings stay in every attempt. Settings capped at 64 entries / 16 KiB; per-file input 64 KiB, with truncation flag.
- No new heap allocation, STL, profile API or synchronization in the crash path: static buffers, bounded parsing, Win32 file reads, existing JSON writer/scrubber. Existing crash-handler limitations (corrupt process/stack, blocked file I/O, concurrent crashes) are not solved by this change.

## Validation and artifacts

- `build.bat`: exit 0, portable MSVC, existing warnings. EF SHA256 `ba4a21661af47a2561c6724e60d2fb321e9f6b75cc770bd0b65978bb28f120ef`, bridge **#5340**, build log **#5341**.
- Extended `tests/crashupload_test.cpp`: record, badstack, null, utf16, stress all pass; client log **#5339**. Test executable and synthetic INIs/logs are isolated in `D:/nioh3-wt/crash-context-71062/offline-context`.
- Normal sample **20,441 bytes**, 20 long lines with complete `HP=125176/194938` suffix, known RDX preserved: JSON **#5338**. Bad-stack sample 20,053 bytes; null-context 19,498; UTF16 20,441. Escaped-log stress 59,594 bytes retains newest 5/20 lines, valid JSON, valid server schema.
- Extended `site/tests/crash_api_test.py`: local API 202 storage/repeat, backward-compatible old schema, privacy scrub, malformed registers/stack/settings/oversize lines rejected400, body cap413 and rate429 all pass; log **#5342**. All five context variants pass validation. No live endpoint contacted.
- Client compile: toolchain wrapper + `cl /EHsc /std:c++17 /MT /Feoffline-context/crashupload_test.exe /Fooffline-context/ mod/Nioh3EngineFix/tests/crashupload_test.cpp mod/Nioh3EngineFix/src/crashupload.cpp mod/Nioh3EngineFix/src/inputlog.cpp`. Run with isolated test.ini, CrashUpload=0, CrashSendDelayMs=600000, loopback CrashUrl. Server test from site/, with CRASH_INBOX pointing at an isolated directory and normal sample path.

## Integration

Deploy the API validator **before** the uploader: the old strict validator rejects the new top-level fields and long Training lines. Module inventory `43f16045` is included in the final branch, including modules/modules_available/modules_truncated and its API validation/issue summary. Context and inventory are both captured before payload construction. Current release remains 1.8.6. This is compile/offline validation only; real crash/in-game behavior remains unproven.

## Combined verification (#71133)

The combined runner `tests/run_crashupload_test.py` builds a dummy DLL, loads 70 copies after the normal snapshot, then exercises all five context modes with inventory enabled. Normal payload 23,581 bytes: 64 module entries (truncated=true), all 18 registers, 16 stack words, settings, and 20 complete Training lines. Bad stack 23,192; null context 22,637; UTF16 23,580; escaped-log stress 62,733 bytes retaining newest 5/20 full Training lines plus all 64 inventory entries. Every variant passes the combined validator. Localhost upload returns success and removes the pending file. Module privacy/exclusions, old-client schema, malformed fields, body cap and rate limit tests pass.

Run using the toolchain wrapper with `py mod/Nioh3EngineFix/tests/run_crashupload_test.py`. Optional CRASH_TEST_ARTIFACTS saves the five sample JSONs to an isolated folder. No site deployment or game install. No BossRematch/Training/Camera source changes relative to a7f27be0.
`nFinal combined build.bat exit0. EF SHA256 `1de96af57f58dcc0dac646da7f1b76957c1cc86e6e178c3560d75a8bc7c766e1`, ASI **#5350**, build log **#5351**, combined tests **#5346**, combined sample **#5347**. Supersedes standalone EFba4a2166/#5340.
