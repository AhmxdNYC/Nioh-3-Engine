"""Build crashupload_test and check crash recording, API validation and localhost retry.

Run from an MSVC developer environment. No game process or external endpoint used.
"""
from pathlib import Path
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import threading
from http.server import ThreadingHTTPServer

ROOT = Path(__file__).resolve().parents[3]
MOD = ROOT / "mod/Nioh3EngineFix"


def run(args, **kwargs):
    result = subprocess.run(args, capture_output=True, text=True, timeout=90, **kwargs)
    if result.returncode:
        print(result.stdout, result.stderr)
        raise RuntimeError(f"command failed: {args[0]} ({result.returncode})")
    return result.stdout


with tempfile.TemporaryDirectory(prefix="n3-crash-merge-") as folder:
    temp = Path(folder)
    exe = temp / "crashupload_test.exe"
    run(["cl", "/nologo", "/O2", "/MT", "/EHsc", "/std:c++17", "/DUNICODE", "/D_UNICODE",
         str(MOD / "tests/crashupload_test.cpp"), str(MOD / "src/crashupload.cpp"), str(MOD / "src/inputlog.cpp"),
         f"/Fe{exe}", "/link", "kernel32.lib", "user32.lib", "winhttp.lib", "bcrypt.lib", "version.lib"], cwd=temp)
    # Never inherit production storage credentials or Vercel mode into local tests.
    for key in ("GITHUB_TOKEN", "GH_TOKEN", "BLOB_READ_WRITE_TOKEN", "VERCEL", "VERCEL_ENV"):
        os.environ.pop(key, None)
    os.environ["CRASH_INBOX"] = str(temp / "inbox")
    spec = importlib.util.spec_from_file_location("local_crash_api", ROOT / "site/api/crash.py")
    api = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(api)
    server = ThreadingHTTPServer(("127.0.0.1", 0), api.handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    ini = temp / "test.ini"
    ini.write_text(f"[Report]\nCrashUrl=http://127.0.0.1:{server.server_port}/api/crash\nCrashUpload=1\nCrashSendDelayMs=600000\n")
    try:
        output = run([str(exe), str(ini), "record"], cwd=temp)
        assert "ALL PASSED" in output, output
        pending = list((temp / "Nioh3CrashReports/pending").glob("*.json"))
        assert len(pending) == 1
        record = json.loads(pending[0].read_text())
        assert record["id"] in ini.read_text()
        # Validate the exact C++ payload through the supplied API regression suite.
        output = run([sys.executable, str(ROOT / "site/tests/crash_api_test.py"), str(pending[0])], cwd=ROOT / "site")
        assert "ALL PASSED" in output, output
        print(output.strip())
        output = run([str(exe), str(ini), "send"], cwd=temp)
        assert "sent 1 pending 0" in output, output
        assert not list((temp / "Nioh3CrashReports/pending").glob("*.json"))
        print("ALL PASSED: crash recording, ini ID persistence, crash API, localhost upload and pending removal")
    finally:
        server.shutdown()
        server.server_close()
