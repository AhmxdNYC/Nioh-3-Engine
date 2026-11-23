"""POST /api/report: player reports from the in-game mod menu Report box.

Body (JSON, at most 256 KB):
  {"kind":"player","id":"<32 hex install id>","ver":"1.8.5","via":"keyboard|pad|steam","t":"2026-11-04T12:00:00",
   "player_text":"...", "snapshot":"<the Nioh3EngineFix report text>", "event_count":N, "events":[{...}],
   "input_count":N, "input":[{...}]}

Checks: size cap, schema (unknown fields -> 400), install id shape, player_text 1..1000 chars, at most 200 events and
400 input rows, simple rate limit (20 per 10 minutes per IP, 60 per hour per install id; per function instance).
Every string is scrubbed again (user folders -> <user>, 17-digit Steam ids -> <steamid>) before it is stored.

Storage, first that applies:
  BLOB_READ_WRITE_TOKEN set  -> Vercel Blob  reports/YYYY-MM-DD/<time>-<id8>-<rand>.json
  on Vercel (GITHUB_TOKEN)   -> one GitHub issue (label mod-report), like api/submit.py; the full JSON follows in comments
  this PC (py report_api.py) -> %LOCALAPPDATA%/nioh3-site-inbox/reports/<time>-<id8>.json
Reply: 202 {"ok":true,"stored":1,"ref":"..."}; errors {"ok":false,"error":"..."} with 400 / 413 / 429 / 502 / 503.
"""

from __future__ import annotations

import json
import os
import re
import secrets
import sys
import threading
import time
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

MAX_BODY = 256 * 1024
MAX_TEXT = 1000
MAX_SNAPSHOT = 220 * 1024
MAX_EVENTS = 200
MAX_INPUT = 400
FIELDS = {"kind", "id", "ver", "via", "t", "player_text", "snapshot", "event_count", "events", "input_count", "input", "perf"}
SHORT = {"ver": 40, "via": 16, "t": 32}
ID_RE = re.compile(r"^[0-9a-f]{32}$")
PATH_RE = re.compile(r"(?i)([\\/](?:users|home)[\\/])(?!<user>)([^\\/\"'<>|\r\n\t]+)")
STEAM_RE = re.compile(r"(?<!\d)7656119\d{10}(?!\d)")

RATE: dict[str, list[float]] = {}
RATE_LOCK = threading.Lock()


def on_vercel() -> bool:
    return bool(os.environ.get("VERCEL"))


def inbox() -> Path:
    if on_vercel():
        return Path("/tmp/nioh3-site-inbox/reports")
    return Path(os.environ.get("LOCALAPPDATA", str(ROOT))) / "nioh3-site-inbox" / "reports"


def rate_ok(key: str, limit: int, window: float) -> bool:
    now = time.time()
    with RATE_LOCK:
        hits = [t for t in RATE.get(key, []) if now - t < window]
        ok = len(hits) < limit
        if ok:
            hits.append(now)
        RATE[key] = hits
        return ok


def scrub(value):
    if isinstance(value, str):
        return STEAM_RE.sub("<steamid>", PATH_RE.sub(lambda m: m.group(1) + "<user>", value))
    if isinstance(value, list):
        return [scrub(v) for v in value]
    if isinstance(value, dict):
        return {str(k)[:40]: scrub(v) for k, v in value.items()}
    return value


# ---- performance snapshot ("perf"): last ~10 s fps / 1 % low / CPU-GPU bound / hardware / settings ---------------
PERF_MAX = 8192


def check_perf(v, fail) -> None:
    """Bounded JSON tree: object root, depth <= 3, <= 48 keys / 64 items per level, strings <= 300, keys <= 64."""
    if not isinstance(v, dict):
        fail("perf must be an object")
    if len(json.dumps(v)) > PERF_MAX:
        fail(f"perf is larger than {PERF_MAX} bytes")

    def walk(x, depth, path):
        if depth > 3:
            fail(f"{path} is nested too deep")
        if isinstance(x, dict):
            if len(x) > 48:
                fail(f"{path} has too many keys")
            for k, y in x.items():
                if not isinstance(k, str) or len(k) > 64:
                    fail(f"{path} has a bad key")
                walk(y, depth + 1, f"{path}.{k}")
        elif isinstance(x, list):
            if len(x) > 64:
                fail(f"{path} has too many items")
            for i, y in enumerate(x):
                walk(y, depth + 1, f"{path}[{i}]")
        elif isinstance(x, str):
            if len(x) > 300:
                fail(f"{path} is longer than 300")
        elif x is not None and not isinstance(x, (bool, int, float)):
            fail(f"{path} has a bad value")

    walk(v, 0, "perf")


