"""Release regression check for Nioh3EngineFix (no game needed).

  py -I release\\check_release.py            quick: ini lint, src<->ini keys, profiles, version/copies
  py -I release\\check_release.py --full     + tests\\test_patterns.py and every tests\\run_*_harness.py
  py -I release\\check_release.py --strict   warnings also fail (use before a final release)

Exit 0 = no ERROR (and no WARN with --strict). Prints one line per finding and a summary line.
"""
import hashlib
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
MOD = ROOT / "mod" / "Nioh3EngineFix"
BASE = MOD / "Nioh3EngineFix.ini"
SEC_RE = re.compile(r"^\s*\[([^\]]+)\]\s*$")
KEY_RE = re.compile(r"^\s*([A-Za-z0-9_]+)\s*=")

errors, warns = [], []


def err(msg):
    errors.append(msg)
    print("ERROR " + msg)


def warn(msg):
    warns.append(msg)
    print("WARN  " + msg)


def ok(msg):
    print("OK    " + msg)


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def parse_ini(path):
    """Return ({section: set(keys)}, problems) - sections compared case-insensitively like GetPrivateProfile*."""
    lines = path.read_text(encoding="utf-8-sig", errors="replace").split("\n")
    keys, seen_sec, sec, blk = {}, {}, None, set()
    commented = set()
    for n, raw in enumerate(lines, 1):
        line = raw.rstrip("\r")
        bad = [c for c in line if ord(c) < 32 and c != "\t"]
        if bad:
            warn(f"{path.name}:{n} control char {', '.join(hex(ord(c)) for c in bad)} (a '\\x' escape in a path?)")
        if line.lstrip().startswith(";"):
            c = re.match(r"^\s*;\s*([A-Za-z][A-Za-z0-9_]*)\s*=\s*\S", line)
            if c and sec:
                commented.add((sec.lower(), c.group(1).lower()))
            continue
        m = SEC_RE.match(line)
        if m:
            sec = m.group(1).strip()
            low = sec.lower()
            if low in seen_sec:
                warn(f"{path.name}:{n} section [{sec}] repeated (first at line {seen_sec[low]}); Windows reads only the first")
            else:
                seen_sec[low] = n
            keys.setdefault(low, set())
            blk = set()
            continue
        k = KEY_RE.match(line)
        if k and sec:
            kl = k.group(1).lower()
            if kl in blk:
                err(f"{path.name}:{n} key {k.group(1)} twice in one [{sec}] block")
            blk.add(kl)
            keys[sec.lower()].add(kl)
    keys["_commented"] = commented
    return keys


def src_keys(sections):
    """(section, key) pairs the ASI reads, from the call patterns used in src (heuristic)."""
    out = set()
    pair = re.compile(r'L"([A-Za-z]\w*)"\s*,\s*L"([A-Za-z]\w*)"')
    implied = {  # file -> (pattern for single-key wrappers, section)
        "gpuopt.cpp": (r'IniInt\(L"(\w+)"', "gpuopt"),
        "psoswap.cpp": (r'IniInt\(L"(\w+)"', "shaderswap"),
        "readfix.cpp": (r'IniInt\(info\.iniPath,\s*L"(\w+)"', "patches"),
        "prefetch.cpp": (r'IniInt\(info\.iniPath,\s*L"(\w+)"', "patches"),
        "sp.cpp": (r'\bI\(L"(\w+)"', "profiler"),
        "gpuprof.cpp": (r'\bI\(L"(\w+)"', "profiler"),
        "tour.cpp": (r'\bI\(L"(Tour\w+)"', "debug"),
    }
    for f in sorted((MOD / "src").glob("*.cpp")):
        t = f.read_text(encoding="utf-8", errors="replace")
        for s, k in pair.findall(t):
            if s.lower() in sections:
                out.add((s.lower(), k.lower()))
        # keys from string arrays: armK[3] = {L"Tier30", ...}; ... I(L"VRR", armK[i], ...)
        arrays = {n: re.findall(r'L"(\w+)"', body) for n, body in re.findall(r'(\w+)\[\d*\]\s*=\s*\{([^}]*)\}', t)}
        for s, n in re.findall(r'L"([A-Za-z]\w*)"\s*,\s*(\w+)\[', t):
            if s.lower() in sections:
                out.update((s.lower(), k.lower()) for k in arrays.get(n, []))
        if f.name in implied:
            pat, s = implied[f.name]
            out.update((s, k.lower()) for k in re.findall(pat, t))
    return out


