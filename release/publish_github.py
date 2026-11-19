"""Publish a player release on GitHub. The ONLY allowed target is the public repo.

Why (#119235/#119244): v1.8.23 was first created with a bare `gh release create`, which
defaults to the checkout's origin (private AhmxdNYC/nioh3-engine), so players never saw it.

  py -I release/publish_github.py --tag v1.8.24 --notes NOTES.md --dir OUT_DIR [--repo R] [--dry-run]

Uploads every file in --dir listed in its SHA256SUMS.txt plus SHA256SUMS.txt itself
(stable.json stays site-only), marks Latest, then checks that every GitHub digest
matches. Refuses any --repo other than PUBLIC_REPO. Requires a human-approved release.
"""
import argparse, hashlib, json, subprocess, sys
from pathlib import Path

PUBLIC_REPO = "AhmxdNYC/Nioh-3-Engine"


def check_repo(repo: str) -> str:
    if repo.strip().lower() != PUBLIC_REPO.lower():
        raise SystemExit(f"REFUSED: player releases go only to {PUBLIC_REPO}, not {repo!r}")
    return PUBLIC_REPO


def asset_list(out: Path) -> list:
    sums = out / "SHA256SUMS.txt"
    names = [ln.split(None, 1)[1].lstrip("*").strip() for ln in sums.read_text().splitlines() if ln.strip()]
    files = [out / n for n in names if n != "stable.json"] + [sums]
    for f in files:
        if not f.is_file():
            raise SystemExit(f"missing asset {f}")
    return files


def main(argv=None) -> int:
    a = argparse.ArgumentParser()
    a.add_argument("--tag", required=True); a.add_argument("--notes", required=True)
    a.add_argument("--dir", required=True); a.add_argument("--repo", default=PUBLIC_REPO)
    a.add_argument("--target", default="main"); a.add_argument("--dry-run", action="store_true")
    o = a.parse_args(argv)
    repo = check_repo(o.repo)
    files = asset_list(Path(o.dir))
    want = {f.name: "sha256:" + hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
    cmd = ["gh", "release", "create", o.tag, "-R", repo, "--target", o.target, "--title", o.tag,
           "--notes-file", o.notes, "--latest", *map(str, files)]
    print(("DRY RUN: " if o.dry_run else "") + " ".join(cmd[:9]) + f" ... ({len(files)} assets)")
    if o.dry_run:
        return 0
    subprocess.run(cmd, check=True)
    got = json.loads(subprocess.run(["gh", "release", "view", o.tag, "-R", repo, "--json", "assets"],
                                    check=True, capture_output=True, text=True).stdout)
    got = {x["name"]: x.get("digest") for x in got["assets"]}
    bad = [n for n in want if got.get(n) != want[n]]
    print(f"{repo} {o.tag}: {len(want) - len(bad)}/{len(want)} digests match" + (f"; BAD {bad}" if bad else ""))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
