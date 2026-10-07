r"""cpu_hotspots_check.py - offline proof of the byte/register facts the designs in notes/cpu-hotspots-ally.md rely on.

usage: py -I re\\scripts_cpu\\cpu_hotspots_check.py

Reads only re\\Nioh3.dump.exe (decrypted v2.02, raw offset == RVA) and re\\out\\imports.tsv. Writes nothing.
For every new or changed site: original bytes, pattern uniqueness in .text, the register context the cave uses,
that no direct branch lands strictly inside the overwritten bytes, and the IAT slot names.
"""
import os, re, struct, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
B = open(os.path.join(ROOT, 're', 'Nioh3.dump.exe'), 'rb').read()
pe = struct.unpack_from('<I', B, 0x3C)[0]
nsec = struct.unpack_from('<H', B, pe + 6)[0]
optsz = struct.unpack_from('<H', B, pe + 20)[0]
EXEC = []
for i in range(nsec):
    sh = pe + 24 + optsz + i * 40
    vs, va = struct.unpack_from('<II', B, sh + 8)
    if struct.unpack_from('<I', B, sh + 36)[0] & 0x20000000:
        EXEC.append((va, vs))
IAT = {}
for l in open(os.path.join(ROOT, 're', 'out', 'imports.tsv'), encoding='utf-8'):
    f = l.rstrip('\n').split('\t')
    if len(f) >= 3 and f[2].startswith('0x'):
        IAT[int(f[2], 16)] = f[0] + '!' + f[1]

fails = 0
def check(ok, what):
    global fails
    print(('  OK    ' if ok else '  FAIL  ') + what)
    if not ok:
        fails += 1

def hx(s):
    return bytes.fromhex(s.replace(' ', ''))

def at(rva, hexbytes, what):
    want = hx(hexbytes)
    got = B[rva:rva + len(want)]
    check(got == want, f'{rva:#x} {what}: {got.hex(" ").upper()}')

def unique(pattern_hex, expect_rva, what):
    pat = hx(pattern_hex)
    hits = [va + m.start() for va, vs in EXEC for m in re.finditer(re.escape(pat), B[va:va + vs])]
    check(hits == [expect_rva], f'pattern {what} unique at {expect_rva:#x} (hits {[hex(h) for h in hits]})')

def iat_call(rva, name):
    ok = B[rva] == 0xFF and B[rva + 1] == 0x15
    slot = rva + 6 + struct.unpack_from('<i', B, rva + 2)[0]
    check(ok and IAT.get(slot, '').upper() == name.upper(), f'{rva:#x} call [{slot:#x}] = {IAT.get(slot)} (want {name})')

def no_branch_into(lo, hi, what):
    """direct rel32 call/jmp/jcc and rel8 jmp/jcc whose target is strictly inside (lo, hi)"""
    bad = []
    for va, vs in EXEC:
        sec = B[va:va + vs]
        for m in re.finditer(rb'[\xe8\xe9]', sec):
            o = va + m.start()
            if o + 5 <= va + vs:
                t = o + 5 + struct.unpack_from('<i', B, o + 1)[0]
                if lo < t < hi:
                    bad.append(o)
        for m in re.finditer(rb'\x0f[\x80-\x8f]', sec):
            o = va + m.start()
            if o + 6 <= va + vs:
                t = o + 6 + struct.unpack_from('<i', B, o + 2)[0]
                if lo < t < hi:
                    bad.append(o)
    for o in range(lo - 130, hi + 128):
        if (B[o] == 0xEB or 0x70 <= B[o] <= 0x7F):
            t = o + 2 + struct.unpack_from('<b', B, o + 1)[0]
            if lo < t < hi:
                bad.append(o)
    # rel8 byte hits are byte-pattern matches (may be operand bytes): report them, verify by hand if any
    check(not bad, f'no direct branch into {lo:#x}..{hi:#x} {what}' + (f' (candidates {[hex(b) for b in bad]})' if bad else ''))

print('== GF (ASI 1.4.0, in place; re-checked because the SP data is attributed to it)')
at(0x7D9E66, '33 C9 FF 15 CA 84 10 03', 'FenceWait event==NULL loop: xor ecx,ecx; call [Sleep]')
iat_call(0x7D9E68, 'KERNEL32.DLL!Sleep')
at(0x7D9E74, 'E8', 'loop re-check: call IsFenceComplete 1180A4')
at(0x1180A4, '40 53 48 83 EC 20 48 8B 09 48 8B DA 48 8B 01 FF 50 40 48 3B D8 0F 96 C0', 'IsFenceComplete = fence->GetCompletedValue() >= v')
at(0x117D9F, '33 C9 FF 15 91 A5 7C 03', 'worker fence poll 117D6C: xor ecx,ecx; call [Sleep]')

print('== 4E390 is not a DirectInput poll (SP stale-slot attribution, see notes section 3.2)')
at(0x4E3A5, '48 8B 01 48 8B DA 48 8D 54 24 20 FF 50 50 48 39 5C 24 30 0F 92 C0',
   'ResourceWidthBelow: obj->vtbl[0x50](&desc) = ID3D12Resource::GetDesc; return desc.Width(+10h) < n')

print('== TG tick governor: rides the existing P4 site (VrrLimiter_P4) and caveLim, no new site')
at(0x5AD860, '8B 87 14 06 00 00', 'P4 original `mov eax,[rdi+614h]` (SetLimiterFps argument)')

