"""Build and run test_holdkeygap.cpp (GH #147 HoldKeyGap window rule + tracker table) with asserts ON.

/Od /UNDEBUG so assert() is never compiled out (zig/-O2 NDEBUG lesson #119198). Run from an MSVC developer
environment. No game process. Named run_*_harness.py so release/check_release.py --full, tools/testhub and
tools/testmatrix/regsuite.py all pick it up every build.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

MOD = Path(__file__).resolve().parents[1]


def main(t: Path) -> int:
    b = subprocess.run(["cl", "/nologo", "/Od", "/UNDEBUG", "/MT", "/EHsc", "/std:c++17", "/W4",
                        "/I", str(MOD / "src"), str(MOD / "tests/test_holdkeygap.cpp"), "/Fetest_holdkeygap.exe"],
                       cwd=t, capture_output=True, text=True, timeout=180)
    if b.returncode:
        print(b.stdout, b.stderr)
        print("FAIL build test_holdkeygap")
        return 1
    r = subprocess.run([str(t / "test_holdkeygap.exe")], cwd=t, capture_output=True, text=True, timeout=120)
    print(r.stdout)
    print(r.stderr)
    print(f"exit code {r.returncode:#x}")
    return 0 if r.returncode == 0 and "PASS test_holdkeygap" in r.stdout else 1


if len(sys.argv) > 1:  # keep the build folder for debugging
    Path(sys.argv[1]).mkdir(parents=True, exist_ok=True)
    raise SystemExit(main(Path(sys.argv[1]).resolve()))
with tempfile.TemporaryDirectory(prefix="n3-hkg-") as d:
    raise SystemExit(main(Path(d)))
