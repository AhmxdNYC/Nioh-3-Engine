"""Offline check for the Dummy mission label. Display only: no route, replay id, or placement change.

    py -I tools/row_display_name_test.py
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import rematch_catalog as rc

# old mission_name -> new. These are the only rows whose game title is the placeholder.
EXPECT = {
    "D084F": ("Ito Ittosai", "unlisted stage DUEL_MASTER_NPC_001_BOSS", "Ito Ittosai", "0x084F", "2018827"),
    "DB90D": ("Fuma Kotaro", "unlisted stage DUEL_MASTER_NPC_000_BOSS", "Fuma Kotaro", "0xB90D", "2000784"),
    "DCE5A": ("Sugitani Zenjubo", "unlisted stage DUEL_MASTER_NPC_002_BOSS", "Sugitani Zenjubo", "0xCE5A", "2024153"),
    "D63A9": ("Shinran", "unlisted stage HEI_OP4_DUEL_MASTER_NPC_004_BOSS", "Shinran", "0x63A9", "2135664"),
    "D6B58": ("Tomoe Gozen", "unlisted stage DUEL_MASTER_NPC_HEI_OP1_BOSS", "Beneath a Pure White Flag", "0x6B58", "2107933"),
    "D95FA": ("Nasu no Yoichi", "unlisted stage DUEL_MASTER_HEI_NPC_002_BOSS", "Beneath a Pure White Flag", "0x95FA", "2117063"),
    "D5A7B": ("Sawamura Yasusuke", "unlisted stage DUEL_MASTER_NPC_BAKU_OP3_BOSS", "Sawamura Yasusuke", "0x5A7B", "2083931"),
    "DA30C": ("Takahashi Deishu", "unlisted stage DUEL_MASTER_NPC_BAKU_OP1_BOSS", "Takahashi Deishu", "0xA30C", "2054018"),
    "D7BA1": ("Nakane Masamori", "unlisted stage DUEL_MASTER_NPC_EDO_001_BOSS", "Nakane Masamori", "0x7BA1", "2185058"),
    "DA709": ("Maeda Mitsutaka", "unlisted stage DUEL_MASTER_NPC_EDO_OP2_BOSS", "Maeda Mitsutaka", "0xA709", "2171204"),
}


def check_rule():
    boss = "Nasu no Yoichi"
    assert rc.display_mission_name("Beneath a Pure White Flag", boss, ["Other"]) == "Beneath a Pure White Flag"
    assert rc.display_mission_name("Dummy", boss, ["Beneath a Pure White Flag"]) == "Beneath a Pure White Flag"
    assert rc.display_mission_name("Dummy", boss, ["Dummy", "Beneath a Pure White Flag"]) == "Beneath a Pure White Flag"
    assert rc.display_mission_name("Dummy", boss, []) == boss
    assert rc.display_mission_name("Dummy", boss, ["Dummy", ""]) == boss
    assert rc.display_mission_name("Dummy", boss, ["One Story", "Another Story"]) == boss
    assert rc.display_mission_name("Dummy", boss, ["Same Name", "Same Name"]) == boss
    assert rc.display_mission_name("", "Ito Ittosai", []) == "Ito Ittosai"
    assert rc.is_placeholder_title("Dummy") and rc.is_placeholder_title("")
    assert not rc.is_placeholder_title("Beneath a Pure White Flag")


def check_rows():
    _all, out = rc.build()
    by_id = {o["id"]: o for o in out}
    changed = []
    for rid, (name, old, new, mid, pid) in EXPECT.items():
        o = by_id[rid]
        assert o["name"] == name, (rid, o["name"])
        assert o["mission_name"] == new, (rid, o["mission_name"])
        assert o["replay_mission_id"] == mid and o["route"] == "D" and o["placement_id"] == pid
        assert o["story_mission"] == mid and o["defeat_id"] == mid
        changed.append("%s: %s -> %s" % (rid, old, new))
    expect_ids = set(EXPECT)
    got = {o["id"] for o in out if o["mission_name"] in {v[2] for v in EXPECT.values()} and o["id"] in expect_ids}
    assert got == expect_ids
    # no row still shows the old unlisted-stage label
    assert not any(o["mission_name"].startswith("unlisted stage") for o in out)
    # a real title on another row is untouched
    story = by_id["RCB08"]
    assert story["mission_name"] == "Beneath a Pure White Flag"
    assert story["replay_mission_id"] == "0xCB08" and story["route"] == "R"
    print("\n".join(changed))
    print("PASS %d display labels" % len(changed))


if __name__ == "__main__":
    check_rule()
    check_rows()
