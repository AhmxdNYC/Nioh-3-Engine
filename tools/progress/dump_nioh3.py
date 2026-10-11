"""dump_nioh3.py - save the running game's Nioh3.exe image as a memory-layout PE for static analysis (Windows, stdlib).

usage: py -I tools\\progress\\dump_nioh3.py [OUTPUT]       default output: data\\local\\Nioh3.dump.exe (gitignored)

Start the game normally (Steam), wait for the title screen, then run this. Disable mods first (no ASI loaders or
patchers), so the code you dump is the game's own. It only READS the game's memory (OpenProcess with
PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, then ReadProcessMemory); it never writes to the process, injects
or suspends anything.

Why a dump instead of Nioh3.exe from disk: the file on disk is wrapped by Steam's DRM stub, which decrypts the code only
in memory. Once the game runs, the module image in memory is the plain program.

What the file looks like (the same layout our scripts were built with):
  * byte i of the file = byte at <module base> + i in the game process, for i in 0 .. SizeOfImage-1, read page by page
    (pages that cannot be read stay zero). So FILE OFFSET == RVA everywhere.
  * the PE header is patched so other tools agree: ImageBase := the runtime module base, and for every section
    PointerToRawData := VirtualAddress and SizeOfRawData := VirtualSize (a PE loader/parser then maps file offset X of a
    section to RVA X, which is already true of the bytes).
  * .data holds live values from the moment of the dump, and absolute pointers hold real addresses of YOUR process
    (scripts subtract the ImageBase from the header, so this works whatever address Windows loaded the game at).
The file is not runnable and contains the game's code: keep it to yourself, never commit or share it.
Runtime: a few seconds (about 87 MB). Needs no administrator rights when the game runs as the same user.
"""
import ctypes, os, struct, sys
from ctypes import wintypes as W

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402

EXE = 'nioh3.exe'
TH32CS_SNAPPROCESS, TH32CS_SNAPMODULE, TH32CS_SNAPMODULE32 = 0x2, 0x8, 0x10
PROCESS_VM_READ, PROCESS_QUERY_LIMITED_INFORMATION = 0x10, 0x1000
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
PAGE = 0x1000


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [('dwSize', W.DWORD), ('cntUsage', W.DWORD), ('th32ProcessID', W.DWORD),
                ('th32DefaultHeapID', ctypes.c_size_t), ('th32ModuleID', W.DWORD), ('cntThreads', W.DWORD),
                ('th32ParentProcessID', W.DWORD), ('pcPriClassBase', ctypes.c_long), ('dwFlags', W.DWORD),
                ('szExeFile', ctypes.c_wchar * 260)]


class MODULEENTRY32W(ctypes.Structure):
    _fields_ = [('dwSize', W.DWORD), ('th32ModuleID', W.DWORD), ('th32ProcessID', W.DWORD), ('GlblcntUsage', W.DWORD),
                ('ProccntUsage', W.DWORD), ('modBaseAddr', ctypes.c_void_p), ('modBaseSize', W.DWORD),
                ('hModule', W.HMODULE), ('szModule', ctypes.c_wchar * 256), ('szExePath', ctypes.c_wchar * 260)]


k32 = ctypes.WinDLL('kernel32', use_last_error=True)
k32.CreateToolhelp32Snapshot.restype = W.HANDLE
k32.CreateToolhelp32Snapshot.argtypes = (W.DWORD, W.DWORD)
k32.Process32FirstW.argtypes = k32.Process32NextW.argtypes = (W.HANDLE, ctypes.POINTER(PROCESSENTRY32W))
k32.Module32FirstW.argtypes = k32.Module32NextW.argtypes = (W.HANDLE, ctypes.POINTER(MODULEENTRY32W))
k32.OpenProcess.restype = W.HANDLE
k32.OpenProcess.argtypes = (W.DWORD, W.BOOL, W.DWORD)
k32.ReadProcessMemory.argtypes = (W.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t))
k32.CloseHandle.argtypes = (W.HANDLE,)


def find_pid():
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snap == INVALID_HANDLE_VALUE:
        raise OSError(ctypes.get_last_error(), 'CreateToolhelp32Snapshot failed')
    try:
        e = PROCESSENTRY32W(); e.dwSize = ctypes.sizeof(e)
        ok = k32.Process32FirstW(snap, ctypes.byref(e))
        while ok:
            if e.szExeFile.lower() == EXE:
                return e.th32ProcessID
            ok = k32.Process32NextW(snap, ctypes.byref(e))
    finally:
        k32.CloseHandle(snap)
    return None


