"""test_padguard.py - PadListGuard (#79 / #55) sites against the decrypted v2.02 dump (needs capstone + numpy).

    py -I tests/test_padguard.py [path/to/Nioh3.dump.exe]

For every entry of src/padguard_cave.h kWalks:
  * the table == patches.json PGn (rva, len, orig) and rvas.json has the RVA (Grva on other builds)
  * the dump bytes at the site == orig; it decodes as `cmp node, head ; je exit` and the table shape agrees (Shaped())
  * the first instruction at the je target == exitBytes, and the site is a loop head (a back edge lands on it, or the
    node load right before it is one)
  * no branch anywhere in the host function (and its chained cold chunks) or any rel32 branch in the executable sections
    lands strictly inside the replaced bytes
  * the node register is dead at the exit: on every path from the exit it is written before it is read (calls preserve it;
    ret / tail jmp end the path), so ending the walk with node = 0 instead of node = head changes nothing after the loop
  * the host function contains no other `mov node,[node] ; cmp node, ...` walk that the table misses
"""
import json
import pathlib
import re
import struct
import sys

import capstone
from capstone import x86
import numpy as np

ROOT = pathlib.Path(__file__).resolve().parent.parent
DUMP = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe")
fails = []
# the per-frame pad manager walkers (PadMgrUpdate 43671C -> 4385xx): 42FAC8 mode 3 (jmp from 438752), 430B58 mode 1 (call 43873A),
# 438604 (call 4385F0), 4309F8 (call 4385F8)
# 433A4C: the pad state update (calls the 2nd pad map at() 431E6C), reached from PadMgrUpdate (GitHub #123 stack 433ADF <- 435546 <- 435473 <- 436797)
HOSTS = (0x42FAC8, 0x4309F8, 0x430B58, 0x438604, 0x433A4C)


def check(cond, msg):
    if not cond:
        fails.append(msg)
    return cond


def table():
    src = (ROOT / "src" / "padguard_cave.h").read_text(encoding="utf-8")
    rows = re.findall(r"\{0x([0-9A-F]+),\s*(\d+), (\d), (kRbx|kRdi), \{([^}]*)\},\s*(\d), \{([^}]*)\}\}", src)
    out = []
    for rva, ln, cl, node, orig, el, ex in rows:
        out.append(dict(rva=int(rva, 16), len=int(ln), cmp=int(cl), node="rbx" if node == "kRbx" else "rdi",
                        orig=bytes(int(x, 16) for x in re.findall(r"0x([0-9A-F]{2})", orig)),
                        exit=bytes(int(x, 16) for x in re.findall(r"0x([0-9A-F]{2})", ex))[:int(el)]))
    return out


