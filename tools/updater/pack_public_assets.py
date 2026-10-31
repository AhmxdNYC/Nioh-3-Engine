#!/usr/bin/env python3
"""Build the public GitHub asset folder for one version. Does not publish.

v1.7.87 through v1.7.89 uploaded a combined zip of the three ASIs, those ASIs
again as separate files, and stable.json. Nioh3Updater.exe was not in that zip
or in the asset list, so the in-game Update row could only open the website
and the game folder.

This writes the same folder and also:
  - puts Nioh3Updater.exe at the root of Nioh3-<version>-combined.zip
  - copies that same exe beside the zip, as its own release asset

It does not edit stable.json and does not add the exe to the update manifest.
The manifest is allowed to name mod files only. Nothing here calls gh or vercel.
"""
from __future__ import annotations

import argparse
import hashlib
import re
import zipfile
from pathlib import Path

ASIS = ("Nioh3EngineFix.asi", "Nioh3Training.asi", "Nioh3BossRematch.asi")
UPDATER = "Nioh3Updater.exe"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def pack(version: str, out: Path, asis: dict, updater: Path, stable: Path) -> dict:
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("version must be major.minor.patch")
    missing = [name for name in ASIS if name not in asis]
    if missing:
        raise ValueError("missing ASI: " + ", ".join(missing))
    extra = sorted(set(asis) - set(ASIS))
    if extra:
        raise ValueError("unexpected ASI: " + ", ".join(extra))
    blobs = {}
    for name in ASIS:
        data = Path(asis[name]).read_bytes()
        if not data:
            raise ValueError(name + " is empty")
        blobs[name] = data
    exe = Path(updater).read_bytes()
    if not exe:
        raise ValueError("updater is empty")
    blobs[UPDATER] = exe
    stable_bytes = Path(stable).read_bytes()
    if not stable_bytes:
        raise ValueError("stable.json is empty")
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    for name, data in blobs.items():
        (out / name).write_bytes(data)
    (out / "stable.json").write_bytes(stable_bytes)
    zip_name = f"Nioh3-{version}-combined.zip"
    zip_path = out / zip_name
    members = (*ASIS, UPDATER)
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
    names_on_disk = (*ASIS, UPDATER, "stable.json", zip_name)
    sums = {name: sha256((out / name).read_bytes()) for name in names_on_disk}
    lines = [f"{sums[name]}  {name}" for name in sorted(sums)]
    (out / "SHA256SUMS.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    assets = [zip_name, *ASIS, "stable.json", UPDATER]
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
    args = parser.parse_args(argv)
    result = pack(args.version, args.out, {
        "Nioh3EngineFix.asi": args.engine,
        "Nioh3Training.asi": args.training,
        "Nioh3BossRematch.asi": args.boss,
    }, args.updater, args.stable)
    print(f"PACKED {result['out']}")
    print("zip", result["zip"].name, result["sums"][result["zip"].name])
    print("updater", result["sums"][UPDATER])
    print("gh assets:", " ".join(result["assets"]))
    print("NOT PUBLISHED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
