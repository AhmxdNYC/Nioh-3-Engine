"""run_fnprobe_harness.py - FnProbe offline tests (src/fnprobe_core.h, src/fnprobe.cpp). Not a game test.

    py -I tests/run_fnprobe_harness.py [path/to/Nioh3.dump.exe]

1. Builds tests/fnprobe_harness.cpp + .asm (MSVC: cl on PATH or ..\\..\\tools\\msvc\\setup_x64.bat) into build/fptest and runs the
   thunk tests (register-exact incl. xmm0-5 and flags, exact counts from 4 threads, sampling, args, entry rsp).
2. Decoder vs capstone on the dump: PrologueLen for every function entry in re/symbols/symbols.tsv; every accepted length must be a
   whole-instruction boundary with no rip-relative operand and no branch/call (capstone), and the 14 default entries must decode to
   the lengths in fnprobe.cpp kDefault (= patches.json group FP).
3. No branch in the image lands inside a default entry's relocated bytes (rel8 within +-128 B, rel32 E8/E9/0F 8x over all code).
Must end ALL PASSED.
"""
import json
import pathlib
import re
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
DUMP = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(r"re\Nioh3.dump.exe")
OUT = ROOT / "build" / "fptest"
OUT.mkdir(parents=True, exist_ok=True)
fails = []


def check(c, msg):
    if not c:
        fails.append(msg)
    return c


setup = REPO / "tools" / "msvc" / "setup_x64.bat"
if not setup.exists():  # git worktree: tools\ is not tracked, use the main checkout's portable MSVC
    setup = pathlib.Path(r"tools\msvc\setup_x64.bat")
pre = ('call "%s" >nul && ' % setup) if setup.exists() else ""
cmd = pre + (
    'ml64 /nologo /c /W3 /Fo"{o}\\fnprobe_harness_asm.obj" "{t}\\fnprobe_harness.asm" >nul && '
    'cl /nologo /O2 /MT /EHsc /std:c++17 /W4 "{t}\\fnprobe_harness.cpp" "{o}\\fnprobe_harness_asm.obj" '
    '/Fo"{o}\\\\" /Fe"{o}\\fnprobe_harness.exe" /link kernel32.lib'
).format(o=OUT, t=ROOT / "tests")
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
if r.returncode:
    print(r.stdout[-4000:], r.stderr[-2000:])
    sys.exit("compile failed")
exe = OUT / "fnprobe_harness.exe"
r = subprocess.run([str(exe)], capture_output=True, text=True)
print(r.stdout.strip())
check(r.returncode == 0 and "ALL PASSED" in r.stdout, "thunk tests failed")

# ---- 2. decoder vs capstone ----
import capstone  # noqa: E402
from capstone import x86  # noqa: E402

img = DUMP.read_bytes()
pe = struct.unpack_from("<I", img, 0x3C)[0]
nsec = struct.unpack_from("<H", img, pe + 6)[0]
opt = struct.unpack_from("<H", img, pe + 20)[0]
code = []
for i in range(nsec):
    o = pe + 24 + opt + 40 * i
    vs, va = struct.unpack_from("<II", img, o + 8)
    if struct.unpack_from("<I", img, o + 36)[0] & 0x20000000:
        code.append((va, va + vs))

src = (ROOT / "src" / "fnprobe.cpp").read_text(encoding="utf-8")
kdef = [(int(a, 16), int(n)) for a, n in re.findall(r"\{0x([0-9A-F]+), (\d+), \{0x", src)]
check(len(kdef) == 14, "kDefault has %d entries" % len(kdef))
doc = json.loads((ROOT / "patches.json").read_text(encoding="utf-8"))
fp = {int(s["rva"], 16): (s["len"], bytes.fromhex(s["orig"])) for s in doc["sites"] if s.get("group") == "FP"}
check(set(fp) == {a for a, _ in kdef}, "patches.json FP sites != fnprobe.cpp kDefault")
for a, n in kdef:
    check(fp.get(a, (0,))[0] == n and img[a:a + n] == fp[a][1], "FP %X: len/bytes differ between fnprobe.cpp, patches.json and the dump" % a)
    m = re.search(r"\{0x%X, %d, \{([^}]*)\}" % (a, n), src)
    if check(m is not None, "kDefault %X not parsed" % a):
        b = bytes(int(x, 16) for x in m.group(1).replace(" ", "").split(","))
        check(b == img[a:a + n], "kDefault %X bytes differ from the dump" % a)

rvas = []
for line in (REPO / "re" / "symbols" / "symbols.tsv").read_text(encoding="utf-8").splitlines():
    p = line.split("\t")
    if len(p) > 1 and p[0].startswith("0x") and p[1] == "function":
        a = int(p[0], 16)
        if any(lo <= a < hi for lo, hi in code):
            rvas.append(a)
