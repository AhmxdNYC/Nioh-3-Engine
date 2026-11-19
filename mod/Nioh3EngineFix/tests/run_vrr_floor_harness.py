r"""run_vrr_floor_harness.py - [VRR] Tier120Floor offline test (src/caves.asm caveFps, GitHub #98). Not a game test.

    py -I tests/run_vrr_floor_harness.py      (from an MSVC x64 prompt; must end ALL PASSED)
Assembles src/caves.asm + tests/vrr_floor_shim.asm, builds tests/vrr_floor_harness.cpp and runs it: floor off keeps VRR 120,
2 s below the floor switches to the 60 tier (cap 60, fps 60, ticks 2/1), 30 s later it re-measures, a fast PC with dips and the
60 tier are untouched.
"""
import pathlib, subprocess, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "vrrfloortest"
OUT.mkdir(parents=True, exist_ok=True)
for src, obj in [(ROOT / "src/caves.asm", "caves.obj"), (ROOT / "tests/vrr_floor_shim.asm", "shim.obj")]:
    subprocess.run(["ml64", "/nologo", "/c", "/Fo" + str(OUT / obj), str(src)], check=True, stdout=subprocess.DEVNULL)
exe = OUT / "vrr_floor_harness.exe"
subprocess.run(["cl", "/nologo", "/O2", "/EHsc", "/Fo" + str(OUT) + "\\", "/Fe" + str(exe), str(ROOT / "tests/vrr_floor_harness.cpp"),
                str(OUT / "caves.obj"), str(OUT / "shim.obj")], check=True, stdout=subprocess.DEVNULL)
r = subprocess.run([str(exe)], capture_output=True, text=True)
print(r.stdout, end="")
sys.exit(r.returncode)
