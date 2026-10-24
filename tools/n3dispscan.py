"""Find .text instructions that use a given memory displacement (read-only scan of re/Nioh3.dump.exe).

  py -I tools/n3dispscan.py 0x13c [0x13d ...] [--writes]   -> 'rva  insn' lines (dedup), --writes keeps only stores/RMW
"""
import re
import struct
import sys
import capstone

EXE = r'C:/Users/Ahmad Hamza/nioh3-engine/re/Nioh3.dump.exe'
TEXT = (0x1000, 0x1000 + 0x38e028b)
WR = ('mov', 'or', 'and', 'bts', 'btr', 'xor', 'add', 'sub', 'movzx', 'inc', 'dec', 'movd', 'movss', 'movups', 'movdqu')


def scan(disps, writes=False):
    d = open(EXE, 'rb').read()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    seen = {}
    for disp in disps:
        pat = struct.pack('<I', disp)
        rx = re.compile(r'\+ %s\]' % hex(disp))
        for m in re.finditer(re.escape(pat), d[TEXT[0]:TEXT[1]]):
            at = TEXT[0] + m.start()
            for back in range(2, 8):
                s = at - back
                ins = next(md.disasm(d[s:s + 16], s), None)
                if not ins or not (ins.address < at < ins.address + ins.size):
                    continue
                if not rx.search(ins.op_str):
                    continue
                if 0x40 <= d[s - 1] <= 0x4F:  # keep a REX prefix (qword / r8-r15 forms)
                    alt = next(md.disasm(d[s - 1:s + 15], s - 1), None)
                    if alt and alt.address + alt.size == ins.address + ins.size and rx.search(alt.op_str):
                        ins = alt
                if writes:
                    ops = ins.op_str.split(',')[0]
                    if not rx.search(ops) or ins.mnemonic in ('cmp', 'test', 'movzx', 'movsx'):
                        continue
                seen[ins.address] = '%s %s' % (ins.mnemonic, ins.op_str)
                break
    return seen


if __name__ == '__main__':
    disps = [int(a, 16) for a in sys.argv[1:] if not a.startswith('--')]
    for a, t in sorted(scan(disps, '--writes' in sys.argv).items()):
        print('%x  %s' % (a, t))
