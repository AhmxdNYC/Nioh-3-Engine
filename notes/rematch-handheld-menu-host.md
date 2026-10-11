# Handheld rematch menu: in-game host contract

2026-10-11. Read-only source diagnosis and proposed compatibility improvement; the actual Ally failure is not established by these sources. Obtain installed addon version/hash, Menu Overlay/PadGesture, init log and current process identity before attributing the symptom.

## Current paths

Candidate addon `src/ui.cpp:38` observes only game XInput calls through its IAT hook (`HookPad`,98); it has no own-poll fallback. `Start`,301 returns before input installation when Overlay=0. `Toggle`,207 logs `Menu opened`; this distinguishes gesture recognition from composition. The UI thread creates a separate layered/topmost GDI HWND at224 and positions it at280. `notes/boss-rematch-addon.md:84` explicitly leaves Proton/gamescope composition unverified. Neither an absent screenshot nor a failed gesture proves composition is the cause.

EngineFix already supplies normalized keyboard/controller input, release latching and a DX12 drawing context. `menu.cpp:2130` processes input on Present; `osd.cpp:1151` invokes it after a usable context, and `osd.cpp:631` calls menu drawing. `osd.h:102` exposes DrawRect/DrawText. Its VPad menu IPC currently gets/sets settings; it is not an external row/action API.

## Smallest production integration

Add a Boss Rematch / Practice entry to the Engine menu and render addon row snapshots with the existing quads. Use a versioned, size-checked C export with fixed-width POD fields, bounded text arrays and max24 rows; no STL objects, native pointers in rows, allocator crossing or synchronous named-pipe transactions in Present.

The addon UI worker remains the single owner of UiOpen/Move/Change/Back/Act/Fill. These manipulate shared UI globals/static arrays; UiAct can save settings to disk. Host input must enter a bounded queue; snapshot queries and action posting use try-only synchronization and return Busy on contention. A render-frame callback must never wait for file I/O or mission work. Existing actions continue posting requests to the authoritative native pump, preserving all admission/return gates.

Attach grants an exclusive opaque session token only after the worker retires standalone UI ownership. Hosted-ready and open acknowledgement are distinct. Hide the standalone HWND and stop its input handling; an already installed addon IAT hook must pass through without masking/feeding in hosted mode, rather than removing a hook underneath EngineFix. Clear pending gestures and require neutral release during transition. Only one host/page may own inputs. Engine handles its own close/return and release latch; Back wins mixed Back+Accept frames.

Snapshot generation must change when the actionable model changes, not merely on redraw; reject an action bound to a stale generation. Close/detach must remain possible despite stale rows or a full normal-action queue. Session mismatch, unsupported ABI, failed renderer, dead worker or stale snapshot closes/disables the host page and releases its input mask. Do not silently fall back to an invisible external menu while consuming controller input. Root source writer owns final interface and handshake implementation.

## Focused acceptance checks

Offline: ABI/version/size/capability mismatch; truncated/unterminated rows; invalid row counts/action IDs; stale session/generation; exclusive attach; queue overflow; busy snapshot; close under saturation; source lifetime/disconnect; opening-button release; simultaneous Back+Accept; one press yields one action; no dual standalone/host masking. Verify renderer failure and stale worker release input without posting another mission command. Build only after live headroom hold ends.

Device proof: exact installed hashes/configuration and menu-open log, visible in-game page under Ally Proton/gamescope, controller navigation/confirmation/back/close, game input neutral while hosted and restored after release. This proves the host path on that device; it does not retroactively prove the cause of the old failure. No change to mission save/reward behavior is implied.