def perf_line(rec: dict) -> str:
    p = rec.get("perf")
    if not isinstance(p, dict):
        return "- Performance: (not in this record)"
    return (f"- Performance (last {p.get('window_s', '?')} s): {p.get('avg_fps', '?')} fps avg, {p.get('low1_fps', '?')} fps 1% low, "
            f"bound {p.get('bound', '?')} ({p.get('bound_why', '')}); CPU {p.get('cpu', '?')}; GPU {p.get('gpu', '?')} driver "
            f"{p.get('driver', '?')}; {p.get('backbuffer', '?')} upscaler {p.get('upscaler', '?')} frame gen {p.get('frame_gen', '?')}")


def validate(raw: bytes) -> dict:
    if not raw or len(raw) > MAX_BODY:
        raise ValueError("empty or larger than 256 KB")
    try:
        body = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"not JSON: {exc}") from exc
    if not isinstance(body, dict):
        raise ValueError("body must be a JSON object")
    extra = set(body) - FIELDS
    if extra:
        raise ValueError("unknown field(s): " + ", ".join(sorted(extra))[:200])
    if body.get("kind", "player") != "player":
        raise ValueError("kind must be player")
    if not isinstance(body.get("id"), str) or not ID_RE.match(body["id"]):
        raise ValueError("id must be 32 lowercase hex characters")
    text = body.get("player_text")
    if not isinstance(text, str) or not text.strip():
        raise ValueError("player_text is empty")
    if len(text) > MAX_TEXT:
        raise ValueError(f"player_text is longer than {MAX_TEXT} characters")
    for key, cap in SHORT.items():
        if key in body and (not isinstance(body[key], str) or len(body[key]) > cap):
            raise ValueError(f"{key} must be a string of at most {cap} characters")
    snap = body.get("snapshot", "")
    if not isinstance(snap, str) or len(snap) > MAX_SNAPSHOT:
        raise ValueError("snapshot must be a string under 220 KB")
    for key, cap in (("events", MAX_EVENTS), ("input", MAX_INPUT)):
        rows = body.get(key, [])
        if not isinstance(rows, list) or len(rows) > cap or not all(isinstance(r, dict) for r in rows):
            raise ValueError(f"{key} must be a list of at most {cap} objects")
    for key in ("event_count", "input_count"):
        if key in body and not isinstance(body[key], int):
            raise ValueError(f"{key} must be a number")
    if "perf" in body:
        # A bad perf block is dropped (and logged), never a reason to lose the report
        class _BadPerf(Exception):
            pass

        def fail(msg):
            raise _BadPerf(msg)
        try:
            check_perf(body["perf"], fail)
        except _BadPerf as e:
            print(f"report: perf block dropped: {e}", file=sys.stderr)
            del body["perf"]
    return scrub(body)


def store_blob(record: dict, name: str) -> str:
    token = os.environ["BLOB_READ_WRITE_TOKEN"]
    path = f"reports/{time.strftime('%Y-%m-%d')}/{name}"
    req = urllib.request.Request(f"https://blob.vercel-storage.com/{path}", data=json.dumps(record).encode("utf-8"), method="PUT")
    req.add_header("Authorization", f"Bearer {token}")
    req.add_header("x-api-version", "7")
    req.add_header("x-content-type", "application/json")
    req.add_header("x-add-random-suffix", "0")
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            json.loads(resp.read().decode("utf-8", "replace") or "{}")
    except urllib.error.HTTPError as exc:
        raise RuntimeError(f"blob {exc.code}") from exc
    return path


