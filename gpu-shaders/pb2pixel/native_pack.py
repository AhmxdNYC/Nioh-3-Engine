"""One-record private packing plus captured Windows graphics PSO admission; no draws."""
import argparse
import contextlib
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
PS = "0205c80d9035cd8de989cd758a3cf987eda459156ed2bdd9f6224c5ead258268"
CAND = "bbd47920e47762776bf232ac49b06f4dd344a35e81b9cb80b14c1ca4bd9b2b34"
VS = "5a7d10634d334e35ed62a5706fa784c905793f8d8a20a60539be9abd0ea970cc"
JOURNAL = "f1845f5f3d61f7f7d59538cb187fef57df433f6e3f3fced9c852b75cec97c40d"
FILE = "c32b5ae870a80f68f8fced65e85ff4306a81ba2ab5dddb6983aff80c652c817a"
ROOT = "dc65a1fc5ca7cb3fbcd100895637c8eab667904e6f613625597bad8890d0031c"


def sha(b):
    return hashlib.sha256(b).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--repo", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--admit", action="store_true")
    a = p.parse_args()
    source = Path(r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3")
    sys.path[:0] = [str(a.repo / "gpu-shaders/pb2guard"),
                   str(a.repo / "gpu-shaders/materials"), str(a.repo / "gpu-shaders")]
    import pack_check as pack
    import pipeline_extract as extract
    import dxcom
    import idrk
    from build_sfb import Group
    import uv_probe
    import sfb_rewrite as S
    from check_uv import certificate
    for name, expected in {
        "pipeline_extract.py": "ee407ce0384b058ec6d2a809edad03ac2bc6a947407b513f115f28178250b5b2",
        "pipeline_admit.cpp": "eb23ec8e978e81f2aa51bfd3e369db454acccc9586e50e67fd83a262c07db7a1",
        "pack_check.py": "a5d67ae042f37c91af6e681a0379aa70c9efbce9cfa676d27ca53386ffb687ad",
    }.items():
        require(sha((a.repo / "gpu-shaders/pb2guard" / name).read_bytes()) == expected, "dependency changed " + name)
    candidate = (a.out / "uv-hoist.dxbc").read_bytes()
    original = (a.out / "original.dxbc").read_bytes()
    require(sha(candidate) == CAND and sha(original) == PS, "PS binding")
    oll = dxcom.dumpbin(str(a.out / "original.dxbc"))
    cll = dxcom.dumpbin(str(a.out / "uv-hoist.dxbc"))
    require(dxcom.body(cll) == dxcom.body(uv_probe.rewrite(oll, S, dxcom)), "whole candidate body")
    require(certificate(oll, S) == certificate(cll, S), "output/resource/control certificate")
    require(dxcom.validate_sign(candidate) == candidate, "signed candidate changed")
    op, cp = dxcom.parts(original), dxcom.parts(candidate)
    require(all(op.get(k) == cp.get(k) for k in ("ISG1", "OSG1", "PSV0", "SFI0")), "metadata")
    gamefile = source / "shader_dx12/0xb4751b0f.file"
    raw = gamefile.read_bytes()
    require(sha(raw) == FILE, "original packed file")
    pack.ORIGINAL, pack.ORIGINAL_SHA = original[4:20].hex(), PS
    pack.CANDIDATE, pack.CANDIDATE_SHA = candidate[4:20].hex(), CAND
    receipt = pack.prove(raw, candidate, 1)
    group = Group(raw)
    packed = idrk.pack(group.replace({1: candidate}), raw)
    require(sha(packed) == receipt["hypothetical_packed_sha256"], "pack proof mismatch")
    restored = Group(packed).replace({1: original})
    require(restored == group.p, "roundtrip payload differs")
    negatives = 0
    for blob, slot in ((candidate, 0), (candidate[:-1] + bytes([candidate[-1] ^ 1]), 1)):
        try:
            pack.prove(raw, blob, slot)
        except AssertionError:
            negatives += 1
        else:
            raise ValueError("pack mutant accepted")
    packedpath = a.out / "private-pack/0xb4751b0f.file"
    packedpath.parent.mkdir(parents=True, exist_ok=True)
    packedpath.write_bytes(packed)
    journal = source / "Nioh3EngineFix_pj/pj_win_dgpu_sm68.bin"
    require(sha(journal.read_bytes()) == JOURNAL, "recorded journal changed")
    out = a.out / "native-pipeline"
    extract.OUT = out
    args = sys.argv
    try:
        sys.argv = ["pipeline_extract", str(journal)]
        with contextlib.redirect_stdout(io.StringIO()):
            extract.main()
    finally:
        sys.argv = args
    binding = json.loads((out / "binding.json").read_text())
    require(binding["original.dxbc"] == VS and binding["material.dxbc"] == PS, "captured companions")
    require(len(binding["records"]) == 8, "captured record count")
    for i, row in enumerate(binding["records"]):
        require(row["index"] == i and row["root_sha256"] == ROOT, "record root")
        require(sha((out / f"desc{i}.bin").read_bytes()) == row["desc_sha256"], "descriptor")
        require(sha((out / f"root{i}.bin").read_bytes()) == ROOT, "root blob")
    # The shared extractor binds its prior VS candidate too. Replace ONLY the
    # private helper input with this PS; retained VS and original PS stay exact.
    (out / "candidate.dxbc").write_bytes(candidate)
    binding.update({"candidate.dxbc": CAND, "replacement_stage": "PS", "original.dxbc": VS,
                    "material.dxbc": PS, "pack_sha256": sha(packed)})
    (out / "binding.json").write_text(json.dumps(binding, indent=2))
    code = (a.repo / "gpu-shaders/pb2guard/pipeline_admit.cpp").read_text()
    for before, after in (
        ("d.pRootSignature=rs;d.PS={ps.data(),ps.size()};", "d.pRootSignature=rs;d.VS={vs.data(),vs.size()};"),
        ("auto& blob=which?cand:vs;d.VS={blob.data(),blob.size()};", "auto& blob=which?cand:ps;d.PS={blob.data(),blob.size()};"),
    ):
        require(code.count(before) == 1, "helper adaptation drift")
        code = code.replace(before, after)
    helper = out / "pixel_admit.cpp"
    helper.write_text(code)
    result = dict(scope="PRIVATE_SINGLE_RECORD_PACK_NO_INSTALL", pack=receipt,
                  roundtrip_original_payload=True, negative_checks=negatives,
                  candidate_sha256=CAND, binding_sha256=sha((out / "binding.json").read_bytes()),
                  helper_source_sha256=sha(helper.read_bytes()), native_draws=False,
                  proton_gate=False, visual_gate=False, performance_gate=False)
    snapshots = {f: f.read_bytes() for f in out.iterdir() if f.suffix in (".dxbc", ".bin")}
    if a.admit:
        setup = a.repo / "tools/msvc/setup_x64.bat"
        cmd = f'call "{setup}" >nul && cl /nologo /W4 /WX /EHsc /std:c++17 /O2 "{helper}" /Fepixel_admit.exe /link d3d12.lib dxgi.lib'
        build = subprocess.run('cmd /d /s /c "' + cmd + '"', cwd=out, capture_output=True, text=True, timeout=90)
        require(build.returncode == 0, build.stdout + build.stderr)
        run = subprocess.run([str(out / "pixel_admit.exe")], cwd=out, capture_output=True, text=True, timeout=120)
        result.update(scope="WINDOWS_CAPTURED_GRAPHICS_PSO_ONLY_NO_DRAW", returncode=run.returncode,
                      stdout=run.stdout, stderr=run.stderr, helper_exe_sha256=sha((out / "pixel_admit.exe").read_bytes()))
        print(run.stdout + run.stderr)
        require(run.returncode == 0, "native compiler admission failed")
    require(all(f.read_bytes() == b for f, b in snapshots.items()), "compiler input changed")
    require(gamefile.read_bytes() == raw and sha(journal.read_bytes()) == JOURNAL, "game source changed")
    require(packedpath.read_bytes() == packed, "private pack changed")
    result["game_sources_unchanged"] = True
    (a.out / "pack-native.json").write_text(json.dumps(result, indent=2))
    print("PASS: one record, original roundtrip, 2 negatives; captured admission=" + str(a.admit))


if __name__ == "__main__":
    main()