def find_module(pid):
    """-> (base, size) of Nioh3.exe in that process"""
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    if snap == INVALID_HANDLE_VALUE:
        raise OSError(ctypes.get_last_error(), 'module snapshot failed (is the game still starting?)')
    try:
        m = MODULEENTRY32W(); m.dwSize = ctypes.sizeof(m)
        ok = k32.Module32FirstW(snap, ctypes.byref(m))
        while ok:
            if m.szModule.lower() == EXE:
                return m.modBaseAddr, m.modBaseSize
            ok = k32.Module32NextW(snap, ctypes.byref(m))
    finally:
        k32.CloseHandle(snap)
    return None


def rewrite_headers(img, base):
    """in place: ImageBase := base; every section PointerToRawData := VirtualAddress, SizeOfRawData := VirtualSize
    (a PE parser then maps file offset X to RVA X, which the page-by-page copy already made true) -> [(name, va, vsize)]"""
    pe = struct.unpack_from('<I', img, 0x3c)[0]
    if img[pe:pe + 4] != b'PE\0\0':
        raise ValueError('no PE signature')
    nsec = struct.unpack_from('<H', img, pe + 6)[0]
    opt_size = struct.unpack_from('<H', img, pe + 20)[0]
    opt = pe + 24
    struct.pack_into('<Q', img, opt + 24, base)
    out = []
    for i in range(nsec):
        sh = opt + opt_size + 40 * i
        vsize, va = struct.unpack_from('<II', img, sh + 8)
        struct.pack_into('<II', img, sh + 16, vsize, va)        # SizeOfRawData, PointerToRawData
        out.append((bytes(img[sh:sh + 8]).rstrip(b'\0').decode('latin-1'), va, vsize))
    return out


def main():
    out =os.path.abspath(sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith('--') else
                          os.path.join(C.LOCAL, 'Nioh3.dump.exe'))
    pid = find_pid()
    if pid is None:
        sys.exit('Nioh3.exe is not running. Start the game, wait for the title screen, then run this again.')
    mod = find_module(pid)
    if mod is None:
        sys.exit('Nioh3.exe module not found in process %d' % pid)
    base, size = mod
    h = k32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, False, pid)
    if not h:
        sys.exit('OpenProcess failed (error %d)' % ctypes.get_last_error())
    img = bytearray(size)
    buf = ctypes.create_string_buffer(PAGE)
    got = ctypes.c_size_t()
    bad = 0
    try:
        for off in range(0, size, PAGE):
            n = min(PAGE, size - off)
            if k32.ReadProcessMemory(h, ctypes.c_void_p(base + off), buf, n, ctypes.byref(got)) and got.value == n:
                img[off:off + n] = buf.raw[:n]
            else:
                bad += 1       # unreadable page: stays zero
    finally:
        k32.CloseHandle(h)
    print('pid %d  base 0x%X  SizeOfImage 0x%X  unreadable pages: %d' % (pid, base, size, bad))

    try:
        secs = rewrite_headers(img, base)
    except (ValueError, struct.error):
        sys.exit('no PE header at the module base; nothing written')
    for name, va, vsize in secs:
        print('  %-8s va=%08X vsize=%08X' % (name, va, vsize))
    text = next(((va, vs) for n, va, vs in secs if n == '.text'), None)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'wb') as f:
        f.write(img)
    print('wrote %s (%d MB)' % (out, len(img) >> 20))

    b = bytes(img)
    if text:
        t = b[text[0]:text[0] + text[1]]
        if t.count(0xCC) < 0.01 * len(t):   # decrypted MSVC code is ~2 % int3 padding; encrypted bytes ~0.4 %
            print('WARNING: .text looks encrypted (the DRM stub had not finished). Dump again from the title screen.')
    try:
        m = C.meta()
        if C.pe_identity(b) != (m.get('dump_timestamp'), m.get('dump_size_of_image'), m.get('dump_pdata_sha256')):
            print('WARNING: this is not the game build the published data/ tables were made from (see '
                  'data\\progress_meta.tsv); the charts will not match.')
        else:
            print('game build matches data\\progress_meta.tsv')
    except OSError:
        pass


if __name__ == '__main__':
    main()