def store_github(record: dict) -> str:
    # Self-contained in the unified site deployment; no unshipped server.py.
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    repo = os.environ.get("GITHUB_REPO", "AhmxdNYC/Nioh-3-Engine")
    if not token:
        raise RuntimeError("GitHub token unavailable")

    def post(path, payload):
        req = urllib.request.Request(
            f"https://api.github.com/repos/{repo}/{path}",
            data=json.dumps(payload).encode("utf-8"), method="POST",
            headers={"Authorization": f"Bearer {token}", "Accept": "application/vnd.github+json",
                     "Content-Type": "application/json", "X-GitHub-Api-Version": "2022-11-28"})
        with urllib.request.urlopen(req, timeout=20) as response:
            return json.load(response)

    def create_issue(title, body, labels):
        issue = post("issues", {"title": title, "body": body, "labels": labels})
        return issue["number"], issue.get("html_url", "")

    def comment(number, body):
        return post(f"issues/{number}/comments", {"body": body})

    text = record["player_text"].strip()
    first = text.splitlines()[0] if text else "report"
    title = f"Mod report: {first[:80]}"
    full = json.dumps(record, indent=1)
    head = [
        "### Player report (mod menu Report box)",
        "",
        "> " + text.replace("\n", "\n> "),
        "",
        f"- Mod version: `{record.get('ver', '')}`  typed with: `{record.get('via', '')}`  at: `{record.get('t', '')}`",
        f"- Install id: `{record['id'][:8]}...`",
        perf_line(record),
        f"- Snapshot: {len(record.get('snapshot', ''))} characters, events: {len(record.get('events', []))}, input rows: {len(record.get('input', []))}",
        "",
        "The full JSON record follows in this issue and its comments.",
        "",
    ]
    chunks = [full[i:i + 60000] for i in range(0, len(full), 60000)] or [""]
    body = "\n".join(head) + "```json\n" + chunks[0] + "\n```\n"
    number, url = create_issue(title, body, ["site-feedback", "mod-report"])
    for i, chunk in enumerate(chunks[1:], start=2):
        comment(number, f"Part {i}/{len(chunks)}\n```json\n{chunk}\n```")
    return f"#{number}"


def store_file(record: dict, name: str) -> str:
    folder = inbox()
    folder.mkdir(parents=True, exist_ok=True)
    (folder / name).write_text(json.dumps(record, indent=1), encoding="utf-8")
    return str(folder / name)


def handle(raw: bytes, ip: str) -> tuple[int, dict]:
    if not rate_ok("ip:" + ip, 20, 600):
        return 429, {"ok": False, "error": "too many reports from this address, try again in a few minutes"}
    try:
        record = validate(raw)
    except ValueError as exc:
        return 400, {"ok": False, "error": str(exc)}
    if not rate_ok("id:" + record["id"], 60, 3600):
        return 429, {"ok": False, "error": "too many reports from this install, try again later"}
    record["received"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    name = f"{time.strftime('%Y%m%d-%H%M%S')}-{record['id'][:8]}-{secrets.token_hex(3)}.json"
    try:
        if os.environ.get("BLOB_READ_WRITE_TOKEN"):
            ref = store_blob(record, name)
        elif on_vercel():
            ref = store_github(record)
        else:
            ref = store_file(record, name)
    except Exception as exc:  # storage down: the mod keeps its local copy
        return 502, {"ok": False, "error": f"storage failed: {str(exc)[:200]}"}
    return 202, {"ok": True, "stored": 1, "ref": ref}


class ReportHandler(BaseHTTPRequestHandler):
    server_version = "nioh3-mod-report/1.0"

    def log_message(self, fmt: str, *args) -> None:
        return

    def send_json(self, code: int, payload: dict) -> None:
        raw = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self) -> None:  # noqa: N802
        self.send_json(405, {"ok": False, "error": "POST a report from the mod menu"})

    def do_POST(self) -> None:  # noqa: N802
        length = int(self.headers.get("Content-Length", "0") or "0")
        if length <= 0 or length > MAX_BODY:
            self.send_json(413, {"ok": False, "error": "report is empty or larger than 256 KB"})
            return
        raw = self.rfile.read(length)
        ip = (self.headers.get("x-forwarded-for") or self.client_address[0]).split(",")[0].strip()
        code, payload = handle(raw, ip)
        self.send_json(code, payload)


def main() -> None:
    port = int(os.environ.get("REPORT_PORT", "8792"))
    httpd = ThreadingHTTPServer(("127.0.0.1", port), ReportHandler)
    print(f"report api http://127.0.0.1:{port}/api/report -> {inbox()}", flush=True)
    httpd.serve_forever()


if __name__ == "__main__":
    main()