lst = OUT / "rvas.txt"
lst.write_text("\n".join("%X" % a for a in rvas))
r = subprocess.run([str(exe), "decode", str(DUMP), str(lst)], capture_output=True, text=True)
got = {int(a, 16): int(n) for a, n in (l.split() for l in r.stdout.split("\n") if l.strip())}
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
md.detail = True
acc = bad = 0
BR = {x86.X86_GRP_JUMP, x86.X86_GRP_CALL, x86.X86_GRP_RET, x86.X86_GRP_INT, x86.X86_GRP_IRET}
for a, n in got.items():
    if n < 0:
        continue
    acc += 1
    pos, okk = 0, True
    for ins in md.disasm(img[a:a + 24], a):
        if pos >= n:
            break
        if any(g in BR for g in ins.groups) or "rip" in ins.op_str:
            okk = False
        pos += ins.size
    if pos != n or not okk:
        bad += 1
        if bad <= 10:
            fails.append("decoder: %X len %d is not a clean capstone boundary (%s)" % (a, n, img[a:a + n].hex()))
for a, n in kdef:
    check(got.get(a) == n, "decoder: default %X decodes to %s, kDefault says %d" % (a, got.get(a), n))
print("decoder: %d of %d symbol entries accepted, %d disagree with capstone" % (acc, len(got), bad))

# ---- 3. no branch into a default entry's relocated bytes ----
inside = {}
for a, n in kdef:
    for t in range(a + 1, a + n):
        inside[t] = a
hits = []
for a, n in kdef:  # short jumps can only come from +-130 B
    for s in range(a - 130, a + n + 130):
        b = img[s]
        if b == 0xEB or 0x70 <= b <= 0x7F:
            t = s + 2 + struct.unpack_from("<b", img, s + 1)[0]
            if t in inside and s not in range(a, a + n):
                # confirm it decodes as an instruction stream from a nearby entry is too costly; report raw
                hits.append("rel8 %X -> %X" % (s, t))
try:
    import numpy as np
    for lo, hi in code:
        seg = np.frombuffer(img, dtype=np.uint8, count=hi - lo + 8, offset=lo)
        # rel32 at every offset: build int32 from 4 shifted byte views
        n_ = hi - lo
        b0 = seg[1:n_ + 1].astype(np.int64)
        b1 = seg[2:n_ + 2].astype(np.int64)
        b2 = seg[3:n_ + 3].astype(np.int64)
        b3 = seg[4:n_ + 4].astype(np.int64)
        r32 = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)
        r32 = np.where(r32 >= 2 ** 31, r32 - 2 ** 32, r32)
        op = seg[:n_]
        idx = np.arange(n_, dtype=np.int64) + lo
        tgt = idx + 5 + r32
        cand = (op == 0xE8) | (op == 0xE9)
        for a, n in kdef:
            m = cand & (tgt > a) & (tgt < a + n)
            for s in idx[m][:5]:
                hits.append("rel32 %X -> %X" % (s, s + 5 + int(r32[s - lo])))
        # 0F 8x rel32 (opcode at s, s+1)
        op2 = seg[1:n_ + 1]
        b0 = seg[2:n_ + 2].astype(np.int64)
        b1 = seg[3:n_ + 3].astype(np.int64)
        b2 = seg[4:n_ + 4].astype(np.int64)
        b3 = seg[5:n_ + 5].astype(np.int64)
        r32b = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)
        r32b = np.where(r32b >= 2 ** 31, r32b - 2 ** 32, r32b)
        tgt2 = idx + 6 + r32b
        cand2 = (op == 0x0F) & (op2 >= 0x80) & (op2 <= 0x8F)
        for a, n in kdef:
            m = cand2 & (tgt2 > a) & (tgt2 < a + n)
            for s in idx[m][:5]:
                hits.append("jcc32 %X -> inside %X" % (s, a))
    print("branch scan: rel32/jcc32 over %d code bytes + rel8 near each entry: %d raw byte matches" % (sum(h - l for l, h in code), len(hits)))
except ImportError:
    print("branch scan: numpy missing, rel32 scan skipped (rel8 only): %d raw matches" % len(hits))
# raw byte matches are not necessarily instructions: confirm each by disassembling from the containing symbol entry
starts = sorted(rvas)
import bisect  # noqa: E402
real = []
for h in hits:
    s = int(h.split()[1], 16)
    i = bisect.bisect_right(starts, s) - 1
    if i < 0:
        continue
    f0 = starts[i]
    if s - f0 > 0x20000:
        continue
    for ins in md.disasm(img[f0:s + 8], f0):
        if ins.address == s:
            real.append(h + " (decodes from %X)" % f0)
            break
        if ins.address > s:
            break
check(not real, "branch into a relocated prologue: " + "; ".join(real[:5]))
print("branch scan: %d real branches into relocated bytes" % len(real))

if fails:
    print("\n".join("FAIL: " + f for f in fails))
    print("%d FAILED" % len(fails))
    sys.exit(1)
print("fnprobe: ALL PASSED")
