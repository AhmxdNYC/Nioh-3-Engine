# Nioh-series boss modding: community resources (web research, no tools run)

Caveat: Nexus Mods pages returned HTTP 403 to fetch, so details below come from
search snippets only. Nothing here was downloaded or executed. Verify before relying.

## Resources
| Resource | URL | Gives |
|---|---|---|
| Nioh 3 Cheat Engine tables (V3 equipment editor, V6 infinite item/soul core use) | https://www.nexusmods.com/nioh3/mods/148 ; https://vgtimes.ru/games/nioh-3/files/cheats/tables/ | Equipment/effect editing. No boss/mission scripts found. Its AOB/structure layout is still a lead for player/inventory structs. |
| Nioh 3 RDB Explorer | https://www.nexusmods.com/nioh3/mods/62 (likely; also 83/84/90 unconfirmed) | Browse/extract/modify .fdata / archive_xx.bin; search by filename, KTID (hex id), type name. Best route to enemy param tables and mission data. |
| Nioh 3 File Loading Logger (ASI) | nexusmods.com/nioh3/mods (one of 83/84/90) | Logs resource type, extension, KTID live as files load. Run during a boss mission to learn which KTIDs it uses. |
| Nioh 3 Loose File Loader | nexusmods.com/nioh3/mods (one of 82/83/84/90) | Loads overrides from <Nioh3>/mods via plugins/LooseFileLoader.dll; any deserializable asset type. Lets edited param/mission files replace originals with no repack. |
| Nioh3Fix (Lyall) | https://zff.dev/Lyall/Nioh3Fix | ASI-based fixes; shows the ASI loader works for Nioh 3. |
| Nioh 2 Enemy Randomizer | https://www.nexusmods.com/nioh2/mods/321 | Cheat Engine-driven; swaps enemies at mission load, option to allow bosses as enemies, seeded. Proof that spawn-table swap at load works. Needs "Nioh 2 Memory Patch" mod for multiplied spawns. |
| Nioh 2 Save Editor | https://github.com/alfizari/Nioh-2-Save-Editor | Edits stats/gear on PC saves; PS4 handled via Save Wizard-decrypted files then re-encrypted. Nioh 2 only, no Nioh 3 support noted, no encryption details documented. |
| n2soc (Rust) | https://github.com/tmstb/n2soc | Nioh 2 save ownership converter (import others' saves under your profile). Implies saves are bound to a user/account id. |
| Nexus Nioh 2 mods 626, 204, 671 | https://www.nexusmods.com/nioh2/mods/626 etc. | Save/loose-file/manager mods; not read. |

## Carry-over schemes
- Koei Tecmo "KT" engine: assets addressed by KTID, a 32-bit hex hash of the file/name (the "KIDS" scheme). Archives are RDB (.fdata + archive_xx.bin) in Nioh 3, as in Wo Long and Rise of the Ronin. Expect the same container and hashing across the family, but the hash inputs and type ids may differ per game.
- Enemy ids: no public range found. Plan to derive them by diffing mission RDB entries via RDB Explorer and the logger.
- Mission replay: no public tool found. Nioh 2's randomizer hooks mission load; Nioh 3 should be similar.
- Save encryption: nothing public for Nioh 3. Nioh 2 PC saves are editable with the editor above. PS4 needs Save Wizard decryption.

## Gaps and next steps
1. Open Nexus pages in a real browser to confirm mod numbers and any GitHub links for RDB Explorer, the logger and the loose loader.
2. Check GitHub for the RDB Explorer source (it likely documents the KTID hash and type enum).
3. Search Wo Long / Rise of the Ronin modding (CUE4Parse-like KT tools, "KTGR" / Koei Tecmo RDB extractors) for hash algorithms.
