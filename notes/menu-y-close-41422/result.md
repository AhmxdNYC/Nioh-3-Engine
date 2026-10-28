# Y / Triangle closes mod menus ? 41422

Release base `cxl-pc/release-185-40866` at36a6f808, implementationf30449ab on `cx-pc/menu-y-close-41422`.

Y maps to a new close edge processed only after the menu-open check. It calls the existing Close path for root/subpages and hosted Boss/Practice menus, preserving close requests, freeze release, auto-save and per-controller neutral-release latches. Closed-menu Y is unchanged; the press that closes is held neutral until release. B/Escape and the existing menu gestures still work.

Both ordinary and hosted footers now show a Y Close badge (Triangle under PlayStation ButtonStyle). It is early in the hint row so the close hint stays visible on narrow panels. Existing B close/back hints remain. The Start x3 gesture still works; its old footer hint yields space to the new shortcut.

The current settings UI has direct left/right adjustment, not a modal text editor. Y mixed with or held during Left/Right/Accept consumes that frame without closing or committing another operation. Hosted pending actions block Y; the additive Editing flag in the existing snapshot ABI reserves modal text/slider ownership for providers (no size/version change). Current provider has no modal text/slider editor, so no BossRematch/Training rebuild is needed. Future modal editors must publish Editing while owning input.

Both actual menu suites pass: root/nested/hosted close, held/mixed edit guards, hosted Editing/busy guards, close-press release latch, closed-menu Y pass-through and Xbox/PS symbol rendering. Release EF passes. Native gameplay/menu acceptance remains pc's task; no game or installed files touched.

EF bridge3049 SHA256 c7954203113c83e2285044fab10498f0924edf46778803921b21e22ef67ddda7. Training unchanged. Both footer previews sent Discord using universal notify normal-Python fallback; images rasterize the real menu draw commands with approximate stroke text, not game captures. Reproduce optional draw export with N3_Y_CLOSE_PREVIEW then run render.py.
