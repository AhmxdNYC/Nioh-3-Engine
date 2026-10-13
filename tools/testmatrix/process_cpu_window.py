"""Read-only Windows/Linux game-process CPU measurement for existing A/B runners.

Does not launch, control, tune, or terminate the target. Compare CPU seconds per
wall second alongside frame data from the SAME interval, settings and scene.
"""
import argparse
import ctypes
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import time


def parse_proc_stat(text, hz):
    # comm may contain spaces and parentheses; numeric fields follow its LAST ).
    fields = text[text.rindex(")") + 2:].split()
    if fields[0] in {"Z", "X", "x"}:
        raise RuntimeError("target exited")
    return {"identity": fields[19],
            "cpu_seconds": (int(fields[11]) + int(fields[12])) / hz}


def read_cpu(pid):
    if os.name != "nt":
        return parse_proc_stat(Path(f"/proc/{pid}/stat").read_text(),
                               os.sysconf("SC_CLK_TCK"))
    from ctypes import wintypes as w
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
    k.OpenProcess.restype = w.HANDLE
    k.GetProcessTimes.argtypes = [w.HANDLE] + [ctypes.POINTER(w.FILETIME)] * 4
    k.GetProcessTimes.restype = w.BOOL
    k.WaitForSingleObject.argtypes = [w.HANDLE, w.DWORD]
    k.WaitForSingleObject.restype = w.DWORD
    k.CloseHandle.argtypes = [w.HANDLE]
    k.CloseHandle.restype = w.BOOL
    h = k.OpenProcess(0x1000 | 0x100000, False, pid)
    if not h:
        raise OSError(ctypes.get_last_error(), "cannot query target")
    try:
        if k.WaitForSingleObject(h, 0) != 258:  # WAIT_TIMEOUT = still running
            raise RuntimeError("target exited or wait failed")
        values = [w.FILETIME() for _ in range(4)]
        if not k.GetProcessTimes(h, *(ctypes.byref(v) for v in values)):
            raise OSError(ctypes.get_last_error(), "cannot read process times")
        ticks = [(v.dwHighDateTime << 32) | v.dwLowDateTime for v in values]
        return {"identity": str(ticks[0]),
                "cpu_seconds": (ticks[2] + ticks[3]) / 10_000_000}
    finally:
        k.CloseHandle(h)


def snapshot(pid):
    before = time.monotonic_ns()
    row = read_cpu(pid)
    after = time.monotonic_ns()
    row.update(monotonic_ns=(before + after) // 2,
               query_ns=after - before,
               utc=datetime.now(timezone.utc).isoformat())
    return row


def interval(first, last):
    if first["identity"] != last["identity"]:
        raise RuntimeError("target process identity changed")
    wall = (last["monotonic_ns"] - first["monotonic_ns"]) / 1e9
    cpu = last["cpu_seconds"] - first["cpu_seconds"]
    if not math.isfinite(cpu) or cpu < 0 or wall <= 0:
        raise ValueError("invalid CPU/time counters")
    return {"wall_seconds": wall, "cpu_seconds": cpu,
            "cpu_cores": cpu / wall}


def collect(pid, seconds, output, label, expected_identity=None):
    # Exclusive output prevents accidentally overwriting an earlier result.
    with output.open("x", encoding="utf-8") as f:
        def emit(row):
            f.write(json.dumps(row, allow_nan=False) + "\n")
            f.flush()
        emit({"kind": "header", "schema": 1, "pid": pid, "label": label,
              "requested_seconds": seconds, "logical_cpus": os.cpu_count(),
              "expected_identity": expected_identity,
              "scope": "process CPU only; frame/scene/state proof supplied by caller"})
        try:
            first = snapshot(pid)
            if expected_identity is not None and first["identity"] != expected_identity:
                raise RuntimeError("target does not match expected identity")
            emit(dict(first, kind="sample"))
            last = first
            deadline = first["monotonic_ns"] + int(seconds * 1e9)
            while last["monotonic_ns"] < deadline:
                time.sleep(max(0, min(1, (deadline - time.monotonic_ns()) / 1e9)))
                row = snapshot(pid)
                delta = interval(last, row)
                emit(dict(row, kind="sample", interval=delta))
                last = row
            result = dict(interval(first, last), kind="summary", complete=True,
                          identity=first["identity"], start_utc=first["utc"],
                          end_utc=last["utc"],
                          start_monotonic_ns=first["monotonic_ns"],
                          end_monotonic_ns=last["monotonic_ns"])
            emit(result)
            return result
        except Exception as exc:
            emit({"kind": "summary", "complete": False,
                  "error": type(exc).__name__})
            raise


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--pid", type=int, required=True)
    p.add_argument("--seconds", type=float, default=30)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--label", required=True)
    p.add_argument("--expected-identity")
    a = p.parse_args()
    if a.pid <= 0 or not math.isfinite(a.seconds) or not 1 <= a.seconds <= 120:
        p.error("positive PID and duration 1..120 seconds required")
    print(json.dumps(collect(a.pid, a.seconds, a.out, a.label, a.expected_identity)))


if __name__ == "__main__":
    main()
