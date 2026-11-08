"""Unified deployment imports and report storage checks; all network calls mocked."""
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SITE = Path(__file__).resolve().parents[1]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class MergedReport(unittest.TestCase):
    def setUp(self):
        self.api = load("merged_report_logic", SITE / "feedback-api/report_api.py")
        self.record = {"kind": "player", "id": "a" * 32, "player_text": "Test report",
                       "snapshot": r"C:\Users\Example\log.txt 76561198012345678", "ver": "test"}

    def test_deployment_imports(self):
        with patch.dict(os.environ, {}, clear=True):
            report = load("merged_report_entry", SITE / "api/report.py")
            crash = load("merged_crash_entry", SITE / "api/crash.py")
        self.assertTrue(callable(report.handler))
        self.assertTrue(callable(crash.handler))
        config = json.loads((SITE / "vercel.json").read_text())
        include = config["functions"]["api/report.py"]["includeFiles"]
        self.assertTrue((SITE / include).is_file())
        self.assertFalse((SITE / "feedback-api/vercel.json").exists())

    def test_local_report_and_redaction(self):
        with tempfile.TemporaryDirectory() as d, patch.dict(os.environ, {"LOCALAPPDATA": d}, clear=True):
            code, response = self.api.handle(json.dumps(self.record).encode(), "127.0.0.1")
            self.assertEqual(code, 202, response)
            saved = list(Path(d).rglob("*.json"))
            self.assertEqual(len(saved), 1)
            text = saved[0].read_text()
            self.assertNotIn("Example", text)
            self.assertNotIn("76561198012345678", text)
            self.assertIn(self.record["id"], text)

    def test_github_without_external_server_module(self):
        calls = []

        def post(request, timeout):
            calls.append((request.full_url, json.loads(request.data)))
            return io.BytesIO(json.dumps({"number": 42, "html_url": "https://example.invalid/42"}).encode())

        self.record["snapshot"] = "x" * 120000
        with patch.dict(os.environ, {"GITHUB_TOKEN": "test-only", "GITHUB_REPO": "fixture/repo"}, clear=True), \
                patch.object(self.api.urllib.request, "urlopen", post):
            result = self.api.store_github(self.record)
        self.assertEqual(result, "#42")
        self.assertEqual(calls[0][0], "https://api.github.com/repos/fixture/repo/issues")
        self.assertIn("mod-report", calls[0][1]["labels"])
        self.assertGreater(len(calls), 1)
        self.assertTrue(all(url.endswith("/issues/42/comments") for url, _ in calls[1:]))


if __name__ == "__main__":
    unittest.main()
