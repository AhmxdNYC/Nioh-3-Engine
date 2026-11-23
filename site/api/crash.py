"""POST /api/crash: automatic crash records from the EngineFix mod.

Self-contained on purpose (no import from server.py), so the feedback form and the
Report box code (api/report.py) stay separate files.

Body: one JSON crash object (a crash record):
  {"schema":1,"t","mod":"EngineFix","ver","event":"crash","id":<32 hex>,
   "fields":{"code","module","offset","game_stamp","game_size","reason"},
   "exc_name","access","stack":[...],"game":{...},"mods":[...],"loaders":[...],
   "os","uptime_s","test","last":[...],"input":[...],"log":{"EngineFix":[...],...}}

Checks: 64 KB cap, schema and per-field caps (400 on anything else), install id
32 hex, rate limit 20 per 10 min per IP and 6 per hour per install id (429),
the scrubber runs again before anything is stored.

Storage, first that is configured:
  BLOB_READ_WRITE_TOKEN  Vercel Blob crashes/YYYY-MM-DD/<sig>.json (+ <sig>-<n>.json repeats)
  GITHUB_TOKEN / GH_TOKEN  like api/submit.py: one GitHub issue per signature and day
                           (label crash-report), a repeat adds a comment
  off Vercel (PC)       %LOCALAPPDATA%/nioh3-site-inbox/crashes/YYYY-MM-DD/<sig>.json
If none works the answer is 503 and the mod keeps the record pending.

Answer: 202 {"ok":true,"new":true|false,"sig":...,"stored":<where>}.

Local run: py api/crash.py [port]   (serves POST /api/crash on 127.0.0.1, default 8792)
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

REPO = "AhmxdNYC/Nioh-3-Engine"
MAX_BODY = 64 * 1024
MAX_STR = 400
TOP_KEYS = {
    "schema", "t", "mod", "ver", "event", "id", "fields", "exc_name", "access", "stack", "game", "mods",
    "loaders", "os", "uptime_s", "test", "last", "input", "log",
    "modules", "modules_truncated", "modules_available", "registers", "stack_top", "settings", "training_tail",
    "perf",
}
FIELD_KEYS = {"code", "module", "offset", "game_stamp", "game_size", "reason"}
GAME_KEYS = {"exe", "stamp", "image_size", "file_size", "known", "version"}
MOD_KEYS = {"name", "size", "sha", "loaded", "ver"}
LOG_KEYS = {"EngineFix", "BossRematch", "Training", "CamKiProbe", "Updater"}
CAPS = {"stack": 40, "mods": 32, "loaders": 16, "last": 32, "input": 150, "modules": 64}
LOG_GROUP_CAP = 60
LOG_TOTAL_CAP = 220

_RATE_LOCK = threading.Lock()
_RATE_IP: dict[str, list[float]] = {}
_RATE_ID: dict[str, list[float]] = {}
_SEEN_LOCK = threading.Lock()
_SEEN: dict[str, int] = {}


class Reject(Exception):
    def __init__(self, code: int, msg: str):
        super().__init__(msg)
        self.code = code


# ---- scrubber (same rules as the mod) ------------------------------------------------------------------------------
_USER_PATH = re.compile(r"(?:[A-Za-z]:)?[\\/](?:users|home)[\\/][^\\/\"'\r\n]*", re.I)
_STEAM_ID = re.compile(r"(?<!\d)7656119\d{10}(?!\d)")


def scrub(s: str) -> str:
    s = _USER_PATH.sub("<user>", s)
    return _STEAM_ID.sub("<steamid>", s)


def _scrub_tree(v):
    if isinstance(v, str):
        return scrub(v)
    if isinstance(v, list):
        return [_scrub_tree(x) for x in v]
    if isinstance(v, dict):
        return {k: _scrub_tree(x) for k, x in v.items()}
    return v


# ---- validation ------------------------------------------------------------------------------------------------------
def _str(v, name: str, cap: int = MAX_STR, empty: bool = True) -> str:
    if not isinstance(v, str):
        raise Reject(400, f"{name} must be a string")
    if len(v) > cap:
        raise Reject(400, f"{name} is longer than {cap}")
    if not empty and not v:
        raise Reject(400, f"{name} is empty")
    return v


def _num(v, name: str) -> None:
    if isinstance(v, bool) or not isinstance(v, (int, float)):
        raise Reject(400, f"{name} must be a number")


def _flat_event(v, name: str) -> None:
    if not isinstance(v, dict) or len(v) > 24:
        raise Reject(400, f"{name} must be an event object")
    for k, x in v.items():
        _str(k, f"{name} key", 32)
        if isinstance(x, dict):
            if len(x) > 16:
                raise Reject(400, f"{name}.{k} has too many keys")
            for k2, y in x.items():
                _str(k2, f"{name}.{k} key", 32)
                if isinstance(y, (dict, list)):
                    raise Reject(400, f"{name}.{k}.{k2} must be flat")
                if isinstance(y, str):
                    _str(y, f"{name}.{k}.{k2}", 120)
        elif isinstance(x, list):
            raise Reject(400, f"{name}.{k} must not be a list")
        elif isinstance(x, str):
            _str(x, f"{name}.{k}", 200)


# ---- performance snapshot ("perf"): last ~10 s fps / 1 % low / CPU-GPU bound / hardware / settings ---------------
PERF_MAX = 8192


def _check_perf(v, fail) -> None:
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


def _perf_line(rec: dict) -> str:
    p = rec.get("perf")
    if not isinstance(p, dict):
        return "- Performance: (not in this record)"
    return (f"- Performance (last {p.get('window_s', '?')} s): {p.get('avg_fps', '?')} fps avg, {p.get('low1_fps', '?')} fps 1% low, "
            f"bound {p.get('bound', '?')} ({p.get('bound_why', '')}); CPU {p.get('cpu', '?')}; GPU {p.get('gpu', '?')} driver "
            f"{p.get('driver', '?')}; {p.get('backbuffer', '?')} upscaler {p.get('upscaler', '?')} frame gen {p.get('frame_gen', '?')}")


def validate(rec) -> dict:
    if not isinstance(rec, dict):
        raise Reject(400, "body must be a JSON object")
    extra = set(rec) - TOP_KEYS
    if extra:
        raise Reject(400, f"unknown fields: {', '.join(sorted(extra))[:200]}")
    if rec.get("schema") != 1:
        raise Reject(400, "schema must be 1")
    if rec.get("event") != "crash":
        raise Reject(400, "event must be crash")
    if not re.fullmatch(r"[0-9a-f]{32}", str(rec.get("id", ""))):
        raise Reject(400, "id must be 32 lowercase hex characters")
    _str(rec.get("mod", ""), "mod", 32, empty=False)
    _str(rec.get("ver", ""), "ver", 40)
    _str(rec.get("t", ""), "t", 32)
    f = rec.get("fields")
    if not isinstance(f, dict) or set(f) - FIELD_KEYS or not {"code", "module", "offset"} <= set(f):
        raise Reject(400, "fields must hold code, module, offset (+ game_stamp, game_size, reason)")
    if not re.fullmatch(r"0x[0-9A-Fa-f]{1,16}", str(f.get("code"))):
        raise Reject(400, "fields.code must be hex")
    if not re.fullmatch(r"0x[0-9A-Fa-f]{1,16}", str(f.get("offset"))):
        raise Reject(400, "fields.offset must be hex")
    _str(f["module"], "fields.module", 96)
    if re.search(r"[\\/]", f["module"]):
        raise Reject(400, "fields.module must be a file name, not a path")
    for k in ("game_stamp", "reason"):
        if k in f:
            _str(f[k], f"fields.{k}", 160)
    if "game_size" in f:
        _num(f["game_size"], "fields.game_size")
    for k in ("exc_name", "access", "os"):
        if k in rec:
            _str(rec[k], k, 300)
    for k in ("uptime_s", "test"):
        if k in rec:
            _num(rec[k], k)
    for k, cap in CAPS.items():
        if k not in rec:
            continue
        v = rec[k]
        if not isinstance(v, list) or len(v) > cap:
            raise Reject(400, f"{k} must be a list of at most {cap}")
        for i, x in enumerate(v):
            if k in ("stack", "loaders"):
                _str(x, f"{k}[{i}]", 200)
            elif k == "modules":
                if not isinstance(x, dict) or set(x) != {"name", "image_size"}:
                    raise Reject(400, f"modules[{i}] must hold name and image_size")
                _str(x["name"], f"modules[{i}].name", 96, empty=False)
                if re.search(r"[\\/]", x["name"]):
                    raise Reject(400, "modules name must be a file name, not a path")
                _num(x["image_size"], f"modules[{i}].image_size")
            elif k == "mods":
                if not isinstance(x, dict) or set(x) - MOD_KEYS:
                    raise Reject(400, f"mods[{i}] has unknown fields")
                for mk, mv in x.items():
                    if mk in ("size",):
                        _num(mv, f"mods[{i}].size")
                    elif mk == "loaded":
                        if not isinstance(mv, bool):
                            raise Reject(400, f"mods[{i}].loaded must be true/false")
                    else:
                        _str(mv, f"mods[{i}].{mk}", 96)
            else:
                _flat_event(x, f"{k}[{i}]")
    for flag in ("modules_truncated", "modules_available"):
        if flag in rec and not isinstance(rec[flag], bool):
            raise Reject(400, f"{flag} must be true/false")
    if "game" in rec:
        g = rec["game"]
        if not isinstance(g, dict) or set(g) - GAME_KEYS:
            raise Reject(400, "game has unknown fields")
        for gk, gv in g.items():
            if gk == "known":
                if not isinstance(gv, bool):
                    raise Reject(400, "game.known must be true/false")
            elif gk in ("image_size", "file_size"):
                _num(gv, f"game.{gk}")
            else:
                _str(gv, f"game.{gk}", 96)
    if "registers" in rec and rec["registers"] is not None:
        regs = rec["registers"]
        names = {"Rip", "Rsp", "Rbp", "Rax", "Rbx", "Rcx", "Rdx", "Rsi", "Rdi", "EFlags"} | {f"R{i}" for i in range(8, 16)}
        if not isinstance(regs, dict) or set(regs) != names:
            raise Reject(400, "registers must contain the x64 context")
        for value in regs.values():
            if not isinstance(value, str) or not re.fullmatch(r"0x[0-9a-fA-F]{1,16}", value):
                raise Reject(400, "registers must be hexadecimal words")
    if "stack_top" in rec:
        words = rec["stack_top"]
        if not isinstance(words, list) or len(words) > 16 or any(x is not None and (not isinstance(x, str) or not re.fullmatch(r"0x[0-9a-fA-F]{16}", x)) for x in words):
            raise Reject(400, "stack_top must contain at most 16 hexadecimal words or null")
    if "settings" in rec:
        settings = rec["settings"]
        if not isinstance(settings, dict) or set(settings) != {"source", "values", "truncated"} or settings["source"] != "ini_at_crash_not_live_state" or not isinstance(settings["truncated"], bool):
            raise Reject(400, "invalid settings snapshot")
        values = settings["values"]
        if not isinstance(values, list) or len(values) > 64:
            raise Reject(400, "settings has too many values")
        summon = {"kibreaklog", "kibreaknofinisher", "kibreakactfix", "kibreakactfixmode", "kibreakactbadids", "kibreakactgoodid", "moveblock", "moveblockfallback", "playernodeath", "sessionnodeath", "playercantdie"}
        training = {"repeatgapms", "bossaggro", "aggroface", "aggrorangecm", "aggrowalktimeoutms"}
        for value in values:
            if not isinstance(value, dict) or set(value) != {"mod", "section", "key", "value"}:
                raise Reject(400, "invalid setting")
            for key in value:
                _str(value[key], "setting." + key, 512 if key == "value" else 48)
            mod, sec, key = value["mod"], value["section"].lower(), value["key"].lower()
            allowed = mod == "BossRematch" and sec == "spirit" and key == "soulcoreforms"
            allowed |= mod == "Training" and ((sec == "summon" and key in summon) or (sec == "training" and key in training) or (bool(re.fullmatch(r"moves\.[0-9]+", sec)) and (key == "repeatgapms" or bool(re.fullmatch(r"0x[0-9a-f]+", key)))))
            if not allowed or not re.fullmatch(r"[0-9a-fA-FxX,+. \t-]*", value["value"]):
                raise Reject(400, "setting is not an allowlisted numeric override")
    if "training_tail" in rec:
        tail = rec["training_tail"]
        if not isinstance(tail, dict) or set(tail) != {"captured", "included", "line_cap"} or any(type(v) is not int for v in tail.values()) or not 0 <= tail["included"] <= tail["captured"] <= 20 or tail["line_cap"] != 2047:
            raise Reject(400, "invalid training_tail bounds")
    if "perf" in rec:
        # A bad perf block is dropped (and logged), never a reason to lose the crash record
        class _BadPerf(Exception):
            pass

        def fail(msg):
            raise _BadPerf(msg)
        try:
            _check_perf(rec["perf"], fail)
        except _BadPerf as e:
            print(f"crash: perf block dropped: {e}", file=sys.stderr)
            del rec["perf"]
    if "log" in rec:
        lg = rec["log"]
        if not isinstance(lg, dict) or set(lg) - LOG_KEYS:
            raise Reject(400, "log has unknown groups")
        total = 0
        for gk, lines in lg.items():
            if not isinstance(lines, list) or len(lines) > LOG_GROUP_CAP:
                raise Reject(400, f"log.{gk} must be a list of at most {LOG_GROUP_CAP}")
            total += len(lines)
            for i, line in enumerate(lines):
                _str(line, f"log.{gk}[{i}]", 2047 if gk == "Training" else 240)
        if total > LOG_TOTAL_CAP:
            raise Reject(400, f"log has more than {LOG_TOTAL_CAP} lines")
    return _scrub_tree(rec)


def signature(rec: dict) -> str:
    f = rec["fields"]
    parts = [f.get("module", ""), f.get("offset", ""), f.get("code", ""), rec.get("ver", "")]
    if not f.get("module"):  # popup / abort without an exception: the reason text is the signature
        parts.append(re.sub(r"0x[0-9a-fA-F]+|\d+", "#", f.get("reason", ""))[:80])
    key = "|".join(parts).lower()
    return hashlib.sha256(key.encode("utf-8")).hexdigest()[:16]


def rate_ok(ip: str, install_id: str, now: float | None = None) -> bool:
    now = time.time() if now is None else now
    with _RATE_LOCK:
        hits = [t for t in _RATE_IP.get(ip, []) if now - t < 600]
        ids = [t for t in _RATE_ID.get(install_id, []) if now - t < 3600]
        if len(hits) >= 20 or len(ids) >= 6:
            _RATE_IP[ip], _RATE_ID[install_id] = hits, ids
            return False
        hits.append(now)
        ids.append(now)
        _RATE_IP[ip], _RATE_ID[install_id] = hits, ids
        return True


# ---- storage -----------------------------------------------------------------------------------------------------------
def _blob_put(path: str, data: bytes, token: str) -> str:
    req = urllib.request.Request(f"https://blob.vercel-storage.com/{path}", data=data, method="PUT")
    req.add_header("Authorization", f"Bearer {token}")
    req.add_header("x-api-version", "7")
    req.add_header("x-content-type", "application/json")
    req.add_header("x-add-random-suffix", "0")
    with urllib.request.urlopen(req, timeout=20) as resp:
        out = json.loads(resp.read().decode("utf-8", "replace") or "{}")
    return out.get("pathname") or path


def _gh(method: str, url: str, token: str, payload: dict | None = None) -> dict:
    body = None if payload is None else json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(url, data=body, method=method)
    req.add_header("Authorization", f"Bearer {token}")
    req.add_header("Accept", "application/vnd.github+json")
    req.add_header("X-GitHub-Api-Version", "2022-11-28")
    if body is not None:
        req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode("utf-8", "replace") or "{}")
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", "replace")[:300]
        raise RuntimeError(f"GitHub {exc.code}: {detail}") from exc


def _issue_text(rec: dict, sig: str) -> tuple[str, str]:
    f = rec["fields"]
    test = rec.get("test") or 0
    where = f"{f['module']}+{f['offset']} {f['code']}" if f["module"] else (f.get("reason") or "no exception record")[:90]
    title = f"Crash{' (test)' if test else ''}: {where} EngineFix {rec.get('ver', '?')} [{sig}]"
    mods = ", ".join(f"{m.get('name')} {m.get('sha', '')} {m.get('ver', '')}".strip() for m in rec.get("mods", []))
    modules = ", ".join(f"{m['name']} ({m['image_size']} image bytes)" for m in rec.get("modules", []))
    game = rec.get("game", {})
    # crashblame verdict lines (EngineFix puts them first in "last"; the crash-blame lines)
    who = [f"- {x['fields']['line']}" for x in rec.get("last", [])
           if isinstance(x, dict) and isinstance(x.get("fields"), dict) and str(x["fields"].get("line", "")).startswith("WHO")][:4]
    lines = [
        "### Automatic crash report",
        "",
        f"- Signature: `{sig}` (module + offset + code + EngineFix version)",
        f"- Exception: `{f['code']}` {rec.get('exc_name', '')} at `{f['module']}+{f['offset']}` {rec.get('access', '')}",
        *who,
        f"- Reason: {f.get('reason', '')}",
        f"- Game: {game.get('exe', '?')} stamp `{f.get('game_stamp', game.get('stamp', '?'))}` size {f.get('game_size', game.get('image_size', '?'))} version {game.get('version', '?')} known {game.get('known', '?')}",
        f"- Mods: {mods or '(none listed)'}",
        f"- Loaded modules at crash (excluding System32, WinSxS and Steam overlay): {modules or '(none listed)'}; available={rec.get('modules_available', 'unknown')}, truncated={rec.get('modules_truncated', 'unknown')}",
        f"- OS: {rec.get('os', '?')}",
        _perf_line(rec),
        f"- Install id: `{rec['id'][:8]}...`  uptime {rec.get('uptime_s', '?')} s  test {test}",
        f"- Mod time: {rec.get('t', '?')}",
        "",
        "Top of stack:",
        "",
        "```",
        *rec.get("stack", [])[:12],
        "```",
        "",
    ]
    full = json.dumps(rec, indent=1, ensure_ascii=False)
    room = 60000 - sum(len(x) + 1 for x in lines) - 200
    if len(full) > room:
        full = full[:room] + "\n... (cut, full record in the mod's Nioh3CrashReports folder)"
    lines += ["<details><summary>Full record (JSON)</summary>", "", "```json", full, "```", "</details>", "",
              "---", "Sent automatically by Nioh3EngineFix (no prompt). Personal paths and names are scrubbed."]
    return title, "\n".join(lines)


def store(rec: dict, sig: str) -> tuple[bool, str]:
    """Returns (new, where)."""
    day = time.strftime("%Y-%m-%d", time.gmtime())
    data = json.dumps(rec, ensure_ascii=False).encode("utf-8")
    blob = os.environ.get("BLOB_READ_WRITE_TOKEN", "").strip()
    if blob:
        with _SEEN_LOCK:
            n = _SEEN.get(f"{day}/{sig}", 0)
            _SEEN[f"{day}/{sig}"] = n + 1
        path = f"crashes/{day}/{sig}.json" if n == 0 else f"crashes/{day}/{sig}-{int(time.time())}.json"
        return n == 0, "blob:" + _blob_put(path, data, blob)
    token = (os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN") or "").strip()
    if token:
        q = urllib.parse.quote(f'repo:{REPO} is:issue in:title "[{sig}]" created:>={day}')
        found = _gh("GET", f"https://api.github.com/search/issues?q={q}&per_page=1", token)
        items = found.get("items") or []
        if items:
            num = int(items[0]["number"])
            f = rec["fields"]
            _gh("POST", f"https://api.github.com/repos/{REPO}/issues/{num}/comments", token,
                {"body": f"Seen again: install `{rec['id'][:8]}...`, {rec.get('os', '?')}, uptime {rec.get('uptime_s', '?')} s, "
                         f"test {rec.get('test', 0)}, reason {f.get('reason', '')[:120]}"})
            return False, f"github:#{num}"
        title, body = _issue_text(rec, sig)
        labels = ["crash-report"] + (["crash-test"] if rec.get("test") else [])
        try:
            out = _gh("POST", f"https://api.github.com/repos/{REPO}/issues", token, {"title": title[:240], "body": body, "labels": labels})
        except RuntimeError:
            out = _gh("POST", f"https://api.github.com/repos/{REPO}/issues", token, {"title": title[:240], "body": body})
        return True, f"github:#{out.get('number')}"
    if os.environ.get("VERCEL"):
        raise Reject(503, "crash storage is not configured")
    base = os.environ.get("CRASH_INBOX") or str(Path(os.environ.get("LOCALAPPDATA", str(Path(__file__).resolve().parents[1]))) / "nioh3-site-inbox" / "crashes")
    root = Path(base) / day
    root.mkdir(parents=True, exist_ok=True)
    dest = root / f"{sig}.json"
    new = not dest.exists()
    if not new:
        dest = root / f"{sig}-{int(time.time() * 1000)}.json"
    dest.write_bytes(data)
    return new, f"file:{dest}"


def handle(body: bytes, ip: str) -> tuple[int, dict]:
    if not body or len(body) > MAX_BODY:
        return 413, {"ok": False, "error": f"body is empty or larger than {MAX_BODY} bytes"}
    try:
        rec = json.loads(body.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return 400, {"ok": False, "error": "body is not UTF-8 JSON"}
    try:
        rec = validate(rec)
        if not rate_ok(ip, rec["id"]):
            return 429, {"ok": False, "error": "too many crash reports"}
        sig = signature(rec)
        new, where = store(rec, sig)
    except Reject as exc:
        return exc.code, {"ok": False, "error": str(exc)}
    except Exception as exc:  # storage down: the mod keeps the record and retries
        return 503, {"ok": False, "error": f"storage failed: {str(exc)[:200]}"}
    return 202, {"ok": True, "new": new, "sig": sig, "stored": where.split(":", 1)[0] if where.startswith("file:") else where}


class handler(BaseHTTPRequestHandler):
    server_version = "nioh3-crash/1.0"

    def log_message(self, fmt: str, *args) -> None:
        return

    def _send(self, code: int, payload: dict) -> None:
        raw = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(raw)

    def _ip(self) -> str:
        fwd = self.headers.get("x-forwarded-for", "")
        return (fwd.split(",")[0].strip() if fwd else "") or self.client_address[0]

    def do_GET(self) -> None:  # noqa: N802
        self._send(405, {"ok": False, "error": "POST a crash record"})

    def do_POST(self) -> None:  # noqa: N802
        if urllib.parse.urlparse(self.path).path.rstrip("/") not in ("/api/crash", ""):
            self._send(404, {"ok": False, "error": "not found"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0") or "0")
        except ValueError:
            length = 0
        if length <= 0 or length > MAX_BODY:
            self._send(413, {"ok": False, "error": f"body is empty or larger than {MAX_BODY} bytes"})
            return
        code, payload = handle(self.rfile.read(length), self._ip())
        self._send(code, payload)


if __name__ == "__main__":
    import sys

    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8792
    print(f"crash endpoint http://127.0.0.1:{port}/api/crash", flush=True)
    ThreadingHTTPServer(("127.0.0.1", port), handler).serve_forever()
