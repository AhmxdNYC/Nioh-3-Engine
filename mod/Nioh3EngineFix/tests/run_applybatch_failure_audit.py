"""Reproduce nine fail-open inspection paths in pinned pre-fix ApplyBatch.

No game, ASI build or deployment. This is a hazard audit, not a safety PASS.
Optional --source path validates an unrefactored replacement using the same cases.
"""
import pathlib
import subprocess
import sys
import argparse

args = argparse.ArgumentParser(description=__doc__)
args.add_argument("--source", type=pathlib.Path)
args = args.parse_args()

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
OUT = ROOT / "build" / "applybatch_failure_audit"
OUT.mkdir(parents=True, exist_ok=True)
template = (ROOT / "tests" / "applybatch_failure_audit.cpp").read_text()
baseline = subprocess.check_output(
    ["git", "show", "8c45cd0:mod/Nioh3EngineFix/src/dllmain.cpp"], cwd=REPO, text=True
)
setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():
    setup = pathlib.Path(r"C:\nioh3-engine\tools\msvc\setup_x64.bat")
versions = [("prefix", baseline)]
if args.source:
    versions.append(("fixed", args.source.read_text()))
for name, text in versions:
    start = text.index("bool ApplyBatch(")
    end = text.index("\n}", start) + 2
    cpp = OUT / (name + ".cpp")
    cpp.write_text(template.replace("// PRODUCTION_FUNCTION", text[start:end]))
    exe = OUT / (name + ".exe")
    command = f'call "{setup}" >nul && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /WX "{cpp}" /Fo"{OUT / (name + ".obj")}" /Fe"{exe}"'
    r = subprocess.run('cmd /s /c "' + command + '"', capture_output=True, text=True)
    if r.returncode:
        print(r.stdout[-4000:], r.stderr[-2000:])
        sys.exit("compile failed")
    r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
    print(name + ":\n" + r.stdout)
    expected_faults = {"snapshot", "first", "partial-next", "open-first", "open-second", "suspend-first", "suspend-second", "context-first", "context-second"}
    observed = {line[5:] for line in r.stdout.splitlines() if line.startswith("FAIL ")}
    if name == "fixed" and (r.returncode or "17 cases, 0 failures" not in r.stdout):
        sys.exit("fixed production proof failed")
    if name == "prefix" and (r.returncode != 1 or observed != expected_faults):
        sys.exit("pre-fix negative control did not reproduce all nine omissions")
    if name == "prefix":
        witnesses = {line for line in r.stdout.splitlines() if line.startswith("WITNESS ")}
        expected = {f"WITNESS {label} result=1 writes=2 flushes=1 unchanged=0 cleanup=1" for label in expected_faults}
        if witnesses != expected:
            sys.exit("negative control did not prove writes with balanced cleanup")
print("AUDIT PASSED: nine fail-open paths reproduced; not a repair/safety verdict")
if args.source:
    print("FIXED PROOF PASSED: 17 cases")
