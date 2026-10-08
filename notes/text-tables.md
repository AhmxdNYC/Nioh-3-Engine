# Text tables (LFM archive_17), static dump (2026-10-08)

Tool: `py -I re\scripts_re\text_dump.py` (read-only, stdlib) -> `D:\nioh3-re\text_en.tsv` (27,421 strings) and `text_ja.tsv` (29,416).
Columns: `group  id  lang  text`; backslash/TAB/CR/LF escaped as `\ \t \r \n`; empty and `^13~UNUSED~` slots skipped.
Reuses the format from `boss-names.md`: files `data\extract\lfm\archive_17\XX\<hash>`, records `{u32 id, u32 nchars+NUL, UTF-16LE}`,
39 text groups x 23 language files (+1 non-text 81,920 B file ignored). **group = id >> 22** (ids are `group<<22 | offset`; e.g. names 7662000 -> group 1).

Language is not in the file. Detection (heuristic, verified on samples): en = non-CJK/Cyrillic file with English-stopword ratio >0.1, most
non-empty strings (fallback for name-only groups: vocabulary hit rate learned from the other groups); ja = most kana. Several partial
"variant" English files exist (e.g. ~870 of 1280 names); the fullest is used. English is shorter than Japanese in some groups (genuinely
untranslated slots, e.g. g11 3465 vs 4497, g28 274 vs 402); groups 25,26,29,32,37,40,41 are 1-record stubs and are empty in en/ja.
Inline markup kept verbatim: `^01~CYAN~..`, `^05~STYLE_*~`, `^00~BUTTON~`, `{}`/`{0}` args, `/m$..$` voice cues, `<swGenderByHero(a,b)>`, `#\n` line breaks.

## Groups (label = judged from English samples; "?" = unsure)
| g | recs | what |
|---|---|---|
| 1 | 1280 | **names**: enemies/bosses/NPCs/player-cast (+ guardian spirits); colour-tagged. Key for char `name_text_id` |
| 2 | 559 | character-creator labels (JAW DEPTH, HEAD SIZE ...) |
| 3 | 2334 | armor/helmet/gauntlet etc. **item names + descriptions** (names and lore interleave) |
| 4 | 1417 | materials, consumables, key items ("A yokai material taken from a Gamatsuki") + craft guides |
| 5 | 1039 | **weapon** names + descriptions (kusarigama, nagamaki, bows) |
| 6 | 897 | options/settings menu text, language names |
| 7 | 756 | **missions**: mission titles, objectives, location names, mission synopses ("The Battle of Hitokoto Slope", "Defeat Yamagata Masakage") |
| 8 | 1802 | in-mission voiced dialogue (`/m$..$`) |
| 9 | 1148 | hub/multiplayer/clan/guardian-spirit UI and messages |
| 11 | 4552 | **skill** names + descriptions (Sword Control: Earth II), Soul-core/yokai ability, gear effects |
| 12 | 493 | martial-art/Shadow-Art move names and descriptions (Earthsplitter, Flying Buck) |
| 13 | 3084 | status/buff/set-bonus text, combat-log labels |
| 14 | 1119 | story cutscene dialogue (Takechiyo, Tokugawa) |
| 15 | 4189 | general UI, menus, shop, store, tutorial prompts, proficiency requirements |
| 16 | 498 | tutorial/hint popups with button tokens |
| 17 | 283 | guardian-spirit/perk (Life Siphon, Gust) names + effect text |
| 18 | 466 | NPC/ally battle barks |
| 19 | 394 | controller button/key labels, style names (Purification, Single Combat Challenge) |
| 20 | 776 | **titles / achievements** ("Yamagata Masakage Subjugator", "Defeat ... without getting hit") |
| 21 | 147 | proficiency/notification messages |
| 22 | 420 | item-UI strings (Drops, rarity, enhancement) |
| 23 | 37 | death-cause strings (Drowned, Committed harakiri) |
| 24 | 100 | emote names (Meditate, Play Shamisen) |
| 27 | 150 | story chapter summaries |
| 28 | 402 | stat/record labels ("Total damage dealt with spears", "Level during first completion of X") |
| 30 | 487 | side-quest text (investigate X) |
| 31 | 164 | skill/shadow-art effect descriptions (second skill set) |
| 33 | 21 | character bios (protagonist's grandfather ...) |
| 35 | 17 | loading-screen historical quotes |
| 36 | 666 | **encyclopedia / lore** entries per boss/yokai/person (long) |
| 38 | 663 | cutscene/NPC dialogue |
| 39 | 139 | help/tips/tutorial-mode text (Ninja Style, Soul Core) |
| 25,26,29,32,37,40,41 | 1 | stubs |

Not found as separate groups: scroll names (battle/picture scrolls) are in g20 (title text "Hundred Demon Realms Picture Scroll Admirer") and g15/g4; dedicated reward tables are numeric, not text.

## Boss index
`notes\boss-text-index.csv` (`py -I re\scripts_re\boss_text_index.py`): one row per boss atom from `boss-names-online.csv` and
`boss-per-mission-online.csv`. Columns: source,row,boss_text,atom,**name_ids** (g1, English exact; `;` for dupes),name_en,name_ja,mission_text,
**mission_name_ids** (g7 exact; per-mission csv only),**objective_ids** (g7, the `obj:` text),related_counts (hits per group of the boss name),
related (<=12 `group:id:text`). Unresolved atoms are non-names ("training partners", "unnamed", Okiku, Crucible Wraith ...).
Caveat: `related` is a substring match on the name, so lore/dialogue mentions are included; filter by group (3/5 gear, 4 materials, 20 titles, 28 stats).
