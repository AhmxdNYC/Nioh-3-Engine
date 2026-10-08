r"""autoname.py - evidence-based automatic function names -> re\symbols\incoming\autoname.tsv

usage: py -I re\scripts_re\autoname.py
Reads re\Nioh3.dump.exe (memory-layout PE: file offset == RVA), its .pdata, re\symbols\symbols.tsv + incoming\*.tsv
(never emits a row for a function start that already has a non-auto name, or any row at an RVA already present).
Rules (tag in the description): rtti (vtable slots, ctor/dtor), thunk/wrap (import/function forwarders),
lib (CRT/zlib error-string tables), str (unique strings, __FILE__ paths, qualified-name strings). See notes\autoname.md.
"""
import bisect, collections, glob, os, re, struct, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
B = open(os.path.join(ROOT, 're', 'Nioh3.dump.exe'), 'rb').read()
BASE = 0x7ff74f410000
E = struct.unpack_from('<I', B, 0x3c)[0]
NS, SO = struct.unpack_from('<H', B, E + 6)[0], struct.unpack_from('<H', B, E + 20)[0]
SEC = {}
for i in range(NS):
    o = E + 24 + SO + 40 * i
    n = B[o:o + 8].rstrip(b'\0').decode()
    vs, va = struct.unpack_from('<II', B, o + 8)
    SEC[n] = (va, va + vs)
TEXT, RDATA, DATA = SEC['.text'], SEC['.rdata'], SEC['.data']
def intext(x): return TEXT[0] <= x < TEXT[1]
u32 = lambda o: struct.unpack_from('<I', B, o)[0]
u64 = lambda o: struct.unpack_from('<Q', B, o)[0]

