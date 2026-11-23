# ls_sites.py - find and verify the LoadStats (src\loadstats.cpp) hook sites in re\Nioh3.dump.exe (raw offset == RVA).
#   py -I re\scripts_load\ls_sites.py            prints every site with its bytes; last line "LOADSTATS SITES OK" or "... MISMATCH"
# Sites: E8 rel32 calls into the zlib chunk wrappers / PSO creators / dedicated upload alloc, and FF 15 calls through the
# Sleep / ReadFile IAT slots inside the two loader managers. loadstats.cpp re-checks the same bytes at run time.
import struct, sys
import os
from pathlib import Path
_dump = os.environ.get("NIOH3_DUMP") or str(Path(__file__).resolve().parents[1] / "Nioh3.dump.exe")
D = open(_dump, "rb").read()
pe = struct.unpack_from('<I', D, 0x3C)[0]; nsec = struct.unpack_from('<H', D, pe + 6)[0]; opt = struct.unpack_from('<H', D, pe + 20)[0]
text = None
for i in range(nsec):
    o = pe + 24 + opt + 40 * i
    name = D[o:o + 8].rstrip(b'\0'); va, vsz = struct.unpack_from('<II', D, o + 8)[1], struct.unpack_from('<I', D, o + 8)[0]
    if name == b'.text': text = (va, va + vsz)
lo, hi = text
def e8_callers(target):
    out = []; i = D.find(b'\xE8', lo)
    while 0 <= i < hi - 5:
        if i + 5 + struct.unpack_from('<i', D, i + 1)[0] == target: out.append(i)
        i = D.find(b'\xE8', i + 1)
    return out
def ff15_in(iat, a, b):
    return [i for i in range(a, b - 6) if D[i] == 0xFF and D[i + 1] == 0x15 and i + 6 + struct.unpack_from('<i', D, i + 2)[0] == iat]
TARGETS = {  # name: (target rva, expected caller count or None)
    'InflateChunkA 420654': 0x420654, 'InflateChunkB 324D064': 0x324D064, 'InflateChunkC A09A40': 0xA09A40,
    'CreateGfxPso 9290D8': 0x9290D8, 'CreateComputePso 58C2B0': 0x58C2B0, 'TextureStagingAllocDedicated 9C2EE0': 0x9C2EE0,
    'LoadIsLoadEnd 3B3424': 0x3B3424,
}
for n, t in TARGETS.items():
    cs = e8_callers(t)
    print(f'{n:40} {len(cs):3} call sites: ' + ' '.join(f'{c:X}' for c in cs[:16]) + (' ...' if len(cs) > 16 else ''))
SLEEP, READFILE = 0x38E2338, 0x38E23B8
for n, (a, b) in {'ResLoadMgrA 1FEE28': (0x1FEE28, 0x1FF1E4), 'ResLoadMgrB 1FF320': (0x1FF320, 0x1FFE08)}.items():
    print(f'{n:40} Sleep sites: ' + ' '.join(f'{c:X}' for c in ff15_in(SLEEP, a, b)))
print(f'{"ReadFile IAT sites (whole .text)":40} ' + ' '.join(f'{c:X}' for c in ff15_in(READFILE, lo, hi)))

# The exact table of src\loadstats.cpp kSites: (rva, kind, expect). Checks the bytes and that the bytes LoadStats rewrites
# (E8: the 4-byte rel32 at rva+1; FF 15: all 6 bytes) fit one aligned 8- or 16-byte window (else the site is skipped live).
SITES = [(0x1FEE68,'ff15',SLEEP),(0x1FF18C,'ff15',SLEEP),(0x1FF371,'ff15',SLEEP),
 (0x258A5A,'e8',0x420654),(0x4201F5,'e8',0x420654),(0x420605,'e8',0x420654),(0x324BD61,'e8',0x324D064),(0x324C409,'e8',0x324D064),
 (0xA099DD,'e8',0xA09A40),(0x324C372,'e8',0xA09A40),(0x324C4A1,'e8',0xA09A40),
 (0x11970A,'e8',0x9290D8),(0x2EFE47C,'e8',0x9290D8),(0x119AE1,'e8',0x58C2B0),(0x119D86,'e8',0x58C2B0),(0x58BF92,'e8',0x58C2B0),
 (0x58C07A,'e8',0x58C2B0),(0x2EFE1F2,'e8',0x58C2B0),
 (0x311D17,'ff15',READFILE),(0x312DD2,'ff15',READFILE),(0x31305A,'ff15',READFILE),(0x3130D6,'ff15',READFILE),
 (0x740392,'ff15',READFILE),(0x1FD9CA3,'ff15',READFILE),(0x32562BE,'ff15',READFILE)]
