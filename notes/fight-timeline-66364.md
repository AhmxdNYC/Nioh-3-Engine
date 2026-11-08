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


### 1.8.6 integration (#68543)

Rebased tested 179e925f onto f60c56f9 as 930a5127 on cx-pc/fight-timeline-186. The merge was conflict-free; the rebased tree matches the reviewed merge tree. Shrine #23 actor guard retained byte-for-byte; crashupload.cpp (last[] log fallback and 120-char cap) unchanged from 1.8.6. Only timeline/report formatting and identity changes added.

EF build.bat exit 0; BR build.bat exit 0; Training build.sh compiler command (Zig O2, async exceptions) exit 0. fightlog_test and recorded replay both exit 0. No install/launch or new in-game result claimed; earlier pc2 pass is #68539 on the old base.

Artifacts: EF #5059 sha256 138c92326190e03b82945f956de5473f3643b8aa928157ff86ac0530ce505273; BR #5060 sha256 0219c009765a187baadddfe1775b04ebcdc5c8ab6036961e23cfa2266fcc1cbe; TR #5061 sha256 a66e7f6d50cd0c528b6585b14d638c3dcc2bb9fd5d2376fb92c9ad158fc8fbdf. Local copies D:/nioh3-re/fight186-artifacts. Build logs D:/nioh3-re/fight186-{ef,br,tr}-build.log. Next: lead integration/validation.
