"""Tiny read-only disassembler for re/Nioh3.dump.exe (file offset == RVA in this dump).

  py -I tools/n3dis.py <rva hex> [max_insns=80] [--follow-ret]   -> prints insns until max or first ret (unless --follow-ret)
"""
import sys
import capstone

EXE = r'C:/Users/Ahmad Hamza/nioh3-engine/re/Nioh3.dump.exe'
BASE = 0x7ff74f410000


def dis(rva, n=80, stop_ret=True, data=None):
    d = data or open(EXE, 'rb').read()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    out = []
    for i in md.disasm(d[rva:rva + 16 * n], rva):
        out.append('%x  %s %s' % (i.address, i.mnemonic, i.op_str))
        if len(out) >= n or (stop_ret and i.mnemonic in ('ret', 'int3')):
            break
    return out


if __name__ == '__main__':
    rva = int(sys.argv[1], 16)
    n = int(sys.argv[2]) if len(sys.argv) > 2 and not sys.argv[2].startswith('--') else 80
    print('\n'.join(dis(rva, n, '--follow-ret' not in sys.argv)))
