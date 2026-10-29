"""Read-only v2.02 title/save-worker gate; no writes, hooks or process suspension."""
import ctypes
from ctypes import wintypes as w
import struct
import time
from after_exit import Game, no_other_game

TOP_BYTES = bytes.fromhex('488b15e941680433c04885d2741c4c8b42084c39027413498b50f84885d2740a488b01483942080f94c0c3')


def sample(rd, base):
    """Return identity only for a fully idle title; caller samples twice for torn reads."""
    def u(p, fmt='Q'): return struct.unpack('<'+fmt, rd(p, struct.calcsize(fmt)))[0]
    m = u(base+0x4709B50)
    if not m: raise ValueError('Missing mode manager')
    begin, end = u(m), u(m+8)
    if not begin or not 0 < end-begin <= 512 or (end-begin) % 8:
        raise ValueError('Invalid mode stack')
    top = u(end-8)
    if not top or u(top+8) != base+0x44C91D0 or u(top+0x10, 'I') != 3:
        raise ValueError('Not stable main-menu state 3')
    if u(m+0x18) != u(m+0x20): raise ValueError('Mode queue busy')
    fields = tuple(u(top+x) for x in (0x30, 0x38, 0x40, 0x58))
    if fields == (top+0x30,)*3+(0,): raise ValueError('Press-any screen')
    gm, messages, manager = (u(base+x) for x in (0x4759910, 0x45BBF80, 0x45C8480))
    if not gm or not messages or not manager: raise ValueError('Missing game managers')
    worker = u(manager)
    completion = u(manager+0x10, 'I')
    if u(gm+0x58A, 'B') or u(messages+0xC0): raise ValueError('Loading or system dialog')
    if not worker or completion not in (1, 2, 3, 4, 5) or u(worker+0x38, 'B') or u(base+0x45C6EC0):
        raise ValueError('Save request or backend busy/unknown')
    return m, begin, end, top, fields, gm, messages, manager, worker, completion


class Gate:
    def __init__(self, pid, created):
        self.pid = pid
        self.game = Game(pid, created)
        self.handle = None
        try:
            k = self.game.k
            self.handle = k.OpenProcess(0x1010, False, pid)
            if not self.handle: raise OSError('Cannot open read-only game memory')
            k.ReadProcessMemory.argtypes = [w.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
            k.ReadProcessMemory.restype = w.BOOL
            ps = ctypes.WinDLL('psapi', use_last_error=True)
            ps.EnumProcessModulesEx.argtypes = [w.HANDLE, ctypes.POINTER(w.HMODULE), w.DWORD, ctypes.POINTER(w.DWORD), w.DWORD]
            ps.EnumProcessModulesEx.restype = w.BOOL
            mods = (w.HMODULE*1024)(); needed = w.DWORD()
            if not ps.EnumProcessModulesEx(self.handle, mods, ctypes.sizeof(mods), ctypes.byref(needed), 3) or not needed.value:
                raise OSError('Cannot identify game image base')
            self.base = mods[0]
            if self.read(self.base+0x85960, len(TOP_BYTES)) != TOP_BYTES:
                raise ValueError('Unsupported executable: top-mode signature mismatch')
            self.identity = None
        except Exception:
            self.close(); raise

    def read(self, address, length):
        buf = ctypes.create_string_buffer(length); n = ctypes.c_size_t()
        if not self.game.k.ReadProcessMemory(self.handle, address, buf, length, ctypes.byref(n)) or n.value != length:
            raise OSError('Game memory unreadable')
        return buf.raw

    def check(self):
        if self.game.exited() is not None or not no_other_game(self.pid):
            raise ValueError('Origin exited or another game exists')
        a = sample(self.read, self.base); b = sample(self.read, self.base)
        if a != b: raise ValueError('Title/save state changed during sample')
        if self.identity is not None and a != self.identity: raise ValueError('Title/save identity changed')
        return a

    def settle(self):
        start = time.monotonic(); identity = self.check()
        while time.monotonic()-start < 1:
            time.sleep(.1)
            if self.check() != identity: raise ValueError('Title changed during 1-second admission')
        self.identity = identity

    def close(self):
        if self.handle: self.game.k.CloseHandle(self.handle); self.handle = None
        self.game.close()
