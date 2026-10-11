"""regions.py: split .text of your game dump into purpose-labelled regions -> data\\regions.tsv (needs numpy)

Not a decompilation: every function gets EVIDENCE (strings it references, imports it calls, RTTI classes whose vtables
hold it or that it constructs, names in symbols\\symbols.tsv / symbols\\incoming\\*.tsv), each piece of evidence votes for a purpose
label (RULES below), votes are spread over the direct call graph (E8/E9 rel32 to a .pdata start) and over address
neighbours (MSVC links code roughly per object file, so neighbours tend to share a module), and the address space is then
cut into contiguous runs with one label each.

Output columns: start end label confidence evidence funcs code_bytes  (RVAs, end exclusive; label = 'tech / plain').
In the evidence column a game string is written as its address (s@0xRVA), never its text, so the published table holds
no game text; --evidence-text writes the text instead (for your own use; do not commit that file).
String evidence needs <local dir>\\strings.tsv, which make_strings.py builds from your dump on the first run.
  py -I tools\\progress\\regions.py [--md]          write data\\regions.tsv + print the label table
  py -I tools\\progress\\regions.py --regions PATH  write the table to PATH instead
  py -I tools\\progress\\regions.py --survey   per-1MB evidence digest (to write new RULES)
  py -I tools\\progress\\regions.py --why 0xRVA  evidence for the region holding RVA
"""
import bisect, collections, os, re, struct, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import common as C  # noqa: E402

ROOT = C.ROOT
OUTF = os.path.abspath(C._arg('--regions') or C.data('regions.tsv'))
TEXT_EVIDENCE = '--evidence-text' in sys.argv
B = C.load_dump()
E = struct.unpack_from('<I', B, 0x3c)[0]
NS, SO = struct.unpack_from('<H', B, E + 6)[0], struct.unpack_from('<H', B, E + 20)[0]
SEC = {}
for _i in range(NS):
    _o = E + 24 + SO + 40 * _i
    SEC[B[_o:_o + 8].rstrip(b'\0').decode()] = (struct.unpack_from('<I', B, _o + 12)[0],
                                               struct.unpack_from('<I', B, _o + 12)[0] + struct.unpack_from('<I', B, _o + 8)[0])
TEXT = SEC['.text']
u32 = lambda o: struct.unpack_from('<I', B, o)[0]

