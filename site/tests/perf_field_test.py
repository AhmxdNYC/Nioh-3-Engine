"""#97638: crash records and player reports may carry "perf" (EngineFix perfsnap). Offline, no network.
usage (any folder): py site/tests/perf_field_test.py [perf.json from mod/Nioh3EngineFix/tests/run_perfsnap_test.py]"""
import importlib.util
import json
import sys
from pathlib import Path

SITE = Path(__file__).resolve().parents[1]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


crash = load("perf_crash", SITE / "api/crash.py")
report = load("perf_report", SITE / "feedback-api/report_api.py")
perf = json.loads(Path(sys.argv[1]).read_text()) if len(sys.argv) > 1 else {
    "window_s": 10.0, "frames": 601, "avg_fps": 60.0, "low1_fps": 58.1, "bound": "gpu", "bound_why": "waits on the GPU 42%",
    "cpu": "AMD Ryzen 7 7800X3D", "gpu": "AMD Radeon RX 9070", "driver": "32.0.21025.1024", "ram_mb": {"total": 32448, "used": 14210},
    "vram_mb": {"total": 16304, "used": 9120, "budget": 15480}, "backbuffer": "2560x1440", "upscaler": "FSR", "frame_gen": "OFF",
    "game_settings": {"REN_RAT": "75"}, "mod_settings": {"VRR": {"MaxFps": "120"}, "patches_off": ["MW"]},
    "parts": [{"name": "GPU fence wait (GF)", "ms": 6.67}]}
fails = 0


def check(name, ok):
    global fails
    print(("PASS " if ok else "FAIL ") + name)
    fails += 0 if ok else 1


def crash_ok(p):
    try:
        crash._check_perf(p, lambda m: (_ for _ in ()).throw(crash.Reject(400, m)))
        return True
    except crash.Reject:
        return False


check("crash accepts perf", crash_ok(perf))
check("crash TOP_KEYS has perf", "perf" in crash.TOP_KEYS)
check("crash rejects a list", not crash_ok([1]))
check("crash rejects a long string", not crash_ok({"gpu": "x" * 301}))
check("crash rejects deep nesting", not crash_ok({"a": {"b": {"c": {"d": 1}}}}))
check("crash rejects > 8 KB", not crash_ok({"k%d" % i: "x" * 290 for i in range(40)}))
rec = {"perf": perf}
line = crash._perf_line(rec)
check("crash issue line", "60.0 fps avg" in line and "bound gpu" in line)
check("crash issue line without perf", "not in this record" in crash._perf_line({}))
body = {"kind": "player", "id": "a" * 32, "player_text": "slow in Kyoto", "snapshot": "s", "ver": "t", "perf": perf}
try:
    v = report.validate(json.dumps(body).encode())
    check("report accepts perf", v["perf"]["bound"] == "gpu")
except ValueError as e:
    check("report accepts perf: " + str(e), False)
for bad in ([1], {"gpu": "x" * 301}):
    try:
        report.validate(json.dumps(dict(body, perf=bad)).encode())
        check("report rejects bad perf", False)
    except ValueError:
        check("report rejects bad perf", True)
try:
    report.validate(json.dumps({k: v for k, v in body.items() if k != "perf"}).encode())
    check("report without perf still accepted", True)
except ValueError as e:
    check("report without perf: " + str(e), False)
check("report issue line", "1% low" in report.perf_line(body))
print("ALL PASSED" if not fails else f"{fails} FAILED")
sys.exit(1 if fails else 0)
