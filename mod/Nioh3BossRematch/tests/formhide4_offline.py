#!/usr/bin/env python3
"""Exercise mode 4 ownership/restore behavior in local buffers; never accesses the game."""
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
source = (here.parent / "src/bosslog.cpp").read_text()
start = source.index("// pc #55953 / ally #55928:")
end = source.index("void FormHideDirectTick(", start)
with tempfile.TemporaryDirectory(prefix="formhide4-offline-") as tmp:
    root = Path(tmp)
    (root / "formhide4_extracted.cpp").write_text(source[start:end])
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I", str(root), str(here / "formhide4_fixture.cpp"),
                    "-o", str(root / "fixture")], check=True)
    subprocess.run([str(root / "fixture")], check=True)
