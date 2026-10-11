"""Read-only lodu/fogpar provenance audit; never installs or rebuilds assets."""
import hashlib
import json
from pathlib import Path
import sys
import zipfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
GS = ROOT / "gpu-shaders"
sys.path[:0] = [str(GS), str(GS / "gi"), str(GS / "materials")]
import idrk
import s2g
from build_sfb import Group


def sha(data):
    return hashlib.sha256(data).hexdigest()


def records(path):
    if path.name == "0x8d9282e6.file":
        group = Group(path.read_bytes())
        return [group.rec(i) for i in range(len(group.ents))]
    data = idrk.unpack(path.read_bytes())
    _, base, entries = s2g.layout(data)
    return [data[base + off:base + off + size] for off, size in entries]


def audit(item, file, index, original_hash, replacement_hash, digest, staged):
    original = GS / "backup" / file
    replacement = GS / staged / file
    assert sha(original.read_bytes()) == original_hash, f"{item}: original hash"
    assert sha(replacement.read_bytes()) == replacement_hash, f"{item}: staged hash"
    old, new = records(original), records(replacement)
    assert len(old) == len(new), f"{item}: record count"
    changed = [i for i, (a, b) in enumerate(zip(old, new)) if a != b]
    assert changed == [index], f"{item}: unexpected changed records {changed}"
    record = new[index]
    if record[:8] == b"CXDK2000":
        size = int.from_bytes(record[8:12], "little")
        record = record[16:16 + size]
    assert record[:4] == b"DXBC"
    assert record[4:20].hex() == digest, f"{item}: new digest"
    result = dict(item=item, original_sha256=original_hash,
                  replacement_sha256=replacement_hash, records=len(old),
                  changed_records=changed, unchanged_records=len(old) - 1,
                  record_digest=digest, record_sha256=sha(record))
    return result, record


def main():
    lod, lod_record = audit(
        "lodu", "0x8d9282e6.file", 2,
        "ec0f5283660caa64565e17cfb8f2dc4d06ee498278fe1a832db16d27b916ebeb",
        "5f7f3b120b909c74c0238d7af3f4f3cd43299840b21a5a49e195545ea54150f1",
        "d5a1a069857feee3a6589f1c788aca52", "replacement/cull4")
    with zipfile.ZipFile(GS / "meshcull/out/cull4_records.zip") as pack:
        assert pack.testzip() is None, "lodu: ZIP CRC"
        assert pack.read("manifest.tsv") == (GS / "meshcull/cull4_manifest.tsv").read_bytes()
        assert pack.read("63c07c21f09fbf65ed3486e04da80c00.dxbc") == lod_record
    assert (GS / "meshcull/work/lod2_dup.dxbc").read_bytes() == lod_record
    lod["transport_and_retained_dxil_match"] = True
    lod["flat_replacement_sha256"] = sha((GS / "replacement/0x8d9282e6.file").read_bytes())
    lod["flat_replacement_is_lodu"] = lod["flat_replacement_sha256"] == lod["replacement_sha256"]
    fog, _ = audit(
        "fogpar", "0xa9910db8.file", 235,
        "4784aabe40fdb10646babde1df734123ce713dc56089a55b4efa0db43bf4f993",
        "81007b3f2bc066e4f4951b9bc4bac37be29bdb934efbc9fe83275d65b767334e",
        "5473bfb797f181002f6cd88547d207e4", "replacement")
    fog["proven_ll_present"] = (GS / "gi/out/vf235_par.ll").is_file()
    fog["standalone_shaderswap_present"] = (GS / "fog/out/psoswap/641524ee74b197c7d8f1535066bb2c1a.dxil").is_file()
    print(json.dumps({"artifact_audit": "PASS", "candidates": [lod, fog],
                      "scope": "hashes/record identity only; no new equivalence or runtime verdict"}, indent=2))


if __name__ == "__main__":
    main()
