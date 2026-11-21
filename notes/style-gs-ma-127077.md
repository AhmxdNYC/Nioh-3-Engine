# Style meter: runtime marker for guardian spirit skills and martial arts (#127077 / #127214)

pc/1 Opus 5.5, 2026-11-21 ~5:50 am EST. Static RE only (re/Nioh3.dump.exe, RVA = file offset; capstone), game text
D:/nioh3-re/text_en.tsv, Training log of TR 243d616c. No game run, no edit to pc1/style-bonus-126859.

## Sample (Training log, Main PC, Yoritomo Practice)
- 04:21:34.886 `STYLE ACT act 17027 (0x4283)` from idle 2000, pad X+RT; 8 hit windows / 8 landed, 2254 ms.
  Player Samurai moves are 25xxx, Ninja 1xxx, ninjutsu 34xxx. Id ranges are NOT used as the rule (they are per weapon).

## Guardian spirit skill: Spirit Force gauge spend (universal, like the ninjutsu gauge rule #126854)
- Game text 67638815: "Use Spirit Force to activate a Guardian Spirit Skill. Spirit Force is gained by landing attacks".
  13/57735865: "Reduces Spirit Force consumed when using a Guardian Spirit Skill". So every GS skill spends this gauge.
- Gauge = player status `st = [actor+0x3A0]` (same st as Ki +0xAC and ninjutsu +0x36AC), block `G = st+0x1D8`:
  - `Player::RecoverGuardianAction` 0x23AAC1C: `0x916FAC(st+0x1D8, max = [st+0x3AC])` (refill to max).
  - Slot pick 0x377D30(G): if `[[G+8]+0x188] & 2` and `idx = [G+0x218] != 4` -> `G+0x38 + idx*0x70`, else `G+0x1D0`
    (= st+0x3A8). The picked entry is `{float cur, float max}`; setter 0x916FF0 writes `[entry] = value` (clamped).
  - Default/no-slot case: **cur = float st+0x3A8, max = float st+0x3AC**.
  - Ki pulse feeds the same block: 0x2363151 calls 0x1ABB86C([status+0x1D8]) (notes/ki-perfect-0x2000-consumer-36516.md).
  - Distinct from the Amrita gauge (`Player::FillAmritaGauge` 0x23A8188 uses st+0x148) and the Samurai Arts gauge
    (`FullSamuraiReinforceGauge` 0x23A83EC: cur st+0x3748, max st+0x374C).
- **Rule:** at a new player move (STYLE ACT change), read cur via the 0x377D30 pick (or st+0x3A8 when the flag/idx says
  none); if cur fell by >= ~1 within SM_MOVE window of the move start (not a refill/reset), the move is a Guardian
  Spirit Skill -> "Guardian Spirit +X". One per move run (same de-dupe as SmNinGate). No act ids.
- Caveat: the Training Ground native rules refill Amrita only; our Practice keeps Ki/HP/ninjutsu infinite, not Spirit
  Force, so the drop is visible. If a later Practice option refills Spirit Force, sample before the refill.
- Proof still needed live: log cur/max of the picked entry around act 17027 (one GS cast, expect a drop at move start).

## Martial art: NOT a gauge
- Game text: MA spend **Ki** (11/48329267 "Reduces Ki consumed by Martial Arts"), like normal attacks -> Ki drop is not
  a marker. The Arts Gauge (st+0x3748) is charged by attacks/guards and only consumed when FULL ("using a Strong Attack
  or Martial Art will activate Arts Proficiency", 12/51541462) -> also not a marker (heavy attacks use it too).
- MA are learned skills (item rows 1200000000, "Arts Customization"). Internal HasSkill = 0x555EB8 (called from
  `Player::HasSkill` 0x23A8D18; 185 callers). The universal marker must come from the skill -> action link:
  candidate rule = "move id is an action of an equipped Martial Art skill of the current weapon".
- Open (next RE step, not done): find the equipped MA slot list in player data and the MA skill record field that names
  its action id (or the transition-key condition that gates MA keys: hold-MA key K1 has cond 0x5A3 via vtable +0x138
  = 0xF6878, notes/hold-re-120730.md). Until then the only universal runtime cue is the input: Samurai MA = RB held
  + face button at move start (STYLE MOVE pad column), weaker than a data field.

## Martial art marker found: action-row skill word (#127295, pc/0 Opus 5.5, 2026-11-21 ~6 am EST, static + Training log only)
- The game's own test: hit flag `Collision::Damage::Hit::IsAttackAttackSkill` (0x23C7B18, `[hit+0x13C] & 0x40`) is filled from
  state-test 0x2E9948 **cond 0x2F** (notes/yoritomo-hit-source-22305.md). Resolved with tools/n3condcase.py: case at 0x2EA15B reads
  `dword [R+0xDC]` of the current action's 336-byte file row `R = [[ctrl+0xE8]+0x20]`: true when bit 31 is clear and the low u16 is
  not 0xBA71 (else 0x2EA17A returns 0). So +0xDC is a per-action skill word (FFFFFFFF = no skill).
- Extracted Samurai weapon files (archive_01, all files with 25xxx ids): 2453 rows carry the word; 1838 of them are motion 5000-5999,
  and 1838 of the 1958 motion-5xxx rows carry it (94%). Basic chains never do: 25000-25013, 25036 (X light), 25040 (Y heavy) are
  FFFFFFFF in every file. GS skill 17027 (word 0x4DA8, file 18/F31C...) and ninjutsu 34052 (0x714C) carry it too, ninjutsu 34000 not.
- Rule (mod/Nioh3Training/src/style_meter.h SmSkillRow / SmMaStep / SmMaPay, glue SmSkillWord): a move whose row has the word (and
  `[P] == act`) and that is not a guardian spirit move (Spirit Force drop) or a ninjutsu move (gauge spend) is a Martial Art:
  MartialArtPts (unset = NinjutsuPts, 20) once per move on its first scored points, pop-up `Martial Art <move pts> +20`. No act ids,
  no input. STYLE MOVE logs `| skill 0x<word>[ martial art]` or `| skill -`; STYLE EVENT logs `skill row` / `martial art +X`.
- Log of 04:19-04:33 (TR 243d616c) cannot say which file is the equipped weapon, so per-move proof is still live: words differ per
  weapon file (25514 = 0xC9CA in 70/C3, 0x9BD7 in F8, none in 55/79/97/B4). The RB-held moves 25005/25067/25129 (no attack) have no
  word in any file; 25243/25298/25301/25513/25514/25376/25283 carry it in most files.
- Doubts: (1) "AttackSkill" may also cover learned non-MA skill moves (word 0xCEC4 sits on 25298/25301/25358/25376 across files,
  motion 2xxx-4xxx); (2) a GS skill with Spirit Force unreadable would score as MA; (3) the word is not tied to the equipped-MA list
  (HasSkill 0x555EB8) - it is static per action. Live check: one round of MA + normal chains, read the STYLE MOVE skill column.
