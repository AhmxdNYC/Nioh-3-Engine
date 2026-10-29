"""Finite offline production-wrapper/native-byte witness; never launches a game.

Default success means modeled counterexamples reproduced, not production safety.
--require-safe returns3 when they exist. No normal synthetic/30k replay is run.
"""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys

OWN = Path(__file__).resolve().parents[1]
MAIN = Path(r"C:\nioh3-engine")
ap = argparse.ArgumentParser()
ap.add_argument("--source-root", type=Path, default=OWN)
ap.add_argument("--require-safe", action="store_true")
args = ap.parse_args()
marker = Path(os.environ["LOCALAPPDATA"])/"nioh3-codex/pc-benchmark-active"
if marker.exists(): sys.exit("REFUSED: PC benchmark marker is active; no compile or fixture run")
source = args.source_root.resolve()
for name in ("cbvupload.h", "cbvstage.h"):
    if (OWN/"src"/name).read_bytes() != (source/"src"/name).read_bytes():
        sys.exit("source/test header mismatch: "+name)
for name in ("cbvupload.cpp", "cbvstage.cpp"):
    print(name, "SHA256", hashlib.sha256((source/"src"/name).read_bytes()).hexdigest(), flush=True)
print("relocation helper SHA256", hashlib.sha256((OWN/"tests/cbvupload_harness.cpp").read_bytes()).hexdigest(), flush=True)
setup = MAIN/"tools/msvc/setup_x64.bat"
dump = MAIN/"re/Nioh3.dump.exe"
if not setup.is_file() or not dump.is_file(): sys.exit("missing existing MSVC setup/native dump")
out = OWN/"build/cbu_recreation_witness"
out.mkdir(parents=True, exist_ok=True)
cmd = ('call "{setup}" >nul && cl /nologo /O2 /MT /EHsc /std:c++17 /W4 /DCBU_HARNESS '
       '"{test}" "{cbv}" "{stage}" /Fo"{out}\\\\" /Fe"{out}\\cbu_recreation_witness.exe" /link kernel32.lib').format(
    setup=setup, test=OWN/"tests/cbvupload_recreation_witness.cpp", cbv=source/"src/cbvupload.cpp", stage=source/"src/cbvstage.cpp", out=out)
compiled = subprocess.run('cmd /d /s /c "'+cmd+'"', capture_output=True, text=True, cwd=OWN, timeout=120)
if compiled.returncode:
    print(compiled.stdout[-5000:], compiled.stderr[-2000:]); sys.exit(compiled.returncode)
if marker.exists(): sys.exit("REFUSED: benchmark marker appeared after compile; no fixture run")
command = [str(out/"cbu_recreation_witness.exe"), str(dump)]
if args.require_safe: command.append("--require-safe")
result = subprocess.run(command, cwd=OWN, timeout=30)
sys.exit(result.returncode)
