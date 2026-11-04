# HUD widget-ID sweep (#66108 / board #200), main PC 2026-11-04 4:20-5:08 pm EST

Diagnostic EF d9faf353 (#4799), raw ids 0..255 swept one by one in the field (screens + per-region diff vs OFF).
- Weapon slots (bottom-right weapon silhouettes + stance diamonds): **widget 10** (constructor RVA 0x1CD4688).
- Held soul cores (60/40 circles): **widget 13** (constructor RVA 0x1CD1338). It also hides the green guardian-spirit circle; the game has no cores-only widget (0..255 swept).
- Others seen: 3 = whole bottom status block (cores, spirit, bars, weapons, currency); 5 = HP/Ki/spirit bars; 11 = item shortcuts; 25 = minimap; 17/18/19/147 = bits of the bottom currency row. Old mapping 22/19 hid nothing requested.

Fix (pc/hud-ids-fix): kWeapons/kSoulCores now match the id-10 / id-13 constructors (id byte wildcarded, unique tails); tests expect 10/13. Build exit 0; real-image audit PASS (both unique, ids 10/13). EF sha 8795425d.
In game (EF 8795425d): log `weaponsId=10 soulCoresId=13`; Hide weapon slots hides only the weapons; Hide soul cores hides cores + spirit circle; both ON / both OFF restore OK. Cosmetic: `menu set` reply shows the redirect state from before the change.
Restored combined build EF 6618aa35 / BR 4e72ea64 / Cam c0751a35 / TR 1c73f8e6, inis byte-identical, saves 0 diffs. Evidence Temp/hud200 (named.jpg, sheet.jpg).
