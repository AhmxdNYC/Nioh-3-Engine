# Native held win on early Return / retry — #72150

Build on cxl-pc/multiboss-door, retaining #72084 phase grace and label corrections (168cb8cc, after requested base bcdad8fa). Main pc tested the earlier 65d31658/99600c91 pair: R0034 sequence passed, R0C12 had two reports; that pair preceded #72084's implementation.

Additional change: Return/quit during the pending native win commits a win immediately when the phase has ended, no named actor remains live, and any resolved roster is complete. Player loss remains loss. Quitting a resumed phase, an unfinished roster, a still-live named boss or an attempt with no observed phase end remains quit. No per-row cases.

Menu timing: only report finalization waits 8 seconds. Native boss-mode/menu flow is untouched. Training sets its existing tWin at the confirmed final kill immediately, so Fight again/Return and automatic retry do not inherit an 8-second delay. TrainRetry settles the held previous result before resetting roster/phase state and starting a new attempt; eventual grace completion does not restart an existing menu/retry timer.

Validation: production phase helper tests cover both unresolved rows, next phase at 1.3 s, one record, 8 s finalization, early-exit win, live/incomplete/resumed-phase quits, player loss and preserved retry timer. Production roster/fallback helpers pass all 23 resolved rows. Label fix's prior active/dormant executable tests and BR phase/token ownership checks remain unchanged and passed in #72084.

TR build exit 0. Actual newly built file SHA256 570f9c8772e0cfaeff26f844b0db5d13abe1ff9fe0577c19671360fd09368895. BR unchanged from #72084, built file SHA256 e3822ef1c4b4b7fe0cd647f7277557a1244c0144ae98f890a7dcb136552e7fec (bridge #5438). No game/install here; pc owns live R0C12 continuation and early Return gate. Public release unchanged.
