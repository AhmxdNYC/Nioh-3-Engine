"""Menu rule audit (TASK #137249, human #137196): a feature with 3+ settings gets its own 'Feature   A: open' page.
Hub pages (Mod menu, Training, Player, Boss health and Ki) may list single rows and open rows only: a helper that adds several
rows of one feature, or a row loop, on a hub page fails this test. Also: every menu page id is unique, every sub page has a
title, open row, footer and Back, and the rows that moved keep their ids (no ini key / default / behaviour change). No game."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
src = root / "src"
rm = (src / "rematch.cpp").read_text(encoding="utf-8", errors="replace")
sp = (src / "sub_pages_menu.h").read_text(encoding="utf-8", errors="replace")
ps = (src / "practice_stats_menu.h").read_text(encoding="utf-8", errors="replace")
pd = (src / "page_defaults_menu.h").read_text(encoding="utf-8", errors="replace")
fails, checks = [], 0


def check(ok, msg):
    global checks
    checks += 1
    if not ok:
        fails.append(msg)


def block(page):
    """Source of the ItemsBase block for one page: 'if (g_uiPages.page == uX) {' up to its matching brace."""
    i = rm.find("if (g_uiPages.page == %s) {" % page, rm.find("int ItemsBase("))
    if i < 0:
        return None
    j, depth = rm.index("{", i), 0
    for k in range(j, len(rm)):
        depth += rm[k] == "{"
        depth -= rm[k] == "}"
        if depth == 0:
            return rm[j:k + 1]
    return None


# 1. Hub pages: no multi-row feature helper, no row loop. Allowed helpers carry their reason.
HUBS = ["uRoot", "uPracticeHub", "uPlayer", "uBossVitals"]
ALLOWED = {
    ("uPracticeHub", "PstAddHubRows"): "Time of day / Weather / Freeze / Step / Reset fight: session controls of different features",
    ("uBossVitals", "BvAddPage"): "the page's own feature (this boss's health and Ki)",
}
helper = re.compile(r"\b([A-Z][A-Za-z0-9]*Add[A-Za-z0-9]*(?:Rows|Page))\s*\(")
for hub in HUBS:
    b = block(hub)
    check(b is not None, "%s: page block not found in ItemsBase" % hub)
    if b is None:
        continue
    for name in helper.findall(b):
        check((hub, name) in ALLOWED, "%s lists %s() flat: a feature with 3+ settings needs its own page (sub_pages_menu.h)" % (hub, name))
    check(not re.search(r"\bfor\s*\(", b), "%s adds rows in a loop: move them to a feature page" % hub)
    check(not re.search(r"add\(kIt(Sty|Pst|Sct|ScEq|ScOv|ScCore)\w*Base", b), "%s adds feature rows by id range" % hub)

# 2. Player and Boss health and Ki open the moved features.
pl = block("uPlayer") or ""
for which in ("kSubSoulCores", "kSubStyle", "kSubPlayerStats"):
    check("SubAddOpenRow(add, %s)" % which in pl, "Player lacks the %s open row" % which)
check("SubAddOpenRow(add, kSubBossKi)" in (block("uBossVitals") or ""), "Boss health and Ki lacks the Boss damage and Ki open row")
check("if (SubAddPage(add)) { add(kItBack); return n; }" in rm, "sub pages must end with Back")

# 3. Page ids unique across the add-on (a reused id would open the wrong page).
ids = {}
for f in src.glob("*.*"):
    for name, val in re.findall(r"\b(u[A-Z]\w*)\s*=\s*(\d{3})\b", f.read_text(encoding="utf-8", errors="replace")):
        if name in ids and ids[name] != val:
            fails.append("page %s declared twice (%s, %s)" % (name, ids[name], val))
        ids[name] = val
vals = {}
for name, val in ids.items():
    check(val not in vals, "page id %s used by %s and %s" % (val, vals.get(val), name))
    vals[val] = name

# 4. Every sub page: table row (label, title, footer), a rows case and a page-defaults case.
pages = re.findall(r"\{(u\w+), \"([^\"]+)\", \"([^\"]+)\",\s*\"([^\"]+)\"\}", sp)
check(len(pages) == 4, "expected 4 sub pages, found %d" % len(pages))
for page, label, title, foot in pages:
    check(page in ids, "%s has no page id" % page)
    check(("if (p == %s)" % page) in sp, "%s lists no rows in SubAddPage" % page)
    check(page in pd, "%s has no Reset to defaults case" % page)
    check(len(foot) > 20 and title, "%s needs a title and a footer" % page)

# 5. Moved rows keep their ids and helpers (behaviour, ini keys and defaults unchanged).
check("for (int i = 0; i < n; ++i) { if (i == show) continue; add(kItStyBase + i); if (i == 0 && show < n) add(kItStyBase + show); }" in ps, "style rows: same ids kItStyBase + i (Show style meter row 25 right after row 0, Zack7214 1.8.43)")
check("add(kItPstBase + PS_PLAYER_INF_KI)" in ps and "PS_PLAYER_SPEED" in ps, "player stat rows: same PS_* ids")
for row in ("add(kItScForms);", "SctAddCoreRows(add);", "ScEqAddRows(add);", "SctAddHubRow(add);"):
    check(row in sp, "Soul cores page lost %s" % row)
check("PstAddBossRows(add)" in sp, "Boss damage and Ki page lists PstAddBossRows")

if fails:
    print("\n".join("FAIL " + f for f in fails))
    sys.exit(1)
print("menu sub-page rule (TASK #137249): %d checks PASS, %d hub pages flat-free, %d sub pages" % (checks, len(HUBS), len(pages)))
