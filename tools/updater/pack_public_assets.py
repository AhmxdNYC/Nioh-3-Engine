#!/usr/bin/env python3
"""Build the public GitHub asset folder for one version. Does not publish.

v1.7.87 through v1.8.1 uploaded a combined zip without Nioh3CamKiProbe, so the
camera rows had nothing to load. This writes the same folder and also:
  - Nioh3CamKiProbe.asi and Nioh3CamKiProbe.ini (comments only; camera stays native)
  - Nioh3Updater.exe at the root of Nioh3-<version>-combined.zip and beside it
  - the five name-list .tsv files
The three player settings files stay out of this zip. Unzipping it over the game
folder must not replace Nioh3EngineFix.ini, Nioh3Training.ini, or Nioh3BossRematch.ini.
A fresh install still ships those from the full-install zip.

A package that is missing one of the four ASIs or Nioh3CamKiProbe.ini is refused.
stable.json "news" (what the updater prints) is written from this release's bullet
changelog (--notes). The package is refused if news does not start with the new
version or repeats the previous release's news (1.8.26-1.8.37 shipped 1.8.25 news, #128383).
Nothing here calls gh or vercel.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import zipfile
from pathlib import Path

ASIS = ("Nioh3EngineFix.asi", "Nioh3Training.asi", "Nioh3BossRematch.asi", "Nioh3CamKiProbe.asi")
# New add-on only. The three player settings files are not in the update zip.
INIS = ("Nioh3CamKiProbe.ini",)
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


def manifest_with_name_lists(stable_bytes: bytes, version: str, extra_blobs: dict) -> bytes:
    manifest = json.loads(stable_bytes.decode("utf-8"))
    if not isinstance(manifest, dict):
        raise ValueError("stable.json must be a JSON object")
    files = manifest.get("files") or []
    if not isinstance(files, list):
        raise ValueError("stable.json files must be a list")
    prefix = f"https://nioh3-mod-feedback.vercel.app/releases/{version}"
    if files and isinstance(files[0], dict) and isinstance(files[0].get("url"), str) and "/" in files[0]["url"]:
        prefix = files[0]["url"].rsplit("/", 1)[0]
        # Folder is always the new --version; the base's folder is the OLD release (TECH_DEBT 11-13/11-15).
        if "/releases/" in prefix:
            prefix = prefix.rsplit("/releases/", 1)[0] + f"/releases/{version}"
    manifest["version"] = version
    manifest["files"] = [{
        "name": name,
        "sha256": sha256(extra_blobs[name]),
        "url": prefix + "/" + name,
    } for name in (*ASIS, *INIS, *TSVS)]
    return (json.dumps(manifest, indent=2) + "\n").encode("utf-8")


NEWS_MAX = 240  # updater.validate() limit


def news_from_notes(notes_md: str, version: str) -> str:
    """'<version>: bullet one; bullet two' from the first top-level '- ' bullet block of the changelog."""
    bullets, started = [], False
    for line in notes_md.splitlines():
        if line.startswith(("- ", "* ")):
            started = True
            text = re.sub(r"[`*_]", "", line[2:]).strip().rstrip(".")
            if text:
                bullets.append(text)
        elif started and line.startswith((" ", "\t")) and line.strip() and bullets:
            bullets[-1] += " " + re.sub(r"[`*_]", "", line.strip()).rstrip(".")
        elif started and line.strip():
            break
    if not bullets:
        raise ValueError("release notes have no '- ' bullet changelog; stable.json news needs one")
    return f"{version}: " + "; ".join(bullets) + "."


def check_news(news, version: str, previous_news) -> None:
    """Gate: news belongs to this version and is not the previous release's text."""
    if not isinstance(news, str) or not news.strip():
        raise ValueError("stable.json news is empty")
    m = re.match(r"\s*(\d+\.\d+\.\d+)\s*:\s*(.*)", news, re.S)
    if not m or m.group(1) != version:
        raise ValueError(f"stable.json news is for {m.group(1) if m else 'no version'}, not {version}: {news[:60]!r}")
    if len(news) > NEWS_MAX or "\r" in news or "\n" in news:
        raise ValueError(f"stable.json news is {len(news)} chars; the updater rejects news over {NEWS_MAX} chars or with line breaks (shorten the bullets)")
    body = m.group(2).strip()
    if not body:
        raise ValueError("stable.json news has no text after the version")
    if isinstance(previous_news, str) and previous_news.strip():
        pm = re.match(r"\s*\d+\.\d+\.\d+\s*:\s*(.*)", previous_news, re.S)
        prev_body = (pm.group(1) if pm else previous_news).strip()
        if news.strip() == previous_news.strip() or body.casefold() == prev_body.casefold():
            raise ValueError("stable.json news repeats the previous release's news")


