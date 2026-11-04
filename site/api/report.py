"""Vercel entry for POST /api/report (mod menu Report box, bridge #64882). Logic: report_api.py beside server.py."""

import os
import sys
from pathlib import Path

os.environ["VERCEL"] = "1"
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "feedback-api"))

from report_api import ReportHandler  # noqa: E402


class handler(ReportHandler):
    pass