def check_keys(ini):
    commented = ini.pop("_commented")
    used = src_keys(set(ini))
    missing = sorted(f"[{s}] {k}" for s, k in used if k not in ini.get(s, set()) and (s, k) not in commented)
    unread = sorted(f"[{s}] {k}" for s in ini for k in ini[s] if (s, k) not in used)
    if missing:
        warn(f"{len(missing)} keys read by src but not in the base ini (undocumented; default used): {', '.join(missing)}")
    else:
        ok(f"every key read by src ({len(used)}) is in the base ini")
    if unread:
        warn(f"{len(unread)} base-ini keys not found by the src scan (dead key or menu-table read): {', '.join(unread)}")


def check_profiles():
    r = subprocess.run([sys.executable, "-I", str(ROOT / "release" / "make_profiles.py"), "--check"],
                       capture_output=True, text=True)
    for line in r.stdout.splitlines():
        (ok if line.startswith("OK") else err)(line.split(None, 1)[1] if line.startswith("OK") else line)
    if r.returncode and not r.stdout.strip():
        err("make_profiles.py --check failed: " + r.stderr.strip()[-300:])
    import json
    prof = json.loads((ROOT / "release" / "profiles.json").read_text(encoding="utf-8"))
    for dev, p in prof.items():
        if dev.startswith("_"):
            continue
        prov = [o["key"] for o in p["overrides"] if o["status"] == "provisional"]
        if prov:
            warn(f"profile {dev}: provisional (needs the final re-test): {', '.join(prov)}")
        for s in p.get("shaders", []):
            if s["status"] == "provisional":
                warn(f"profile {dev}: shader {s['file']} provisional")


def check_version():
    src = (MOD / "src" / "dllmain.cpp").read_text(encoding="utf-8", errors="replace")
    m = re.search(r'#define MOD_VERSION "([^"]+)"', src)
    if not m:
        err("MOD_VERSION not found in src\\dllmain.cpp")
        return None
    ver = m.group(1)
    asi = MOD / "bin" / "Nioh3EngineFix.asi"
    if not asi.exists():
        err("bin\\Nioh3EngineFix.asi missing (build.bat)")
    else:
        b = asi.read_bytes()
        if ver.encode() in b or ver.encode("utf-16-le") in b:
            ok(f"bin asi contains MOD_VERSION {ver}")
        else:
            err(f"bin asi does not contain MOD_VERSION {ver}: stale build (run build.bat after rebasing)")
    modlog = (ROOT / "MODLOG.md").read_text(encoding="utf-8", errors="replace")
    (ok if ver in modlog else warn)(f"MODLOG.md {'mentions' if ver in modlog else 'has no entry for'} {ver}")
    bini = MOD / "bin" / "Nioh3EngineFix.ini"
    if bini.exists() and sha(bini) != sha(BASE):
        warn("bin\\Nioh3EngineFix.ini differs from the source ini (build.bat copies it; rebuild or copy)")
    ally = ROOT / "mod" / "ally-package" / "Nioh3EngineFix.asi"
    if ally.exists() and asi.exists() and sha(ally) != sha(asi):
        warn("mod\\ally-package asi differs from bin (old Ally package; release\\package.py builds fresh zips)")
    return ver


def run_tests():
    tests = MOD / "tests"
    cmds = [("test_patterns.py", r"ALL \d+ SITES OK")] + [(p.name, None) for p in sorted(tests.glob("run_*_harness.py"))]
    for name, must in cmds:
        try:
            r = subprocess.run([sys.executable, "-I", str(tests / name)], cwd=str(MOD), capture_output=True,
                               text=True, timeout=900)
        except subprocess.TimeoutExpired:
            err(f"{name}: timeout")
            continue
        tail = (r.stdout.strip().splitlines() or [""])[-1][:160]
        good = r.returncode == 0 and (must is None or re.search(must, r.stdout))
        (ok if good else err)(f"{name}: rc={r.returncode} | {tail}")
        if not good:
            # Keep the actual assertion/guard refusal, not just a final FAILED
            # line. In particular, sys.exit("reason") is written to stderr.
            for label, output in (("stdout", r.stdout), ("stderr", r.stderr)):
                if output.strip():
                    print(f"      {label} (last 6000 characters):\n{output[-6000:].rstrip()}")


def main(argv):
    ini = parse_ini(BASE)
    n = {k: v for k, v in ini.items() if k != "_commented"}
    ok(f"base ini: {len(n)} sections, {sum(len(v) for v in n.values())} keys")
    check_keys(ini)
    check_profiles()
    check_version()
    if "--full" in argv:
        run_tests()
    strict = "--strict" in argv
    fail = bool(errors) or (strict and bool(warns))
    print(f"SUMMARY {'FAIL' if fail else 'PASS'}: {len(errors)} error(s), {len(warns)} warning(s)"
          + (" [strict]" if strict else ""))
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