def pdata(img):
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    rva, size = struct.unpack_from("<II", img, pe + 24 + 112 + 3 * 8)
    return [struct.unpack_from("<III", img, rva + 12 * i) for i in range(size // 12)]


def chain_parent(img, unwind):
    flags, ncodes = img[unwind] >> 3, img[unwind + 2]
    if not flags & 4:
        return None
    return struct.unpack_from("<III", img, unwind + 4 + ((ncodes + 1) & ~1) * 2)


def main():
    img = DUMP.read_bytes()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    ents = pdata(img)
    sites = table()
    check(len(sites) == 16, "padguard_cave.h: parsed %d sites, want 16" % len(sites))
    doc = json.loads((ROOT / "patches.json").read_text(encoding="utf-8"))
    pg = {int(s["rva"], 16): s for s in doc["sites"] if s["group"] == "PG"}
    check(sorted(pg) == sorted(s["rva"] for s in sites), "patches.json PG sites != padguard_cave.h kWalks")
    rv = {int(e["rva"], 16) for e in json.loads((ROOT / "rvas.json").read_text(encoding="utf-8"))["rvas"]}

    # executable sections for the global rel32 scan
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    nsec = struct.unpack_from("<H", img, pe + 6)[0]
    sh = pe + 24 + struct.unpack_from("<H", img, pe + 20)[0]
    a = np.frombuffer(img, dtype=np.uint8)
    hits_into = {}
    for i in range(nsec):
        vs, va = struct.unpack_from("<II", img, sh + 40 * i + 8)
        if not struct.unpack_from("<I", img, sh + 40 * i + 36)[0] & 0x20000000:
            continue
        end = min(va + vs, len(img) - 7)
        for lo in range(va, end, 1 << 22):  # 4 MB chunks keep the scan's memory small
            hi = min(lo + (1 << 22), end)
            n = hi - lo
            seg = a[lo:hi + 7]
            idx = np.arange(lo, hi, dtype=np.int64)

            def disp(o):  # signed rel32 starting o bytes after each position
                v = seg[o:o + n].astype(np.int64) | (seg[o + 1:o + 1 + n].astype(np.int64) << 8) | \
                    (seg[o + 2:o + 2 + n].astype(np.int64) << 16) | (seg[o + 3:o + 3 + n].astype(np.int64) << 24)
                return np.where(v >= 1 << 31, v - (1 << 32), v)
            op, op2 = seg[0:n], seg[1:1 + n]
            tgt5 = np.where(np.isin(op, (0xE8, 0xE9)), idx + 5 + disp(1), -1)
            tgt6 = np.where((op == 0x0F) & (op2 >= 0x80) & (op2 <= 0x8F), idx + 6 + disp(2), -1)
            for s in sites:
                r0, r1 = s["rva"] + 1, s["rva"] + s["len"]
                m = ((tgt5 >= r0) & (tgt5 < r1)) | ((tgt6 >= r0) & (tgt6 < r1))
                hits_into.setdefault(s["rva"], []).extend(int(x) for x in idx[m])

    for s in sites:
        rva, ln, tag = s["rva"], s["len"], "PG %X" % s["rva"]
        ok = check(img[rva:rva + ln] == s["orig"], "%s: dump bytes %s != orig %s" % (tag, img[rva:rva + ln].hex(), s["orig"].hex()))
        p = pg.get(rva)
        ok &= check(p is not None and p["len"] == ln and bytes(int(x, 16) for x in p["orig"].split()) == s["orig"],
                    "%s: patches.json entry differs" % tag)
        ok &= check(rva in rv, "%s: not in rvas.json (run tools/gen_rvas.py DUMP)" % tag)
        ins = list(md.disasm(img[rva:rva + 16], rva))[:2]
        cmp_, je = ins
        ok &= check(cmp_.mnemonic == "cmp" and cmp_.size == s["cmp"] and je.mnemonic == "je" and cmp_.size + je.size == ln,
                    "%s: not `cmp ; je` of the table lengths" % tag)
        ok &= check(cmp_.op_str.startswith(s["node"] + ","), "%s: node register is not %s (%s)" % (tag, s["node"], cmp_.op_str))
        ok &= check("rip" not in cmp_.op_str, "%s: rip-relative cmp cannot be copied" % tag)
        exit_ = int(je.op_str, 16)
        ok &= check(img[exit_:exit_ + len(s["exit"])] == s["exit"], "%s: exit %X bytes differ" % (tag, exit_))
        first = next(md.disasm(img[exit_:exit_ + 16], exit_))
        ok &= check(first.size == len(s["exit"]), "%s: exitBytes is not exactly the first instruction at %X" % (tag, exit_))
        # host function + chunks chained to it
        host = next(((b, e, u) for b, e, u in ents if b <= rva < e), None)
        ok &= check(host is not None and host[0] in HOSTS, "%s: host function %s is not a pad manager walker %s" % (tag, host and hex(host[0]), [hex(h) for h in HOSTS]))
        s["host"] = host[0] if host else 0
        chunks = [(b, e) for b, e, u in ents if (chain_parent(img, u) or (None,))[0] == s["host"]]
        ranges = [(host[0], host[1])] + chunks
        code = {}
        for b, e in ranges:
            for i in md.disasm(img[b:e], b):
                code[i.address] = i
        # branch targets inside the replaced bytes (local decode + global rel32 scan)
        bad = [hex(i.address) for i in code.values() if i.group(capstone.CS_GRP_JUMP) and i.operands and
               i.operands[0].type == x86.X86_OP_IMM and rva < i.operands[0].imm < rva + ln]
        bad += [hex(x) for x in hits_into.get(rva, [])]
        ok &= check(not bad, "%s: branches land inside the replaced bytes: %s" % (tag, bad))
        # loop head: a back edge lands on the cmp or on the node load right before it
        # (straight-line run of up to 4 instructions before the cmp that starts at the node load)
        heads, pc = {rva}, rva
        for _ in range(4):
            prev = [i for i in code.values() if i.address + i.size == pc]
            if not prev or prev[0].group(capstone.CS_GRP_JUMP):
                break
            pc = prev[0].address
            heads.add(pc)
            if prev[0].mnemonic == "mov" and prev[0].op_str.startswith(s["node"] + ", qword ptr ["):
                break
        back = [i for i in code.values() if i.group(capstone.CS_GRP_JUMP) and i.operands and
                i.operands[0].type == x86.X86_OP_IMM and i.operands[0].imm in heads and i.address > rva]
        ok &= check(bool(back), "%s: no back edge to the loop head" % tag)
        # node register dead at the exit (DFS over the host's code; calls preserve rbx/rdi; ret / jmp out end a path)
        reg = md.reg_name
        want = s["node"]

        def reads_writes(i):
            rd, wr = i.regs_access()
            rn = {reg(r) for r in rd}
            wn = {reg(r) for r in wr}
            fam = {"rbx": {"rbx", "ebx", "bx", "bl", "bh"}, "rdi": {"rdi", "edi", "di", "dil"}}[want]
            r = bool(rn & fam)
            w = bool(wn & fam) and not r
            # a 32-bit write zero-extends: a full write; 8/16-bit writes are partial (count as a read)
            if wn & fam and not (wn & {want, {"rbx": "ebx", "rdi": "edi"}[want]}):
                r = True
            return r, w

        seen, stack, dead_ok = set(), [exit_], True
        while stack and dead_ok:
            pc = stack.pop()
            while pc not in seen:
                seen.add(pc)
                i = code.get(pc)
                if i is None:
                    dead_ok = False  # left the host without a verdict
                    break
                if i.mnemonic == "call":
                    pc = i.address + i.size
                    continue
                r, w = reads_writes(i)
                if r:
                    dead_ok = False
                    break
                if w or i.mnemonic == "ret":
                    break
                if i.group(capstone.CS_GRP_JUMP):
                    t = i.operands[0].imm if i.operands and i.operands[0].type == x86.X86_OP_IMM else None
                    if i.mnemonic == "jmp":
                        if t is None or t not in code:
                            break  # tail jump out: the callee sees callee-saved rbx/rdi it must preserve
                        pc = t
                        continue
                    if t is not None:
                        stack.append(t)
                pc = i.address + i.size
        ok &= check(dead_ok, "%s: node register %s may be read after the exit %X before it is written" % (tag, want, exit_))
        print("%-10s host %X  %-34s exit %X  %s" % (tag, s["host"], cmp_.mnemonic + " " + cmp_.op_str + " / je", exit_, "OK" if ok else "FAIL"))

    # completeness: every `mov r,[r]` + `cmp r, ...` walk head in the four hosts is in the table
    for h in HOSTS:
        b, e, _ = next(x for x in ents if x[0] == h)
        ins = list(md.disasm(img[b:e], b))
        for k in range(len(ins) - 2):
            m = re.match(r"(r\w+), qword ptr \[(r\w+)\]$", ins[k].op_str)
            if ins[k].mnemonic != "mov" or not m or m.group(1) not in ("rbx", "rdi"):
                continue
            for j in (k + 1, k + 2):
                if ins[j].mnemonic == "cmp" and ins[j].op_str.startswith(m.group(1) + ", ") and ins[j + 1].mnemonic == "je":
                    check(any(s["rva"] == ins[j].address for s in sites), "host %X: walk head %X not in the table" % (h, ins[j].address))
    print()
    if fails:
        print("FAILED (%d):" % len(fails))
        for f in fails:
            print("  - " + f)
        return 1
    print("ALL %d PADGUARD SITES OK: bytes, shape, exit, loop head, no branch into the range, node dead at exit, table complete" % len(sites))
    return 0


if __name__ == "__main__":
    sys.exit(main())
