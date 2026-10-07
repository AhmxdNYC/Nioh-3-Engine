r"""finddisp.py <disp_hex> [...] -- find x64 instructions that use [reg+disp32] with the given displacement in
re/Nioh3.dump.exe (heuristic decode of the opcode/ModRM bytes in front of the displacement). Prints RVA of the
instruction start guess, opcode bytes and containing function (re/out/functions.tsv)."""
import sys, struct, bisect, csv
EXE = r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe"
FT = r"C:\Users\Ahmad Hamza\nioh3-engine\re\out\functions.tsv"
b = open(EXE, "rb").read()
starts, ends = [], []
with open(FT, newline="") as f:
    rd = csv.reader(f, delimiter="\t"); next(rd)
    for r in rd:
        try:
            s = int(r[0], 16); n = int(r[1]) if r[1].isdigit() else int(r[1], 16)
        except Exception: continue
        starts.append(s); ends.append(s + n)
o = sorted(range(len(starts)), key=lambda i: starts[i]); starts = [starts[i] for i in o]; ends = [ends[i] for i in o]
def fn(a):
    k = bisect.bisect_right(starts, a) - 1
    return hex(starts[k]) if k >= 0 and a < ends[k] else "-"
OPS1 = {0x80: "grp1/imm8", 0x81: "grp1/imm32", 0x83: "grp1/imm8s", 0x38: "cmp r/m8,r8", 0x3a: "cmp r8,r/m8", 0x8a: "mov r8,m",
        0x88: "mov m,r8", 0xc6: "mov m8,imm", 0xf6: "test/grp3 m8", 0x84: "test m8,r8", 0x8b: "mov r,m", 0x89: "mov m,r",
        0xc7: "mov m32,imm", 0x39: "cmp m,r", 0x3b: "cmp r,m", 0x85: "test m,r", 0xf3: "movss?"}
for a in sys.argv[1:]:
    d = struct.pack("<I", int(a, 16)); i = b.find(d); n = 0
    print("== disp", a)
    while i >= 0:
        for modrm_at, has_sib in ((i - 1, False), (i - 2, True)):
            m = b[modrm_at]
            if m >> 6 != 2: continue
            if has_sib != ((m & 7) == 4): continue
            op = b[modrm_at - 1]; op2 = b[modrm_at - 2]
            name = None
            if op2 == 0x0f and op in (0xb6, 0xb7, 0xbe, 0x10, 0x11, 0x28, 0x2e, 0x2f): name = "0f %02x" % op
            elif op in OPS1: name = OPS1[op]
            if name:
                st = modrm_at - 1 - (1 if op2 == 0x0f else 0)
                if 0x40 <= b[st - 1] <= 0x4f: st -= 1
                print("  0x%x fn %-10s %-14s reg/op=%d  bytes %s" % (st, fn(st), name, (m >> 3) & 7, b[st:i + 5].hex()))
                n += 1
        i = b.find(d, i + 1)
    print("  hits:", n)
