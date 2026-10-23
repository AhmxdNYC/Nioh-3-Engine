
## 2026-10-14 14:34:49 session 598f117b-088d-4401-bbde-0a55dab1229c
I read Ally's result (#4014) and made no code change. I didn't send a bridge reply, because nothing needs an answer or an action from me.

Ally's finding is that "Practice Yamagata" in BossRematch 1.4.13 starts the boss at its shrine through the game's normal loading. It worked once on one request. Summoning any boss you choose still isn't possible. I added this to `next.md` in the handoff folder.

Still open:
- **This PC's speed test:** it can't run because Steam is signed out.
- **pc2:** the four Ibaraki runs (OFF, ON, ON, OFF) are still pending.
- **Ally:** its repeat of the +9% FPS test is still pending.

## 2026-10-14 20:08:20 session b92dd6df-8105-42bb-a429-3aa951964cf9
I've started on live arena swapping, but nothing is built or tested yet.

- **Ally:** I sent it the job on the bridge (#4431), set to run on Opus. The goal is to switch a loaded map, such as the training stage, into a boss arena and spawn the boss with no loading screen. It will build on its working delete-and-spawn-boss code and the instant-retry build (#459). It will time seconds from menu click to control in the real game and compare that against the current 37 s.
- **PC:** I also sent myself a task for the static reverse engineering of the arena loading path, to run in parallel. I haven't started that work yet.
- **Handoff:** I added the task to `.handoff/open.md`. I didn't update `next.md` or `owners.md`.
- **Your phone:** I sent you a short confirmation (#4433).

## 2026-10-15 07:15:12 session e1899780-a197-487d-abd5-84c192426def
I started three read-only optimization scouts and told the user on the bridge. None of them launches the game yet.

- **PC:** a subagent is running now. It will write `notes/perf-opportunities-pc-20261015.md`.
- **PC2:** I sent it a task. It writes a note under `notes/` and sends a one-line top pick.
- **Ally:** I sent it the same task, but the passive-boss fix comes first.

Each scout ranks 3-5 candidates by expected gain and risk, and gives a quick OFF-vs-ON test for each. The rules are no graphics-quality cuts and no OS/driver tuning.

No results are in yet. I haven't updated `next.md` or `owners.md`. I'll do that once the scouts report back.

## 2026-10-15 13:32:13 session e2d41409-7199-4053-85e9-911050205b0d
I haven't started on your five points yet. The fade-out code is in another pc session's working copy, and I couldn't find it on this machine.

Here is what I've verified:
- **Fade-out timing:** the log shows the body was faded and deleted 6 seconds after the kill, which is why it vanished mid-cutscene.
- **Health bar:** the HP-0 fix works, and the bar cleared.
- **My source copy:** `D:\nioh3-wt\x533\Nioh3Training\src\training.c` has the HP-0 fix and an older instant-delete clean-up. It does not have the fade code.
- **Not tried yet:** I haven't looked at how the game handles loot drops, the finisher requirement or the cutscene, so I don't know how hard the toggles are.

Your five points, as I understand them:
1. Wait for the kill cutscene to finish before touching the body.
2. Leave the body on the floor for a few seconds, then fade it.
3. Make the loot drop toggleable.
4. Make "needs a death blow" toggleable.
5. Make the cutscene toggleable.

I've written all this into `.handoff/next.md` so the other session or the next run can pick it up. Nioh 3 is still running from your last test, and I haven't changed anything installed.
