"""Offline test of api/updates.py: validation, local storage, count, badge, rate limit, HTTP."""
import json
import os
import sys
import tempfile
import threading
import unittest
import urllib.request
from http.server import ThreadingHTTPServer
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "api"))
import updates as up  # noqa: E402


class UpdatesApiTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.env = {k: os.environ.pop(k) for k in ("BLOB_READ_WRITE_TOKEN", "VERCEL") if k in os.environ}
        os.environ["UPDATES_INBOX"] = self.tmp.name
        up._RATE_IP.clear()

    def tearDown(self):
        os.environ.pop("UPDATES_INBOX", None)
        os.environ.update(self.env)

    def post(self, rec, ip="1.2.3.4"):
        return up.handle_post(json.dumps(rec).encode(), ip)

    def test_validation(self):
        for bad in ({"schema": 2, "to": "1.8.24"}, {"schema": 1, "to": "x"}, {"schema": 1, "to": "1.8.24", "id": "a" * 32},
                    {"schema": 1, "from": "C:/Users/x", "to": "1.8.24"}, [1]):
            self.assertEqual(self.post(bad)[0], 400, bad)
        self.assertEqual(up.handle_post(b"", "ip")[0], 413)
        self.assertEqual(up.handle_post(b"x" * 600, "ip")[0], 413)
        self.assertEqual(up.handle_post(b"{nope", "ip")[0], 400)

    def test_count_and_badge(self):
        self.assertEqual(up.handle_get()[1]["message"], "0")
        self.assertEqual(self.post({"schema": 1, "from": "1.8.23", "to": "1.8.24"})[0], 202)
        self.assertEqual(self.post({"schema": 1, "from": None, "to": "1.8.24"}, "5.6.7.8")[0], 202)
        code, b = up.handle_get()
        self.assertEqual((code, b["schemaVersion"], b["label"], b["message"]), (200, 1, "updates", "2"))
        stored = json.loads(next(Path(self.tmp.name).glob("1.8.24/*.json")).read_text())
        self.assertEqual(set(stored), {"schema", "from", "to", "t"})

    def test_rate_limit(self):
        codes = [self.post({"schema": 1, "from": "1.8.23", "to": "1.8.24"})[0] for _ in range(11)]
        self.assertEqual(codes, [202] * 10 + [429])

    def test_vercel_without_token(self):
        os.environ["VERCEL"] = "1"
        try:
            self.assertEqual(self.post({"schema": 1, "from": "1.8.23", "to": "1.8.24"})[0], 503)
            self.assertEqual(up.handle_get()[1]["message"], "n/a")
        finally:
            os.environ.pop("VERCEL")

    def test_http(self):
        srv = ThreadingHTTPServer(("127.0.0.1", 0), up.handler)
        threading.Thread(target=srv.serve_forever, daemon=True).start()
        self.addCleanup(srv.shutdown)
        url = f"http://127.0.0.1:{srv.server_address[1]}/api/updates"
        req = urllib.request.Request(url, data=json.dumps({"schema": 1, "from": "1.8.23", "to": "1.8.24"}).encode(),
                                     method="POST", headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req) as r:
            self.assertEqual(r.status, 202)
        with urllib.request.urlopen(url) as r:
            self.assertIn("s-maxage=3600", r.headers["Cache-Control"])
            self.assertEqual(json.loads(r.read())["message"], "1")


if __name__ == "__main__":
    unittest.main()
