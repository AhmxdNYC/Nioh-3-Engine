"""Inventory Nioh 3 checks; execute only the reviewed, lightweight offline suite.

No arguments lists the catalog. Plans never execute commands. Native builds,
dump scans, wrapper simulations, release packaging and device work are plan-only.
Standard library only; no installed game, save, bridge or device access.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
SUITES = ("offline", "native", "dump", "wrappers", "release", "analysis", "device")


@dataclass(frozen=True)
class Check:
    id: str
    suite: str
    path: str
    args: tuple[str, ...] = ()
    note: str = ""
    runnable: bool = False

    def command(self) -> str:
        prefix = "py -I -B" if self.path.endswith(".py") else "powershell -NoProfile -ExecutionPolicy Bypass -File"
        return " ".join((prefix, self.path, *self.args))


def offline_checks() -> list[Check]:
    # Execution is deliberately explicit, never filename discovery. A newly
    # added run_*.py cannot accidentally start a build or game through this lane.
    return [
        Check("generated-header", "offline", "mod/Nioh3EngineFix/tools/gen_header.py", ("--check",),
              "patches.json / generated-header consistency; public-portable", True),
        Check("presentmon-stats", "offline", "tools/testmatrix/test_tm_stats.py", (),
              "synthetic CSV selection, quantiles and balanced launch pairs; private helpers", True),
        Check("presentmon-time", "offline", "tools/testmatrix/test_pm_time.py", (),
              "synthetic clock alignment and reports; private helpers", True),
        Check("release-pdb-scrub", "offline", "release/tests/test_scrub_pdb.py", (),
              "synthetic PE privacy/malformed/nonoverwrite checks; no real ASI touched", True),
        Check("hub-contract", "offline", "tools/testhub/test_hub.py", (),
              "execution boundary and failure/timeout reporting", True),
    ]


def catalog(root: Path = ROOT) -> list[Check]:
    checks = offline_checks()
    for mod in ("Nioh3EngineFix", "Nioh3BossRematch"):
        folder = root / "mod" / mod / "tests"
        for path in sorted(folder.glob("run_*.py")):
            note = "Windows/MSVC; some need real D3D12, private dump, Capstone or helper inputs"
            if path.stem == "run_gp_harness":
                note = "real D3D12 device + Windows/MSVC; hardware test, not a generic hosted-CI check"
            if path.stem == "run_cbvupload_recreation_witness":
                note = "Windows/MSVC + private dump; default success reproduces counterexamples, not production safety"
            if path.stem == "run_rematch_harness":
                note = "Windows/MSVC + private rematch_catalog.py helper; public copy needs porting"
            checks.append(Check(f"{mod}:{path.stem}", "native", path.relative_to(root).as_posix(), note=note))
    for mod, filename in (("Nioh3EngineFix", "test_patterns.py"), ("Nioh3BossRematch", "test_sites.py")):
        checks.append(Check(f"{mod}:sites", "dump", f"mod/{mod}/tests/{filename}",
                            note="private decrypted v2.02 dump; pattern checks do not belong in hosted CI"))
    for path in sorted((root / "tools" / "testmatrix").glob("test_*.ps1")):
        checks.append(Check(path.stem, "wrappers", path.relative_to(root).as_posix(),
                            note="PowerShell harness fixture; review its dependencies/side effects before execution"))
    checks.extend([
        Check("tour-wrapper-fixture", "wrappers", "tools/tour/run_tour.ps1", ("-DryRun", "-NoPause"),
              "fake game + temporary save/game tree; creates private state; never invoked by hub"),
        Check("release-quick", "release", "release/check_release.py", (),
              "source/ini/profile lint + existing build/version copies; missing build is failure"),
        Check("release-full", "release", "release/check_release.py", ("--full", "--strict"),
              "quick + EngineFix patterns/run_*_harness.py; compiles, scans dump; warnings fail"),
        Check("cpu-ready-aabb", "analysis", "tools/bench/test_cpu_ready_aabb_control.py", ("--setup", "<vcvars-or-setup-bat>"),
              "builds a native control; uses private analysis inputs"),
        Check("tour-main", "device", "tools/tour/run_tour.ps1", ("-Mode", "1", "-List", "main", "-Passes", "1", "-Input", "ui", "-OwnerPid", "$PID"),
              "live map travel, installed ASI/INI changes, backups, restore, lock and captures; press Continue manually"),
        Check("tour-leaks", "device", "tools/tour/run_tour.ps1", ("-Mode", "1", "-List", "main", "-Passes", "2", "-Input", "ui", "-OwnerPid", "$PID"),
              "two live map passes for memory/handle comparison; same requirements as tour-main"),
        Check("pinned-benchmark", "device", "tools/testmatrix/pinned_final_pc.ps1",
              ("-OptimizedIni", "<validated-pinned-ini>", "-AsiPath", "<validated-pinned-asi>", "-OwnerPid", "$PID", "-SourceOwnershipConfirmed"),
              "current driver requires tested 1.7.61 artifacts; gameplay prompts and actual hardware; template only"),
    ])
    for path in sorted((root / "tools" / "bench").glob("*.py")):
        if path.name.startswith(("audit_", "verify_")):
            checks.append(Check(path.stem, "analysis", path.relative_to(root).as_posix(), ("<script-specific-inputs>",),
                                "private source/capture/dump audit; inspect arguments; template only"))
    return checks


def selected(checks: list[Check], suite: str) -> list[Check]:
    return checks if suite == "all" else [check for check in checks if check.suite == suite]


def run_one(check: Check, root: Path, timeout: float) -> dict:
    if not check.runnable or check.suite != "offline":
        raise ValueError("only the explicit offline allowlist can execute")
    # Do not trust callers to convert a plan-only check into an executable one.
    if check not in offline_checks():
        raise ValueError("check is not in the offline allowlist")
    result = {"id": check.id, "command": check.command(), "status": "MISSING", "returncode": None}
    path = root / check.path
    if not path.is_file():
        result["detail"] = "required script is missing"
        result["seconds"] = 0.0
        return result
    start = time.monotonic()
    try:
        process = subprocess.run([sys.executable, "-I", "-B", str(path), *check.args],
                                 cwd=root, capture_output=True, text=True, errors="replace", timeout=timeout)
        result["returncode"] = process.returncode
        result["status"] = "PASS" if process.returncode == 0 else "FAIL"
        detail = "\n".join((process.stdout, process.stderr)).strip()
        result["detail"] = detail[-2000:]
    except subprocess.TimeoutExpired:
        result["status"] = "TIMEOUT"
        result["detail"] = f"exceeded {timeout:g} seconds"
    except OSError as error:
        result["status"] = "ERROR"
        result["detail"] = str(error)
    result["seconds"] = round(time.monotonic() - start, 3)
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("list", "plan", "run"), nargs="?", default="list")
    parser.add_argument("suite", choices=("all", *SUITES), nargs="?", default="all")
    parser.add_argument("--json", action="store_true", help="JSON catalog/plan (still read-only)")
    parser.add_argument("--report", type=Path, help="write an offline-run JSON report; treat it as private")
    parser.add_argument("--timeout", type=float, default=60, help="per offline check seconds (default 60)")
    args = parser.parse_args(argv)
    if args.action == "run" and args.suite != "offline":
        parser.error("only 'run offline' is executable; use 'plan <suite>' for other work")
    if args.timeout <= 0:
        parser.error("--timeout must be greater than zero")
    if args.report and args.action != "run":
        parser.error("--report is only valid with 'run offline'")
    if args.action == "run" and args.json:
        parser.error("--json is for list/plan; use --report for execution results")
    checks = selected(catalog(), args.suite)
    if args.action != "run":
        if args.json:
            print(json.dumps([{"id": check.id, "suite": check.suite, "command": check.command(),
                               "runnable": check.runnable, "present": (ROOT / check.path).is_file(),
                               "note": check.note} for check in checks], indent=2))
        else:
            print("Catalog/plan only. Commands below have not run. Native/device entries are manual.")
            for suite in SUITES:
                group = selected(checks, suite)
                if not group:
                    continue
                print(f"\n{suite} ({len(group)})")
                for check in group:
                    state = "ready" if (ROOT / check.path).is_file() else "missing"
                    print(f"  {check.id} [{state}]: {check.note}")
                    if args.action == "plan":
                        print(f"    {check.command()}")
            print("\nExecute the reviewed small suite with: py -I -B tools/testhub/hub.py run offline")
        return 0
    results = []
    for check in checks:
        result = run_one(check, ROOT, args.timeout)
        results.append(result)
        print(f"{result['status']:7} {result['id']} ({result['seconds']:.3f}s)", flush=True)
        if result["status"] != "PASS":
            print(result.get("detail", ""))
    passed = sum(result["status"] == "PASS" for result in results)
    report = {"schema": 1, "suite": "offline", "passed": passed, "total": len(results),
              "scope": "lightweight offline checks only; native/dump/wrappers/release/device not executed",
              "results": results}
    print(f"SUMMARY {'PASS' if passed == len(results) else 'FAIL'} {passed}/{len(results)} offline checks")
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
