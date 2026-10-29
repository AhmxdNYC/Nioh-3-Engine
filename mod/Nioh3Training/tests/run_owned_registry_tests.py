"""Offline native-target lifecycle tests; does not open or modify the game."""
from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="n3-owned-") as temp:
    exe = Path(temp) / "owned_registry_test.exe"
    subprocess.run([sys.executable, "-m", "ziglang", "cc", "-target",
                    "x86_64-windows-gnu", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    str(Path(__file__).with_name("owned_registry_test.c")), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=30)
