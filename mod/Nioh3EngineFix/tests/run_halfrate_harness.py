"""Offline half-rate policy: skip, reuse, camera cut. Not a game test.

    py -I tests/run_halfrate_harness.py
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build" / "hrtest"
OUT.mkdir(parents=True, exist_ok=True)
src = ROOT / "tests" / "halfrate_harness.cpp"
exe = OUT / "halfrate_harness.exe"
cmd = (
    f'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /permissive- "{src}" '
    f'/Fo"{OUT}\\\\" /Fe"{exe}"'
)
if subprocess.run(cmd, shell=True).returncode != 0:
    bat = ROOT.parent.parent / "tools" / "msvc" / "setup_x64.bat"
    if not bat.exists():
        sys.exit(1)
    wrap = f'call "{bat}" && {cmd}'
    if subprocess.run(wrap, shell=True).returncode != 0:
        sys.exit(1)
sys.exit(subprocess.run([str(exe)]).returncode)
