"""Private, bounded fresh fogpar proof; read-only donor/staging, no shader build.

Run from this checkout with Python -I. Only the output manifest and a disposable
temporary directory are written. Inputs/harness are frozen before interpretation;
no LL, DXIL, containers, scene values or output values enter the manifest.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib
import json
import multiprocessing as mp
import os
from pathlib import Path
import platform
import re
import struct
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
MAIN = Path("C:/Users/Ahmad Hamza/nioh3-engine")
DONOR = Path("D:/nioh3-wt/allygpu/gpu-shaders")
DXC_REL = "tools/rdts/RadeonDeveloperToolSuite-2026-05-28-1806/utils/dx12/dxc/dxc.exe"
PINS = {
    "stock_pack": "4784aabe40fdb10646babde1df734123ce713dc56089a55b4efa0db43bf4f993",
    "candidate_pack": "81007b3f2bc066e4f4951b9bc4bac37be29bdb934efbc9fe83275d65b767334e",
    "candidate_record": "c259803c1774a723054b1e0caf99cc8c5fc9d4a341d61f7c53d3928121d56e7a",
    "candidate_ll": "c120d993bcfdd962c6908e3c9027b3ce8addf2b0d9b33f8ff1e626c6be50f375",
    "candidate_body": "6436f9f08a17d2eaa37e8881e97134125b4c6db0a4fbd27eec4adfe8c10bbade",
    "stock_ll": "03357c63e1f97418917347dccb8d97bf1440e4524006dd4fae0f529d05fa17b9",
    "stock_record": "63e2aadb479ce45ccc29f625deaffe03fed4bfeb72628401c586d179ff858fa4",
    "stock_body": "e396bcb95555be28622a68513897c581ee2186495472ca06a1e092d4077c0cdd",
    "dxc": "ae4bf100b8c64ab03cdc0ee5996d5f212e96c59d6d37340359342d714ac6c004",
}
SOURCES = ["gi/verify.py", "gi/dxil_interp.py", "gi/llir.py",
           "gi/ll2hlsl.py", "gi/scenes.py", "fog/vfscene.py",
           "idrk.py", "gi/s2g.py"]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def body(text):
    return "\n".join(line for line in text.replace("\r\n", "\n").split("\n")
                     if line and not line.startswith(";"))


def require(condition, message):
    if not condition:
        raise ValueError(message)


def same_body(left, right):
    require(body(left) == body(right), "dumpbin/retained LL body mismatch")


def file_info(path, data=None):
    data = path.read_bytes() if data is None else data
    return {"path": str(path.resolve()), "bytes": len(data), "sha256": sha(data)}


def freeze(path, destination, report, label, pin=None):
    data = path.read_bytes()
    info = file_info(path, data)
    if pin:
        require(info["sha256"] == pin, "pinned input hash mismatch: " + label)
    report[label] = info
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)
    return data


def record(data):
    import idrk
    import s2g
    unpacked = idrk.unpack(data)
    _, base, entries = s2g.layout(unpacked)
    require(len(entries) == 363, "unexpected stock/candidate record count")
    return [unpacked[base + offset:base + offset + size] for offset, size in entries]


def atom(value):
    # Preserve float32 sign and NaN bits represented by this interpreter.
    if isinstance(value, float):
        return ("f32", struct.pack("<f", value).hex())
    if isinstance(value, bytes):
        return ("bytes", value.hex())
    if isinstance(value, (list, tuple)):
        return tuple(atom(item) for item in value)
    return value


def strict_state(state):
    out = {}
    for resource, (final, writes) in state.items():
        trace = {}
        for write in writes:
            require(len(write) in (4, 5), "unsupported UAV write shape")
            key = write[:-2]
            trace.setdefault(key, []).append(atom(write[-2:]))
        out[resource] = (atom(final), trace)
    return out


def comparator_self_tests():
    def state(value, mask=15, resource=(1, 0), coordinate=(0, 0, 0)):
        values = (value, 0.0, 0.0, 0.0)
        return {resource: ([(coordinate, values)], [((0, 0, 0), coordinate, values, mask)])}

    baseline = strict_state(state(0.0))
    require(baseline == strict_state(state(0.0)), "identical strict state rejected")
    for changed in (state(-0.0), state(0.0, mask=7), state(0.0, resource=(1, 1)),
                    state(0.0, coordinate=(1, 0, 0))):
        require(baseline != strict_state(changed), "strict state mutation accepted")
    nan_a = struct.unpack("<f", struct.pack("<I", 0x7fc00001))[0]
    nan_b = struct.unpack("<f", struct.pack("<I", 0x7fc00002))[0]
    require(strict_state(state(nan_a)) != strict_state(state(nan_b)), "NaN payload mutation accepted")
    same_body("target xyz\n; comment\n", "; header\ntarget xyz\n")
    for changed in ("", "target xyz\nBAD\n"):
        try:
            same_body("target xyz\n", changed)
        except ValueError:
            pass
        else:
            raise ValueError("synthetic identity mutation accepted")
    return "PASS: signed zero, represented NaN payload, mask, resource, address; identity accept/reject"


_verify = None
_captured = []
_stock = None
_candidate = None


def initialize(snapshot, stock, candidate):
    global _verify, _stock, _candidate
    sys.dont_write_bytecode = True
    sys.path[:0] = [str(Path(snapshot) / "gi"), str(snapshot)]
    _verify = importlib.import_module("verify")
    original_uav = _verify.D.uav_state

    def capture(scene):
        state = original_uav(scene)
        _captured.append(strict_state(state))
        return state

    _verify.D.uav_state = capture
    _stock, _candidate = stock, candidate


def run_seed(task):
    seed, realistic, lights = task
    os.environ["VFSCAT_LIGHTS"] = lights
    _captured.clear()
    result = _verify.run_one((_stock, _candidate, "vfscat", seed, False, realistic))
    require(len(_captured) == 2, "expected two UAV snapshots")
    left, right = _captured
    resources = set(left) | set(right)
    resource_diffs = len(set(left) ^ set(right))
    trace_diffs = 0
    final_diffs = 0
    for resource in resources & set(left) & set(right):
        final_a, trace_a = left[resource]
        final_b, trace_b = right[resource]
        final_diffs += int(final_a != final_b)
        trace_diffs += sum(trace_a.get(key) != trace_b.get(key)
                           for key in set(trace_a) | set(trace_b))
    return {"seed": seed, "writes": result["n"], "exact_ulp_writes": result["exact"],
            "maxulp": result["maxulp"], "setdiff": result["setdiff"],
            "strict_resource_diffs": resource_diffs, "strict_trace_diffs": trace_diffs,
            "strict_final_resource_diffs": final_diffs, "coverage": result["cov"]}


def summarize(results, lo, hi, realistic, lights):
    coverage = [sum(result["coverage"][i] for result in results)
                for i in range(len(results[0]["coverage"]))]
    require([result["seed"] for result in results] == list(range(lo, hi)),
            "missing or duplicated seed result")
    sums = ["writes", "exact_ulp_writes", "setdiff", "strict_resource_diffs",
            "strict_trace_diffs", "strict_final_resource_diffs"]
    report = {key: sum(result[key] for result in results) for key in sums}
    report.update({"seed_range_half_open": [lo, hi], "seeds": hi - lo,
                   "realistic": realistic, "VFSCAT_LIGHTS": lights,
                   "maxulp": max(result["maxulp"] for result in results),
                   "blocks_hit": sum(value > 0 for value in coverage),
                   "blocks_total": len(coverage), "block_execution_counts": coverage,
                   "per_seed_results_sha256": sha(json.dumps(results, sort_keys=True).encode())})
    report["result"] = "PASS" if (report["writes"] > 0 and
        report["writes"] == report["exact_ulp_writes"] and report["maxulp"] == 0 and
        all(report[key] == 0 for key in sums[2:])) else "FAIL"
    return report


def execute(snapshot, stock, candidate, lo, hi, realistic, lights, jobs, timeout):
    start = time.monotonic()
    with mp.get_context("spawn").Pool(jobs, initializer=initialize,
                                      initargs=(snapshot, stock, candidate)) as pool:
        pending = pool.map_async(run_seed, [(seed, realistic, lights)
                                           for seed in range(lo, hi)], chunksize=1)
        results = pending.get(timeout=timeout)
    report = summarize(results, lo, hi, realistic, lights)
    report["elapsed_seconds"] = round(time.monotonic() - start, 3)
    return report


def runtime_files():
    import numpy
    numpy_root = Path(numpy.__file__).resolve().parent
    python_root = Path(sys.executable).resolve().parent
    paths = {Path(sys.executable), numpy_root / "__init__.py"}
    paths.update(python_root.glob("python*.dll"))
    for base in (numpy_root, numpy_root.parent / "numpy.libs"):
        if base.is_dir():
            paths.update(base.rglob("*.pyd"))
            paths.update(base.rglob("*.dll"))
    return {"python": sys.version, "platform": platform.platform(),
            "numpy_version": numpy.__version__,
            "files": [file_info(path) for path in sorted(paths)]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--staging", type=Path, default=MAIN / "gpu-shaders")
    parser.add_argument("--donor", type=Path, default=DONOR)
    parser.add_argument("--stock-ll", type=Path, default=ROOT / "re/scripts_gpu/out/VFSCAT_235.ll")
    parser.add_argument("--dxc", type=Path, default=MAIN / DXC_REL)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true", help="light comparator checks only; no shader/tool access")
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--timeout", type=int, default=1200, help="seconds per pass")
    parser.add_argument("--smoke", action="store_true", help="two seeds/pass; never full coverage")
    args = parser.parse_args()
    if args.self_test:
        print(comparator_self_tests())
        return 0
    require(1 <= args.jobs <= 16 and args.timeout > 0, "invalid bounds")
    require(args.output is not None, "--output is required for shader proof runs")
    output = args.output.resolve()
    require(output.is_relative_to(ROOT), "manifest must stay in the owned checkout")
    require(not output.is_relative_to(ROOT / "gpu-shaders"), "manifest cannot alter shader staging")
    require(not output.exists(), "manifest exists; choose a new output path")
    start = time.monotonic()
    manifest = {"schema": "nioh3-private-fogpar-bound-proof-v1", "result": "ERROR",
                "started_utc": datetime.now(timezone.utc).isoformat(),
                "command": [str(Path(__file__).resolve()), *sys.argv[1:]],
                "smoke": args.smoke, "jobs": args.jobs, "timeout_seconds_per_pass": args.timeout,
                "pins": PINS, "files": {}, "passes": {}, "controls": {},
                "scope": "Fresh finite synthetic DXIL interpretation; no native GPU execution, image equality or performance claim.",
                "strict_comparison": "All UAV resource keys, per-thread/address ordered writes including masks and float32 bits, plus final contents. NaN payloads are limited to Python float/interpreter representation."}
    try:
        manifest["comparator_self_tests"] = comparator_self_tests()
        manifest["files"]["runner"] = file_info(Path(__file__))
        manifest["runtime"] = runtime_files()
        with tempfile.TemporaryDirectory(prefix="nioh3-fogpar-bound-") as directory:
            temporary = Path(directory)
            snapshot = temporary / "gpu-shaders"
            for source in SOURCES:
                freeze(ROOT / "gpu-shaders" / source, snapshot / source,
                       manifest["files"], "harness/" + source)
            sys.path[:0] = [str(snapshot / "gi"), str(snapshot)]
            packed = {}
            for label, relative in (("stock_pack", "backup"), ("candidate_pack", "replacement")):
                data = freeze(args.staging / relative / "0xa9910db8.file", temporary / (label + ".file"),
                              manifest["files"], label, PINS[label])
                packed[label] = record(data)
            require([i for i, (a, b) in enumerate(zip(packed["stock_pack"], packed["candidate_pack"]))
                     if a != b] == [235], "unexpected packed record changes")
            candidate_record = packed["candidate_pack"][235]
            require(sha(candidate_record) == PINS["candidate_record"], "candidate record hash")
            require(sha(packed["stock_pack"][235]) == PINS["stock_record"], "stock record hash")
            manifest["record_index"] = 235
            manifest["record_hashes"] = {"stock": sha(packed["stock_pack"][235]),
                                          "candidate": sha(candidate_record)}
            for label, relative in (("retained_dxil", "gi/out/vf235_par.dxil"),
                                    ("retained_shaderswap", "fog/out/psoswap/641524ee74b197c7d8f1535066bb2c1a.dxil")):
                data = freeze(args.donor / relative, temporary / (label + ".dxil"),
                              manifest["files"], label, PINS["candidate_record"])
                require(data == candidate_record, "retained record byte mismatch")
            stock = temporary / "stock.ll"
            candidate = temporary / "candidate.ll"
            stock_text = freeze(args.stock_ll, stock, manifest["files"], "stock_ll", PINS["stock_ll"]).decode("utf-8")
            candidate_text = freeze(args.donor / "gi/out/vf235_par.ll", candidate,
                                    manifest["files"], "candidate_ll", PINS["candidate_ll"]).decode("utf-8")
            require(sha(body(candidate_text).encode()) == PINS["candidate_body"], "candidate body hash")
            tool = temporary / "dxc" / "dxc.exe"
            freeze(args.dxc, tool, manifest["files"], "dxc", PINS["dxc"])
            for name in ("dxcompiler.dll", "dxil.dll"):
                freeze(args.dxc.parent / name, tool.parent / name, manifest["files"], "dxc/" + name)
            dumped = {}
            for label, retained in (("stock", stock_text), ("candidate", candidate_text)):
                binary = temporary / (label + ".dxil")
                binary.write_bytes(packed[label + "_pack"][235])
                process = subprocess.run([str(tool), "-dumpbin", str(binary)],
                    capture_output=True, check=True, timeout=60)
                text = process.stdout.decode("utf-8")
                # The tracked stock LL begins with a UTF-8 BOM (EF BB BF).
                # Strip only this marker before its leading comment for comparison.
                # The interpreter still reads the exact pinned frozen LL bytes.
                comparison = retained
                transport_comment = label == "stock" and retained.startswith("\ufeff;")
                if transport_comment:
                    comparison = retained[1:]
                same_body(text, comparison)
                require(sha(body(text).encode()) == PINS[label + "_body"], "packed disassembly body hash")
                dumped[label] = text
                manifest.setdefault("disassembly", {})[label] = {
                    "normalized_body_sha256": sha(body(text).encode()),
                    "normalized_body_lines": len(body(text).splitlines()),
                    "stock_leading_comment_UTF8_BOM_removed_for_comparison": transport_comment,
                    "normalized_body_matches_retained_input": True,
                    "execution_input": "Exact byte-frozen retained LL, hashed in files."}
            # Existing proof-bridge identity controls, all operating on snapshots.
            alternate = freeze(args.staging / "gi/out/vf235_onlypar.ll", temporary / "alternate.ll",
                               manifest["files"], "alternate_onlypar_ll").decode("utf-8")
            for name, damaged in (("truncated_ll", "\n".join(body(candidate_text).splitlines()[:-1])),
                                  ("appended_ll", candidate_text + "\nINVALID_PROOF_INPUT\n"),
                                  ("alternate_onlypar_ll", alternate)):
                try:
                    same_body(dumped["candidate"], damaged)
                except ValueError:
                    manifest["controls"][name] = "REJECTED"
                else:
                    raise ValueError("identity control accepted: " + name)
            same_body(dumped["candidate"], "; ignored header\n" + candidate_text)
            manifest["controls"]["comment_header"] = "ACCEPTED"
            specifications = [("coverage", 0, 768, False, "moderate"),
                              ("holdout", 768, 2304, False, "moderate"),
                              ("realistic", 0, 64, True, "moderate"),
                              ("realistic_heavy", 0, 32, True, "heavy")]
            for name, lo, hi, realistic, lights in specifications:
                if args.smoke:
                    hi = lo + 2
                result = execute(str(snapshot), str(stock), str(candidate), lo, hi,
                                 realistic, lights, args.jobs, args.timeout)
                manifest["passes"][name] = result
                print(name + ": " + json.dumps({k: result[k] for k in
                    ("result", "seeds", "writes", "maxulp", "strict_trace_diffs",
                     "strict_final_resource_diffs", "blocks_hit", "blocks_total", "elapsed_seconds")}), flush=True)
                require(result["result"] == "PASS", "equivalence failed: " + name)
            if not args.smoke:
                for name in ("coverage", "holdout"):
                    require(manifest["passes"][name]["blocks_hit"] == manifest["passes"][name]["blocks_total"],
                            "incomplete original block coverage: " + name)
            # Mutate only the frozen LL. Detect value changes and removed output.
            pattern = re.compile(r"(?m)^  call void @dx\.op\.textureStore\.f32\(.*$")
            lines = pattern.findall(candidate_text)
            require(len(lines) == 1, "expected one output store for mutation controls")
            store = lines[0]
            value_mutant = re.sub(r"float %\w+", "float 1.000000e+00", store, count=1)
            require(value_mutant != store, "value mutation made no change")
            for name, changed in (("output_value_mutation", value_mutant), ("output_store_removed", "")):
                mutant = temporary / (name + ".ll")
                mutant.write_text(candidate_text.replace(store, changed), encoding="utf-8", newline="\n")
                result = execute(str(snapshot), str(stock), str(mutant), 0, 2, False,
                                 "moderate", min(2, args.jobs), args.timeout)
                require(result["result"] == "FAIL", "equivalence mutation not caught: " + name)
                manifest["controls"][name] = {"result": "REJECTED", "mutant_ll_sha256": sha(mutant.read_bytes()),
                                                "summary": result}
            # Assert all original inputs/tools are still the hashed files; interpreter
            # execution used frozen input/source bytes even if a source changed.
            watched = list(manifest["files"].values()) + manifest["runtime"]["files"]
            require(all(sha(Path(info["path"]).read_bytes()) == info["sha256"] for info in watched),
                    "input/tool changed during execution")
            manifest["all_source_hashes_unchanged_after_run"] = True
            manifest["result"] = "PASS_SMOKE" if args.smoke else "PASS"
    except Exception as error:
        manifest["error"] = type(error).__name__ + ": " + str(error)
    manifest["finished_utc"] = datetime.now(timezone.utc).isoformat()
    manifest["elapsed_seconds"] = round(time.monotonic() - start, 3)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"result": manifest["result"], "manifest": str(output),
                      "manifest_sha256": sha(output.read_bytes()), "error": manifest.get("error")}), flush=True)
    return 0 if manifest["result"] in ("PASS", "PASS_SMOKE") else 1


if __name__ == "__main__":
    mp.freeze_support()
    sys.exit(main())