# ---- .pdata
_r, _sz = struct.unpack_from('<II', B, E + 24 + 112 + 3 * 8)
RF = sorted(struct.unpack_from('<III', B, _r + 12 * i) for i in range(_sz // 12))
ST = [r[0] for r in RF]
def primary(x):
    i = bisect.bisect_right(ST, x) - 1
    if i < 0 or not (RF[i][0] <= x < RF[i][1]): return None
    r = RF[i]
    for _ in range(10):
        ui = r[2] & ~1
        flags, cnt = B[ui] >> 3, B[ui + 2]
        if not flags & 4: break
        r = struct.unpack_from('<III', B, ui + 4 + ((cnt + 1) & ~1) * 2)
    return r[0]
FEND = {}
FSTART = set()
for r in RF:
    p = primary(r[0])
    if p is None: continue
    FSTART.add(p)
    if p == r[0]: FEND[p] = r[1]
FSTART = sorted(FSTART)
NFUNC = len(FSTART)

# ---- existing symbols
AUTO = re.compile(r'^(FUN_|sub_|LAB_|thunk_|Sleep\d?Call_|\w+Call_[0-9A-F]+$|(Sched0Hot|MainHot|RenderSubmitHot|PauseSpin)_[0-9A-F]+$|Unknown_)')
SYMDIR = os.path.join(ROOT, 're', 'symbols')
named, taken_rva, names_used = {}, set(), set()
def load_syms(path):
    for l in open(path, encoding='utf-8'):
        f = l.rstrip('\n').split('\t')
        if len(f) < 3 or not f[0].startswith('0x'): continue
        rva = int(f[0], 16); taken_rva.add(rva); names_used.add(f[2])
        if f[1] == 'function' and not AUTO.match(f[2]):
            named[primary(rva) or rva] = f[2]
load_syms(os.path.join(SYMDIR, 'symbols.tsv'))
for p in glob.glob(os.path.join(SYMDIR, 'incoming', '*.tsv')):
    if os.path.basename(p) != 'autoname.tsv': load_syms(p)
cov_before = len(named)

out = {}   # rva -> [name, desc, conf]
def propose(p, name, desc, conf):
    if p in named or p in taken_rva or p in out: return False
    out[p] = [name, desc, conf]
    return True
CNT = collections.Counter()

# ---- imports
IMP = {}  # iat slot rva -> (dll, name)
ir, isz = struct.unpack_from('<II', B, E + 24 + 112 + 8)
o = ir
while ir and u32(o + 12):
    dll = B[u32(o + 12):].split(b'\0')[0].decode().lower().replace('.dll', '')
    int_ = u32(o) or u32(o + 16); iat = u32(o + 16)
    k = 0
    while True:
        v = u64(int_ + 8 * k)
        if not v: break
        if not v >> 63:
            nm = B[(v & 0x7fffffff) + 2:].split(b'\0')[0].decode('latin1')
            IMP[iat + 8 * k] = (dll, nm)
        k += 1
    o += 20

# ---- rip-relative lea r,[rip+d] in .text
LEA = collections.defaultdict(list)   # target rva -> [site rva]
tx = B[TEXT[0]:TEXT[1]]
for m in re.finditer(rb'[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]', tx):
    s = m.start() + TEXT[0]
    LEA[s + 7 + struct.unpack_from('<i', B, s + 3)[0]].append(s)

# ---- 1. RTTI
TDN = {}
for m in re.finditer(rb'\.\?A[VU][A-Za-z0-9_@$?]+@@\x00', B[DATA[0]:DATA[1]]):
    TDN[m.start() + DATA[0] - 16] = m.group()[:-1].decode()
def demangle(n):
    return '::'.join(reversed([p for p in n[4:-2].split('@') if p]))
COL = {}
rd = B[RDATA[0]:RDATA[1]]
for m in re.finditer(rb'\x01\x00\x00\x00', rd):
    if m.start() % 4: continue
    a = m.start() + RDATA[0]
    if a + 24 > len(B): continue
    td = u32(a + 12)
    if td in TDN and u32(a + 20) == a:
        COL[a] = (TDN[td], u32(a + 4))
colva = {BASE + a: v for a, v in COL.items()}
VT = []
for a in range(RDATA[0], RDATA[1] - 8, 8):
    if u64(a) in colva: VT.append((a + 8, colva[u64(a)]))
slotfn = collections.defaultdict(set)
for vt, (cn, off) in VT:
    for k in range(400):
        p = u64(vt + 8 * k) - BASE
        if not intext(p): break
        slotfn[p].add((demangle(cn), k, vt, off))
for p0, ents in slotfn.items():
    p = primary(p0) or p0
    cls = sorted({e[0] for e in ents})
    if len(cls) != 1: continue
    slot = min(e[1] for e in ents)
    if propose(p, f'{cls[0]}::vfunc_{slot}', f'rtti: vtable slot {slot} of {cls[0]} (vtbl {min(e[2] for e in ents):#x}, RTTI COL)', 'high'): CNT['rtti_vfunc'] += 1
ctor = collections.defaultdict(set)
for vt, (cn, off) in VT:
    for s in LEA.get(vt, []):
        p = primary(s)
        if p is not None: ctor[p].add(demangle(cn))
for p, cs in ctor.items():
    if len(cs) == 1:
        c = next(iter(cs))
        if propose(p, f'{c}::ctor_or_dtor', f'rtti: stores vtable of {c} (lea of vtbl; constructor/destructor)', 'high'): CNT['rtti_ctor'] += 1

def resolve_name(p):
    return named.get(p) or (out[p][0] if p in out else None)

# ---- 3. thunks / wrappers
def thunk_target(p):
    b = B[p:FEND.get(p, p)]
    if len(b) == 6 and b[:2] == b'\xff\x25':
        return ('imp', p + 6 + struct.unpack_from('<i', b, 2)[0])
    if len(b) == 5 and b[0] == 0xe9:
        return ('fn', p + 5 + struct.unpack_from('<i', b, 1)[0])
    return None
def wrap_target(p):
    b = B[p:FEND.get(p, p)]
    if not (9 <= len(b) <= 24): return None
    if not (b[:4] == b'\x48\x83\xec\x28' and b[-5:] == b'\x48\x83\xc4\x28\xc3'): return None
    if b.count(b'\xe8') != 1 or b[-10] != 0xe8 and b[-10:-5].count(b'\xe8') != 1: pass
    i = len(b) - 10
    if b[i] != 0xe8 or b.count(b'\xe8') != 1: return None
    return p + i + 5 + struct.unpack_from('<i', b, i + 1)[0]
ok = lambda n: n and not AUTO.match(n)
for rnd in range(3):
    for p in FSTART:
        if p in named or p in out or p in taken_rva: continue
        t = thunk_target(p)
        if t:
            if t[0] == 'imp' and t[1] in IMP:
                d, n = IMP[t[1]]
                if propose(p, f'{n}_thunk', f'thunk: jmp [IAT {d}!{n}]', 'high'): CNT['thunk_import'] += 1
            elif t[0] == 'fn':
                tn = resolve_name(primary(t[1]) or t[1])
                if ok(tn) and propose(p, re.sub(r'_thunk$', '', tn) + '_thunk', f'thunk: jmp {tn} ({t[1]:#x})', 'high'): CNT['thunk_fn'] += 1
            continue
        t = wrap_target(p)
        if t:
            tn = resolve_name(primary(t) or t)
            if ok(tn) and propose(p, f'Wrap_{tn}', f'wrap: only sub rsp/call {tn} ({t:#x})/ret', 'high'): CNT['wrap'] += 1
for p in FSTART:
    if p in named or p in out or p in taken_rva: continue
    b = B[p:FEND.get(p, p)]
    if 9 <= len(b) <= 24 and b[:4] == b'\x48\x83\xec\x28' and b[-5:] == b'\x48\x83\xc4\x28\xc3' and b.count(b'\xff\x15') == 1 and b.count(b'\xe8') == 0:
        i = b.index(b'\xff\x15')
        slot = p + i + 6 + struct.unpack_from('<i', b, i + 2)[0]
        if slot in IMP and i == len(b) - 11:
            d, n = IMP[slot]
            if propose(p, f'Wrap_{n}', f'wrap: only calls import {d}!{n}', 'high'): CNT['wrap_import'] += 1

# ---- strings referenced from code
def cstr(a):
    if not (RDATA[0] <= a < DATA[1]): return None
    if a > RDATA[0] and B[a - 1] != 0: return None
    e = B.find(b'\0', a, a + 400)
    if e < 0 or e - a < 6: return None
    s = B[a:e]
    if any(c < 32 or c > 126 for c in s): return None
    return s.decode()
SFN = collections.defaultdict(set)
FSTRS = collections.defaultdict(list)
for t, sites in LEA.items():
    if not (RDATA[0] <= t < DATA[1]): continue
    s = cstr(t)
    if s is None: continue
    for site in sites:
        p = primary(site)
        if p is None: continue
        SFN[s].add(p); FSTRS[p].append(s)

# ---- 4. library strings (curated)
LIB = [
 ('incorrect header check', 'zlib_inflate_state_machine', 'zlib inflate()'),
 ('invalid distance too far back', 'zlib_inflate_fast_check', 'zlib inflate'),
 ('invalid block type', 'zlib_inflate_block_type', 'zlib inflate'),
 ('incorrect data check', 'zlib_inflate_data_check', 'zlib inflate'),
 ('R6030', 'CRT_rterr_crt_not_initialized', 'MSVC CRT runtime error text'),
 ('R6008', 'CRT_rterr_no_space_for_arguments', 'MSVC CRT runtime error text'),
 ('R6009', 'CRT_rterr_no_space_for_environment', 'MSVC CRT runtime error text'),
 ('R6010', 'CRT_rterr_abort_called', 'MSVC CRT runtime error text'),
 ('R6016', 'CRT_rterr_no_space_for_thread_data', 'MSVC CRT runtime error text'),
 ('R6017', 'CRT_rterr_multithread_lock_error', 'MSVC CRT runtime error text'),
 ('R6018', 'CRT_rterr_heap_error', 'MSVC CRT runtime error text'),
 ('R6019', 'CRT_rterr_no_console_device', 'MSVC CRT runtime error text'),
 ('R6024', 'CRT_rterr_no_space_for_onexit_table', 'MSVC CRT runtime error text'),
 ('R6025', 'CRT_rterr_pure_virtual_call', 'MSVC CRT runtime error text'),
 ('R6026', 'CRT_rterr_no_space_for_stdio', 'MSVC CRT runtime error text'),
 ('R6027', 'CRT_rterr_no_space_for_lowio', 'MSVC CRT runtime error text'),
 ('R6028', 'CRT_rterr_cannot_init_heap', 'MSVC CRT runtime error text'),
 ('R6031', 'CRT_rterr_crt_init_twice', 'MSVC CRT runtime error text'),
 ('R6032', 'CRT_rterr_no_space_for_locale', 'MSVC CRT runtime error text'),
 ('R6033', 'CRT_rterr_msil_code', 'MSVC CRT runtime error text'),
]
LIBHIT = {}
for pat, nm, why in LIB:
    for s, fns in SFN.items():
        if s.startswith(pat) and len(fns) == 1:
            p = next(iter(fns))
            if p not in LIBHIT: LIBHIT[p] = (nm, f'lib: {why}; refs "{s[:60]}"')
for p, (nm, d) in LIBHIT.items():
    if len(FSTRS[p]) <= 12 and propose(p, nm, d, 'high'): CNT['lib'] += 1

# ---- 2. unique strings
SRC = re.compile(r'([\w\-. ]+\.(?:cpp|c|h|hpp|inl|cc))$', re.I)
IDENT = re.compile(r'^[A-Za-z_][A-Za-z0-9_]*(::[A-Za-z_~][A-Za-z0-9_]*)+$')
FIELDS = {}
for l in open(os.path.join(ROOT, 're', 'out', 'renderconfig_fields.tsv'), encoding='utf-8'):
    f = l.rstrip('\n').split('\t')
    if len(f) >= 4 and f[3].startswith('0x'): FIELDS.setdefault(primary(int(f[3], 16)), []).append(f[1])
def sanitize(s, n=48):
    return re.sub(r'[^A-Za-z0-9]+', '_', s).strip('_')[:n].rstrip('_')
GENERIC = re.compile(r'^(unknown error.*|%[-0-9.l]*[sdfxu]|true|false|null|none|default|unknown|error|invalid|success|failed)$', re.I)
def srcbase(s):
    m = SRC.search(s.replace('\\', '/').split('/')[-1])
    return m.group(1) if m and ('/' in s or '\\' in s) else None
def good(s):
    if len(s) < 8: return False
    if ' ' in s and s.islower() and len(s) < 20: return False   # short lowercase sentence fragments (log glue)
    if sum(c.isalpha() for c in s) < 0.55 * len(s): return False
    if ' ' not in s and not re.search(r'[A-Za-z]{4}', s): return False
    return not GENERIC.match(s)
def score(s):
    if srcbase(s) or IDENT.match(s): return 3
    if len(s) >= 14 and ' ' in s: return 2
    return 1
for p, ss in sorted(FSTRS.items()):
    if p in named or p in out or p in taken_rva: continue
    if p in FIELDS: continue            # field-registration sites: handled elsewhere
    uniq = [s for s in dict.fromkeys(ss) if good(s) and len(SFN[s]) <= 2]
    if not uniq or len(FSTRS[p]) > 12: continue
    best = max(uniq, key=lambda s: (score(s), -len(s)))
    srcs = [s for s in uniq if srcbase(s)]
    idn = [s for s in uniq if IDENT.match(s)]
    size = FEND.get(p, p) - p
    if len(idn) == 1:
        name = idn[0]; conf = 'medium' if len(uniq) == 1 and size <= 8000 else 'low'; why = 'string is a qualified function/method name'; rule = 'strname'
    elif len(uniq) == 1:
        s = uniq[0]
        if srcbase(s): name = 'Src_' + sanitize(srcbase(s)); why = '__FILE__ path'
        else: name = 'Str_' + sanitize(s); why = 'distinctive string'
        conf = 'medium' if len(SFN[s]) == 1 and len(FSTRS[p]) <= 4 and size <= 8000 else 'low'; rule = 'str'
    else:
        if srcs: name = 'Src_' + sanitize(srcbase(srcs[0])); why = f'{len(uniq)} strings, __FILE__ path chosen'
        else: name = 'Str_' + sanitize(best); why = f'{len(uniq)} distinct strings, most specific chosen'
        conf = 'low'; rule = 'strmulti'
    if len(name) < 5: continue
    if propose(p, name, f'str: {why}; refs "{best[:80]}" ({len(FSTRS[p])} string refs, {size} bytes)', conf): CNT[f'{rule}_{conf}'] += 1

# ---- uniquify
cnt = collections.Counter(v[0] for v in out.values())
for p, v in out.items():
    if cnt[v[0]] > 1 or v[0] in names_used: v[0] = f'{v[0]}_{p:X}'

with open(os.path.join(SYMDIR, 'incoming', 'autoname.tsv'), 'w', encoding='utf-8', newline='\n') as f:
    f.write('# autoname.py: automatic evidence-based names (rule + evidence in description); NEW rows only\nrva\tkind\tname\tsignature\tdescription\tconfidence\tsource\n')
    for p in sorted(out):
        n, d, c = out[p]
        f.write(f'{p:#x}\tfunction\t{n}\t\t{d}\t{c}\tre/scripts_re/autoname.py\n')
print('functions (pdata primary):', NFUNC, ' named before:', cov_before)
print('rows:', len(out))
for k, v in sorted(CNT.items()): print(' ', k, v)
print('RTTI types:', len(TDN), 'COL:', len(COL), 'vtables:', len(VT), 'imports:', len(IMP), 'fns with strings:', len(FSTRS))
