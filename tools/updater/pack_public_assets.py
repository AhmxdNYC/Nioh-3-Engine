#!/usr/bin/env python3
"""Build the public GitHub asset folder for one version. Does not publish.

v1.7.87 through v1.7.89 uploaded a combined zip of the three ASIs, those ASIs
again as separate files, and stable.json. Nioh3Updater.exe was not in that zip
or in the asset list, so the in-game Update row could only open the website
and the game folder.

This writes the same folder and also:
  - puts Nioh3Updater.exe at the root of Nioh3-<version>-combined.zip
  - copies that same exe beside the zip, as its own release asset

It adds the five name-list .tsv files to that zip and to the copy of stable.json it writes.
Player .ini files are not part of either. Nothing here calls gh or vercel.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import zipfile
from pathlib import Path

ASIS = ("Nioh3EngineFix.asi", "Nioh3Training.asi", "Nioh3BossRematch.asi")
TSVS = (
    "Nioh3BossRematch_bosses.tsv",
    "Nioh3Training_actionsets.tsv",
    "Nioh3Training_action_records.tsv",
    "Nioh3Training_arenas.tsv",
    "Nioh3Training_bgm.tsv",
)
UPDATER = "Nioh3Updater.exe"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def manifest_with_name_lists(stable_bytes: bytes, version: str, tsv_blobs: dict) -> bytes:
    manifest = json.loads(stable_bytes.decode("utf-8"))
    if not isinstance(manifest, dict):
        raise ValueError("stable.json must be a JSON object")
    files = manifest.get("files") or []
    if not isinstance(files, list):
        raise ValueError("stable.json files must be a list")
    prefix = f"https://nioh3-mod-feedback.vercel.app/releases/{version}"
    if files and isinstance(files[0], dict) and isinstance(files[0].get("url"), str) and "/" in files[0]["url"]:
        prefix = files[0]["url"].rsplit("/", 1)[0]
    kept = [entry for entry in files if not (isinstance(entry, dict) and entry.get("name") in TSVS)]
    for name in TSVS:
        kept.append({
            "name": name,
            "sha256": sha256(tsv_blobs[name]),
            "url": prefix + "/" + name,
        })
    manifest["files"] = kept
    return (json.dumps(manifest, indent=2) + "\n").encode("utf-8")


def pack(version: str, out: Path, asis: dict, updater: Path, stable: Path, tsvs: dict) -> dict:
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("version must be major.minor.patch")
    missing = [name for name in ASIS if name not in asis]
    if missing:
        raise ValueError("missing ASI: " + ", ".join(missing))
    extra = sorted(set(asis) - set(ASIS))
    if extra:
        raise ValueError("unexpected ASI: " + ", ".join(extra))
    missing_tsv = [name for name in TSVS if name not in tsvs]
    if missing_tsv:
        raise ValueError("missing name list: " + ", ".join(missing_tsv))
    extra_tsv = sorted(set(tsvs) - set(TSVS))
    if extra_tsv:
        raise ValueError("unexpected name list: " + ", ".join(extra_tsv))
    blobs = {}
    for name in (*ASIS, *TSVS):
        source = asis[name] if name in ASIS else tsvs[name]
        data = Path(source).read_bytes()
        if not data:
            raise ValueError(name + " is empty")
        blobs[name] = data
    exe = Path(updater).read_bytes()
    if not exe:
        raise ValueError("updater is empty")
    blobs[UPDATER] = exe
    stable_bytes = manifest_with_name_lists(Path(stable).read_bytes(), version, blobs)
    if not stable_bytes:
        raise ValueError("stable.json is empty")
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    for name, data in blobs.items():
        (out / name).write_bytes(data)
    (out / "stable.json").write_bytes(stable_bytes)
    zip_name = f"Nioh3-{version}-combined.zip"
    zip_path = out / zip_name
    members = (*ASIS, *TSVS, UPDATER)
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name in members:
            archive.writestr(name, blobs[name])
    with zipfile.ZipFile(zip_path) as archive:
        if archive.testzip() is not None:
            raise ValueError("zip failed its integrity check")
        names = archive.namelist()
        if names != list(members):
            raise ValueError("zip members: " + ", ".join(names))
        for name in names:
            if sha256(archive.read(name)) != sha256(blobs[name]):
                raise ValueError("zip bytes differ: " + name)
    names_on_disk = (*ASIS, *TSVS, UPDATER, "stable.json", zip_name)
    sums = {name: sha256((out / name).read_bytes()) for name in names_on_disk}
    lines = [f"{sums[name]}  {name}" for name in sorted(sums)]
    (out / "SHA256SUMS.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    assets = [zip_name, *ASIS, *TSVS, "stable.json", UPDATER]
    return {"out": out, "zip": zip_path, "sums": sums, "assets": assets}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument("--training", required=True, type=Path)
    parser.add_argument("--boss", required=True, type=Path)
    parser.add_argument("--updater", required=True, type=Path)
    parser.add_argument("--stable", required=True, type=Path)
    parser.add_argument("--bosses", required=True, type=Path)
    parser.add_argument("--actionsets", required=True, type=Path)
    parser.add_argument("--records", required=True, type=Path)
    parser.add_argument("--arenas", required=True, type=Path)
    parser.add_argument("--bgm", required=True, type=Path)
    args = parser.parse_args(argv)
    result = pack(args.version, args.out, {
        "Nioh3EngineFix.asi": args.engine,
        "Nioh3Training.asi": args.training,
        "Nioh3BossRematch.asi": args.boss,
    }, args.updater, args.stable, {
        "Nioh3BossRematch_bosses.tsv": args.bosses,
        "Nioh3Training_actionsets.tsv": args.actionsets,
        "Nioh3Training_action_records.tsv": args.records,
        "Nioh3Training_arenas.tsv": args.arenas,
        "Nioh3Training_bgm.tsv": args.bgm,
    })
    print(f"PACKED {result['out']}")
    print("zip", result["zip"].name, result["sums"][result["zip"].name])
    print("updater", result["sums"][UPDATER])
    print("gh assets:", " ".join(result["assets"]))
    print("NOT PUBLISHED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
