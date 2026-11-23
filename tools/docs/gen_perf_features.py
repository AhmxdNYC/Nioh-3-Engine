"""gen_perf_features.py - build the performance-feature table in docs/engineer/performance-features.md
from mod/Nioh3EngineFix/Nioh3EngineFix.ini, so the doc never drifts from the shipped defaults.

    py -I tools/docs/gen_perf_features.py           # rewrite the generated block in place
    py -I tools/docs/gen_perf_features.py --check   # exit 1 if the doc is stale (use in regression)

Every `KEY = value` line in the performance sections becomes one row: key, shipped default, and the first
comment line written above it in the ini (the ini comments are the authoritative per-key description).
Stdlib only.
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
INI = ROOT / "mod/Nioh3EngineFix/Nioh3EngineFix.ini"
PROFILES = ROOT / "release/profiles.json"
DOC = ROOT / "docs/engineer/performance-features.md"
SECTIONS = ["Patches", "HalfRate", "ExtraPerf", "VRR", "Latency", "GpuOpt", "Memory", "Tuning", "Profiler",
            "ShaderSwap", "ShaderHot"]
BEGIN, END = "<!-- BEGIN GENERATED: gen_perf_features.py -->", "<!-- END GENERATED -->"
KEY_RE = re.compile(r"^([A-Za-z0-9_]+)\s*=\s*([^;]*)")


SEP = re.compile(r"^[-=]{2,}.*[-=]{2,}$")


def pick(key, comments):
    """Best description: a comment line that starts with this key (W6: / RC (1.7.x):), then one that names it
    in a shared list (CBS / CBN / CBT ...:), else the first non-separator line of the block above the key."""
    own = re.compile(r"^" + re.escape(key) + r"(?![A-Za-z0-9_])[^:=]{0,40}:")
    shared = re.compile(r"^(?:[A-Za-z0-9_]+\s*/\s*)+" + re.escape(key) + r"(?![A-Za-z0-9_])[^:=]{0,40}:")
    for pat in (own, shared):
        for c in comments:
            if pat.match(c):
                return c
    return next((c for c in comments if c and not SEP.match(c)), "")


def parse(text):
    rows, section, comments, last_was_key = {}, None, [], False
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith("[") and line.endswith("]"):
            section, comments = line[1:-1], []
            continue
        if line.startswith(";"):
            if last_was_key:  # a comment after a key starts a new block; keys right after each other share one
                comments, last_was_key = [], False
            comments.append(line.lstrip("; ").rstrip())
            continue
        m = KEY_RE.match(line)
        if m and section in SECTIONS:
            key, val = m.group(1), m.group(2).strip()
            desc = pick(key, comments)
            # strip a leading "KEY:" / "KEY (1.7.x):" so the description column is not repetitive
            desc = re.sub(r"^" + re.escape(key) + r"\b[^:]{0,40}:\s*", "", desc)
            desc = desc.replace("|", "/")
            if len(desc) > 180:
                desc = desc[:177].rstrip() + "..."
            rows.setdefault(section, []).append((key, val, desc))
            last_was_key = True
            continue
        if not line:
            comments, last_was_key = [], False
    return rows


def overrides():
    """release/profiles.json: what the release zip changes on top of the repo ini, with status + measured why."""
    if not PROFILES.exists():
        return {}
    d = json.loads(PROFILES.read_text(encoding="utf-8"))
    return {(o["section"], o["key"]): o for o in d.get("ALL", {}).get("overrides", [])}


def render(rows):
    ov = overrides()
    out = [BEGIN, "",
           "_Generated from `mod/Nioh3EngineFix/Nioh3EngineFix.ini` + `release/profiles.json` by "
           "`tools/docs/gen_perf_features.py`. Do not edit by hand; edit the ini comment / profile entry and rerun._",
           "", "`shipped` = the value in the release zip (profile override applied). `-1` = auto by OS (Proton vs Windows), "
           "`2` = installed but switched off (live menu row), `0` = stock game code. `evidence` is the profile's status "
           "and measured reason where one exists.", ""]
    for sec in SECTIONS:
        if sec not in rows:
            continue
        out += [f"### [{sec}]", "", "| key | shipped | evidence | what it does (ini comment) |", "|---|---|---|---|"]
        for k, v, d in rows[sec]:
            o = ov.get((sec, k))
            ev = ""
            if o:
                v = o["value"]
                why = o["why"].replace("|", "/")
                ev = f'{o["status"]}: ' + (why[:137].rstrip() + "..." if len(why) > 140 else why)
            out.append(f"| `{k}` | `{v}` | {ev} | {d} |")
        out.append("")
    out.append(END)
    return "\n".join(out)


def main():
    block = render(parse(INI.read_text(encoding="utf-8-sig", errors="replace")))
    doc = DOC.read_text(encoding="utf-8")
    if BEGIN not in doc or END not in doc:
        sys.exit(f"markers missing in {DOC}")
    new = doc[:doc.index(BEGIN)] + block + doc[doc.index(END) + len(END):]
    if "--check" in sys.argv:
        if new != doc:
            print("STALE: run py -I tools/docs/gen_perf_features.py")
            sys.exit(1)
        print("OK: performance-features.md matches the ini")
        return
    DOC.write_text(new, encoding="utf-8", newline="\n")
    print(f"wrote {DOC.relative_to(ROOT)} ({sum(l.startswith('| `') for l in block.splitlines())} rows)")


if __name__ == "__main__":
    main()
