"""Build Nioh3-Setup-Linux.pyz: one runnable file (python3 Nioh3-Setup-Linux.pyz) with the full-install zip inside.

  py -I tools/setup/build_linux.py Nioh3-<ver>-full-install.zip OUT.pyz
"""
import pathlib
import re
import subprocess
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]


def main(src, out):
    src, out = pathlib.Path(src), pathlib.Path(out)
    if subprocess.run([sys.executable, "-I", str(ROOT / "release/full_install_zip.py"), "check", str(src)]).returncode:
        raise SystemExit("Payload is not a complete full-install zip")
    version = re.search(r"\d+\.\d+\.\d+", src.name).group(0)
    with open(out, "wb") as f:
        f.write(b"#!/usr/bin/env python3\n")
        with zipfile.ZipFile(f, "w", zipfile.ZIP_DEFLATED) as z:
            z.write(ROOT / "tools/setup/setup.py", "setup.py")
            z.writestr("__main__.py", "import sys, setup\nsys.exit(setup.main())\n")
            z.write(ROOT / "release/install_mod.py", "install_mod.py")
            z.write(ROOT / "tools/updater/updater.py", "updater.py")
            z.write(src, "payload.zip", compress_type=zipfile.ZIP_STORED)
            z.writestr("version.txt", version)
    print(f"Ready: {out} ({version})")


if __name__ == "__main__":
    main(*sys.argv[1:3])
