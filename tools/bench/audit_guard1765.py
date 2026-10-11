"""Read-only audit of bridge230; startup evidence only, no benchmark verdict."""
import csv
import hashlib
import io
import math
import re
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ASI = "997C1C0E92BAB3E936C033DEECA4E53B8B37DD3A3866FB2E2C31EB1CB0C85537"
INI = ["93ca1930165fb49073e0e4cb291c48164936b2b38041b14ee82486f2766b95b6",
       "e9077db663f64b5a5d74789fd4afd23450bfeefcb7947b85e291a4c654c86bf1"]
path = ROOT / "captures/pc2/guard1765/pc2_guard1765.zip"
assert hashlib.sha256(path.read_bytes()).hexdigest() == "03a11e51e47c7c3595cb4d246b60be6c21bddd440c49ea0d35aede4e9a0435d6"
with zipfile.ZipFile(path) as archive:
    assert archive.testzip() is None
    inis = []
    for index, (run, pcm, pid) in enumerate((("run1_pcm0", 0, "23092"), ("run2_pcm2", 2, "21544"))):
        def read(name):
            return archive.read(run + "/" + name).decode("utf-8-sig")
        ini = archive.read(run + "/ini_used.ini")
        assert hashlib.sha256(ini).hexdigest() == INI[index]
        inis.append(ini.decode("utf-8-sig").splitlines())
        guard, log = read("guard.log"), read("mod_run.log")
        assert ASI in guard and INI[index].upper() in guard
        assert "Nioh3EngineFix 1.7.65 (built Oct 11 2026)" in log
        for required in ("render-config options installed", "GI gather scale installed", "meshlet LOD scale installed", "GPU particle share installed", "AutoContinue: installed", "FrameGenSettingWrite", "CBN ON, CBT ON", "installing the Present hook", "OSD: stats: rendered"):
            assert required in log, (run, required)
        assert not re.search(r"inspection.abort|not installed|mismatch|\bERROR\b|WATCHDOG", log, re.I)
        if pcm == 0:
            assert "PCM:" not in log and "stats/s PCM" not in log
        else:
            assert "graphics-PSO lookup memo installed" in log
            stats = [line for line in log.splitlines() if "stats/s PCM" in line]
            assert len(stats) == 2
            assert all("switch off | lookups 0/s, hits 0/s" in line for line in stats)
        assert "game closed gracefully from the menu" in guard
        assert "saves unchanged True; entry hashes equal: all (asi, ini, log, config)" in guard
        assert "lock nioh3-pc2: acquired" in guard and "lock release:" in guard
        assert "frames: PresentMon rows 0 -> 0" in guard and "frames False" in guard
        rows = list(csv.DictReader(io.StringIO(read("presentmon.csv"))))
        complete, incomplete = [], []
        for row in rows:
            target = complete if None not in row and all(value is not None for value in row.values()) else incomplete
            target.append(row)
        assert len(incomplete) == 1 and incomplete[0] is rows[-1]
        assert len(complete) == (7937, 7943)[index]
        assert all(row["Application"] == "Nioh3.exe" and row["ProcessID"] == pid for row in complete)
        assert all(math.isfinite(float(row["FrameTime"])) and float(row["FrameTime"]) > 0 for row in complete)
        assert archive.read(run + "/shot_menu.png").startswith(b"\x89PNG\r\n\x1a\n")
        print(f"{run}: INI/hash/log/install evidence PASS; {len(complete)} complete game-frame rows, 1 incomplete tail excluded; graceful exit/restoration attested")
    assert len(inis[0]) == len(inis[1])
    assert [(i + 1, a, b) for i, (a, b) in enumerate(zip(*inis)) if a != b] == [(332, "PCM = 0", "PCM = 2")]
print("PASS: ZIP CRC, exact INIs/one-line PCM delta, native startup/frame evidence. Installed ASI and restoration remain log attestations; no field, equivalence, serving or performance verdict.")
