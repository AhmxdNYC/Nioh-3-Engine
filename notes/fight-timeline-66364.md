# Fight timeline completion (#66364)

Base: public 1.8.5 source pc/release-185-rc2 9157583c.
The existing e819259f timeline was already merged: starts, ends, win/loss/quit,
duration, names and the Report export were present. This change fills the missing
stable boss identity and makes bounded report formatting safe.

- Training sessions capture their catalog token and character ID at start.
- Native Boss Match paths pass their selected catalog ID using a new optional
  FightMarkEx export; old callers and older Training retain FightMark compatibility.
- Names, times and results retain the existing event lifecycle, with no new hooks.
- Report labels its existing last-20/session retention and local clock convention.
  Unknown character IDs remain 0, unknown catalog IDs are explicit; no inferred ID.
- 8192-byte Report buffer fits 20 complete rows. Smaller buffers stop before a row
  rather than printing a partial result/time. Existing end/quit/death rules unchanged.

Offline tests: production fightlog header passed existing Practice win, Boss Match
replacement-quit, loss, in-progress, duplicate-end and 21-fight ring rollover tests,
plus identity capture and tiny-buffer checks. A replay fixture contains verbatim
selected lines from recorded PC logs Temp/t65913/Nioh3Training.log and
Temp/t66029/logsA/Nioh3Training.log (2026-11-04). All 4 fights reproduce: 2 wins and
2 quits, durations 48,803 / 62,772 / 243,505 / 101,903 ms; catalog IDs persist across
retry and match captured arena tokens. Loss/Practice coverage is synthetic,
not claimed as a new recorded live test. No game accessed.

Changes span Training (state/format/export), BossRematch (optional ID bridge), and
EngineFix (Report buffer/label); install all three candidate ASIs for validation.
