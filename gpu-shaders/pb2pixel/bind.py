"""Private PS binding/codegen; reads game containers, never stages replacements."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import tempfile

sys.dont_write_bytecode = True
DIGEST = "7dcfe3de3f3fcbc199be405eae1596b4"
FILE_SHA = "c32b5ae870a80f68f8fced65e85ff4306a81ba2ab5dddb6983aff80c652c817a"
RECORD_SHA = "28bbce9b2d4585c69be7439fc59c75a21bc339dfdff3fede9d5c693938c46533"
PS_SHA = "0205c80d9035cd8de989cd758a3cf987eda459156ed2bdd9f6224c5ead258268"
BODY_SHA = "f01abd800dbbe594ffe931f3f373db599abb749695e031b0064a9825ceeeeabd"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--repo", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--isa", action="store_true")
    a = p.parse_args()
    material = a.repo / "gpu-shaders/materials"
    sys.path[:0] = [str(material), str(material.parent)]
    from build_sfb import Group
    import dxcom
    source = Path(r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12\0xb4751b0f.file")
    raw = source.read_bytes()
    if sha(raw) != FILE_SHA:
        raise ValueError("source container changed")
    group = Group(raw)
    binary = group.dxbc(1)
    if binary[4:20].hex() != DIGEST or sha(group.rec(1)) != RECORD_SHA or sha(binary) != PS_SHA:
        raise ValueError("wrong PS record")
    a.out.mkdir(parents=True, exist_ok=True)
    dst = a.out / "original.dxbc"
    dst.write_bytes(binary)
    ll = dxcom.dumpbin(str(dst))
    if sha(dxcom.body(ll).encode()) != BODY_SHA:
        raise ValueError("fresh DXC body changed")
    (a.out / "original.ll").write_bytes(ll.encode())
    vs = a.repo / "gpu-shaders/meshvs/work/1559714c.dxbc"
    if sha(vs.read_bytes()) != "5a7d10634d334e35ed62a5706fa784c905793f8d8a20a60539be9abd0ea970cc":
        raise ValueError("VS changed")
    result = dict(source=str(source), file_sha256=FILE_SHA, slot=1,
                  record_sha256=RECORD_SHA, ps_sha256=sha(binary), digest=DIGEST,
                  dump_sha256=sha(ll.encode()), body_sha256=sha(dxcom.body(ll).encode()),
                  dxc_sha256=sha(Path(dxcom.DXC).read_bytes()),
                  binding_script_sha256=sha(Path(__file__).read_bytes()),
                  extraction_tool_sha256=sha((material / "build_sfb.py").read_bytes()),
                  dxcom_sha256=sha((material / "dxcom.py").read_bytes()),
                  native_root_signature=False, runtime_execution=False)
    if a.isa:
        import rga_ps
        tempfile.tempdir = str(a.out.resolve())
        st, isa = rga_ps.stats(str(vs), str(dst), "gfx1103")
        (a.out / "original.gfx1103.isa.txt").write_bytes(isa.encode())
        result.update(isa_sha256=sha(isa.encode()), stats=st,
                      rga_sha256=sha(Path(rga_ps.RGA).read_bytes()),
                      rga_helper_sha256=sha((material / "rga_ps.py").read_bytes()),
                      generated_root_sha256=sha(rga_ps.root_sig(ll, dxcom.dumpbin(str(vs))).encode()),
                      generated_gpso_sha256=sha(rga_ps.gpso(dxcom.dumpbin(str(vs))).encode()),
                      codegen_scope="Synthetic RGA root/8 RGBA16F targets; offline diagnostic only")
    if source.read_bytes() != raw or dst.read_bytes() != binary:
        raise ValueError("input changed during binding")
    (a.out / "binding.json").write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
