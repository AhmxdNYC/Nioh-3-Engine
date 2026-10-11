"""Prepare an unvalidated C72E arena-start experiment; never install or launch.

The A/B changes only the native mission start request. Arena warps, spawn-record
writes, automatic self-tests and capture import are disabled in both overlays.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ROW_ID = "RC72E"
MISSION = "0xC72E"
OLD_START = "335814"
NEW_START = "334140"
NATIVE_POINT = (207629.0, 6027.0, 142678.0, 40.6)
FIGHT_POSITION = (207072.90625, 6026.490234375, 142371.828125)
SCRIPT_SHA256 = "c7273a0639ec540df82517a6ff39837aa4efdf194270031e4bc8373a5f0d9302"


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def prepare(catalog: bytes, index: bytes) -> tuple[bytes, dict]:
    """Fail closed on changed input records; preserve every unrelated byte."""
    lines = catalog.decode("utf-8-sig").splitlines(keepends=True)
    if not lines:
        raise ValueError("empty catalogue")
    header_number = next((i for i, line in enumerate(lines)
                          if line.strip() and not line.lstrip().startswith("#")), None)
    if header_number is None:
        raise ValueError("missing catalogue header")
    header = lines[header_number].rstrip("\r\n").split("\t")
    if header[0] != "id":
        raise ValueError("missing id header")
    if len(set(header)) != len(header):
        raise ValueError("duplicate catalogue columns")
    matches = []
    for number, line in enumerate(lines[header_number + 1:], header_number + 1):
        fields = line.rstrip("\r\n").split("\t")
        if fields[0] == ROW_ID:
            if len(fields) != len(header):
                raise ValueError("wrong C72E field count")
            matches.append((number, fields, dict(zip(header, fields))))
    if len(matches) != 1:
        raise ValueError("expected exactly one RC72E row")
    number, fields, row = matches[0]
    expected = {"route": "R", "replay_mission_id": MISSION,
                "placement_id": "333550", "stage_mission": OLD_START,
                "fight_kind": "shrine", "gate_id": "538668029",
                "gate_kind": "door", "boss_pos": "207698_6034_143132"}
    for key, value in expected.items():
        if row.get(key) != value:
            raise ValueError(f"unexpected {key}: {row.get(key)!r}")
    for key in ("arena_x", "arena_y", "arena_z", "arena_src"):
        if row.get(key) not in ("", "-"):
            raise ValueError(f"RC72E already has arena data: {key}")
    points = [r for r in csv.DictReader(index.decode("utf-8-sig").splitlines())
              if r["mission_id"] == MISSION and r["kind"] == "start"
              and r["id"] == NEW_START]
    if len(points) != 1:
        raise ValueError("expected exactly one native C72E/334140 point")
    point = points[0]
    if int(point["flags"], 0) != 0 or int(point["sub"], 0) != 0:
        raise ValueError("native point flags/sub changed")
    actual = tuple(float(point[k]) for k in ("x", "y", "z", "yaw"))
    if not all(math.isfinite(v) for v in actual) or actual != NATIVE_POINT:
        raise ValueError("native point transform changed")
    boss = tuple(float(v) for v in row["boss_pos"].split("_"))
    boss_distance = math.dist(actual[:3], boss) / 100.0
    fight_distance = math.dist(actual[:3], FIGHT_POSITION) / 100.0
    changes = {"stage_mission": NEW_START, "fight_kind": "native arena start",
               "fight_dist_m": str(round(boss_distance)), "fight_conf": "low",
               **{f"warp_{k}": point[k] for k in ("x", "y", "z", "yaw")}}
    for key, value in changes.items():
        fields[header.index(key)] = value
    original_line = lines[number]
    ending = original_line[len(original_line.rstrip("\r\n")):]
    lines[number] = "\t".join(fields) + ending
    bom = b"\xef\xbb\xbf" if catalog.startswith(b"\xef\xbb\xbf") else b""
    candidate = bom + "".join(lines).encode("utf-8")
    before = catalog.splitlines(keepends=True)
    after = candidate.splitlines(keepends=True)
    if len(before) != len(after) or any(a != b for i, (a, b) in enumerate(zip(before, after)) if i != number):
        raise ValueError("unrelated catalogue bytes changed")
    manifest = {
        "status": "UNVALIDATED: NOT INSTALLED OR RUN", "row": ROW_ID,
        "catalog_sha256": sha(catalog), "native_index_sha256": sha(index),
        "candidate_sha256": sha(candidate), "native_point": point,
        "native_script_evidence": {"sha256": SCRIPT_SHA256,
            "OmazinaiBeforeRTM_call_offset": "0xef64",
            "Player_SetPos_helper_offset": "0xedbe",
            "destination": [*FIGHT_POSITION, 38.70000076293945],
            "BossBattleMode_call_offset": "0xefcc",
            "note": "Read-only bytecode audit; no direct script reference to start334140."},
        "changed_cells": {k: {"A": row[k], "B": v} for k, v in changes.items()},
        "distance_to_catalog_boss_m": boss_distance,
        "distance_to_native_script_fight_position_m": fight_distance,
        "runtime_requirements": [
            "User recording finished; PC and Ally shared-account reservation clear.",
            "Existing guard, lock and fresh current-entry save/file backup.",
            "Same BossRematch1.4.0 ASI in both arms; apply overlay keys to a backed-up INI.",
            "Request only RC72E manually; no SelfTest/Tour/AutoContinue.",
            "Confirm C72E and retry334140, no spawn override or arena warp armed.",
            "Observe FIRST controllable position in arena and actual BossBattleMode/enemy combat.",
            "Check death/retry, exit, exact config and entry-save restoration for benchmark-only pilot.",
            "Stop on wrong landing, missing encounter, online/co-op, crash or ownership/restore failure."
        ],
        "limitations": [
            "Arena-near native point is static evidence, not runtime acceptance.",
            "Boss gate and native intro still run; no cutscene bypass.",
            "Existing F PASS proves eventual fight only, not an instant first landing.",
            "Only RC72E; no default or other-boss catalogue change."
        ],
    }
    return candidate, manifest


def overlay(catalog_name: str) -> bytes:
    return ("; Overlay keys only: merge into backed-up user INI for the isolated pilot.\r\n"
            "; Both arms disable record mutations and old captured shrine positions.\r\n"
            "[Rematch]\r\nFightNow = 1\r\nFightNowWarp = 0\r\nFightNowSpawn = 0\r\n"
            "ArenaCapture = 0\r\nSelfTest =\r\n"
            f"Catalog = {catalog_name}\r\nArenas = C72E-pilot-empty-arenas.tsv\r\n").encode("utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, default=ROOT / "mod/Nioh3BossRematch/Nioh3BossRematch_bosses.tsv")
    parser.add_argument("--index", type=Path, default=ROOT / "notes/rematch-fight-index.csv")
    parser.add_argument("--out", type=Path, help="New external kit directory; omit for read-only validation")
    args = parser.parse_args()
    try:
        catalog = args.catalog.read_bytes()
        candidate, manifest = prepare(catalog, args.index.read_bytes())
        if args.out:
            target = args.out.resolve()
            if any((p / "Nioh3.exe").exists() for p in (target, *target.parents)):
                raise ValueError("refusing output inside installed game")
            target.mkdir(parents=True, exist_ok=False)
            artifacts = {"C72E-A-control.tsv": catalog, "C72E-B-native334140.tsv": candidate,
                         "C72E-A-overlay.ini": overlay("C72E-A-control.tsv"),
                         "C72E-B-overlay.ini": overlay("C72E-B-native334140.tsv"),
                         "C72E-pilot-empty-arenas.tsv": b"mission\tplacements\tx\ty\tz\twhen\tsrc\r\n"}
            for name, data in artifacts.items():
                (target / name).write_bytes(data)
            manifest["artifacts"] = {name: sha(data) for name, data in artifacts.items()}
            (target / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({k: manifest[k] for k in ("status", "row", "candidate_sha256", "distance_to_catalog_boss_m", "distance_to_native_script_fight_position_m")}, indent=2))
        return 0
    except (ValueError, OSError, KeyError, UnicodeError) as error:
        parser.exit(1, f"REFUSED: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