def apply_news(stable_bytes: bytes, version: str, notes_md: str, previous_news) -> bytes:
    manifest = json.loads(stable_bytes.decode("utf-8"))
    manifest["news"] = news_from_notes(notes_md, version)
    check_news(manifest["news"], version, previous_news)
    return (json.dumps(manifest, indent=2) + "\n").encode("utf-8")


def pack(version: str, out: Path, asis: dict, updater: Path, stable: Path, tsvs: dict, inis: dict,
         notes: str = None) -> dict:
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
    missing_ini = [name for name in INIS if name not in inis]
    if missing_ini:
        raise ValueError("missing default ini: " + ", ".join(missing_ini))
    blobs = {}
    for name in (*ASIS, *TSVS, *INIS):
        source = asis[name] if name in ASIS else tsvs[name] if name in TSVS else inis[name]
        data = Path(source).read_bytes()
        if not data:
            raise ValueError(name + " is empty")
        blobs[name] = data
    exe = Path(updater).read_bytes()
    if not exe:
        raise ValueError("updater is empty")
    blobs[UPDATER] = exe
    base_bytes = Path(stable).read_bytes()
    stable_bytes = manifest_with_name_lists(base_bytes, version, blobs)
    if notes is None:
        raise ValueError("release notes (--notes) are required to write stable.json news")
    stable_bytes = apply_news(stable_bytes, version, notes, json.loads(base_bytes.decode("utf-8")).get("news"))
    here = str(Path(__file__).resolve().parent)
    if here not in sys.path:
        sys.path.insert(0, here)
    from updater import publish_gate
    publish_gate(blobs)
    if not stable_bytes:
        raise ValueError("stable.json is empty")
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    for name, data in blobs.items():
        (out / name).write_bytes(data)
    (out / "stable.json").write_bytes(stable_bytes)
    zip_name = f"Nioh3-{version}-combined.zip"
    zip_path = out / zip_name
    members = (*ASIS, *INIS, *TSVS, UPDATER)
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
    names_on_disk = (*ASIS, *INIS, *TSVS, UPDATER, "stable.json", zip_name)
    sums = {name: sha256((out / name).read_bytes()) for name in names_on_disk}
    lines = [f"{sums[name]}  {name}" for name in sorted(sums)]
    (out / "SHA256SUMS.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    assets = [zip_name, *ASIS, *INIS, *TSVS, "stable.json", UPDATER]
    return {"out": out, "zip": zip_path, "sums": sums, "assets": assets}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument("--training", required=True, type=Path)
    parser.add_argument("--boss", required=True, type=Path)
    parser.add_argument("--camera", required=True, type=Path)
    parser.add_argument("--camera-ini", required=True, type=Path)
    parser.add_argument("--updater", required=True, type=Path)
    parser.add_argument("--stable", required=True, type=Path)
    parser.add_argument("--bosses", required=True, type=Path)
    parser.add_argument("--actionsets", required=True, type=Path)
    parser.add_argument("--records", required=True, type=Path)
    parser.add_argument("--arenas", required=True, type=Path)
    parser.add_argument("--bgm", required=True, type=Path)
    parser.add_argument("--notes", required=True, type=Path,
                        help="this release's notes (markdown); its '- ' bullets become stable.json news")
    args = parser.parse_args(argv)
    result = pack(args.version, args.out, {
        "Nioh3EngineFix.asi": args.engine,
        "Nioh3Training.asi": args.training,
        "Nioh3BossRematch.asi": args.boss,
        "Nioh3CamKiProbe.asi": args.camera,
    }, args.updater, args.stable, {
        "Nioh3BossRematch_bosses.tsv": args.bosses,
        "Nioh3Training_actionsets.tsv": args.actionsets,
        "Nioh3Training_action_records.tsv": args.records,
        "Nioh3Training_arenas.tsv": args.arenas,
        "Nioh3Training_bgm.tsv": args.bgm,
    }, {
        "Nioh3CamKiProbe.ini": args.camera_ini,
    }, notes=args.notes.read_text(encoding="utf-8"))
    print(f"PACKED {result['out']}")
    print("zip", result["zip"].name, result["sums"][result["zip"].name])
    print("updater", result["sums"][UPDATER])
    print("news", json.loads((result["out"] / "stable.json").read_text(encoding="utf-8"))["news"])
    print("gh assets:", " ".join(result["assets"]))
    print("NOT PUBLISHED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
