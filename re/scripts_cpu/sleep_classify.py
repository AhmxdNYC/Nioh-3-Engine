r"""sleep_classify.py - classify every Sleep/SleepEx/SwitchToThread call site of re\out\spinscan.txt by idiom.

usage: py -I re\scripts_cpu\sleep_classify.py > re\out\sleepclass.txt

spin256      `test r8,r8 ; jne ; xor ecx,ecx ; call [Sleep]` right before the call: Sleep(0) every 256 failed tries
lockcx=1     a `lock cmpxchg` within -40h..+30h of the call (spinlock / try-lock / CAS loop)
pause=1      a PAUSE within the same window
Summary first, then one row per site with its primary function (chained .pdata folded). See notes/cpu-hotspots-ally.md 8.
"""
import os, re, struct, bisect, collections
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
B = open(os.path.join(ROOT, 're', 'Nioh3.dump.exe'), 'rb').read()
e = struct.unpack_from('<I', B, 0x3c)[0]
rva, sz = struct.unpack_from('<II', B, e + 24 + 112 + 3 * 8)
RF = sorted(struct.unpack_from('<III', B, rva + 12 * i) for i in range(sz // 12)); ST = [r[0] for r in RF]

def primary(x):
    i = bisect.bisect_right(ST, x) - 1
    if i < 0 or not RF[i][0] <= x < RF[i][1]:
        return x
    r = RF[i]
    for _ in range(10):
        ui = r[2] & ~1
        if not (B[ui] >> 3) & 4:
            break
        r = struct.unpack_from('<III', B, ui + 4 + ((B[ui + 2] + 1) & ~1) * 2)
    return r[0]

def lockcx(lo, hi):
    for o in range(lo, hi):
        if B[o] == 0xF0:
            j = o + 1 + (0x40 <= B[o + 1] <= 0x4F)
            if B[j] == 0x0F and B[j + 1] in (0xB0, 0xB1):
                return True
    return False

rows, summ = [], collections.Counter()
for l in open(os.path.join(ROOT, 're', 'out', 'spinscan.txt')):
    m = re.match(r'\s+((?:Sleep|SleepEx|SwitchToThread)\([^)]*\))\s+fn\s+([0-9A-F]+)\s+site\s+([0-9A-F]+)', l)
    if not m:
        continue
    imp, fn, s = m.group(1), int(m.group(2), 16), int(m.group(3), 16)
    pre = B[s - 16:s]
    mm = re.search(rb'\x84([\xc0-\xff])\x75.(?:\x33|\x31)\xc9$', pre)
    kind = 'spin256' if mm and ((mm.group(1)[0] >> 3) & 7) == (mm.group(1)[0] & 7) else 'other'
    lc, pz = lockcx(s - 0x40, s + 0x30), B.find(b'\xf3\x90', s - 0x40, s + 0x30) >= 0
    rows.append((imp, fn, primary(fn), s, kind, lc, pz))
    summ[(imp, kind, lc)] += 1
print('# count  import(arg)  kind  lock-cmpxchg-nearby')
for k, v in sorted(summ.items(), key=lambda x: -x[1]):
    print(f'{v:5}  {k[0]:14} {k[1]:8} lockcx={int(k[2])}')
print('\n# import(arg)     fragment   primary    site       kind     lockcx pause')
for r in rows:
    print(f'{r[0]:16} {r[1]:9X} {r[2]:9X} {r[3]:9X}  {r[4]:8} {int(r[5])}      {int(r[6])}')
