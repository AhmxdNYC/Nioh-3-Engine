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
import shutil
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
    dll_source = temp / "inventory.cpp"
    dll_source.write_text('extern "C" __declspec(dllexport) int InventoryProbe() { return 1; }')
    run(["cl", "/nologo", "/LD", "/MT", str(dll_source), f"/Fe{temp / 'inventory.dll'}"], cwd=temp)
    for name in [f"inventory-{i:02d}.dll" for i in range(70)] + ["gameoverlayrenderer64.dll", "System32/excluded-system.dll", "WinSxS/excluded-sxs.dll"]:
        target = temp / name
        target.parent.mkdir(exist_ok=True)
        shutil.copyfile(temp / "inventory.dll", target)
    exe = temp / "crashupload_test.exe"
    run(["cl", "/nologo", "/O2", "/MT", "/EHsc", "/std:c++17", "/DUNICODE", "/D_UNICODE",
         str(MOD / "tests/crashupload_test.cpp"), str(MOD / "src/crashupload.cpp"), str(MOD / "src/crashblame.cpp"), str(MOD / "src/inputlog.cpp"),
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
        artifacts = os.environ.get("CRASH_TEST_ARTIFACTS")
        if artifacts:
            Path(artifacts).mkdir(parents=True, exist_ok=True)
            shutil.copyfile(pending[0], Path(artifacts) / "combined-record.json")
        print("normal bytes", pending[0].stat().st_size, "modules", len(record["modules"]), "Training", record["training_tail"])
        assert record["perf"]["bound"] == "gpu" and record["perf"]["avg_fps"] == 60.0, record.get("perf")  # #97638
        assert record["registers"]["Rdx"] == "0x123456789abcdef0"
        assert len(record["stack_top"]) == 16
        mem = [x["fields"]["line"] for x in record["last"] if x.get("fields", {}).get("line", "").startswith("MEM")]  # #72
        assert mem and mem[0].startswith("MEM fault 0x0: free") and "MEM rbx+00: 1111 2222 3333 4444 5555 6666" in mem and "MEM rbx+30: c0ffee 8888 txt txt bbbb cccc" in mem, mem  # #98728
        assert all(len(m) <= 118 for m in mem), mem
        assert record["training_tail"]["included"] == 20
        assert record["id"] in ini.read_text()
        assert record["modules_available"] is True
        assert record["modules_truncated"] is True
        assert 1 <= len(record["modules"]) <= 64
        names = [m["name"] for m in record["modules"]]
        assert "inventory-00.dll" in names, names
        assert all("/" not in n and "\\" not in n for n in names)
        assert not any(n.lower() in ("gameoverlayrenderer64.dll", "excluded-system.dll", "excluded-sxs.dll", "kernel32.dll") for n in names)
        assert all(m["image_size"] > 0 for m in record["modules"])
        _, summary = api._issue_text(record, api.signature(record))
        assert "inventory-00.dll" in summary and "image bytes" in summary
        assert "Performance (last 10.0 s): 60.0 fps avg, 55.2 fps 1% low, bound gpu" in summary, summary  # #97638
        for bad in ([{"name": "C:/Users/person/mod.dll", "image_size": 1}], [record["modules"][0]] * 65):
            malformed = dict(record, modules=bad)
            code, _ = api.handle(json.dumps(malformed).encode(), "127.0.0.2")
            assert code == 400, code
        # Validate the exact C++ payload through the supplied API regression suite.
        output = run([sys.executable, str(ROOT / "site/tests/crash_api_test.py"), str(pending[0])], cwd=ROOT / "site")
        assert "ALL PASSED" in output, output
        print(output.strip())
        output = run([str(exe), str(ini), "send"], cwd=temp)
        assert "sent 1 pending 0" in output, output
        assert not list((temp / "Nioh3CrashReports/pending").glob("*.json"))
        for mode in ("badstack", "null", "utf16", "stress", "bigperf"):
            output = run([str(exe), str(ini), mode], cwd=temp)
            assert "ALL PASSED" in output, output
            sample = max((temp / "Nioh3CrashReports/pending").glob("*.json"), key=lambda p: p.stat().st_mtime_ns)
            variant = json.loads(sample.read_text())
            api.validate(variant)
            assert sample.stat().st_size <= api.MAX_BODY
            assert variant["modules_truncated"] and len(variant["modules"]) <= 64
            if mode == "bigperf":  # #97734: real-size perf (2-6 KB) + log tails at their cap, all inside 64 KB
                perf_len = len(json.dumps(variant["perf"], separators=(",", ":")))
                assert 2000 <= perf_len <= 6000, perf_len
                assert variant["perf"]["busiest_thread"]["tid"] == 4242 and len(variant["perf"]["threads"]) == 5
                assert sample.stat().st_size <= 64 * 1024, sample.stat().st_size
                print("bigperf perf bytes", perf_len)
            if mode in ("stress", "bigperf"):
                assert 0 < variant["training_tail"]["included"] < 20
                assert "EVENT24" in variant["log"]["Training"][-1]
            else:
                assert variant["training_tail"]["included"] == 20
            if artifacts:
                shutil.copyfile(sample, Path(artifacts) / ("combined-" + mode + ".json"))
            print(mode, "bytes", sample.stat().st_size, "modules", len(variant["modules"]), "Training", variant["training_tail"])
        print("ALL PASSED: five context modes with 70 DLLs, privacy/caps, API, localhost upload and pending removal")
    finally:
        server.shutdown()
        server.server_close()
