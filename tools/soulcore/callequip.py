# pc/2 #129137: call the game's own equip-soul-core fn (Nioh3.exe+2183744: rcx=base+0x47514E0, dl=slot, r8=item rec inv+0x10+idx*0xF0) on a remote thread.
import ctypes as C, ctypes.wintypes as W, struct, sys
src = open("coremem.py").read().split("if __name__")[0]; g = {}; exec(src, g)
k, h, B, inv, cnt, u32 = g["k"], g["h"], g["B"], g["inv"], g["cnt"], g["u32"]
slot, idx = int(sys.argv[1]), int(sys.argv[2]); assert 0 <= slot < 10 and 0 <= idx < cnt
assert idx in [i for i, _ in g["state"]()[1]], "idx is not a soul core"
ctx, F, item = B + 0x47514E0, B + 0x2183744, inv + 0x10 + idx * 0xF0
assert g["rd"](F, 5) == bytes.fromhex("48895c2408"), "fn prologue mismatch"
sc = b"\x48\x83\xEC\x28" + b"\x48\xB9" + struct.pack("<Q", ctx) + b"\x31\xD2\xB2" + bytes([slot]) + b"\x49\xB8" + struct.pack("<Q", item) + b"\x48\xB8" + struct.pack("<Q", F) + b"\xFF\xD0\x48\x83\xC4\x28\xC3"
hp = k.OpenProcess(0x1F0FFF, False, g["P"])
k.VirtualAllocEx.restype = C.c_void_p; k.VirtualAllocEx.argtypes = [W.HANDLE, C.c_void_p, C.c_size_t, W.DWORD, W.DWORD]
k.CreateRemoteThread.restype = W.HANDLE; k.CreateRemoteThread.argtypes = [W.HANDLE, C.c_void_p, C.c_size_t, C.c_void_p, C.c_void_p, W.DWORD, C.c_void_p]
k.WriteProcessMemory.argtypes = [W.HANDLE, C.c_void_p, C.c_char_p, C.c_size_t, C.c_void_p]
m = k.VirtualAllocEx(hp, None, 0x1000, 0x3000, 0x40); r = C.c_size_t()
assert m and k.WriteProcessMemory(hp, m, sc, len(sc), C.byref(r))
c0 = u32(inv + 0x23B6F0); old = u32(inv + 0x23B6B8 + slot * 4)
t = k.CreateRemoteThread(hp, None, 0, m, None, 0, None); assert t
print("wait", k.WaitForSingleObject(W.HANDLE(t), 5000))
print("slot %d: %d -> %d, counter %X -> %X" % (slot, old, u32(inv + 0x23B6B8 + slot * 4), c0, u32(inv + 0x23B6F0)))
