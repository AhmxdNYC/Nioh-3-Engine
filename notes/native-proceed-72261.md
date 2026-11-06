# Native Proceed opens the Boss Match gate page (#72261 / #72280)

Base 71ea9f0b; branch cxl-pc/native-proceed. Offline only; no game, install, save or lock touched.

## Cause and identity
Human session Temp/h72261: R3513 native start 23:18:50.847, FIGHT end win 23:20:12.008; no custom GATE spawn or gate interception. Proceed is the mission's native clear gadget, not the synthesized Return object. Logs do not record an actual button press or prove a clear write.

R3513 script archive_09/6F/2DB4694D793C854B2BDB56F9E0FD2D8E1C16AFB7D3BDEF34B020A91F70C7B4, verified symbol IDs (disassembler statement labels can be one function off): BossDefeatEventMain literal SubID 10 at 0x10264, XYZ (90229.296875,35279,127902.5) at 0x1026A; CreateClearGadgetPosition at 0x1027A calls Gadget::CreatePlacementGadgetPosition, type 0x353. WaitClearGadgetAccess at 0x10286 checks GetStatusPlacement(851,6,sub) at 0x8058 == 2 at 0x805C. Then EndMissionClearState at 0x102B8 reaches Mission::SetMissionClearFlag at 0x7DC4.

G_CLEAR_GADGET uses UI::OpenCommonWindowTwoButton 0x23F8CC0 for end-mission texts 0x3E0C230/0x3E8D85E, not EndMissionConfirmOpen. See notes/native-endmission-box-source.md. Native entry deliberately skipped GateTick and every prompt hook using !g_en.confirmed, g_gate.up and g_tr.active. No synthetic gate means the 400 cm pad-mask zone did not exist either. The 8-second history hold is not the cause.

## Change
- Training exports NativeGateMission only for the admitted, warped, matching mission/world, outside load/return. Neither g_tr.active nor synthetic gate presence is required.
- Existing script hook recognizes the game's end-mission text on a validated gadget in that owned native session. It opens the same GateChoice menu, writes script button result -1 (not Yes), returns the script reader's native return count, and keeps the gadget waiting for interaction. No positional/boss/mission lists. Duplicate opens suppressed. Existing Practice interception unchanged; unrelated dialogs and ordinary missions pass through.
- EndMissionConfirmOpen action 0x2613 also redirects for this ownership, as a secondary path. Other actions unchanged. No blanket raw-button mask added to native arenas.
- BR native Return uses existing guarded kReqExit/CallReturn. Fight again first returns, waits for old transaction restoration and stable admission gates, then posts ordinary kReqFight for the same row. It is cancelled by new actions, changed attempt, session loss, quarantine or timeout. New entry re-evaluates normal save/progress permission, never caches a bypass.
- Page is immediate during the 8-second held win. Opening it does not settle history. Selecting Return/retry uses existing cleanup, which settles a confirmed held kill once. Retry uses a real mission reload, not a fake spawned substitute.

## Progress safety
Existing VetoClear and its lifetime are unchanged. IMPORTANT: base71ea9f0b arms it for unbeaten bypass only, not blanket every Boss Match. Human log says IgnoreProgress off and no R3513 UNBEATEN arm: native Proceed could reach normal clear before this fix. Do not claim the logs prove it did. Redirect now prevents that gadget's Yes/status2 path; explicit menu return uses the existing native return transaction. Other potential native clear triggers are outside this patch.

## Validation
BR and TR builds exit0; checked hashes of actual output files:
- TR 48a999738e0f8041d091fbcbfa65b7a3f552735c41e78afda05ff7ab3a94ab9e
- BR da3c83db52307d1b3fa1a15882d5bd453db69f1a5b675458de1e5125117cebc8
Production hook executable:16 cases (native no dynamic session/no gate, both texts, exact cancellation result, immediate page, repeats/page-open, bad reader/gadget, other text, stale mission, load/return, ordinary mission, old Practice). Native phase/fallback suite passes all resolved23 rosters plus unresolved restart/8s/early-return/loss behavior. Production delayed-retry executable passes one-shot return completion, admission wait, five cancellation causes and busy requests. Existing native-attempt executable passes4 admission,4 standalone-return, unreadable gap,15 map-ownership and96 transaction checks.

## Live proof still required
On one device: R3513 ordinary kill, use Proceed both during and after held-win window; only mod page, no native box/dead prompt, B closes and interaction re-arms. Return completes with one win; Fight again returns then loads same real fight. Check an unbeaten save stays unchanged and Practice Return still works. Verify capture of actual hook line/text/subid and send screenshot of page. No live screenshot fabricated; offline task explicitly forbids this PC game access.

## Regression attribution (#72310)
Compared public1.8.6 source f60c56f9 (TR04781485/BRfe7ec45d) against the failing native-entry branch. Exact behavioral bypass was introduced by 9bd72fbe45a5413d5bc1456ca4929eef60dd0b31, "Give native Fight now one destination owner and pass through mission exit", Nov5 7:09 pm EST.
`git log -S 'if (g_en.confirmed) return; // native mission trips'` and `git log -S '!g_en.confirmed && g_cfg_gate'` both identify that commit. Its diff adds the GateTick native early return, excludes native entry from h_cw2/h_endconf/PmDecideSwallow/h_dlgopen, and passes native MissionRequestReturn through after cleanup. Public1.8.6 has none of those exclusions. Verified ancestor of bcdad8fa (source of human TR99600c91/BR65d31658).
This is source-level causal attribution, not a live binary bisect. Public worked through the synthetic Return gate; simply reverting the exclusion is insufficient for the new true-native route because it has no g_tr.active/g_gate.up. Fix72d5b0d1 explicitly handles the owned native clear gadget and preserves safe native return transactions.