# ---------------------------------------------------------------- functions (.pdata, chained entries folded)
_r, _sz = struct.unpack_from('<II', B, E + 24 + 112 + 3 * 8)
RF = sorted(struct.unpack_from('<III', B, _r + 12 * i) for i in range(_sz // 12))
RST = np.array([r[0] for r in RF], dtype=np.int64)
REN = np.array([r[1] for r in RF], dtype=np.int64)


def _primary(r):
    for _ in range(10):
        ui = r[2] & ~1
        if not (B[ui] >> 3) & 4:
            break
        r = struct.unpack_from('<III', B, ui + 4 + ((B[ui + 2] + 1) & ~1) * 2)
    return r[0]


PRIM = np.array([_primary(r) for r in RF], dtype=np.int64)       # pdata entry -> owning function start
FSZ = collections.Counter()
for (s, e, _), p in zip(RF, PRIM):
    FSZ[int(p)] += e - s
FST = np.array(sorted(FSZ), dtype=np.int64)                        # function starts
FIDX = {int(s): i for i, s in enumerate(FST)}
NF = len(FST)
SIZE = np.array([FSZ[int(s)] for s in FST], dtype=np.int64)


def owner(rvas):
    """array of RVAs -> function index (or -1)"""
    rvas = np.asarray(rvas, dtype=np.int64)
    i = np.searchsorted(RST, rvas, side='right') - 1
    ok = (i >= 0) & (rvas < REN[np.clip(i, 0, None)])
    p = PRIM[np.clip(i, 0, None)]
    j = np.searchsorted(FST, p)
    return np.where(ok, j, -1)


# ---------------------------------------------------------------- raw byte scans
A = np.frombuffer(B, dtype=np.uint8)


def rel32_targets(mask_pos, extra=0):
    """positions p of an opcode/modrm byte -> (p, target) with disp32 at p+1, next-insn = p+5+extra"""
    p = mask_pos
    d = (A[p + 1].astype(np.int64) | (A[p + 2].astype(np.int64) << 8) | (A[p + 3].astype(np.int64) << 16)
         | (A[p + 4].astype(np.int64) << 24))
    d = np.where(d >= 1 << 31, d - (1 << 32), d)
    return p, p + 5 + extra + d


def call_edges():
    t0, t1 = TEXT
    seg = A[t0:t1 - 5]
    pos = np.nonzero((seg == 0xE8) | (seg == 0xE9))[0] + t0
    p, t = rel32_targets(pos)
    k = np.searchsorted(FST, t)
    hit = (k < NF) & (FST[np.clip(k, 0, NF - 1)] == t)
    p, k = p[hit], k[hit]
    src = owner(p)
    ok = (src >= 0) & (src != k)
    pairs = np.unique(np.stack([src[ok], k[ok]], 1), axis=0)
    return pairs                                                    # [caller_idx, callee_idx]


def riprel_refs(targets_sorted):
    """functions referencing any RVA in targets_sorted via [rip+disp32] (modrm mod=00 rm=101) -> [(fidx, target)]"""
    t0, t1 = TEXT
    seg = A[t0:t1 - 5]
    pos = np.nonzero((seg & 0xC7) == 0x05)[0] + t0
    out = []
    for extra in (0, 1, 4):                                        # no imm / imm8 / imm32 after the disp
        p, t = rel32_targets(pos, extra)                            # modrm at p, disp at p+1, next = p+5+extra
        k = np.searchsorted(targets_sorted, t)
        hit = (k < len(targets_sorted)) & (targets_sorted[np.clip(k, 0, len(targets_sorted) - 1)] == t)
        src = owner(p[hit])
        out.append(np.stack([src, t[hit]], 1))
    o = np.concatenate(out)
    return np.unique(o[o[:, 0] >= 0], axis=0)


# ---------------------------------------------------------------- evidence
BASE = C.image_base(B)   # runtime load address of the dumped process (dump_nioh3.py writes it into the header)


def load_strings():
    """-> {rva: text}, and Ghidra's code refs [(fidx, rva)]"""
    S, refs = {}, []
    if not os.path.exists(C.STRINGS):
        import make_strings
        make_strings.build()
    for l in open(C.STRINGS, encoding='utf-8', errors='replace'):
        f = l.rstrip('\n').split('\t')
        if len(f) < 5 or not f[0].startswith('0x'):
            continue
        r = int(f[0], 16)
        S[r] = f[4]
        for x in f[2].split(','):
            if x.startswith('0x'):
                refs.append((int(x, 16), r))
    if refs:
        o = owner([a for a, _ in refs])
        refs = [(int(i), r) for i, (_, r) in zip(o, refs) if i >= 0]
    return S, refs


def load_imports():
    """-> [(fidx, 'DLL!name')] (jmp-thunk callers included)"""
    out = []
    for l in open(C.data('imports.tsv'), encoding='utf-8'):
        f = l.rstrip('\n').split('\t')
        if len(f) < 5 or f[0] == 'library':
            continue
        xs = [int(x.lstrip('T'), 16) for x in f[4].split(',') if x.lstrip('T').startswith('0x')]
        for i in owner(xs) if xs else []:
            if i >= 0:
                out.append((int(i), f[0].upper() + '!' + f[1]))
    return out


def load_names():
    """-> {fidx: [(name, desc, source)]} from symbols.tsv + incoming/*.tsv (auto FUN_ names skipped)"""
    auto = re.compile(r'^(FUN_|sub_|LAB_|thunk_|Sleep\d?Call_|\w+Call_[0-9A-F]+$|(Sched0Hot|MainHot|RenderSubmitHot|PauseSpin)_[0-9A-F]+$)')
    rows = []
    # (our private run also read autoname_low_hints.tsv; every row in it is a string-derived 'str:' hint, which
    # evidence() skips, so it never changed the result and is not published)
    for fn in [C.SYMS] + C.incoming_files():
        if not os.path.exists(fn):
            continue
        src = os.path.basename(fn)
        for l in open(fn, encoding='utf-8', errors='replace'):
            f = l.rstrip('\n').split('\t')
            if len(f) > 2 and f[0].startswith('0x') and f[1] in ('function', 'label') and f[2] and not auto.match(f[2]):
                rows.append((int(f[0], 16), f[2], f[4] if len(f) > 4 else '', src))
    o = owner([r[0] for r in rows])
    out = collections.defaultdict(list)
    for i, (_, n, d, s) in zip(o, rows):
        if i >= 0:
            out[int(i)].append((n, d, s))
    return out


def data_pointers():
    """8-byte-aligned absolute pointers in .rdata/.data/_RDATA -> (slot_rva array, target_rva array)"""
    ss, tt = [], []
    for sec in ('.rdata', '.data', '_RDATA'):
        a, b = SEC[sec]
        q = np.frombuffer(B[a:b - (b - a) % 8], dtype=np.uint64).astype(np.int64)
        t = q - BASE
        ok = (t >= 0x1000) & (t < SEC['.reloc'][0])
        ss.append(np.nonzero(ok)[0] * 8 + a)
        tt.append(t[ok])
    return np.concatenate(ss), np.concatenate(tt)


def vtables(slots, tgts):
    """runs of >=2 consecutive function-start pointers in .rdata -> [(vt_rva, [fidx...], col_rva_or_None)]"""
    k = np.searchsorted(FST, tgts)
    isf = (k < NF) & (FST[np.clip(k, 0, NF - 1)] == tgts)
    fs, fk = slots[isf], k[isf]
    out, i = [], 0
    ptr = dict(zip(slots.tolist(), tgts.tolist()))
    while i < len(fs):
        j = i
        while j + 1 < len(fs) and fs[j + 1] == fs[j] + 8:
            j += 1
        if j > i and SEC['.rdata'][0] <= fs[i] < SEC['.rdata'][1]:
            col = ptr.get(int(fs[i]) - 8)
            out.append((int(fs[i]), [int(x) for x in fk[i:j + 1]], col))
        i = j + 1
    return out


def rtti_names(ptr):
    """COL rva -> demangled class name (MSVC x64 RTTI: COL+12 = TypeDescriptor rva, TD+16 = '.?AV...@@')"""
    def cls(col):
        try:
            if u32(col) != 1:
                return None
            td = u32(col + 12)
            m = re.match(rb'\.\?A[VUT]([^\0]{1,200}?)@@\0', B[td + 16:td + 240])
            if not m:
                return None
            parts = m.group(1).decode('latin-1').split('@')
            return '::'.join(reversed([p for p in parts if p]))
        except Exception:
            return None
    return cls


def survey():
    S, srefs = load_strings()
    imps = load_imports()
    names = load_names()
    ss, tt = data_pointers()
    vts = vtables(ss, tt)
    cls = rtti_names(None)
    nref = collections.Counter(r for _, r in srefs)
    MB = 1 << 20
    bstr, bimp, bcls, bnm = (collections.defaultdict(collections.Counter) for _ in range(4))
    for i, r in srefs:
        s = S[r]
        if 4 <= len(s) <= 90 and nref[r] <= 6 and re.search(r'[A-Za-z]{4}', s):
            bstr[FST[i] // MB][s] += 1
    for i, n in imps:
        bimp[FST[i] // MB][n.split('!')[0].replace('.DLL', '')] += 1
    nv = 0
    for vt, fl, col in vts:
        c = cls(col) if col else None
        if c:
            nv += 1
            for i in fl:
                bcls[FST[i] // MB][c] += 1
    for i, lst in names.items():
        for n, d, s in lst:
            if s != 'autoname.tsv':
                bnm[FST[i] // MB][n] += 1
    print('functions %d, string refs %d, import refs %d, vtables %d (rtti %d)' % (NF, len(srefs), len(imps), len(vts), nv))
    for m in range(TEXT[0] // MB, TEXT[1] // MB + 1):
        sel = (FST // MB) == m
        print('== %2dMB fn %d %.2fMB' % (m, sel.sum(), SIZE[sel].sum() / 1e6))
        print('  S:', ' | '.join(s[:40] for s, _ in bstr[m].most_common(7)))
        if bcls[m]:
            print('  C:', ' '.join(c[:40] for c, _ in bcls[m].most_common(5)))
        if bimp[m]:
            print('  I:', ' '.join('%s%d' % kv for kv in bimp[m].most_common(5)))
        if bnm[m]:
            print('  N:', ' '.join(n[:28] for n, _ in bnm[m].most_common(5)))


# ---------------------------------------------------------------- purpose labels
LABELS = {  # key -> 'tech / plain'
    'CRT': 'MSVC CRT·STL / C runtime & std library',
    'KTUL': 'ktul / strings, text codecs & serializer',
    'ZLIB': 'zlib 1.1.4 / decompression',
    'VORBIS': 'libvorbis / Ogg audio decoding',
    'AUDIO': 'Ktsl2·XAudio2 / sound engine & mixer',
    'STEAM': 'Steamworks / Steam platform',
    'NET': 'ktgl Online·P2P / online & matchmaking',
    'COOP': 'Online:: sync / co-op gameplay sync',
    'SAVE': 'SaveData / saving & config',
    'SCL': 'ktgl ScreenLayout / menus & HUD',
    'TEXT': 'Game menus·text markup / menu screens & item text',
    'SCRIPT': 'KIDS script binds / game logic scripting',
    'GAME': 'Mission·Quest·Items / game rules & logic',
    'CHAR': 'Player·Equip·Skill / characters & combat',
    'AI': 'AI·Behavior / enemy & NPC AI',
    'ANIM': 'Anim·Motion·Rig / animation',
    'PHYS': 'Collision·SoftBody / physics & collision',
    'CAMERA': 'Camera / camera control',
    'GFX': 'SynCtx·D3D12 / GPU submission',
    'SHADERC': 'DXC·D3DCompiler / shader compile & PSOs',
    'MATERIAL': 'PB2·render params / materials & render settings',
    'POST': 'PostEffect·TAA / post-processing',
    'LIGHT': 'RefProbe·GI / lighting',
    'SHADOW': 'Shadow·ESM / shadows',
    'NATURE': 'Nature·Ocean / sky, weather & water',
    'FX': 'Effect·Particle / visual effects',
    'MESHLET': 'Meshlet·Culling / geometry & culling',
    'UPSCALE': 'FSR3·XeSS·Streamline / upscalers & frame-gen',
    'MOVIE': 'MediaFoundation / video playback',
    'INPUT': 'XInput·HID / controls',
    'JOBS': 'KIDS jobs / job system & threads',
    'MEM': 'Alloc·Pool / memory',
    'RES': 'Stream·RDB / resource loading',
    'APP': 'WinMain·Win32 / app, window & OS',
    'MATH': 'Vec·Mtx / math & containers',
    'OBJ': 'Obj·Stage / object & scene update',
    'PHOTO': 'PhotoMode / photo mode',
    'GEN': 'RTRig codegen / compiled animation rigs',
    'SCHEMA': 'EnvParam schema / editor param tables',
    'CRYPTO': 'BCrypt·hash / crypto & checksums',
    'DEBUG': 'Debug·profiler / dev & debug tools',
}

R_ = [  # (key, regex) on one evidence token; first match wins. Order: libraries first, then specific, then generic.
    ('ZLIB', r'\bzlib|inflate|deflate|Mark Adler|Jean-loup|ZlibInflate|CZlib|InflateTask|adler32'),
    ('VORBIS', r'vorbis|OggS|\bogg_|Xiph'),
    ('UPSCALE', r'FSR3|Fsr[0-9A-Z]|FidelityFX|\bffx|Ffx[A-Z]|FFX_|XeSS|Xess|XESS|XeLL|xell|LIBXE|Streamline|SL\.INTERPOSER|\bsl[A-Z]\w+|DLSS|Dlss|Nvapi|nvapi|Reflex|NGX|FrameGen|FrameInterpolation|cbFSR|r_input_|rw_|_upscaler|AMD_AGS|agsDriver'),
    ('STEAM', r'STEAM_API|Steam(?!Online)|ISteam|steam_appid|STEAMREMOTE'),
    ('MOVIE', r'MFPLAT|MFREADWRITE|MFCreate|IMFSource|SourceReader|Movie|\.usm|USM|Bink|VideoPlay'),
    ('INPUT', r'XINPUT|^HID!|HidD_|HidP_|DINPUT|DirectInput|SETUPAPI|SetupDi|IMM32|Imm[A-Z]|Input(?!Stream)|PadAnalog|\bPad[A-Z]|Mouse|Keyboard|Vibration|Rumble|KeyConfig|KeyBind|GamePad|DualSense|Controller'),
    ('AUDIO', r'XAUDIO|XAudio|Ktsl|KTSL|ktsl|Sound|Audio|Reverb|Ducking|Compressor|Equalizer|Mastering|Biquad|Voice(?!Chat)|Bgm|BGM|\bsnd|Mixer|Wwise|\bFft|auds\b|Kvs'),
    ('CRYPTO', r'BCRYPT|BCrypt|CRYPT32|Crypt[A-Z]|\bSHA|Sha\d|\bMD5|Md5|\bAES|Aes[A-Z]|\bRSA|public_key|_keys\b|Hmac|HMAC|Crc\d|\bCRC'),
    ('COOP', r'Online::|Synchro|Coop|COOP|Multiplay|Visitor|Bloodstain|Revenant|Katanaduka'),
    ('NET', r'ONLINE|Online|WS2_32|WININET|WINHTTP|IPHLPAPI|P2p|P2P|Session(?!ID)|Lobby|Matchmak|Network|Socket|socket|http|Http|HTTP|\bURL|Url[A-Z]|Protocol|Packet|Leaderboard|VoiceChat|Invitation|EOS_|m_uLocalSessionID|m_cUserName|m_cMemberData|kalvt|application/octet'),
    ('SAVE', r'SAVEDATA|SaveData|Save(?:User|Config|Load|Slot|File|Game)|BACKUP\.BIN|config\.xml|KoeiTecmo|Savedata|savedata|LoadConfig|Autosave|SHGetKnownFolder'),
    ('PHOTO', r'PhotoMode|CPhotoMode|photo_mode|Photo[A-Z]|HDR Sample Image|screen_capture'),
    ('SHADERC', r'DXCOMPILER|D3DCOMPILER|D3DCompile|DxcCreate|Dxc[A-Z]|Shader(?:Compile|Cache|Blob|Reflection)|PsoCache|PreloadPso|\bPSO|Pso[A-Z]|PipelineState|RootSignature|Unsupported (?:Op|Round|Trig)|ShaderRenderStateObjectHash|ShaderObjectHash|Hlsl|HLSL'),
    ('MESHLET', r'Meshlet|MshLod|GPUCulling|Culling|Cull[A-Z]|Precull|HiZ|Occlusion|Octree|LodScale|Lod[A-Z]|\bLOD|IndicesSlot|PositionsSlot|AttributesSlot|Bvh|BVH'),
    ('SHADOW', r'Shadow|\bESM|Esm[A-Z]|GHDM|Ghdm|WDO|ShadowMap|Csm[A-Z]|Cascade'),
    ('LIGHT', r'ReflectionProbe|RefProbe|probeGlobal|\bGI\b|Gi[A-Z]|Irradiance|Radiance|LightMap|Light(?!ning)|nIESIndex|Ambient|AutoHDR|Exposure|\bSSR|Ssr[A-Z]|Ssao|SSAO|ClusterBox|nCluster'),
    ('NATURE', r'Nature|Cloud|Sky|Atmos|Rayleigh|Weather|Rain|Snow|Wind|Ocean|Water|\bOc[A-Z]|Wave|Caustic|Ripple|Fog|VFSCAT|Volumetric|Foliage|Tree(?!Clear|Release)|Grass|PB2TREE|TimeOfDay'),
    ('POST', r'PostEffect|PostFx|Post[A-Z]|TAA|Taa[A-Z]|FXAA|Fxaa|DOF\b|Dof[A-Z]|KIDS_DOF|Bloom|Tonemap|ToneMap|Glare|LensFlare|Lens[A-Z]|Vignette|ChromaticAb|MotionBlur|Blur|ColorGrad|\bLUT|Lut[A-Z]|Sharpen|ToonEdge|TOONEDGE|Outline|EA2Output|Saturation|saturation|Gamma|Brightness|Contrast|HDR(?! Sample)|Hdr[A-Z]'),
    ('FX', r'Effect|EFFECT|Particle|Vfx|VFX|Emitter|Trail|Decal|Dissolve|Spark|Flame|Smoke|Blood|Ribbon|Billboard'),
    ('MATERIAL', r'^PB2|PB2[A-Z]|Material|Texture(?!Stream)|DetailMap|DetailUV|detailUV|Roughness|Metallic|Albedo|Normal(?:Map|Transform)|AlphaBlend|AlphaThreshold|BlendType|Specular|Diffuse|Emissive|Subsurface|\bSSS|Hair|PB2EYE|Weathering|Translucen|Sampler|UVScale|uvInfo|Tint|nScale\d|CommonViewSetting|RenderState|ParamPack|PatternMap|TransSpec'),
    ('GFX', r'D3D12|DXGI|^Syn|Syn[A-Z]|CmdList|CommandList|CmdStream|Barrier|Swapchain|SwapChain|Present|Descriptor|GpuMem|GPU|Gpu[A-Z]|RenderTarget|Render(?:Node|View|System|Pass|Graph)|Draw[A-Z]|Upload|Fence|ResourceBarrier|Viewport|Scissor|Predication|^Wc[A-Z]|KTGL3D|gbuffer|Gbuffer|GBUFFER|FrameBuffer|AsyncCompute|INPUT_SCENE'),
    ('SCL', r'screenlayout|ScreenLayout|Scl[A-Z]|scl_|\bScl|KSCL|Pane|Layout|Menu|MENU|\bHUD|Hud[A-Z]|Widget|Button|Icon|icon_|app_icon|Dialog|Tutorial|Caption|Gauge|Minimap|Ui[A-Z]|^UI|Sprite|Popup|image%d|text%d'),
    ('TEXT', r'\^\d\d~|~\{\}|\{\}<nbsp>|<nbsp>|<number|<article|<plural|<sing|<addInfo|<uppercamel|jaJP|enUS|Font|FONT|Glyph|Text(?:Box|Converter)|Message(?!Box)|Msg[A-Z]|Locali[sz]|Language|Kerning'),
    ('KTUL', r'^ktul|ktul::|KTUL_|CKString|BiSerializer|StringOutputStream|SourceTextOutputStream|CodePage|IUINTToString|CDateTime'),
    ('CRT', r'^std::|std::|bad_cast|bad_alloc|runtime_error|DNameNode|Fls[A-Z]|CompareStringEx|LCMapString|GetLocaleInfo|:Sun:Sunday|0123456789ABCDEF|CONOUT\$|_Facet|\bcrt|_crt|__acrt|__vcrt|\bmalloc|errno|strtol|printf|qsort|Memcpy|Memset|Memmove|_?Crt[A-Z]|RtlUnwind|RtlCapture|RtlLookup|RtlVirtualUnwind|Unhandled|TerminateProcess|IsDebuggerPresent|IsProcessorFeaturePresent|GetStdHandle|WriteConsole|GetCommandLine|GetEnvironment|FreeEnvironment|GetStringType|EncodePointer|DecodePointer|TlsAlloc|InitializeSListHead|_Xlength|_Xout_of_range|type_info'),
    ('GEN', r'GeneratedVec4|Vec4ArrayKernel'),
    ('SCHEMA', r'EnvParamSchema|SchemaSite|ParamSchema|EnvParam|RegisterRenderDebugParams'),
    ('DEBUG', r'Debug|DEBUG|Profil|Marker|Assert|assert|output_dump|OutputDebugString|MiniDump|DBGHELP|StackWalk|Crash|Watchdog|\bStats?\b|Stats[A-Z]'),
    ('SCRIPT', r'Script|script|mud::|from_kidshost|kids::motor|kids::placeable|EvaluateScript|^i+[bdf*]*_$|AutomatonRig|Automaton|Event_%s|\.event\.|kidsscndb|Cutscene|EVENT|event_'),
    ('AI', r'\bAI\b|Ai[A-Z]|AI[A-Z]|Behavior|Behaviour|Groupthink|Cognition|Cogn[A-Z]|NpcAlgo|Brain|NavMesh|Nav[A-Z]|PathFind|Pathing|Waypoint|Patrol|Aggro|Threat|Enemy|ENEMY|Boss|OBORO'),
    ('ANIM', r'Anim|ANIM|Motion|MOTION|Skel|Bone|SK_[A-Z]|Pose|Rig[A-Z]|Facial|facial|\bIK\b|Ik[A-Z]|Ragdoll|BlendTree|BlendSpace|Keyframe|Timeline|Mot[A-Z][a-z]'),
    ('PHYS', r'Collision|COLLISION|Physics|PHYSICS|Havok|Raycast|RayCast|Hit(?!s)|\bhit\b|_hit|SoftBody|Rigid|Constraint|Capsule|Contact|Penetrat|Bresenham|ClothSim|Spring|Gravity|Bullet|Shot::'),
    ('CAMERA', r'Camera|CAMERA|camera|Cam[A-Z]|FocalLength|FieldOfView|\bFov|LookAt|Shake'),
    ('CHAR', r'Player|PLAYER|player|Character|CHARACTER|character|Equip|EQUIP|Weapon|Armou?r|Skill|SKILL|Martial|MARTIAL|Onmyo|ONMYO|Ninja|NINJA|Samurai|SAMURAI|STYLE_|Stamina|Guardian|Spirit|Attack|Damage|damage|Parry|Dodge|Guard(?!ian)|Combo|Stance|Tamashiro|Honou?r|HONOUR|Amrita|PLAYER_Lv'),
    ('GAME', r'Mission|MISSION|Quest|QUEST|quest|Stage(?!Effect)|Shrine|Kodama|KODAMA|Jizo|JIZO|Item|ITEM|\bitem|Inventory|Shop|Blacksmith|Forge|Craft|Reward|Drop|Loot|Hardship|HARDSHIP|Trophy|Achievement|Story|story|Chapter|Warp|warp|retrypoint|Checkpoint|GameMode|game_start|Difficulty|Kamon|kamon|placement_id|Gadget::|Pause|notice_id|is_share'),
    ('JOBS', r'KIDS|Kids[A-Z]|Job[A-Z]?|Sched|Worker|Task|Fiber|Thread(?!Id)|THREAD|\[App\]|WaitForMultiple|KtglEvent|TaskGraph|TaskNode'),
    ('MEM', r'Alloc|ALLOC|Heap|Pool|Memory|MEMORY|Malloc|FreeList|Slab|VirtualAlloc|VirtualFree|VirtualProtect|Arena|Defrag'),
    ('RES', r'Stream|Resource|RESOURCE|Rdb|RDB|Fdata|Load|Bitmap|\.kod|Kod[A-Z]|Asset|File|FILE|\.file|Archive|Package|Ktid|KTID|ReadFile|CreateFile|GetFileSize|SetFilePointer|FindFirstFile|FindNextFile|GetFullPathName|Directory|\.dds|\.g1t|\.g1m|Preload|Mount'),
    ('APP', r'WinMain|AppLoop|Application|Window|WNDCLASS|USER32|GDI32|SHELL32|OLE32|OLEAUT32|ADVAPI32|Reg[A-Z][a-z]+Key|Registry|Service|Shell32|Setupapi|CommandLine|Monitor|Display|Resolution|Fullscreen|Borderless|Clipboard|MessageBox|DefWindowProc|PeekMessage|DispatchMessage|WINMM|timeBeginPeriod|QueryPerformance|GetTickCount|GetSystemInfo|GetModuleHandle|LoadLibrary|GetProcAddress|FreeLibrary|Invalid app configuration|Failed to initialize|Wine|wine_|cryptbase'),
    ('OBJ', r'^Obj|Obj[A-Z]|Placeable|placeable|Stage|BgThread|Bounds|ParamTable|GlobalEntity|Entity|Actor|Scene|SCENE|Spawn|Instance|Transform|Gimmick|Gadget'),
    ('MATH', r'^Vec\d|Vec\d|Mtx|Matrix|Quat|Aabb|Obb|TransformedBox|SinCos|Atan|Sqrt|Random|Rand[A-Z]|nRandomNumber|Sort|BTree|RbTree|Rb[A-Z]|Hash|Deque|LowerBound|UpperBound|SetAssoc|Fnv|Spline|Bezier|Curve|Lerp|Interp|Simd|SIMD|Clamp|Array|Vector|Bitset|Bit[A-Z]|RingBuffer|Queue'),
]
RULES = [(k, re.compile(r)) for k, r in R_]
assert all(k in LABELS for k, _ in R_)


NAME_ONLY = {'MATH'}   # leaf-math words in strings are KIDS property names (WorldQuaternion...), not math code


def classify(tok, kind='n'):
    for k, rx in RULES:
        if rx.search(tok):
            return None if (kind == 's' and k in NAME_ONLY) else k
    return None


KEYS = list(LABELS)
KI = {k: i for i, k in enumerate(KEYS)}
GEN_PRIMS = (0x504d7c, 0x504ea0, 0x50521c, 0x53b38, 0x5056d8, 0x505494, 0x505618)  # float4-array ops of the generated kernels


def evidence():
    """-> seed [NF, L] float32, tokens {fidx: [(key, kind, text, w)]}, edges [E,2], weights"""
    S, srefs = load_strings()
    ss, tt = data_pointers()
    # strings that sit in a {name, fnptr} data table next to a function pointer (script/reflection tables)
    sset = np.array(sorted(S), dtype=np.int64)
    k = np.searchsorted(FST, tt)
    isf = (k < NF) & (FST[np.clip(k, 0, NF - 1)] == tt)
    j = np.searchsorted(sset, tt)
    iss = (j < len(sset)) & (sset[np.clip(j, 0, len(sset) - 1)] == tt)
    fslot = dict(zip(ss[isf].tolist(), k[isf].tolist()))
    tab = []
    for slot, srva in zip(ss[iss].tolist(), tt[iss].tolist()):
        for d in (8, 16, -8):
            f = fslot.get(slot + d)
            if f is not None:
                tab.append((f, srva))
                break
    # our own [rip+disp32] scan for string starts (Ghidra misses some)
    rr = riprel_refs(sset)
    srefs = set(srefs) | {(int(a), int(b)) for a, b in rr} | set(tab)
    nref = collections.Counter(r for _, r in srefs)
    tok = collections.defaultdict(list)
    for i, r in srefs:
        s = S[r]
        c = classify(s, 's')
        if c:
            w = 1.0 if nref[r] <= 8 else (0.4 if nref[r] <= 40 else 0.1)
            tok[i].append((c, 's', s, w, r))
    imps = load_imports()
    iref = collections.Counter(n for _, n in imps)
    for i, n in imps:
        c = classify(n)
        if c:
            tok[i].append((c, 'i', n, 1.5 if iref[n] <= 12 else (0.5 if iref[n] <= 60 else 0.1)))
    for i, lst in load_names().items():
        for n, d, src in lst:
            hint = src == 'autoname_low_hints.tsv'
            auto = hint or d.startswith(('str:', 'rtti:', 'wrap', 'lib'))
            if n.startswith('Str_') or (auto and d.startswith('str:')):
                continue                                         # the string itself is already counted
            nn = n.split('::vfunc_')[0].split('::ctor_or_dtor')[0] if auto else n
            if nn == 'ktgl::CResource':
                continue                                         # generic base class stamped on thousands of ctors
            c = classify(nn) or (classify(d[:80]) if not auto else None)
            if c:
                tok[i].append((c, 'n', n, 0.5 if hint else (2.5 if auto else 4.0)))
    E = call_edges()
    # RTRig table: {flags, fn (often via jmp thunk), 0, "UnknownRTRig", hash, hash, ptr} x124 in .data
    pmap = dict(zip(ss.tolist(), tt.tolist()))
    for e in [a - 0x10 for a, t in pmap.items() if S.get(t) == 'UnknownRTRig']:
        f = pmap.get(e)
        if f is not None and B[f] == 0xE9:
            f = f + 5 + struct.unpack_from('<i', B, f + 1)[0]
        if f in FIDX:
            tok[FIDX[f]].append(('GEN', 'd', 'RTRig table entry 0x%x ("UnknownRTRig")' % e, 6.0))
    # generated vector-graph kernels: huge bodies calling the float4-array primitives
    prim = [FIDX[p] for p in GEN_PRIMS if p in FIDX]
    calls = collections.defaultdict(set)
    for a, b in E[np.isin(E[:, 1], prim)]:
        calls[int(a)].add(int(b))
    for i, cs in calls.items():
        if len(cs) >= 3 and SIZE[i] >= 30000:
            tok[i].append(('GEN', 'n', 'float4-array kernel (%d KB, calls %d vec4 primitives)' % (SIZE[i] >> 10, len(cs)), 6.0))
    seed = np.zeros((NF, len(KEYS)), np.float32)
    for i, lst in tok.items():
        for c, _, _, w, *_ in lst:
            seed[i, KI[c]] += w
    # vtable neighbours: link consecutive slots of each vtable
    vt = vtables(ss, tt)
    ve = [(a, b) for _, fl, _ in vt for a, b in zip(fl, fl[1:]) if a != b]
    # module-local globals: functions that touch the same static/vtable/constant (referenced by 2..30 functions)
    # are almost always in the same .cpp; chain them in address order. A ctor's lea of a vtable links it to the slots.
    t0, t1 = TEXT
    seg = A[t0:t1 - 5]
    pos = np.nonzero((seg & 0xC7) == 0x05)[0] + t0
    p, tg = rel32_targets(pos, 0)
    ok = (tg >= SEC['.rdata'][0]) & (tg < SEC['.data'][1])
    src = owner(p[ok])
    pr = np.unique(np.stack([src, tg[ok]], 1)[src >= 0], axis=0)
    by = collections.defaultdict(list)
    for f, t in pr.tolist():
        by[t].append(f)
    vtab = {v: fl for v, fl, _ in vt}
    for t, fl in by.items():
        if 2 <= len(fl) <= 30:
            ve += list(zip(fl, fl[1:]))
        if t in vtab and len(fl) <= 30:
            ve += [(f, vtab[t][0]) for f in fl]
    V = np.array(ve, dtype=np.int64).reshape(-1, 2)
    return seed, tok, E, V


SERVICE = {'CRT', 'MATH', 'MEM', 'JOBS', 'ZLIB', 'VORBIS', 'CRYPTO', 'KTUL', 'STEAM', 'DEBUG', 'APP', 'RES', 'GFX'}  # callers of these are not them


def propagate(seed, E, V, it=6, alpha=0.6, up=0.3):
    """label propagation: caller -> callee at full weight (a callee used by module X is X's), callee -> caller at
    `up` weight and never for SERVICE labels (calling malloc does not make you an allocator); shared-global/vtable
    edges (V) both ways. Each neighbour's vote is divided by its degree (hubs say nothing)."""
    svc = np.array([0.0 if k in SERVICE else up for k in KEYS], np.float32)
    a = np.concatenate([E[:, 1], E[:, 0], V[:, 0], V[:, 1]])        # receiver
    b = np.concatenate([E[:, 0], E[:, 1], V[:, 1], V[:, 0]])        # sender
    kind = np.concatenate([np.zeros(len(E), np.int8), np.ones(len(E), np.int8), np.full(2 * len(V), 2, np.int8)])
    deg = np.bincount(np.concatenate([a, b]), minlength=NF).astype(np.float32) / 2
    w = (1.0 / (1.0 + deg[b])).astype(np.float32)
    wsum = np.bincount(a, weights=w, minlength=NF).astype(np.float32) + 1e-6
    s0 = seed / np.maximum(seed.sum(1, keepdims=True), 1e-6) * np.minimum(seed.sum(1, keepdims=True), 4) / 4
    p = s0.copy()
    upm = kind == 1
    for _ in range(it):
        msg = p[b] * w[:, None]
        msg[upm] *= svc
        nb = np.zeros_like(p)
        np.add.at(nb, a, msg)
        p = s0 + alpha * nb / wsum[:, None]
    return s0, p - s0


def address_smooth(x, half):
    """box filter of rows over +-half neighbours in address order, weighted by function bytes"""
    xb = x * np.sqrt(SIZE)[:, None].astype(np.float32)
    c = np.vstack([np.zeros((1, x.shape[1]), np.float32), np.cumsum(xb, 0, dtype=np.float64).astype(np.float32)])
    i = np.arange(NF)
    lo, hi = np.clip(i - half, 0, NF), np.clip(i + half + 1, 0, NF)
    return (c[hi] - c[lo]) / (hi - lo)[:, None]


def assign(rounds=3):
    """seed -> graph propagation -> address smoothing; then feed the provisional labels back in as a weak prior and
    repeat, so evidence-free code inherits the purpose of the labelled code that calls it / shares its globals."""
    seed, tok, E, V = evidence()
    own = seed.sum(1)
    prior = np.zeros_like(seed)
    for r in range(rounds):
        s0, g = propagate(seed + prior, E, V)
        x = s0 + 0.5 * g
        score = 3 * s0 + 1.5 * g + 1.0 * address_smooth(x, 6) + 0.4 * address_smooth(x, 48) + 0.15 * address_smooth(x, 256)
        lab = score.argmax(1)
        tot = score.sum(1)
        share = score[np.arange(NF), lab] / np.maximum(tot, 1e-9)
        conf = np.full(NF, 'low', object)
        if r == 0:                                               # confidence comes from real evidence only
            gnone = g.sum(1) <= 0                                # not linked to any evidence at all
            g0, ok0 = g.argmax(1), (g.sum(1) > 1e-3) & (g.max(1) >= 0.5 * np.maximum(g.sum(1), 1e-9))  # ~2 hops from evidence, dominant
        conf[ok0 & (g0 == lab)] = 'medium'
        conf[(own > 0) & ((seed).argmax(1) == lab)] = 'high'
        conf[(tot < 1e-4) | ((own == 0) & gnone)] = 'unknown'   # address-only guesses stay 'unknown'
        prior = np.zeros_like(seed)
        w = np.where(conf == 'medium', 0.6, np.where(conf == 'low', 0.3, 0.0)).astype(np.float32)
        prior[np.arange(NF), lab] = w * (own == 0)
    return lab, conf, tok, s0, g


def segment(lab, conf, minb=2048):
    """contiguous runs of one label in address order; tiny low-confidence runs fold into the larger neighbour"""
    lab = lab.copy()
    for _ in range(4):
        runs = []
        i = 0
        while i < NF:
            j = i
            while j + 1 < NF and lab[j + 1] == lab[i]:
                j += 1
            runs.append([i, j])
            i = j + 1
        changed = 0
        for r, (i, j) in enumerate(runs):
            b = SIZE[i:j + 1].sum()
            if b >= minb or (conf[i:j + 1] == 'high').any():
                continue
            nb = [runs[x] for x in (r - 1, r + 1) if 0 <= x < len(runs)]
            if not nb:
                continue
            best = max(nb, key=lambda q: SIZE[q[0]:q[1] + 1].sum())
            lab[i:j + 1] = lab[best[0]]
            changed += 1
        if not changed:
            break
    return lab, runs


def main():
    lab, conf, tok, s0, g = assign()
    lab2, _ = segment(lab, conf)
    moved = lab2 != lab
    conf = conf.copy()
    conf[moved & (conf != 'unknown')] = 'low'
    lab = lab2
    rows, i = [], 0
    while i < NF:
        j = i
        while j + 1 < NF and lab[j + 1] == lab[i]:
            j += 1
        rows.append((i, j))
        i = j + 1
    out = ['start\tend\tlabel\tconfidence\tevidence\tfuncs\tcode_bytes']
    agg = collections.defaultdict(lambda: collections.Counter())
    for i, j in rows:
        k = KEYS[lab[i]]
        sz = SIZE[i:j + 1]
        cb, ev = collections.Counter(), collections.Counter()
        for c, x in zip(conf[i:j + 1], sz):
            cb[c] += int(x)
        tb = sz.sum()
        if cb['unknown'] > 0.5 * tb:
            lbl, cf = 'unknown / no evidence', 'none'
            ev['address-only guess: ' + LABELS[k]] = 1
        else:
            lbl = LABELS[k]
            cf = 'high' if cb['high'] >= 0.25 * tb else ('medium' if cb['high'] + cb['medium'] >= 0.3 * tb else 'low')
            if k == 'GEN' and cf == 'high':
                cf = 'medium'   # table membership is certain; 'animation rig' is read off the table/hash names only
        srva = {}
        for f in range(i, j + 1):
            for c, kind, t, w, *r in tok.get(f, ()):
                if c == k:
                    key = '%s:%s' % (kind, t[:48].replace('\t', ' ').replace('\n', ' '))
                    ev[key] += w
                    if r:   # a string: remember its lowest address
                        srva[key] = min(srva.get(key, r[0]), r[0])
        show = (lambda e: e) if TEXT_EVIDENCE else (lambda e: 's@0x%x' % srva[e] if e in srva else e)
        evs = '; '.join(show(e) for e, _ in ev.most_common(4)) or 'inferred: callers / shared globals / address neighbours'
        out.append('0x%x\t0x%x\t%s\t%s\t%s\t%d\t%d' % (FST[i], REN[np.searchsorted(RST, FST[j])], lbl, cf, evs, j - i + 1, tb))
        agg[lbl][cf] += int(tb)
    open(OUTF, 'w', encoding='utf-8', newline='\n').write('\n'.join(out) + '\n')
    print('wrote', OUTF)
    tot = SIZE.sum()
    print('regions: %d rows, %d functions, %.1f MB code' % (len(rows), NF, tot / 1e6))
    for lbl, c in sorted(agg.items(), key=lambda kv: -sum(kv[1].values())):
        b = sum(c.values())
        print('  %-50s %6.2f MB %5.1f%%  high %4.0f%% med %4.0f%% low %4.0f%%' % (lbl, b / 1e6, 100 * b / tot, 100 * c['high'] / b, 100 * c['medium'] / b, 100 * c['low'] / b))
    unk = sum(sum(c.values()) for l, c in agg.items() if l.startswith('unknown'))
    print('unknown purpose: %.2f%% of code bytes' % (100 * unk / tot))
    if '--md' in sys.argv:   # table for docs/code-map.md
        print('| what it is (tech / plain) | code MB | share | confidence (high / medium / low, by bytes) |')
        print('|---|---:|---:|---|')
        for lbl, c in sorted(agg.items(), key=lambda kv: -sum(kv[1].values())):
            b = sum(c.values())
            top = max(('high', 'medium', 'low', 'none'), key=lambda x: c[x])
            print('| %s | %.2f | %.1f %% | **%s** (%d / %d / %d %%) |' % (lbl, b / 1e6, 100 * b / tot, top, 100 * c['high'] / b,
                                                                     100 * c['medium'] / b, 100 * c['low'] / b))


def why(rva):
    """all raw evidence (classified or not) in the regions.tsv row holding rva"""
    row = None
    for l in open(OUTF, encoding='utf-8'):
        f = l.rstrip('\n').split('\t')
        if f[0].startswith('0x') and int(f[0], 16) <= rva < int(f[1], 16):
            row = f
    print('row:', row)
    a, b = (int(row[0], 16), int(row[1], 16)) if row else (rva - 0x8000, rva + 0x8000)
    sel = set(np.nonzero((FST >= a) & (FST < b))[0].tolist())
    S, srefs = load_strings()
    c = collections.Counter(S[r][:60] for i, r in srefs if i in sel)
    print('strings:', ' | '.join('%s(%d)' % kv for kv in c.most_common(40)))
    print('imports:', collections.Counter(n for i, n in load_imports() if i in sel).most_common(12))
    print('names:', [n for i, lst in load_names().items() if i in sel for n, _, _ in lst][:30])
    E = call_edges()
    ins = np.isin(E[:, 0], list(sel)) & ~np.isin(E[:, 1], list(sel))
    lab = dict(zip(range(NF), [None] * 0))
    rows = [l.rstrip('\n').split('\t') for l in open(OUTF, encoding='utf-8')][1:]
    rs = np.array([int(r[0], 16) for r in rows])
    def lab_of(i):
        return rows[np.searchsorted(rs, FST[i], 'right') - 1][2]
    print('calls out to:', collections.Counter(lab_of(j) for j in E[ins][:, 1]).most_common(6))
    inc = np.isin(E[:, 1], list(sel)) & ~np.isin(E[:, 0], list(sel))
    print('called from:', collections.Counter(lab_of(j) for j in E[inc][:, 0]).most_common(6))


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    if '--survey' in sys.argv:
        survey()
    elif '--why' in sys.argv:
        why(int(sys.argv[sys.argv.index('--why') + 1], 16))
    else:
        main()
