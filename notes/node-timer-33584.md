# Render-node timer (#33584)

Default off. No code patch. Branch `gx-pc/node-timer-33584`, version `1.7.65-rnt`, on top of the installed label-fix `9f506062` (`1.7.61-hrabi`).

The outside recipe timed `JobInvokeTimed` `1113B4`, which reads a job's stats pointer at `+0x38`. A render entry is a different object. On v2.02, checked in the dump:

- `+0x38` is the next entry (`112F93`). Writing it would break the list.
- `+0x40` is the stats pointer (`113292`). `FrameRenderViews` `113204` already QPC-times the node when it is set. Retail leaves it null.
- List: `GameMain = [base+4759910]` (the getter `31A048` only names that global; it returns a pool id). `host = [GameMain+530h]`, `graph = [host+128h]`, `box = [graph+28h]`, first entry = `[box]`, next = `[entry+38h]`.

`[Profiler] NodeTimer = 0` does nothing. `1` logs one line and waits. F11 in a scene stores one record per entry on the top list. Records are never freed and never replaced. A graph change arms the new list. Compound children stay inside the parent pass.

## Log format (once a second after F11)

```
RNT: sum 8.400 ms nodes 412 over 0.05 ms 30 (last-10 mean, top list only)
RNT: 1.204 ms max 2.100 n=600 SceneRenderView exec=10F060 type=AABBCCDD
```

`sum` adds the engine's last-10 mean for every armed pass that has a sample. `n` is the total sample count since F11, not a one-second count. Passes under 0.05 ms are in the sum and omitted from the list. At most 24 passes are printed. `exec` is the node's Execute RVA. `type` is the type-desc hash at `TypeDesc+0x0C`.

This is a timer, not an OFF/ON fps pair. Leave `NodeTimer` at 0 for benches. The QPC runs inside the game's own node call while it is on.
