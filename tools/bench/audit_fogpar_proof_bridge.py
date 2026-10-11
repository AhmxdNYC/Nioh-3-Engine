"""Compare pinned fogpar pack to retained proof inputs, without staging/building.

The donor worktree is read-only. Only a TemporaryDirectory holds an extracted
record for dxc -dumpbin; it is removed on success or failure. No game files used.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_gpu_candidates import GS, ROOT, records, sha

PACK_SHA = "81007b3f2bc066e4f4951b9bc4bac37be29bdb934efbc9fe83275d65b767334e"
RECORD_SHA = "c259803c1774a723054b1e0caf99cc8c5fc9d4a341d61f7c53d3928121d56e7a"
PROOF_SHA = "c120d993bcfdd962c6908e3c9027b3ce8addf2b0d9b33f8ff1e626c6be50f375"
DONOR = Path("D:/nioh3-wt/allygpu/gpu-shaders")
DXC = ROOT / "tools/rdts/RadeonDeveloperToolSuite-2026-05-28-1806/utils/dx12/dxc/dxc.exe"


def body(text):
    # Same comparison as build_fogpar._body: omit empty/comment lines only.
    return [line for line in text.replace("\r\n", "\n").split("\n")
            if line and not line.startswith(";")]


def require_equal(left, right):
    if body(left) != body(right):
        raise ValueError("packed disassembly differs from retained proof input")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--donor", type=Path, default=DONOR)
    args = parser.parse_args()
    pack = GS / "replacement/0xa9910db8.file"
    assert sha(pack.read_bytes()) == PACK_SHA, "staged pack hash"
    record = records(pack)[235]
    assert sha(record) == RECORD_SHA, "packed record hash"
    retained = args.donor / "gi/out/vf235_par.dxil"
    standalone = args.donor / "fog/out/psoswap/641524ee74b197c7d8f1535066bb2c1a.dxil"
    proof = args.donor / "gi/out/vf235_par.ll"
    for path in (retained, standalone):
        assert path.read_bytes() == record, "retained record identity: " + str(path)
    proof_bytes = proof.read_bytes()
    assert sha(proof_bytes) == PROOF_SHA, "retained proof LL hash"
    proof_text = proof_bytes.decode("utf-8")
    with tempfile.TemporaryDirectory(prefix="nioh3-fogpar-proof-") as directory:
        extracted = Path(directory) / "record235.dxil"
        extracted.write_bytes(record)
        dumped = subprocess.run([str(DXC), "-dumpbin", str(extracted)],
                                capture_output=True, text=True, check=True).stdout
    require_equal(dumped, proof_text)
    # Controls test the actual comparator without modifying any input file.
    rejected = 0
    for damaged in ("\n".join(body(proof_text)[:-1]),
                    proof_text + "\nINVALID_PROOF_INPUT\n",
                    (GS / "gi/out/vf235_onlypar.ll").read_text(encoding="utf-8")):
        try:
            require_equal(dumped, damaged)
        except ValueError:
            rejected += 1
        else:
            raise AssertionError("mismatched proof input accepted")
    require_equal(dumped, "; ignored path/header comment\n" + proof_text)
    report = {
        "audit": "PASS",
        "pack_sha256": PACK_SHA,
        "packed_record_sha256": RECORD_SHA,
        "retained_dxil_and_shaderswap_byte_equal": True,
        "retained_proof_ll_sha256": sha(proof_bytes),
        "dxc_sha256": sha(DXC.read_bytes()),
        "normalized_body_sha256": hashlib.sha256(
            "\n".join(body(dumped)).encode("utf-8")).hexdigest(),
        "normalized_body_lines": len(body(dumped)),
        "packed_dump_equals_retained_proof_input": True,
        "rejection_controls": rejected,
        "comment_header_control": "PASS",
        "scope": "current artifact identity; archived proof execution/input hashes and runtime are not revalidated",
    }
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
