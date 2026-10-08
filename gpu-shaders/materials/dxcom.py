r"""dxcom.py - DXIL text (.ll as printed by dxc -dumpbin) -> signed DXIL container, through dxcompiler.dll COM (ctypes).

    py -I dxcom.py in.ll out.dxbc            # assemble + validate/sign
    py -I dxcom.py --roundtrip x.dxbc        # disassemble, reassemble, compare parts

IDxcOptimizer::RunOptimizer parses textual IR (llvm::parseIR) and returns bitcode; IDxcAssembler::AssembleToContainer
builds the container parts (DXIL, ISG1/OSG1/PSV0, ...) from the module metadata; IDxcValidator (dxil.dll) validates
and signs in place. No passes are run, so the code is exactly the text given.
"""
import ctypes as C, os, subprocess, struct, sys, hashlib
from ctypes import wintypes as W

HERE = os.path.dirname(os.path.abspath(__file__))
DXDIR = os.path.join(HERE, '..', '..', 'tools', 'rdts', 'RadeonDeveloperToolSuite-2026-05-28-1806', 'utils', 'dx12', 'dxc')
DXC = os.path.join(DXDIR, 'dxc.exe')


class GUID(C.Structure):
    _fields_ = [('a', C.c_uint32), ('b', C.c_uint16), ('c', C.c_uint16), ('d', C.c_ubyte * 8)]

    def __init__(self, s):
        h = s.replace('-', '')
        super().__init__(int(h[0:8], 16), int(h[8:12], 16), int(h[12:16], 16),
                         (C.c_ubyte * 8)(*bytes.fromhex(h[16:32])))


CLSID_Library = GUID('6245d6af-66e0-48fd-80b4-4d271796748c')
CLSID_Optimizer = GUID('ae2cd79f-cc22-453f-9b6b-b124e7a5204c')
CLSID_Assembler = GUID('d728db68-f903-4f80-94cd-dccf76ec7151')
CLSID_Validator = GUID('8ca3e215-f728-4cf3-8cdd-88af917587a1')
IID_Library = GUID('e5204dc7-d18c-4c3c-bdfb-851673980fe7')
IID_Optimizer = GUID('25740e2e-9cba-401b-9119-4fb42f39f270')
IID_Assembler = GUID('091f7a26-1c1f-4948-904b-e6e3a8a771d5')
IID_Validator = GUID('a6e82bd2-1fd7-4826-9811-2857e797f49a')

_os_cwd = os.getcwd()
os.add_dll_directory(os.path.abspath(DXDIR))
_dxc = C.WinDLL(os.path.join(os.path.abspath(DXDIR), 'dxcompiler.dll'))
_dxil = C.WinDLL(os.path.join(os.path.abspath(DXDIR), 'dxil.dll'))


def _create(dll, clsid, iid):
    p = C.c_void_p()
    hr = dll.DxcCreateInstance(C.byref(clsid), C.byref(iid), C.byref(p))
    if hr != 0:
        raise OSError('DxcCreateInstance failed 0x%08x' % (hr & 0xffffffff))
    return p


def _m(obj, idx, restype, *argtypes):
    vt = C.cast(C.cast(obj, C.POINTER(C.c_void_p))[0], C.POINTER(C.c_void_p))
    return C.WINFUNCTYPE(restype, C.c_void_p, *argtypes)(vt[idx])


def _blob_bytes(b):
    ptr = _m(b, 3, C.c_void_p)(b)
    n = _m(b, 4, C.c_size_t)(b)
    return C.string_at(ptr, n)


def _release(o):
    if o:
        _m(o, 2, C.c_ulong)(o)


_lib = _create(_dxc, CLSID_Library, IID_Library)


def _mkblob(data):
    out = C.c_void_p()
    buf = C.create_string_buffer(data, len(data))
    hr = _m(_lib, 7, C.c_long, C.c_void_p, C.c_uint32, C.c_uint32, C.POINTER(C.c_void_p))(
        _lib, C.cast(buf, C.c_void_p), len(data), 0, C.byref(out))   # CreateBlobWithEncodingOnHeapCopy
    if hr != 0:
        raise OSError('blob 0x%08x' % (hr & 0xffffffff))
    return out


