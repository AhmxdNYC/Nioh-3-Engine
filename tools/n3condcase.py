"""Resolve which code a condition id reaches in the state-test switch at RVA 0x2E9948 (read-only, static).

The switch is a compare tree on r9d (cmp/sub + jg/je/jne/ja/jb/jl). This walks it for one id and prints the first
instructions of the selected case.
  py -I tools/n3condcase.py 0x75 [0x2e ...]
"""
import sys
import capstone

EXE = r'C:/Users/Ahmad Hamza/nioh3-engine/re/Nioh3.dump.exe'
START = 0x2E99D0  # after the prologue: eax=0x98; cmp r9d,eax ...


def walk(cid, d, md, show=14):
    pc, r9, eax, flags = START, cid, None, None
    r14 = 1
    for _ in range(400):
        ins = next(md.disasm(d[pc:pc + 16], pc))
        m, ops = ins.mnemonic, ins.op_str
        nxt = pc + ins.size
        if m == 'mov' and ops.startswith('eax, 0x'):
            eax = int(ops.split(', ')[1], 16)
        elif m == 'cmp' and ops.startswith('r9d, '):
            v = ops.split(', ')[1]
            v = eax if v == 'eax' else r14 if v == 'r14d' else (int(v, 16) if v.startswith('0x') else int(v))
            flags = (r9, v)
        elif m == 'sub' and ops.startswith('r9d, '):
            v = ops.split(', ')[1]
            v = r14 if v == 'r14d' else int(v, 16) if v.startswith('0x') else int(v)
            r9 -= v
            flags = (r9, 0)
        elif m == 'dec' and ops == 'r9d':
            r9 -= 1
            flags = (r9, 0)
        elif m == 'test' and ops == 'r9d, r9d':
            flags = (r9, 0)
        elif m in ('jg', 'je', 'jne', 'ja', 'jb', 'jl', 'jae', 'jbe', 'jle', 'jge', 'jmp'):
            a, b = flags if flags else (0, 0)
            ua, ub = a & 0xFFFFFFFF, b & 0xFFFFFFFF
            take = {'jg': a > b, 'je': a == b, 'jne': a != b, 'ja': ua > ub, 'jb': ua < ub, 'jl': a < b,
                    'jae': ua >= ub, 'jbe': ua <= ub, 'jle': a <= b, 'jge': a >= b, 'jmp': True}[m]
            if take:
                nxt = int(ops, 16)
        else:
            # first non-switch instruction: this is the case body
            out = []
            for i in md.disasm(d[pc:pc + 16 * show], pc):
                out.append('%x  %s %s' % (i.address, i.mnemonic, i.op_str))
                if len(out) >= show:
                    break
            return out
        pc = nxt
    return ['?']


if __name__ == '__main__':
    d = open(EXE, 'rb').read()
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    for a in sys.argv[1:]:
        print('== cond', a)
        print('\n'.join(walk(int(a, 16), d, md)))
