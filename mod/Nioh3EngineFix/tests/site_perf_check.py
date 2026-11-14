"""#98578: run one perf JSON through the site's own crash and player-report validators (offline, no network).
usage: py -I site_perf_check.py perf.json   -> exit 0 when both accept it unchanged."""
import importlib.util
import json
import sys
from pathlib import Path

SITE = Path(__file__).resolve().parents[3] / "site"


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


crash = load("chk_crash", SITE / "api/crash.py")
report = load("chk_report", SITE / "feedback-api/report_api.py")
perf = json.loads(Path(sys.argv[1]).read_text())
crash._check_perf(perf, lambda m: (_ for _ in ()).throw(crash.Reject(400, m)))
body = {"kind": "player", "id": "a" * 32, "player_text": "menu report", "snapshot": "s", "ver": "t", "perf": perf}
v = report.validate(json.dumps(body).encode())
if v.get("perf") != perf:
    sys.exit("report dropped the perf block")
print("site crash + report accept", Path(sys.argv[1]).name, len(json.dumps(perf)), "bytes")
