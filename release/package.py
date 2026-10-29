"""Build the release zip (ONE zip for every Windows PC and Linux/Proton device) from the built ASI. No game needed, nothing in the game folder is touched.

  py -I release\\package.py                 release\\dist\\Nioh3EngineFix-<ver>[-rc].zip (profiles.json ALL; optional zip_suffix per profile)
  py -I release\\package.py --final         refuse 'provisional' profile entries and any check_release ERROR/WARN
  py -I release\\package.py --full          also run the harness/pattern tests first (check_release.py --full)
  py -I release\\package.py --allow-stale   package even if bin\\ was built from an older src version (testing only)
  py -I release\\package.py --out DIR       other output folder

Inputs: mod\\Nioh3EngineFix\\bin\\Nioh3EngineFix.asi (build.bat), release\\profiles\\*.ini (make_profiles.py),
release\\USER_GUIDE.md, mod\\Nioh3EngineFix\\bin\\README.md, the loader mod\\ally-package\\dinput8.dll and the shader
gpu-shaders\\replacement\\0x0104840c.file (both local only, never committed). A zip is '-rc' while its profile has
provisional entries.
"""
import hashlib
import json
import pathlib
import re
import subprocess
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
REL = ROOT / "release"
MOD = ROOT / "mod" / "Nioh3EngineFix"
LOADER = [ROOT / "mod" / "ally-package" / "dinput8.dll"]
SHADERS = [ROOT / "gpu-shaders" / "replacement", ROOT / "mod" / "ally-package" / "shader"]
# GHDM fast shader as shipped since 2026-10-06 (MODLOG 19:35); a different file must be re-proven first.
KNOWN_SHADER_SHA = {"0x0104840c.file": "129f03a0971e0a073b34", "0xa9910db8.file": "81007b3f"}


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def first(paths):
    return next((p for p in paths if p.exists()), None)


def main(argv):
    final, full, stale = "--final" in argv, "--full" in argv, "--allow-stale" in argv
    out = pathlib.Path(argv[argv.index("--out") + 1]) if "--out" in argv else REL / "dist"

    r = subprocess.run([sys.executable, "-I", str(REL / "make_profiles.py")], capture_output=True, text=True)
    if r.returncode:
        print(r.stdout + r.stderr)
        return 1
    cmd = [sys.executable, "-I", str(REL / "check_release.py")] + (["--full"] if full else []) + (["--strict"] if final else [])
    r = subprocess.run(cmd, capture_output=True, text=True)
    errs = [l for l in r.stdout.splitlines() if l.startswith("ERROR")]
    print(r.stdout.splitlines()[-1] if r.stdout else r.stderr)
    if r.returncode and not (stale and not final and all("MOD_VERSION" in e for e in errs)):
        print("\n".join(l for l in r.stdout.splitlines() if l.startswith(("ERROR", "WARN"))))
        print("refusing to package (fix the findings above; --allow-stale only skips a src/bin version mismatch)")
        return 1

    asi = MOD / "bin" / "Nioh3EngineFix.asi"
    m = re.search(rb"Nioh3EngineFix (\d+\.\d+\.\d+)", asi.read_bytes())
    ver = m.group(1).decode() if m else "unknown"
    loader = first(LOADER)
    if not loader:
        print("missing loader dinput8.dll (Ultimate ASI Loader) at mod\\ally-package\\")
        return 1
    if b"ini keys set to -1 (auto)" not in asi.read_bytes() and not stale:
        print("the ASI predates '-1 = auto by OS' (branch osauto): W6/GF/MQ = -1 would read as ON everywhere. Rebuild from a tree that has it.")
        return 1
    guide = REL / "USER_GUIDE.md"
    modref = MOD / "bin" / "README.md"
    profiles = {k: v for k, v in json.loads((REL / "profiles.json").read_text(encoding="utf-8")).items()
                if not k.startswith("_")}
    out.mkdir(parents=True, exist_ok=True)
    rc = 0
    for dev, p in profiles.items():
        prov = [o["key"] for o in p["overrides"] if o["status"] == "provisional"]
        prov += [s["file"] for s in p.get("shaders", []) if s["status"] == "provisional"]
        if final and prov:
            print(f"{dev}: provisional entries {prov} - not packaged with --final")
            rc = 1
            continue
        files = {
            "dinput8.dll": loader,
            "Nioh3EngineFix.asi": asi,
            "Nioh3EngineFix.ini": REL / "profiles" / f"Nioh3EngineFix.{dev}.ini",
            "README.md": guide,
            "README-MOD.md": modref,
            "install_mod.py": REL / "install_mod.py",
            "Install-Mod.cmd": REL / "Install-Mod.cmd",
            "install-mod.sh": REL / "install-mod.sh",
        }
        for s in p.get("shaders", []):
            if s["status"] != "kept":
                continue
            src = first([d / s["file"] for d in SHADERS])
            want = KNOWN_SHADER_SHA.get(s["file"])
            if not src:
                print(f"{dev}: shader {s['file']} not found locally - skipped")
                continue
            if want and not sha(src).startswith(want):
                print(f"{dev}: shader {s['file']} hash {sha(src)[:20]} != shipped {want} - skipped (re-prove it first)")
                continue
            files[f"optional/shader_dx12/{s['file']}"] = src
        lines = [f"Nioh3EngineFix {ver} - {p['title']}", "", "Changed from the default ini (status: why):"]
        lines += [f"  [{o['section']}] {o['key']} = {o['value']}   {o['status']}: {o['why']}" for o in p["overrides"]]
        lines += ["", "Optional shaders:"] + [f"  {s['file']} ({s['name']})  {s['status']}: {s['why']}"
                                              for s in p.get("shaders", [])]
        if p.get("optin"):
            lines += ["", "Off by default, opt-in in the ini or mod menu:"] + [f"  {o['key']}  ({o['for']}): {o['why']}" for o in p["optin"]]
        if prov:
            lines += ["", "RELEASE CANDIDATE: still waiting for the final device re-test: " + ", ".join(prov)]
        profile_txt = "\r\n".join(lines) + "\r\n"
        sums = [f"{sha(src)}  {name}" for name, src in files.items()]
        sums.append(f"{hashlib.sha256(profile_txt.encode()).hexdigest()}  PROFILE.txt")
        sfx = p.get("zip_suffix", f"-{dev}")
        name = f"Nioh3EngineFix-{ver}{sfx}{'-rc' if prov else ''}.zip"
        zp = out / name
        with zipfile.ZipFile(zp, "w", zipfile.ZIP_DEFLATED) as z:
            for arc, src in files.items():
                z.write(src, arc)
            z.writestr("PROFILE.txt", profile_txt)
            z.writestr("SHA256SUMS.txt", "\r\n".join(sums) + "\r\n")
        print(f"wrote {zp} ({zp.stat().st_size // 1024} KB, {len(files) + 2} files)")
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