ok = True; fits = 0
for rva, k, exp in SITES:
    if k == 'e8': good = D[rva] == 0xE8 and rva + 5 + struct.unpack_from('<i', D, rva + 1)[0] == exp; n = 5
    else: good = D[rva] == 0xFF and D[rva+1] == 0x15 and rva + 6 + struct.unpack_from('<i', D, rva + 2)[0] == exp; n = 6
    w = lambda a, n: (a + n <= (a & ~7) + 8) or (a + n <= (a & ~15) + 16)  # RVA alignment == VA alignment (64 KiB base)
    fit = w(rva, n); spin = w(rva, 2)    # fallback: EB FE head (2 bytes atomic), tail, head
    fits += fit; ok &= good and (fit or spin)
    if not good or not fit: print(f'  site {rva:X} {k}: {"bytes OK" if good else "BYTES MISMATCH"}, {"atomic window OK" if fit else ("straddles 16 B -> EB FE fallback" if spin else "NOT PATCHABLE")}')
print(f'{len(SITES)} sites, {fits} in one atomic window, {len(SITES)-fits} via the EB FE fallback')
print('LOADSTATS SITES OK' if ok else 'LOADSTATS SITES MISMATCH')

# 1.7.23 RP read probe (src\readprobe.cpp): 13 ReadFile + ReadFileEx sites, the 3 event/wait sites, the game-cache E8.
RFEX, CREV, CLOSE, WFMO = 0x38E26C8, 0x38E25C8, 0x38E2390, 0x38E25D0
RP = [(r, 'ff15', READFILE) for r in (0x311D17, 0x312DD2, 0x31305A, 0x3130D6, 0x740392, 0x1FD9CA3, 0x32562BE,
                                      0xAF5656, 0xBDCC46, 0xBF9485, 0xBF9647, 0xBF98E5, 0xBF9FC7)]
RP += [(0x631FD5, 'ff15', RFEX), (0x311EE1, 'ff15', CREV), (0x311F89, 'ff15', CLOSE), (0x3127A1, 'ff15', WFMO), (0x198357C, 'e8', 0x2F77A6C)]
rf_all = set(ff15_in(READFILE, lo, hi)); rok = rf_all == {r for r, k, e in RP if e == READFILE} and ff15_in(RFEX, lo, hi) == [0x631FD5]
if not rok: print('  RP: ReadFile/ReadFileEx call sites in .text differ from the table:', sorted(hex(x) for x in rf_all))
for rva, k, exp in RP:
    if k == 'e8': good = D[rva] == 0xE8 and rva + 5 + struct.unpack_from('<i', D, rva + 1)[0] == exp; n = 5
    else: good = D[rva] == 0xFF and D[rva+1] == 0x15 and rva + 6 + struct.unpack_from('<i', D, rva + 2)[0] == exp; n = 6
    w = lambda a, n: (a + n <= (a & ~7) + 8) or (a + n <= (a & ~15) + 16)
    rok &= good and (w(rva, n) or w(rva, 2))
    if not good: print(f'  RP site {rva:X}: BYTES MISMATCH')
print(f'{len(RP)} ReadProbe sites (all ReadFile/ReadFileEx calls in .text: {len(rf_all)} + 1)')
print('READPROBE SITES OK' if rok else 'READPROBE SITES MISMATCH')
