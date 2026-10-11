"""Offline regression tests for the hub's execution and reporting boundary."""
from contextlib import redirect_stdout, redirect_stderr
from dataclasses import replace
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hub


class HubTests(unittest.TestCase):
    def test_list_and_plan_do_not_spawn(self):
        with mock.patch.object(hub.subprocess, "run") as run, redirect_stdout(io.StringIO()):
            for argv in ([], ["plan", "all"], ["list", "native", "--json"]):
                self.assertEqual(hub.main(argv), 0)
            run.assert_not_called()

    def test_unavailable_script_is_failure_without_spawn(self):
        with tempfile.TemporaryDirectory() as temp, mock.patch.object(hub.subprocess, "run") as run:
            result = hub.run_one(hub.offline_checks()[0], Path(temp), 1)
            self.assertEqual(result["status"], "MISSING")
            run.assert_not_called()

    def test_plan_only_and_forged_checks_cannot_execute(self):
        native = hub.Check("native", "native", "x.py", runnable=True)
        forged = replace(hub.offline_checks()[0], path="danger.py")
        with mock.patch.object(hub.subprocess, "run") as run:
            for check in (native, forged):
                with self.assertRaises(ValueError):
                    hub.run_one(check, hub.ROOT, 1)
            run.assert_not_called()

    def test_cli_refuses_nonoffline_execution_and_bad_timeout(self):
        with mock.patch.object(hub.subprocess, "run") as run, redirect_stderr(io.StringIO()):
            for argv in (["run"], ["run", "native"], ["run", "device"], ["run", "offline", "--timeout", "0"]):
                with self.assertRaises(SystemExit) as error:
                    hub.main(argv)
                self.assertEqual(error.exception.code, 2)
            run.assert_not_called()

    def test_nonzero_and_timeout_are_not_passes(self):
        check = hub.offline_checks()[0]
        with mock.patch.object(Path, "is_file", return_value=True):
            with mock.patch.object(hub.subprocess, "run", return_value=subprocess.CompletedProcess([], 7, "", "failure")):
                result = hub.run_one(check, hub.ROOT, 1)
                self.assertEqual((result["status"], result["returncode"]), ("FAIL", 7))
            with mock.patch.object(hub.subprocess, "run", side_effect=subprocess.TimeoutExpired([], 1)):
                self.assertEqual(hub.run_one(check, hub.ROOT, 1)["status"], "TIMEOUT")

    def test_python_execution_uses_argv_and_no_bytecode_writes(self):
        check = hub.offline_checks()[0]
        with mock.patch.object(Path, "is_file", return_value=True), mock.patch.object(hub.subprocess, "run") as run:
            run.return_value = subprocess.CompletedProcess([], 0, "OK", "")
            self.assertEqual(hub.run_one(check, hub.ROOT, 1)["status"], "PASS")
            argv = run.call_args.args[0]
            self.assertEqual(argv[:3], [sys.executable, "-I", "-B"])
            self.assertEqual(argv[-1], "--check")
            self.assertNotIn("shell", run.call_args.kwargs)

    def test_summary_continues_after_failure_and_names_unrun_scope(self):
        def outcome(check, root, timeout):
            return {"id": check.id, "status": "TIMEOUT" if check.id == "generated-header" else "PASS", "seconds": 0.0}
        with tempfile.TemporaryDirectory() as temp, mock.patch.object(hub, "run_one", side_effect=outcome) as run:
            output = io.StringIO()
            report = Path(temp) / "run.json"
            with redirect_stdout(output):
                self.assertEqual(hub.main(["run", "offline", "--report", str(report)]), 1)
            self.assertEqual(run.call_count, len(hub.offline_checks()))
            self.assertIn("SUMMARY FAIL 4/5 offline checks", output.getvalue())
            self.assertIn("device not executed", report.read_text())


if __name__ == "__main__":
    unittest.main()
