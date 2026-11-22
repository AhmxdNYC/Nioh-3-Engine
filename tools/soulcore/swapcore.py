# pc/2 #129231: full shrine-equip order for a live soul core swap (pc/1 RE #129171, notes/soulcore-form-source-128972.md s5).
# Mirrors shrine handler Nioh3.exe+1DECB78 valid path: ok=2285C48(cl=s, rdx=rec); if !ok stop; sc=216B9D0(ctx, dl=s);
# 21607A4(ctx, rec); 2183744(ctx, dl=s, r8=rec); 215F85C(ctx); if sc!=-1 2182968(ctx, edx=sc, r8b=s); 34EDF8(0xF53E7B0C, 0);
# byte [B+0x45C6F2F]=1 (HUD shortcut refresh, as Item::SetShortCut). ctx = [B+0x4751530] read inside the thread.
# usage: py swapcore.py SLOT IDX [--dry]   (IDX = inventory index of the new core, from coremem.py)
import ctypes as C, ctypes.wintypes as W, struct, sys
src = open("coremem.py").read().split("if __name__")[0]; g = {}; exec(src, g)
k, B, inv, cnt, u32, rd = g["k"], g["B"], g["inv"], g["cnt"], g["u32"], g["rd"]
slot, idx = int(sys.argv[1]), int(sys.argv[2]); dry = "--dry" in sys.argv
assert 0 <= slot < 8 and 0 <= idx < cnt
slots, owned = g["state"]()
assert idx in [i for i, _ in owned], "idx is not a soul core"
assert slots[slot] != idx, "already in that slot"
rec = inv + 0x10 + idx * 0xF0
PRO = {0x2285C48: None, 0x216B9D0: None, 0x21607A4: None, 0x2183744: "48895c2408", 0x215F85C: None, 0x2182968: None, 0x34EDF8: None}
dump = open(__import__("os").path.join(__import__("os").path.dirname(__file__), "../../re/Nioh3.dump.exe"), "rb").read()
for f in PRO:  # live code must match the dump we disassembled
    assert rd(B + f, 8) == dump[f:f + 8], "prologue mismatch at +%X" % f
GLB, HUD = B + 0x4751530, B + 0x45C6F2F
def i64(op, v): return op + struct.pack("<Q", v)
def ctx(): return i64(b"\x48\xB8", GLB) + b"\x48\x8B\x08"                # mov rax,GLB; mov rcx,[rax]
def call(f): return i64(b"\x48\xB8", B + f) + b"\xFF\xD0"                  # mov rax,F; call rax
def d11(): return i64(b"\x49\xBB", 0)                                       # mov r11,DATA (patched)
def st(off, v): return d11() + b"\x41\xC7\x43" + bytes([off]) + struct.pack("<I", v)  # mov dword [r11+off],v
code = bytearray(b"\x48\x83\xEC\x38")                                       # sub rsp,0x38
code += st(0, 1)
code += b"\xB1" + bytes([slot]) + i64(b"\x48\xBA", rec) + call(0x2285C48)   # mov cl,s; mov rdx,rec
code += d11() + b"\x41\x89\x43\x04"                                         # [r11+4]=eax (ok)
code += b"\x84\xC0" + b"\x0F\x84" + b"\0\0\0\0"; jz_at = len(code)          # test al,al; jz end
code += st(0, 2) + ctx() + b"\xB2" + bytes([slot]) + call(0x216B9D0)        # sc
code += d11() + b"\x41\x89\x43\x08"                                         # [r11+8]=sc
code += st(0, 3) + ctx() + i64(b"\x48\xBA", rec) + call(0x21607A4)
code += st(0, 4) + ctx() + b"\xB2" + bytes([slot]) + i64(b"\x49\xB8", rec) + call(0x2183744)
code += st(0, 5) + ctx() + call(0x215F85C)
code += st(0, 6) + d11() + b"\x41\x8B\x53\x08" + b"\x83\xFA\xFF" + b"\x74\x00"; je_at = len(code)  # edx=sc; cmp edx,-1; je skip
code += ctx() + b"\x41\xB0" + bytes([slot]) + call(0x2182968)               # mov r8b,s
code[je_at - 1] = len(code) - je_at
code += st(0, 7) + b"\xB9\x0C\x7B\x3E\xF5" + b"\x31\xD2" + call(0x34EDF8)  # ecx=F53E7B0C, edx=0
code += i64(b"\x48\xB8", HUD) + b"\xC6\x00\x01"                             # byte [HUD]=1
code += st(0, 8)
struct.pack_into("<i", code, jz_at - 4, len(code) - jz_at)
code += b"\x48\x83\xC4\x38\xC3"
hp = k.OpenProcess(0x1F0FFF, False, g["P"])
k.VirtualAllocEx.restype = C.c_void_p; k.VirtualAllocEx.argtypes = [W.HANDLE, C.c_void_p, C.c_size_t, W.DWORD, W.DWORD]
k.CreateRemoteThread.restype = W.HANDLE; k.CreateRemoteThread.argtypes = [W.HANDLE, C.c_void_p, C.c_size_t, C.c_void_p, C.c_void_p, W.DWORD, C.c_void_p]
k.WriteProcessMemory.argtypes = [W.HANDLE, C.c_void_p, C.c_char_p, C.c_size_t, C.c_void_p]
k.ReadProcessMemory.argtypes = [W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t, C.c_void_p]
m = k.VirtualAllocEx(hp, None, 0x1000, 0x3000, 0x40); data = m + 0x800
code = bytes(code).replace(i64(b"\x49\xBB", 0), i64(b"\x49\xBB", data))
print("shellcode %d bytes at %X, slot %d old idx %d -> new idx %d (rec %X)" % (len(code), m, slot, slots[slot], idx, rec))
if dry:
    import capstone
    for x in capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64).disasm(code, m): print("  %X %s %s" % (x.address, x.mnemonic, x.op_str))
    sys.exit(0)
r = C.c_size_t()
assert k.WriteProcessMemory(hp, m, code, len(code), C.byref(r))
t = k.CreateRemoteThread(hp, None, 0, m, None, 0, None); assert t
print("wait", k.WaitForSingleObject(W.HANDLE(t), 5000))
b = C.create_string_buffer(12); k.ReadProcessMemory(hp, data, b, 12, C.byref(r))
stage, ok, sc = struct.unpack("<IIi", b.raw)
print("stage %d (8 = done) valid %d shortcut %d" % (stage, ok & 0xFF, sc))
g["state"]()
