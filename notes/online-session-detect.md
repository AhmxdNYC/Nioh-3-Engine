# Online / co-op session detection (static analysis, 2026-10-08, game not run)

v2.02, image base 0x7FF74F410000, all RVAs relative. Source: script natives Online::* (notes\script-bindings.csv) -> their impls, decompiled via decomp_ro. Names: re\symbols\incoming\online-renames.tsv. Confidence: medium-high on structure, runtime values (mode numbering for 0/1/2) not dumped live.

## Signals, ranked

| # | Signal | Read | Reliability |
|---|---|---|---|
| 1 | **Net manager "connected" byte** | `net = *(u64*)(base+0x45C84F8); connected = net && *(u8*)(net+0x1DC)` | Best. Session-wide (hub and mission). Exactly the gate the engine uses for `Online::IsPlayerGuest/IsPlayerHost` (impl 0x2853D4). Non-null only while the online module object exists; cleared to 0 by the dtor 0x2873288. |
| 2 | **Play-mode object mode word** | `pm = *(u64*)(base+0x4709B50); mode = pm ? *(i32*)(pm+0x34) : 0` | Very good, mission-scoped. `mode==1` online play (`Online::IsOnlinePlay` 0x85834 = mode==1 \|\| mode==2), `mode==2` Tokoyo (Crucible) co-op mode (0x85948 = `Online::IsTokoyoCoopMode`), 0/null = solo. Object created per mission by 0x1C2C050, freed by 0x1C31AB8, so null in hub/menus (use #1 there). |
| 3 | Host flag | `*(u8*)(net+0x2C8)` (0x5A9028) | Only meaningful when #1 is set: connected && !host = guest (`IsPlayerGuest`), connected && host = host (`IsPlayerHost` = !guest). Do not use alone: nonzero host flag with no connection is not defined. |
| 4 | `*(i32*)(pm+0x30)` (0x85800) | secondary field of the play-mode object | Unknown meaning (a type/sub-mode). Log it once; do not gate on it. |
| 5 | Coop::IsCooperating / IsDissolution (0x860FE0 / 0x860D00) | per-character-id flag lookup in hash tables inside g_CoopMgr (`*(u64*)(base+0x4751848)`, +8 -> tables at +0x9B8/0x9C8/0x9E0) | NOT a session signal. It asks "is character X in a cooperation state" (NPC ally/event coop included). Skip. |
| 6 | Mission::IsMultiplayProhibitPeriod (0x23A4DAC wrapper), World::*MultiplayInterruptMainStreamReturn | script-level windows where summons are blocked/pending | Informational only. A pending multiplay interrupt return means a session is about to change state. |

Not found: no native or global named for expedition, ghost/grave (bloodstain) or matchmaking in the 1072 bindings (grep Net/Multi/Grave/Ghost/Expedition/Match = only the Online/Coop/Mission/World hits above). Those features, if present, run under the Online module (KTGL_ONLINE_* strings, COnlineModule at 0x38F6D40 string) and so set #1. Treat any "can't read" as online.

## Cheap per-frame check from the ASI (no calls, no locks)

```
bool online() {
  auto net = *(uint8_t**)(base + 0x45C84F8);          // may be null
  if (net && net[0x1DC]) return true;                 // connected: host or guest
  auto pm  = *(uint8_t**)(base + 0x4709B50);          // null outside a mission
  if (pm && *(int32_t*)(pm + 0x34) != 0) return true; // online / Tokoyo coop play mode
  return false;
}
```
Two pointer loads and two compares; wrap in SEH (__try) because both pointers are freed/replaced on transitions (0x12E2438 sets net, 0x2873288 clears it). Prefer "disarm if true OR any exception". Latch: once online() is true, stay disarmed until the user returns to solo and a new mission load (mode 0 and net null for N frames) so a brief flicker cannot re-arm mid-session. Alternative: call the engine's own `0x85834` (IsOnlinePlay, no args, returns al) - same logic as signal 2 plus mode checks, safe to call from the game thread only.

Both globals are plain data RVAs (.data), so no pattern is needed beyond the usual build-timestamp guard (kBuildTimestamp 0x6AA7C106 in patches_gen.h); re-verify with `x:0x4709b50` / `x:0x45c84f8` after a patch.

## Does the existing mod match?

No. `grep -i "online|coop|session|multiplay|network"` over mod\Nioh3EngineFix\src finds no detection code. The only mention is menu.cpp:813, a tooltip saying the smoother-frame-pacing option is "Offline only: OFF for online co-op" - that is user text, nothing enforces it. Everything else matching "session" is unrelated (per-run flags). So the mod currently has no online gate; the check above would be the first. Recommend wiring it for frame pacing too, not just boss rematch.

## Other writers / reference points
- 0x12E2438 sets g_NetMgr (0x45C84F8) = object; 0x2873288 destroys it and zeros the global. Single writer each, so the global is a reliable lifetime indicator.
- 0x1C2C050 / 0x1C31AB8 set/clear the play-mode object (0x4709B50) with its length/guard mirror at 0x4709B58.
- 0x85A78: returns coop slot count-1 (via 0x287B2A0 params) when connected, else a mission-side value from `*(0x4751860)+0x7314`; a bonus "number of players" readout (-1 solo with no data).
