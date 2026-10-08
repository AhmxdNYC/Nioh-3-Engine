# Boss arenas: placement -> arena trigger -> script (static, 2026-10-08; game not run)

Companion to `notes\boss-fights.md` / `boss-fights-table.csv`. Data: `notes\boss-arenas.csv` (long form: script, kind, id, call, extra; 1144 rows, 104 boss scripts).

## 1. Where arena data lives (finding)
- **There is no fog-gate / mist-wall / trigger-volume record in any extracted data.** `data\extract\{stage,root,system}` hold only Octree/Landscape/ProcPlacement(foliage) DBs per stage (`data\stage_dbmap.tsv`); a byte search for a known boss placement id (58780), box id (2118570) and gadget hash (0x733e2361) found nothing real in them (the 2 hits are chance matches in 50 MB / 13 KB blobs). The placement records (enemy, gadget, PlacementBox) are in ObjectDatabaseFiles not extracted (root has 17,038 ObjectDatabaseFile entries in `data\rdb_list.tsv`) and are keyed by the ids below.
- **The arena is defined by the mission script**, not by a gadget type: the script names the ids as constant call arguments (decoder `scratch\dec_a.py`; args = the `29 imm32` pushes before a `0x1b/0x1f` call; only constants recovered, locals `vN` are not).
- Id kinds seen (all are placement ids resolved against the stage's placement DB):
  - **PlacementBox id** (trigger volume, small int, e.g. 2118570): `Player::IsEnterPlacementBox[All|Any|HostOnly](box)`, `Character::IsEnterPlacementBox*`, `Function::Wait_InvadeBox*`, `Function::Loop_*_EnterPlacementBoxAny*`. This is the "player entered the arena" trigger. 203 distinct (script,call,id) rows in 39 scripts.
  - **Gadget placement id** (u32 hash, shown hex; fog/boss doors/clear gadget): `Gadget::GetStatusPlacement(h, status)`, `SetStatusPlacement`, `SetReplacementStatus[Placement]`, `SetDoorControl{Normal,AbsoluteClose,LockKannuki,LockKeyItem,GameOverReset}(h)`, `Function::WaitClearGadgetAccess(10, gadgetId)`, `Gadget::IsInteractionPlayerPlacement(h, n)`. 86 scripts. Door gadgets seen by name: `set_bak_hnj_BOSS_PALLT_GADGET_A`, `set_sen_htmt_csl_boss_door_01_PALLT_GADGET_A`. `SetDoorControlGameOverReset` is the reset-on-death door hook, so a gate re-arm on retry already exists in the game: reuse it.
  - **Boss placement id** (`Duel::GetBossPlacementID(id)`, 5-digit like 58780/18870/17575): 24 scripts have constants (36 rows). Other scripts pass it via a local.
  - **EnemyArea id** (`EnemyArea::IsClearEnemyArea/ActiveEnemyArea`): only 5 constant uses; wave-style fights.
  - Mission flags (`Mission::SetMissionFlag/IsMissionFlag`, id): in the CSV as `flag` rows; the arena script sets its progress/defeat flags here (same as `flags_all` in the boss table).
- Script event chain (from boss-fights.md section 3, now with the ids): box enter (`Wait_InvadeBox(box)`) -> gadget door `SetDoorControlAbsoluteClose(gate)` / `SetReplacementStatusPlacement(gate)` -> `Character::Create*(bossPlacement)` -> `Mission::BossBattleMode` -> on death `SetDoorControlNormal(gate)` + `CreateClearGadgetBossPosition` + `SetMissionFlag`.

## 2. Rematch / re-arm implications
1. Re-arming the gate = put the gate gadget back in its pre-fight status: for scripts using `SetDoorControl*`, call the same `SetDoorControlNormal`/`AbsoluteClose` path with that gadget hash; `Gadget::SetReplacementStatusPlacement(h)` and `SetStatusPlacement(h,n)` rows give the status numbers the script uses (CSV `extra` column).
2. Re-trigger = player outside the box and the arena progress flag cleared (the `flag` rows nearest the box row for that script); the script loops on `Wait_InvadeBox` / `Player::IsEnterPlacementBox`.
3. Bosses are pre-placed actors (boss-fights.md): after re-arm call `Character::ResetReceivedDead` (0x1180940) then `Character::Create` (0x1180740) with the boss placement id (CSV `boss` / `create` rows).
4. For the ~65 scripts with no constant boss id, log `Duel::GetBossPlacementID` and `Wait_InvadeBox*` at runtime during one real fight (hook-only step, as in boss-fights.md section 9).

## 3. Per-script summary
Columns: script (62-hex file prefix in `data\extract\lfm\archive_09`), stage (from string pool, often empty), boss placement ids, PlacementBox ids, gadget placement ids (hex hash or small id; truncated), EnemyArea ids. Only scripts with a boss id or box id are listed; the other scripts' gadget-only rows are in the CSV. Caveat: the call list was filtered by dropping (call,args) signatures appearing in >= 30 scripts (shared library), so a few real shared values may be missing; arg attribution is constants-only. Medium confidence; ids not yet matched to boss names.

| script | stage | boss placement | PlacementBox | gadgets | EnemyArea |
|---|---|---|---|---|---|
| 0F72DF1DECEF | - | - | 2118570;2113380 | 733e2361 | - |
| 23B11AD2E77F | - | 58780;18870;17575 | - | - | - |
| 45C15A1FFB1A | - | - | 8061822;8063756 | f13ab70b;c0faeaaa;811a3f01;2ccc9226;6c54ef15 | - |
| 63742543AEB8 | - | - | 4070877 | f54a90c5;3a84e849;003e3148;dbd0227b | - |
| 663667288B5D | - | 34449;32468;65218;16889;24284;63339 | 576641;573270;578918;579269;577770;573194;579566;578226 | 8ed914f3;ffc6fb07;b972db45;c5a17fb3;c842538a;4a4cfd4c;224b1869;924c5ba | - |
| 6C05ACE2DDE5 | - | - | 152623;156946;156207;150212;154246;158944;153275;153403;1554 | 478f85db;93dc3c17;3b5815d8;ec606256 | - |
| 70D08AB4D345 | - | 10277 | 164670;165163;167571;161796;169555;166210 | 42;b5a6e88d | - |
| 9B7E22C510B1 | - | 12572 | 8038772;8030241;8037046 | 007aa187;84ddaa8e | - |
| C33DF13FF6D2 | - | - | 355806 | 0fa78af6;000564e4 | - |
| D38178C9B28E | - | - | 4091750;4092678 | b023d7cc;450f2511;e1469489;c3d27090;9d597fcb;003e87ab;9 | - |
| EC2CEC687176 | - | 2884 | 177825;173251;176560;170371;175379 | 0fa78af6;0005a8c4;0005c1a8;00053b2d;000548fb | - |
| C34725FB97C6 | H01_RETURN_NEKOMATA;H02_FIRST_NEKOMATA;H02_LAST_NEKOMATA;H02_SECON_NEKOMATA | - | 242955 | a9aeedbb;416b8f9e;0003af82;0003cc8c;0003c6f5;0003cf70;db7123d9;9959f67 | - |
| A5A9F2211EC6 | H02_WORLD_SENGOKU_3;S01_SEN_03_KOZOKU;S01_SEN_03_MATSUNAMIKI | 3895 | 48179;43808;49759;43024;44673;46927;46148;49546;47797;49550 | 9a52d3b8;972a55a9;00002e3a | - |
| C217B8C15062 | H06_WORLD_BAKU_3 | 31546;6985 | 4035254;4038608;4030078;4030664;4035046;10;11;4039765;403770 | 78a66889;29fee10c;7685cf0b;76573742 | - |
| E7923507DECC | S00_EDO_ALL | - | 5031262 | 001e8cd1;4da0fd87 | - |
| 679CBE96AA72 | S01_SEN_01_HAIDERA | 24086;60158 | 28455 | 41;dd45750d;d41388ca;186370e8;e1618a34;9cd47cc0;2ce49180;35d3261e;3975 | - |
| F54D0C2234EF | S01_SEN_01_MIKATA | 101 | 92116;95127;94751;90939;92764;99687;90954;98869;90310;902;90 | 42d9c34b;8a4d02ec;de99dfcf;88bebfea;e6219428;11 | - |
| 0B43EF696A5E | S01_SEN_02_BASE;S01_SEN_02_FUTAMATA | 6134 | 1047905;1043843 | 2003;7b2fcef4;2002;f5203b88;bd7e6027;dd792785;bb6c8d64;000ffb77 | - |
| C65DE6D9C736 | S01_SEN_02_BASE;S01_SEN_02_FUTAMATA | - | 87530 | 949ea699;f6f9bd78;000148fc;d52e0875;b3216e54;9114d433;6f083a12 | - |
| 6F987ACEDA8F | S01_SEN_03_SAIGA | - | 103199 | 7222beb2;06a5ec15;0a56fbd6;97379d50;827ce0bd;ba6dbbdc | - |
| D2DF2627F6F3 | S01_SEN_03_SAIGA;S01_SEN_04_HITOKOTO;S01_SEN_04_YOUGASHIDOU;S02_HEI_02_MATUOTAISYA;S02_HEI_04_SLUM02;S02_HEI_04_YASAKA;S04_BAK_01_GOSHO;S90_DLC1_03_EDOJO | 65535 | - | f3e07cd1;762b31b0;f875e68f;af1c4f2f;92480270;10925836;23460bf4;58e1e5a | - |
| F02A826143C9 | S01_SEN_04_HAMAMATSU_JOU | - | 1077149;1075403;1072821;1076871 | dce5b9ca;4c81f4ec;905ecbcb;c49a269b | - |
| D7C672847B77 | S01_SEN_04_HAMAMATSU_JOU;S01_SEN_04_SYUKUBA | 50430;22093 | 56529;50896;55223;50231;57935;57766;57927;57593;51035;53954; | 4c81f4ec;8c0140a9;905ecbcb;dce5b9ca;98379c56;2 | - |
| 8008F3E2B14D | S01_SEN_04_HITOKOTO | - | 66739;68988;69062;61565;67555;60007;61840 | - | - |
| 48C62C56A110 | S01_SEN_04_YOUGASHIDOU | - | 206651;206240;208062;209372;206837;207301 | - | - |
| 6B75B82A1250 | S01_SEN_05_HORIE;S01_SEN_05_JUJIRO | 49365 | 322559;329319;329018;326307;10;11 | 53802e50;ab0a588a;22cabc17;8c0140a9;905ecbcb;dce5b9ca;716d34b7;0c570e2 | - |
| 6128FD5B8530 | S01_SEN_05_SANDOU | 14764 | 35204;38222;34495;32357;31915;39903 | - | - |
| B6C6854EBF81 | S02_HEI_01_CHIKURIN_EAST;S02_HEI_01_KAMOSHI | 29179;25523 | 141322 | d40ebb26;6438c1df;9e09a0ab | 29179;25523;56721 |
| 8EDB00EEC40D | S02_HEI_01_KURAMA | 23378;61523 | 332700;339755;333128;332494 | 201b6bfd | - |
| D0BBC1DE5287 | S02_HEI_02_MATUOTAISYA | - | 3402;343709;3401 | 93dc3c17;000532d5 | - |
| 6D14B07C9687 | S02_HEI_03_DAIDAIRI | - | 376384;377018;374286;3705;3704 | 204f05b0;31f410e8;b6485b58;24cf6c99;93567dda;01dd8f1b;00004e20;6e510ec | - |
| 5DD0CAD1494A | S02_HEI_04_RAJOMON | 901 | - | 50ada7a4;bf34b8e5;0636982d;14c47670;e3d9e5dc;0010b9fb | - |
| 5A3A552EA629 | S03_KOD_01_REIJU | - | 593677;598601;590029;597713;595207;594956;591906;595499;5903 | ebef5377;e7d9d243;cd852ec4;c28b3548;000912cd;10;4202;0cf4aa63;5bf294e0 | - |
| 586F1EC37255 | S04_BAK_01_HORIKAWA | - | 4019945;4019512;4015707 | 8c6f8d8d;f9dc4aac | - |
| 123ABCEA0B98 | S04_BAK_02_WORLD;S04_BAK_03_GOJO;S04_BAK_03_HIGASHIYAMA | 29179 | 4028888;4022765;4027129;4029341;4029573;4026788;4021575 | 46c590a5;a5df48a0;7ccdba9c;f8fe0516;425c53f6;84738155;c6d6e25a;06c947a | 63442;50368 |
| ABF937CF4D48 | S04_BAK_03_ABURAKOJI | - | -1721531664 | e79bfca5;17e6444d;003e1143 | - |
| 526F7C4E6488 | S04_BAK_03_HIGASHIYAMA;S04_BAK_04_BASE;S04_BAK_04_HOKANJI;S04_BAK_04_WORLD | 42855 | - | 48;b583270f;ea65194d;04d6126c;1f470b8b;3ff3c1e5;42331d00;9aa21cfc;b0ba | - |
| 4876530486F4 | S04_BAK_03_HONNOJI | 400 | - | dc375767;b83eb348;bca24480;003e5d97 | - |
| 5F2E0518FFC8 | S04_BAK_03_HONNOJI | 52697 | - | 3fc6f49f;198d7c48;bca24480 | - |
| 5E29D52DC42F | S04_BAK_03_NISHIHONGANJI | - | 4054775;4050337 | 29fee10c;7685cf0b;dd77f30d;76573742;003de6c3 | - |
| B2074F87F0F2 | S04_BAK_03_SHIMABARAYUKAKU | 23358 | 1138784;1130093;1136570 | 29fee10c;84dc28ec;7685cf0b;dd77f30d;4fe29cf2;7c9a98f4;ebc288e6;7a426f4 | - |
| 0CA8409CACB7 | S90_DLC1_00_COMMON;S90_DLC1_01_HONGO;S90_DLC1_01_UENO | - | 8017096 | f26e1b27;ece6e91b;fdd6bd54;19e8a724 | - |
| 057875E8B246 | S90_DLC1_02_HAIKYO | 6308 | - | bfabb92f;10 | - |
| B023D29AFD54 | S90_DLC1_02_HAIKYO | 6308 | 8023437;8024155;8200;8024999;8026857;8021680;8024944;8024582 | 46bbf7db;14b9549d;2dbaa63c;bfe86668;bfabb92f;16c29c51;e1ed25a8 | - |
| D309DF60D428 | S90_DLC1_03_EDOJO;S90_DLC1_03_OGAWAMACHI | - | 2142750;2141675;2149734;2149139 | - | - |
| 2B59B8417AAE | S90_DLC1_04_KANDA | - | 8077382;8076324;8078497;8076797;8079624 | 272de6bd;61f0e9e6;007b3044;54;17759973;a831e2b1 | - |