print('== WG wake gate: rides the existing WS0/WS1/WS2/WS3/WSs sites; register context the caves need')
at(0x48BFF, '48 69 F5 58 04 00 00', 'JobWakeOneInPool: rsi = slot index * 458h ...')
at(0x48C0B, '48 03 71 10 48 8B 0E', '... + pool->workers; rcx = [rsi] = WorkerThread')
at(0x48C53, '8B D0 83 E2 FE F0 0F B1 51 10', 'CAS clears WorkerThread+10h bit0 before the wake')
at(0x48C67, '48 8B 49 08', 'rcx = WorkerThread+8 (event)')
iat_call(0x48C6B, 'KERNEL32.DLL!SetEvent')
at(0x48C71, '40 8A C7', 'after WS2: al = dil (result does not depend on SetEvent eax)')
at(0x48CA4, '48 69 DF 58 04 00 00 48 03 59 10 48 8B 0B', 'JobWakeOne: rbx = slot, rcx = [rbx] = WorkerThread')
at(0x48D02, '48 8B 49 08', 'rcx = event')
iat_call(0x48D06, 'KERNEL32.DLL!SetEvent')
at(0x48D0C, 'B0 01', 'after WS3: al = 1 (eax of SetEvent ignored)')
at(0x40AB0, '49 69 C1 58 04 00 00 4A 8B 14 18', 'JobWorkerPoll wake scan: rdx = [r11 + r9*458h] = WorkerThread')
at(0x40B06, '48 8B 4A 08', 'rcx = event')
iat_call(0x40B0A, 'KERNEL32.DLL!SetEvent')
at(0x40A4C, '49 8B 8F 40 04 00 00', 'pool of the waker = [r15+440h] (same pool as the target)')
at(0x40CD0, '49 69 C2 58 04 00 00 4C 8B 04 30', 'task wake scan: r8 = [rsi + r10*458h] = WorkerThread')
at(0x40D2B, '49 8B 48 08', 'rcx = event')
iat_call(0x40D2F, 'KERNEL32.DLL!SetEvent')
at(0x1A6BBF5, '48 8B 9F 40 04 00 00 F0 FF 43 30', 'worker sleep: rbx = [rdi+440h] = pool; lock inc pool+30h (idle)')
at(0x1A6BC15, '33 C0 F0 0F B1 71 10', 'worker announces bit0 ...')
at(0x1A6BC2E, '48 8B 49 08 83 CA FF', '... rcx = event, edx = INFINITE')
iat_call(0x1A6BC35, 'KERNEL32.DLL!WaitForSingleObject')
at(0x1A6BC3B, 'EB 0B', 'after WSs: jmp 1A6BC48')
at(0x1A6BC48, '33 C0', '1A6BC48 xor eax,eax: the WFSO result is dead, the cave may clobber eax after the call')

print('== DS: the 1 ms QPC spin of MainLoopFrame (D) sleeps instead (optional, pairs with TG)')
at(0x1A74BDA, 'FF 15 40 D8 E6 01', 'D loop: call [QueryPerformanceCounter] (6 bytes -> 90 E8 rel32, call_at 1)')
iat_call(0x1A74BDA, 'KERNEL32.DLL!QueryPerformanceCounter')
unique('80 7A 12 00 74 10 48 8D 4D A0 FF 15', 0x1A74BD0, 'D loop head (site = hit + 0Ah)')
at(0x1A74BB8, '48 8B 5D 98', 'rbx = loop start QPC (callee-saved, readable in the cave)')
at(0x1A744E4, 'F3 0F 10 35 9C 01 16 02', 'xmm6 = 0.001f from .rdata 3BD4688 (the quantum)')
at(0x3BD4688, '6F 12 83 3A', '.rdata 3BD4688 = 0.001f')
no_branch_into(0x1A74BDA, 0x1A74BE0, '(DS site)')

print('== RL: ResourceLoad manager idle poll (1FEE28): Sleep(1) when nothing is queued -> Sleep(RL_IdleMs)')
at(0x1FEE45, '4C 8B A1 00 02 00 00 33 D2 4C 8B F1 4D 85 E4 75 1D 48 39 91 10 02 00 00 75 14',
   'idle test: [mgr+200h] == 0 && [mgr+210h] == 0')
at(0x1FEE65, '8D 4A 01', 'lea ecx,[rdx+1] (edx = 0) -> 6A 04 59 (push 4; pop rcx) = Sleep(4), same length')
iat_call(0x1FEE68, 'KERNEL32.DLL!Sleep')
unique('88 91 49 02 00 00 8D 4A 01 FF 15', 0x1FEE5F, 'RL idle branch (site = hit + 6)')
no_branch_into(0x1FEE65, 0x1FEE68, '(RL site)')

print('== IATS: IAT slots the ASI loader (Ultimate ASI Loader dinput8.dll) shims for its load trigger')
for slot, name in ((0x38E2338, 'Sleep'), (0x38E2618, 'AcquireSRWLockExclusive'), (0x38E2270, 'GetSystemTimeAsFileTime'),
                   (0x38E2498, 'GetCurrentProcessId'), (0x38E25A0, 'CreateEventW'), (0x38E25C8, 'CreateEventA'),
                   (0x38E26C0, 'GetSystemInfo')):
    check(IAT.get(slot, '').upper() == ('KERNEL32.DLL!' + name).upper(), f'IAT {slot:#x} = {IAT.get(slot)}')

print('\nALL PASSED' if not fails else f'\n{fails} FAILED')
sys.exit(1 if fails else 0)
