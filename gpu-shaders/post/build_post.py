r"""build_post.py -- compile the runtime-post replacements (fxc / D3DCompile ps_5_0, same flags as the game: default
optimisation, no strictness), check that the interface equals the shipped shader (ISGN/OSGN/PCSG parts byte-identical,
same resource bindings and cbuffer layout), and write out\<container digest>.dxbc for psoswap.

Usage: py -I build_post.py [name ...]        (default: all in SHADERS)
Originals are read from the Ally dump (DUMP, outside the repo); outputs (out\*.dxbc) are gitignored.
"""
import ctypes, hashlib, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import dxbc_interp as DI  # noqa: E402

DUMP = os.environ.get("NIOH3_POST_DUMP", r"D:\nioh3-re\ally-dump\shaderdump_172\dump")
SRC = os.path.join(HERE, "src")
OUT = os.path.join(HERE, "out")

# name: (original container digest, hlsl file)
SHADERS = {
    "mbtile": ("f11afe97", "MB_TileMax_fast.hlsl"),
    "mbgather": ("4e13e236", "MB_Gather_fast.hlsl"),
    "dofgather": ("cf792d56", "DOF_Gather_fast.hlsl"),
    "compose_ca": ("04c1052d", "Compose_CA_fast.hlsl"),
    "compose_ca2": ("e6efc8ff", "Compose_CA2_fast.hlsl"),
}

_d3dc = ctypes.WinDLL("D3DCompiler_47.dll")


def _blob(p):
    vt = ctypes.cast(ctypes.cast(p, ctypes.POINTER(ctypes.c_void_p))[0], ctypes.POINTER(ctypes.c_void_p))
    gp = ctypes.WINFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(vt[3])
    gs = ctypes.WINFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p)(vt[4])
    b = ctypes.string_at(gp(p), gs(p))
    ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(vt[2])(p)
    return b


def compile_hlsl(path, entry="main", target="ps_5_0", flags=0):
    src = open(path, "rb").read()
    code, err = ctypes.c_void_p(), ctypes.c_void_p()
    hr = _d3dc.D3DCompile(src, len(src), path.encode(), None, ctypes.c_void_p(1), entry.encode(), target.encode(), flags, 0,
                          ctypes.byref(code), ctypes.byref(err))
    msg = _blob(err).decode("latin1") if err.value else ""
    if hr != 0:
        raise RuntimeError("compile %s failed:\n%s" % (path, msg))
    return _blob(code)


def find_original(prefix):
    for f in os.listdir(DUMP):
        if f.startswith(prefix):
            return os.path.join(DUMP, f)
    raise FileNotFoundError(prefix)


def parts(b):
    n = struct.unpack_from("<I", b, 28)[0]
    out = {}
    for i in range(n):
        off = struct.unpack_from("<I", b, 32 + 4 * i)[0]
        out[b[off:off + 4].decode("latin1")] = b[off + 8: off + 8 + struct.unpack_from("<I", b, off + 4)[0]]
    return out


def interface(text):
    """bindings + cbuffer layout + signatures from the disassembly header"""
    keep = []
    for l in text.splitlines():
        if re.match(r"^//\s+\w.*\b(sampler|texture|cbuffer|UAV)\b.*\b[stbu]\d+\s+\d+", l):
            keep.append(re.sub(r"\s+", " ", l))
        elif re.match(r"^//\s+(row_major\s+)?\w+\s+\w+(\[\d+\])?;\s*// Offset", l):
            # member name + offset + size (a float4x4 may be declared as float4[4]: same registers)
            m = re.match(r"^//\s+(?:row_major\s+)?\w+\s+(\w+)(?:\[\d+\])?;\s*// Offset:\s+(\d+) Size:\s+(\d+)", l)
            keep.append("cb member %s @%s size %s" % m.groups())
        elif re.match(r"^dcl_(input|output|constantbuffer|resource|sampler)", l):
            keep.append(re.sub(r"CB(\d+)\[\d+\]", r"CB\1", l))
    return keep


def check_interface(orig, new):
    po, pn = parts(orig), parts(new)
    probs = []
    for p in ("ISGN", "OSGN", "PCSG", "OSG5", "ISG1", "OSG1"):
        if po.get(p) != pn.get(p):
            probs.append("part %s differs" % p)
    io, inn = interface(DI.disassemble(orig)), interface(DI.disassemble(new))
    so, sn = set(io), set(inn)
    # every binding the original declares must be present identically; the new one may not add bindings
    for l in sorted(so - sn):
        if "[unused]" not in l:
            probs.append("missing: " + l)
    for l in sorted(sn - so):
        probs.append("extra: " + l)
    return probs


def build(name, src_override=None):
    dig, src = SHADERS[name]
    orig = open(find_original(dig), "rb").read()
    new = compile_hlsl(src_override or os.path.join(SRC, src))
    probs = check_interface(orig, new)
    os.makedirs(OUT, exist_ok=True)
    key = orig[4:20].hex()
    outp = os.path.join(OUT, key + (".dxbc" if not src_override else ".test.dxbc"))
    open(outp, "wb").write(new)
    return orig, new, outp, probs


def main():
    names = sys.argv[1:] or list(SHADERS)
    sums = []
    for n in names:
        try:
            orig, new, outp, probs = build(n)
        except FileNotFoundError as e:
            print("%-12s source missing: %s" % (n, e)); continue
        print("%-12s %s  %d B  sha256 %s  %s" % (n, os.path.basename(outp), len(new), hashlib.sha256(new).hexdigest(),
                                               "INTERFACE OK" if not probs else "INTERFACE MISMATCH"))
        for p in probs:
            print("    ", p)
        if not probs:
            sums.append("%s  %s  # %s" % (hashlib.sha256(new).hexdigest(), os.path.basename(outp), n))
    if sums and not sys.argv[1:]:
        with open(os.path.join(OUT, "SHA256SUMS"), "w", newline=chr(10)) as f:
            f.write("".join(l.split("  #")[0] + chr(10) for l in sums))
        print("wrote out/SHA256SUMS")


if __name__ == "__main__":
    main()
