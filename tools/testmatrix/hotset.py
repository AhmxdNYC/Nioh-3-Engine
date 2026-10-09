r"""hotset.py - build a ShaderHot set (runtime shader A/B, notes\fast-testing.md) from a shader_dx12 .file pair.

The game loads shader_dx12\*.file at boot, so a loose-file A/B used to need one launch per arm. With [ShaderHot] the ASI
keeps a twin PSO for every PSO built from a listed shader and LiveCtl 'Hot.<set> = 0/1' picks which one is bound. This
tool writes the set folder from the stock and the replacement .file:

    py -I tools\testmatrix\hotset.py build --name ghdm --orig gpu-shaders\backup\0x0104840c.file \
          --repl gpu-shaders\replacement\0x0104840c.file --installed auto --out <GameDir>\Nioh3EngineFix_hot
    py -I tools\testmatrix\hotset.py selftest

Every DXBC/DXIL container of the two payloads is paired by position (the replacements keep the record table: same count,
same order). For each pair whose bytes differ:
    installed = orig  ->  on_<digest of the stock container>.dxil   = the replacement container (bound while ON)
    installed = repl  ->  off_<digest of the replacement>.dxil      = the stock container       (bound while OFF)
so 'Hot.<name> = 1' always means "replacement active", whatever file is installed. --installed auto compares the SHA-256
of --game-file (default <GameDir>\shader_dx12\<name of --orig>) with both inputs. Several .file pairs can go into one set
(repeat --orig/--repl in order). The folder of the set is replaced; other sets are left alone.
"""
import argparse
import hashlib
import os
import shutil
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
GAME = r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3"


def _idrk():
    sys.path.insert(0, os.path.join(ROOT, "gpu-shaders"))
    import idrk  # noqa: E402
    return idrk


def containers(payload: bytes):
    """[(offset, bytes)] of every DXBC container in a shader-group payload, in file order."""
    out, i = [], payload.find(b"DXBC")
    while i >= 0:
        ok = False
        if i + 32 <= len(payload):
            size, n = struct.unpack_from("<II", payload, i + 24)
            if 32 <= size <= len(payload) - i and 0 < n <= 64:
                out.append((i, payload[i:i + size]))
                i = payload.find(b"DXBC", i + size)
                ok = True
        if not ok:
            i = payload.find(b"DXBC", i + 4)
    return out


def key_of(c: bytes) -> str:
    """psoswap file key: container digest (bytes 4..20), or f + FNV-1a-64 of the container when the digest is zero."""
    d = c[4:20]
    if any(d):
        return d.hex()
    h = 0xcbf29ce484222325
    for b in c:
        h = ((h ^ b) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return "f%016x" % h


def has_dxil(c: bytes) -> bool:
    n = struct.unpack_from("<I", c, 28)[0]
    for k in range(n):
        po = struct.unpack_from("<I", c, 32 + 4 * k)[0]
        if c[po:po + 4] == b"DXIL":
            return True
    return False


def pair_files(po: bytes, pr: bytes, installed: str):
    """-> [(file name, bytes)] for one payload pair (installed = 'orig' | 'repl')."""
    co, cr = containers(po), containers(pr)
    if len(co) != len(cr):
        raise SystemExit("container count differs (%d stock vs %d replacement): not a record-for-record replacement" % (len(co), len(cr)))
    out = []
    for (_, a), (_, b) in zip(co, cr):
        if a == b:
            continue
        if struct.unpack_from("<I", a, 28)[0] and has_dxil(a) != has_dxil(b):
            raise SystemExit("a DXBC record was replaced by DXIL (or back): the runtime cannot mix them in one PSO")
        src, alt, pre = (a, b, "on_") if installed == "orig" else (b, a, "off_")
        out.append(("%s%s.%s" % (pre, key_of(src), "dxil" if has_dxil(alt) else "dxbc"), alt))
    return out


def sha(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()


def build(a):
    idrk = _idrk()
    if len(a.orig) != len(a.repl):
        raise SystemExit("--orig and --repl must come in pairs")
    files = []
    for o, r in zip(a.orig, a.repl):
        inst = a.installed
        if inst == "auto":
            gf = a.game_file or os.path.join(a.game_dir, "shader_dx12", os.path.basename(o))
            h = sha(gf)
            inst = "orig" if h == sha(o) else "repl" if h == sha(r) else None
            if not inst:
                raise SystemExit("%s is neither %s nor %s (game updated? other variant installed?)" % (gf, o, r))
            print("  %s installed: %s" % (os.path.basename(gf), "STOCK" if inst == "orig" else "REPLACEMENT"))
        po, pr = idrk.unpack(open(o, "rb").read()), idrk.unpack(open(r, "rb").read())
        fs = pair_files(po, pr, inst)
        if not fs:
            raise SystemExit("%s and %s hold identical shaders" % (o, r))
        files += fs
    d = os.path.join(a.out, a.name.lower())
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)
    for n, b in files:
        open(os.path.join(d, n), "wb").write(b)
        print("  %s\\%s (%d bytes)" % (a.name.lower(), n, len(b)))
    print("hot set '%s': %d file(s) in %s -> LiveCtl 'Hot.%s = 1' = replacement active" % (a.name.lower(), len(files), d, a.name.lower()))


def fake_container(tag: int, dxil=True, zero_digest=False) -> bytes:
    body = (b"DXIL" if dxil else b"SHEX") + struct.pack("<I", 8) + struct.pack("<II", 0x50060, tag)
    size = 32 + 4 + len(body)
    dig = bytes(16) if zero_digest else hashlib.md5(bytes([tag]) + body).digest()
    return b"DXBC" + dig + struct.pack("<III", 1, size, 1) + struct.pack("<I", 36) + body


def selftest():
    a1, a2, b1, z1 = fake_container(1), fake_container(2), fake_container(3), fake_container(4, zero_digest=True)
    b2, z2 = fake_container(5), fake_container(6, zero_digest=True)
    po = b"_S2G7000" + bytes(40) + a1 + bytes(8) + a2 + b1 + z1
    pr = b"_S2G7000" + bytes(40) + a1 + bytes(8) + a2 + b2 + z2
    assert [c for _, c in containers(po)] == [a1, a2, b1, z1]
    on = pair_files(po, pr, "orig")
    assert [n for n, _ in on] == ["on_%s.dxil" % b1[4:20].hex(), "on_%s.dxil" % key_of(z1)], on
    assert on[0][1] == b2 and on[1][1] == z2 and key_of(z1).startswith("f") and len(key_of(z1)) == 17
    off = pair_files(po, pr, "repl")
    assert [n for n, _ in off] == ["off_%s.dxil" % b2[4:20].hex(), "off_%s.dxil" % key_of(z2)] and off[0][1] == b1
    try:
        pair_files(po, pr + fake_container(7), "orig")
        raise AssertionError("count mismatch not refused")
    except SystemExit:
        pass
    print("hotset selftest OK")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build")
    b.add_argument("--name", required=True)
    b.add_argument("--orig", action="append", required=True, help="stock .file (repeatable, paired with --repl)")
    b.add_argument("--repl", action="append", required=True, help="replacement .file")
    b.add_argument("--installed", choices=["auto", "orig", "repl"], default="auto")
    b.add_argument("--game-dir", default=GAME)
    b.add_argument("--game-file", default="")
    b.add_argument("--out", default=os.path.join(GAME, "Nioh3EngineFix_hot"))
    sub.add_parser("selftest")
    a = ap.parse_args()
    if a.cmd == "selftest":
        selftest()
    else:
        build(a)


if __name__ == "__main__":
    main()
