"""rematch_catalog.py - build the boss-rematch catalogue Nioh3EngineFix_bosses.tsv (notes/boss-rematch-design.md s.4).

    py -I tools/rematch_catalog.py [--csv notes/battle-scrolls.csv] [--out mod/Nioh3EngineFix/Nioh3EngineFix_bosses.tsv] [--check]

Phase 1 (route R only): one row per Battle Scroll mission record that the game's replay list can show and that has a fight
(battle_scroll_candidate = 1, tutorials / training excluded), plus the three data-hidden duels (record +0x40 bit 0x4000, flags 0x24180:
0xD22D Honda Tadakatsu, 0xB3F8 Yoshitsune, 0xFB96 Jubei/Chuya) as hidden = 1, verified = 0 (test R-12 pending).
Route-R rows are verified = 1: the action only opens the game's own replay list (R-a), the game decides what it lists.
E0/E1 columns (emaki / placement / warp / box / hell / region byte) stay '-' until the Phase 0 captures fill them.
The mod reads columns by header name (id, name, mission_name, region, route, replay_mission_id, hidden, verified); extra
columns are ignored, so the file can grow without a rebuild. ASCII only (the OSD font). --check: verify the written file
equals a fresh build (exit 1 if not).
"""
import argparse
import csv
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
HIDDEN = {0xD22D: "Honda Tadakatsu", 0xB3F8: "Minamoto no Yoshitsune", 0xFB96: "Yagyu Jubei & Marubashi Chuya"}
ERA = [("Sengoku", "Sengoku"), ("Heian", "Heian"), ("Yamatai", "Yamatai"), ("finale", "Edo finale"), ("post-game", "Edo post"),
       ("Bakumatsu", "Bakumatsu"), ("Keian", "Keian DLC"), ("Eternal Rift", "Rift")]
KEEP_PAREN = {"human", "yokai", "final", "prologue", "duel", "DLC", "yokai, DLC"}
COLS = ["id", "name", "mission_name", "region", "route", "replay_mission_id", "hidden", "story_mission", "defeat_kind", "defeat_id",
        "emaki_byte", "emaki_key_u32", "placement_id", "stage_mission", "warp_x", "warp_y", "warp_z", "warp_yaw", "box_id", "hell_id",
        "region_byte", "verified", "seen"]
# Rows confirmed by an in-game log (RematchLog / BossLog): mission id -> filled columns + "seen" (session). Evidence in
# notes/session-2026-10-08-gamedata.md: the shrine Battle Scroll replay of mission 52 queued {+8 region 0x96, +0xC stage 72636},
# BossLog saw placement 76781 created 2.4 s before BossBattleMode, and SetMissionClearFlag(52) at the end.
LOGGED = {
    0x34: {"placement_id": "76781", "region_byte": "0x96", "verified": "1", "seen": "2026-10-08-pc-replay"},
}
ASCII_MAP = {0x014D: "o", 0x016B: "u", 0x2019: "'", 0x2013: "-", 0x2014: "-"}


def ascii_(s):
    s = s.translate(ASCII_MAP)
    return "".join(c if 32 <= ord(c) < 127 else "?" for c in s).strip()


def region(era):
    for k, v in ERA:
        if k in era:
            return v
    return ascii_(era)[:20]


def mission_title(name):
    return ascii_(re.sub(r"\^\d+~[A-Z_]+~", "", name)).strip()


def boss_name(text, mission):
    t = text.split(" [")[0]
    t = re.split(r" - NOT listed", t)[0]
    if not t or t.startswith("?") or t[0].islower():
        return mission  # generic content description ("formidable enemies", "yokai duels (multi)", "? (spirit ...)")

    def paren(m):
        inner = m.group(1)
        return " (%s)" % inner if inner in KEEP_PAREN else ""
    t = re.sub(r" \(([^)]*)\)", paren, t)
    t = re.sub(r"\s*\+.*$", "", t)  # "+ Hanzo's underlings"
    return ascii_(t).strip() or mission


def build(csv_path):
    rows = list(csv.DictReader(open(csv_path, encoding="utf-8")))
    out = []
    for r in rows:
        mid = int(r["mission_id"], 16)
        boss = r["boss_or_content"]
        cand = r["battle_scroll_candidate"] == "1"
        hidden = mid in HIDDEN
        if not cand and not hidden:
            continue
        if "(no boss)" in boss:  # tutorials / training ground
            continue
        mt = mission_title(r["name"])
        name = HIDDEN[mid] + " (duel)" if hidden else boss_name(boss, mt)
        out.append({
            "id": "R%04X" % mid, "name": name, "mission_name": mt, "region": region(r["era"]), "route": "R",
            "replay_mission_id": "0x%04X" % mid, "hidden": "1" if hidden else "0", "story_mission": "0x%04X" % mid,
            "defeat_kind": "clear", "defeat_id": "0x%04X" % mid,
            **{k: "-" for k in COLS[10:21]},
            "verified": "0" if hidden else "1", "seen": "-",
        })
        out[-1].update(LOGGED.get(mid, {}))
    # the same boss in several missions (Eternal Rift levels, Hanzo): name the mission too
    seen = {}
    for o in out:
        seen[o["name"]] = seen.get(o["name"], 0) + 1
    for o in out:
        if seen[o["name"]] > 1:
            mt = o["mission_name"]
            o["name"] = "%s (%s)" % (o["name"], mt.split(": ", 1)[1] if ": " in mt else mt)
    # stable order: era as in the game's tabs, then the CSV (menu) order
    order = {v: i for i, (_, v) in enumerate(ERA)}
    out.sort(key=lambda x: order.get(x["region"], 99))
    return out


def render(out):
    lines = ["# Nioh3EngineFix boss-rematch catalogue (tools/rematch_catalog.py from notes/battle-scrolls.csv). Route R only in ASI 1.7.40; 'seen' = confirmed by an in-game log.",
             "\t".join(COLS)]
    for o in out:
        lines.append("\t".join(o[c] for c in COLS))
    return "\r\n".join(lines) + "\r\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", default=str(ROOT / "notes" / "battle-scrolls.csv"))
    ap.add_argument("--out", default=str(ROOT / "mod" / "Nioh3EngineFix" / "Nioh3EngineFix_bosses.tsv"))
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    out = build(a.csv)
    text = render(out)
    p = pathlib.Path(a.out)
    if a.check:
        ok = p.exists() and p.read_bytes() == text.encode("ascii")
        print("catalogue %s: %s" % (p, "in sync" if ok else "OUT OF DATE"))
        return 0 if ok else 1
    p.write_bytes(text.encode("ascii"))
    nh = sum(o["hidden"] == "1" for o in out)
    print("wrote %s: %d rows (%d listable, %d hidden duels)" % (p, len(out), len(out) - nh, nh))
    return 0


if __name__ == "__main__":
    sys.exit(main())
