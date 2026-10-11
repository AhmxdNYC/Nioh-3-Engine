"""Read-only RC1761 archive integrity and mod-clock paired-FPS audit."""
import hashlib
import json
import math
import statistics
import zipfile
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
for device, prefix, log in (
    ("ally", "up/", "up/mod.log"),
    ("pc2", "ab/", "mod_run01.log"),
):
    path = ROOT / "captures" / device / "rc1761" / f"{device}_rc1761.zip"
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None, "ZIP CRC failure"
        version = archive.read(log).decode(errors="replace").splitlines()[0]
        assert "Nioh3EngineFix 1.7.61 " in version
        manifest = [json.loads(line) for line in archive.read(prefix + "manifest.jsonl").decode().splitlines() if line.strip()]
        results = json.loads(archive.read(prefix + "results.json"))
        windows = [row for row in manifest if row.get("kind") == "window"]
        assert len(windows) == 12
        print(device, "ZIP CRC PASS; log version 1.7.61; SHA256", hashlib.sha256(path.read_bytes()).hexdigest())
        for item, report in results["items"].items():
            selected = [row for row in windows if row["item"] == item]
            assert len(selected) == 6
            assert [row["arm"] for row in selected] == list("ABBAAB")
            pairs = {}
            for row in selected:
                assert row["ack_ok"] and row["readback"] == row["value"]
                elapsed = (datetime.fromisoformat(row["t1_utc"].replace("Z", "+00:00")) - datetime.fromisoformat(row["t0_utc"].replace("Z", "+00:00"))).total_seconds()
                assert elapsed >= 13.5 and not row["dwell_ended"]
                clock = row["clock"]
                assert clock["mod_frames"] > 0 and clock["mod_qpc_us"] > 0
                pairs.setdefault(row["pair"], {})[row["arm"]] = clock["mod_frames"] * 1e6 / clock["mod_qpc_us"]
            assert len(pairs) == 3 and all(set(pair) == {"A", "B"} for pair in pairs.values())
            a = statistics.mean(pair["A"] for pair in pairs.values())
            b = statistics.mean(pair["B"] for pair in pairs.values())
            delta = [pair["B"] - pair["A"] for pair in pairs.values()]
            se = statistics.stdev(delta) / math.sqrt(3)
            calculated = {"a": a, "b": b, "delta": b - a, "se": se, "pct": (b - a) / a * 100, "ci": se * 4.303}
            for key, value in calculated.items():
                assert math.isclose(value, report["fps"][key], rel_tol=1e-9, abs_tol=1e-9), (device, item, key)
            print(f"  {item}: 6 valid windows, 3 pairs; {a:.3f}->{b:.3f} fps; {calculated['pct']:+.2f}% +/-{se/a*100:.2f}% SE; report arithmetic PASS")
        if device == "ally":
            assert archive.read("up/saves_before.txt") == archive.read("up/saves_after.txt")
            print("  archived save hash lists identical")
print("PASS: archive integrity, version logs, 24 windows and paired FPS arithmetic; no visual or whole-mod verdict")
