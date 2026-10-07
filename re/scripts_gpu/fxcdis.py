r"""fxcdis.py <file.dxbc> [...] -- disassemble SM4/5 (FXC) DXBC via the system D3DCompiler_47.dll
(D3DDisassemble), writes <file>.asm next to it and prints the declarations/header part."""
import ctypes, sys
d3dc = ctypes.WinDLL("D3DCompiler_47.dll")
class Blob(ctypes.Structure):
    pass
for fn in sys.argv[1:]:
    data = open(fn, "rb").read()
    out = ctypes.c_void_p()
    hr = d3dc.D3DDisassemble(data, len(data), 0, None, ctypes.byref(out))
    if hr != 0:
        print(fn, "hr=%08x" % (hr & 0xffffffff)); continue
    vt = ctypes.cast(ctypes.cast(out, ctypes.POINTER(ctypes.c_void_p))[0], ctypes.POINTER(ctypes.c_void_p))
    GetPtr = ctypes.WINFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(vt[3])
    GetSize = ctypes.WINFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p)(vt[4])
    txt = ctypes.string_at(GetPtr(out), GetSize(out)).decode("latin1")
    open(fn.rsplit(".", 1)[0] + ".asm", "w").write(txt)
    print("=====", fn)
    for l in txt.splitlines():
        if l.startswith("//") and l.strip() != "//" or l.startswith("dcl_") or l.startswith("ps_") or l.startswith("cs_") or l.startswith("vs_"):
            print(l)
    print("instructions:", sum(1 for l in txt.splitlines() if l and not l.startswith("//") and not l.startswith("dcl_")))
