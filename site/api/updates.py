"""Update counter for the README "updates" badge.

POST /api/updates   body {"schema":1,"from":"1.8.23","to":"1.8.24"}
    Sent once by Nioh3Updater after an update that actually changed files (a second run is a
    no-op and sends nothing). No install id, machine id, path or account: only the two version
    numbers. Answer 202 {"ok":true,"stored":...}. Bad body 400, too many from one IP 429.
GET  /api/updates   shields.io endpoint JSON {"schemaVersion":1,"label":"updates","message":"<n>"}
    n = number of stored update records. Edge-cached for an hour.

Storage, first that is configured:
  BLOB_READ_WRITE_TOKEN  Vercel Blob updates/<to>/<time>-<random>.json (one tiny object per update)
  off Vercel (PC)        %LOCALAPPDATA%/nioh3-site-inbox/updates/<to>/<time>-<random>.json
On Vercel without a token the POST answers 503 and the GET shows "n/a".

Local run: py api/updates.py [port]   (127.0.0.1, default 8793)
"""

from __future__ import annotations

import json
import os
import re
import threading
import time
import urllib.parse
import urllib.request
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

MAX_BODY = 512
VERSION = re.compile(r"\d{1,3}\.\d{1,3}\.\d{1,4}")
PREFIX = "updates/"

_RATE_LOCK = threading.Lock()
_RATE_IP: dict[str, list[float]] = {}


class Reject(Exception):
    def __init__(self, code: int, msg: str):
        super().__init__(msg)
        self.code = code


def validate(rec) -> dict:
    if not isinstance(rec, dict) or set(rec) - {"schema", "from", "to"}:
        raise Reject(400, "body must be {schema, from, to}")
    if rec.get("schema") != 1:
        raise Reject(400, "schema must be 1")
    to = rec.get("to")
    if not isinstance(to, str) or not VERSION.fullmatch(to):
        raise Reject(400, "to must be a version like 1.8.24")
    frm = rec.get("from")
    if frm is not None and (not isinstance(frm, str) or not VERSION.fullmatch(frm)):
        raise Reject(400, "from must be a version or null")
    return {"schema": 1, "from": frm, "to": to}


def rate_ok(ip: str, now: float | None = None) -> bool:
    """At most 10 per IP per hour (per warm instance; a household with several PCs still fits)."""
    now = time.time() if now is None else now
    with _RATE_LOCK:
        hits = [t for t in _RATE_IP.get(ip, []) if now - t < 3600]
        ok = len(hits) < 10
        if ok:
            hits.append(now)
        _RATE_IP[ip] = hits
        return ok


def _blob_req(url: str, token: str, data: bytes | None = None, method: str = "GET") -> dict:
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Authorization", f"Bearer {token}")
    req.add_header("x-api-version", "7")
    if data is not None:
        req.add_header("x-content-type", "application/json")
        req.add_header("x-add-random-suffix", "0")
    with urllib.request.urlopen(req, timeout=20) as resp:
        return json.loads(resp.read().decode("utf-8", "replace") or "{}")


def _local_root() -> Path:
    base = os.environ.get("UPDATES_INBOX") or str(
        Path(os.environ.get("LOCALAPPDATA", str(Path(__file__).resolve().parents[1]))) / "nioh3-site-inbox" / "updates")
    return Path(base)


def store(rec: dict) -> str:
    name = f"{rec['to']}/{int(time.time())}-{uuid.uuid4().hex[:12]}.json"
    data = json.dumps({**rec, "t": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}).encode("utf-8")
    token = os.environ.get("BLOB_READ_WRITE_TOKEN", "").strip()
    if token:
        out = _blob_req(f"https://blob.vercel-storage.com/{PREFIX}{name}", token, data, "PUT")
        return "blob:" + (out.get("pathname") or PREFIX + name)
    if os.environ.get("VERCEL"):
        raise Reject(503, "update counter storage is not configured")
    dest = _local_root() / name
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(data)
    return "file"


def count() -> int | None:
    token = os.environ.get("BLOB_READ_WRITE_TOKEN", "").strip()
    if token:
        n, cursor = 0, ""
        for _ in range(200):  # 200 pages x 1000 = 200k updates before the badge stops growing
            q = {"prefix": PREFIX, "limit": "1000"}
            if cursor:
                q["cursor"] = cursor
            out = _blob_req("https://blob.vercel-storage.com?" + urllib.parse.urlencode(q), token)
            n += len(out.get("blobs") or [])
            cursor = out.get("cursor") or ""
            if not out.get("hasMore") or not cursor:
                break
        return n
    if os.environ.get("VERCEL"):
        return None
    root = _local_root()
    return sum(1 for _ in root.glob("*/*.json")) if root.exists() else 0


def badge(n: int | None) -> dict:
    return {"schemaVersion": 1, "label": "updates", "message": "n/a" if n is None else str(n),
            "color": "lightgrey" if n is None else "blue", "cacheSeconds": 3600}


def handle_post(body: bytes, ip: str) -> tuple[int, dict]:
    if not body or len(body) > MAX_BODY:
        return 413, {"ok": False, "error": f"body is empty or larger than {MAX_BODY} bytes"}
    try:
        rec = validate(json.loads(body.decode("utf-8")))
        if not rate_ok(ip):
            return 429, {"ok": False, "error": "too many update records"}
        where = store(rec)
    except (UnicodeDecodeError, json.JSONDecodeError):
        return 400, {"ok": False, "error": "body is not UTF-8 JSON"}
    except Reject as exc:
        return exc.code, {"ok": False, "error": str(exc)}
    except Exception as exc:
        return 503, {"ok": False, "error": f"storage failed: {str(exc)[:200]}"}
    return 202, {"ok": True, "stored": where.split(":", 1)[0]}


def handle_get() -> tuple[int, dict]:
    try:
        return 200, badge(count())
    except Exception:
        return 200, badge(None)


class handler(BaseHTTPRequestHandler):
    server_version = "nioh3-updates/1.0"

    def log_message(self, fmt: str, *args) -> None:
        return

    def _send(self, code: int, payload: dict, cache: str = "no-store") -> None:
        raw = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("Cache-Control", cache)
        self.end_headers()
        self.wfile.write(raw)

    def _ip(self) -> str:
        fwd = self.headers.get("x-forwarded-for", "")
        return (fwd.split(",")[0].strip() if fwd else "") or self.client_address[0]

    def _path_ok(self) -> bool:
        if urllib.parse.urlparse(self.path).path.rstrip("/") in ("/api/updates", ""):
            return True
        self._send(404, {"ok": False, "error": "not found"})
        return False

    def do_GET(self) -> None:  # noqa: N802
        if self._path_ok():
            code, payload = handle_get()
            self._send(code, payload, "public, max-age=600, s-maxage=3600")

    def do_POST(self) -> None:  # noqa: N802
        if not self._path_ok():
            return
        try:
            length = int(self.headers.get("Content-Length", "0") or "0")
        except ValueError:
            length = 0
        if length <= 0 or length > MAX_BODY:
            self._send(413, {"ok": False, "error": f"body is empty or larger than {MAX_BODY} bytes"})
            return
        code, payload = handle_post(self.rfile.read(length), self._ip())
        self._send(code, payload)


if __name__ == "__main__":
    import sys

    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8793
    print(f"updates endpoint http://127.0.0.1:{port}/api/updates", flush=True)
    ThreadingHTTPServer(("127.0.0.1", port), handler).serve_forever()
