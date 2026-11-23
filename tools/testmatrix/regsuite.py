r"""regsuite.py - regression-pc offline suite over one checkout (and optionally a base checkout to diff against).

    py -I regsuite.py TREE [--base BASETREE] [--out DIR] [--only mod:glob ...]

A test FAILS if its exit code != 0 OR its output contains a failure marker (FAIL, FAILED, Traceback, AssertionError,
error C, LNK error, TIMEOUT), unless the last non-empty line says PASS/ALL PASSED/OK with rc 0 and no Traceback.
Exit code: 0 only if every test passes (no --base), or no test fails on TREE that passes on BASE (--base).
#105888: the old shell loop printed rc=0 for failing tests (#105755); this one never reports "all passed" on a fail.
"""
import argparse, fnmatch, glob, os, re, subprocess, sys

SETUP = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "tools", "msvc", "setup_x64.bat")
GLOBS = {
    "Nioh3BossRematch": ["tests/*.py"],
    "Nioh3EngineFix": ["tests/run_*.py", "tests/*_offline.py"],
    "Nioh3Training": ["tests/*_test.py", "tests/run_*.py"],
}
BAD = re.compile(r"\bFAIL(ED)?\b|Traceback \(most recent|AssertionError|\berror C\d{4}|\berror LNK\d{4}|fatal error", re.I)
OKLAST = re.compile(r"\b(ALL PASSED|PASS(ED)?|OK)\b", re.I)


def msvc_env():
    txt = subprocess.run(f'cmd /d /c ""{SETUP}" >nul && set"', capture_output=True, text=True).stdout
    env = dict(l.split("=", 1) for l in txt.splitlines() if "=" in l and not l.startswith("="))
    if "INCLUDE" not in env:
        sys.exit("regsuite: MSVC setup failed (no INCLUDE in env)")
    env["MSVC_SETUP"] = env["N3_MSVC_SETUP"] = SETUP
    return env


def tests_of(tree, only):
    out = []
    for mod, pats in GLOBS.items():
        d = os.path.join(tree, "mod", mod)
        seen = set()
        for p in pats:
            for f in sorted(glob.glob(os.path.join(d, p))):
                rel = os.path.relpath(f, d).replace("\\", "/")
                key = f"{mod}:{rel}"
                if rel in seen or (only and not any(fnmatch.fnmatch(key, o) for o in only)):
                    continue
                seen.add(rel); out.append((mod, rel))
    return out


def verdict(rc, txt):
    lines = [l for l in txt.splitlines() if l.strip()]
    last = lines[-1].strip() if lines else ""
    if rc != 0:
        return "FAIL", f"rc={rc} | {last[:140]}"
    # a PASS/OK line that merely mentions "failed" (e.g. "no game open failed", "after 3 failed forces") is not a failure
    bad = [l.strip() for l in lines if BAD.search(l) and not re.search(r"\b0 fail|FAIL: 0|fails?=0", l, re.I)
           and not re.match(r"\s*(OK|PASS(ED)?)\b", l)]
    if bad and not (OKLAST.search(last) and "Traceback" not in txt):
        return "FAIL", f"rc=0 but log: {bad[0][:140]}"
    return "PASS", last[:140]


def run_tree(tree, only, out, tag, env):
    res = {}
    for mod, rel in tests_of(tree, only):
        d = os.path.join(tree, "mod", mod)
        try:
            r = subprocess.run(["py", "-I", rel], cwd=d, env=env, capture_output=True, text=True, timeout=600,
                               encoding="utf-8", errors="replace")
            rc, txt = r.returncode, (r.stdout or "") + (r.stderr or "")
        except subprocess.TimeoutExpired:
            rc, txt = "TIMEOUT", "TIMEOUT"
        v, why = verdict(rc if rc != "TIMEOUT" else 124, txt)
        with open(os.path.join(out, f"{tag}-{mod}-{os.path.basename(rel)}.log"), "w", encoding="utf-8") as f:
            f.write(txt)
        res[f"{mod}:{rel}"] = v
        print(f"{tag} {v} {mod}:{rel} | {why}", flush=True)
    return res


def main():
    a = argparse.ArgumentParser()
    a.add_argument("tree"); a.add_argument("--base"); a.add_argument("--out", default=".")
    a.add_argument("--only", nargs="*", default=[])
    o = a.parse_args()
    os.makedirs(o.out, exist_ok=True)
    env = msvc_env()
    head = run_tree(o.tree, o.only, o.out, "head", env)
    fails = sorted(k for k, v in head.items() if v != "PASS")
    print(f"SUMMARY head: {len(head) - len(fails)}/{len(head)} pass; fail: {', '.join(fails) or 'none'}")
    if not o.base:
        sys.exit(1 if fails or not head else 0)
    base = run_tree(o.base, o.only, o.out, "base", env)
    new = sorted(k for k in fails if base.get(k) == "PASS")
    gone = sorted(k for k in head if k not in base)
    print(f"SUMMARY base: {sum(v == 'PASS' for v in base.values())}/{len(base)} pass; NEW failures: {', '.join(new) or 'none'}; "
          f"pre-existing: {', '.join(k for k in fails if k not in new) or 'none'}; only on head: {', '.join(gone) or 'none'}")
    sys.exit(1 if new else 0)


if __name__ == "__main__":
    main()