def _opresult(res):
    st = C.c_long()
    _m(res, 3, C.c_long, C.POINTER(C.c_long))(res, C.byref(st))
    if st.value != 0:
        eb = C.c_void_p()
        _m(res, 5, C.c_long, C.POINTER(C.c_void_p))(res, C.byref(eb))
        msg = _blob_bytes(eb).decode('utf-8', 'replace') if eb else ''
        raise RuntimeError('status 0x%08x: %s' % (st.value & 0xffffffff, msg[:4000]))
    rb = C.c_void_p()
    _m(res, 4, C.c_long, C.POINTER(C.c_void_p))(res, C.byref(rb))
    return rb


def ll_to_bitcode(text):
    opt = _create(_dxc, CLSID_Optimizer, IID_Optimizer)
    src = _mkblob(text.encode('utf-8'))
    outm, outt = C.c_void_p(), C.c_void_p()
    hr = _m(opt, 5, C.c_long, C.c_void_p, C.c_void_p, C.c_uint32, C.POINTER(C.c_void_p), C.POINTER(C.c_void_p))(
        opt, src, None, 0, C.byref(outm), C.byref(outt))
    if hr != 0 or not outm:
        msg = _blob_bytes(outt).decode('utf-8', 'replace') if outt else ''
        raise RuntimeError('RunOptimizer 0x%08x %s' % (hr & 0xffffffff, msg[:3000]))
    return _blob_bytes(outm)


def assemble(bitcode):
    asm = _create(_dxc, CLSID_Assembler, IID_Assembler)
    res = C.c_void_p()
    hr = _m(asm, 3, C.c_long, C.c_void_p, C.POINTER(C.c_void_p))(asm, _mkblob(bitcode), C.byref(res))
    if hr != 0:
        raise RuntimeError('AssembleToContainer 0x%08x' % (hr & 0xffffffff))
    return _blob_bytes(_opresult(res))


def validate_sign(container):
    val = _create(_dxil, CLSID_Validator, IID_Validator)
    blob = _mkblob(container)
    res = C.c_void_p()
    hr = _m(val, 3, C.c_long, C.c_void_p, C.c_uint32, C.POINTER(C.c_void_p))(val, blob, 1, C.byref(res))  # InPlaceEdit
    if hr != 0:
        raise RuntimeError('Validate 0x%08x' % (hr & 0xffffffff))
    _opresult(res)
    return _blob_bytes(blob)


def parts(c):
    n = struct.unpack_from('<I', c, 28)[0]
    out = {}
    for i in range(n):
        o = struct.unpack_from('<I', c, 32 + 4 * i)[0]
        fcc, sz = struct.unpack_from('<4sI', c, o)
        out[fcc.decode('latin1')] = c[o + 8:o + 8 + sz]
    return out


def ll_to_container(text):
    return validate_sign(assemble(ll_to_bitcode(text)))


def dumpbin(path):
    return subprocess.run([DXC, '-dumpbin', path], capture_output=True, text=True, check=True).stdout


def body(ll):
    """the IR module text without comments / blank lines (what must match)"""
    out = []
    for l in ll.splitlines():
        if l.startswith(';'):
            continue
        i = l.find('  ;')
        l = (l[:i] if i >= 0 else l).rstrip()
        if l:
            out.append(l)
    return '\n'.join(out)


if __name__ == '__main__':
    a = sys.argv[1:]
    if a and a[0] == '--roundtrip':
        orig = open(a[1], 'rb').read()
        ll = dumpbin(a[1])
        new = ll_to_container(ll)
        po, pn = parts(orig), parts(new)
        for k in sorted(set(po) | set(pn)):
            s = 'same' if po.get(k) == pn.get(k) else ('MISSING-in-new' if k not in pn else 'missing-in-orig' if k not in po else 'DIFF %d/%d' % (len(po[k]), len(pn[k])))
            print('  %s %s' % (k, s))
        tmp = a[1] + '.rt.dxbc'
        open(tmp, 'wb').write(new)
        print('  dumpbin module text equal:', body(dumpbin(tmp)) == body(ll))
    else:
        open(a[1], 'wb').write(ll_to_container(open(a[0], encoding='utf-8').read()))
        print('wrote', a[1])
