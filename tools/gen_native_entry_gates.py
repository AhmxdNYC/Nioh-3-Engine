"""Generate exact native WaitWarpStart gates from mission scripts and enemy records.
No row allowlist: link the post-wait call graph to a selected Create, a parallel
EndWarpRights of the same id, or the wave ID encoded in that selected enemy's record.
Ambiguous gates are rejected.
"""
import argparse,csv,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'re/scripts_re'));import lfm_disasm as dis
sys.path.insert(0,str(ROOT/'tools'));import rematch_catalog as cat

def strings(script):
    b=script.data;end=script.code_start-4
    needed=max((i.args[0] for i in script.insns if i.op==0x2f),default=-1)+1
    for off in range(script.after_syms,end-2):
        count=struct.unpack_from('<H',b,off)[0]
        if count<needed or count>4096:continue
        cur=off+2;out=[]
        for _ in range(count):
            if cur+3>end:break
            n=struct.unpack_from('<H',b,cur)[0]
            if cur+3+n>end or b[cur+2+n]!=0:break
            try:value=b[cur+2:cur+2+n].decode('utf8')
            except UnicodeError:break
            if '\0' in value:break
            out.append(value);cur+=n+3
        if len(out)==count and cur==end:return out
    return []

def gate_paths(script,placement,wave=0,pool=None):
    names={ident:script.names[n] for n,ident,kind in script.syms}
    def call_name(i):
        # Real scripts: CALL_NATIVE is 1-based table id. Synthetic tests pass ident in args[0].
        nm=dis.insn_name(script,i)
        return nm if nm and not nm.startswith('?') else names.get(i.args[0])
    functions={};current=None
    for ins in script.insns or []:
        if ins.op==0x3b:
            current=call_name(ins);functions[current]=[]
        elif current is not None:functions[current].append(ins)
    pool=strings(script) if pool is None else pool
    def traverse(fn,begin=0,seen=None,collect=False):
        seen=set() if seen is None else seen
        if fn in seen:return []
        seen=seen|{fn};out=[];ins=functions.get(fn,[])
        for k in range(begin,len(ins)):
            i=ins[k]
            if i.op not in (0x1b,0x1f):continue
            name=call_name(i);prev=ins[k-1] if k else None
            literal=prev.args[0] if prev and prev.op==0x29 else None
            if name in ('Character::Create','Character::CreateAndWait') and literal is not None and (collect or literal==placement):
                out.append(dict(kind='create',function=fn,offset=i.off,placement=literal))
            if not collect and wave and name=='WaveBattle::RequestID' and literal==wave:
                out.append(dict(kind='enemy-record-wave',function=fn,offset=i.off,wave=wave,placement=placement))
            target=name if i.op==0x1f else None
            if name=='Script::Create' and prev and prev.op==0x2f and prev.args[0]<len(pool):target=pool[prev.args[0]]
            if target in functions:out.extend(traverse(target,0,seen,collect))
        return out
    def call_gate(i):
        global_k=next((j for j,x in enumerate(script.insns) if x.off==i.off),None)
        args=dis.call_args(script,global_k) if global_k is not None else []
        try:return int(args[0],0) if args else 0
        except ValueError:return 0
    ends={}
    for fn,ins in functions.items():
        for k,i in enumerate(ins):
            if i.op not in (0x1b,0x1f) or call_name(i)!='Function::EndWarpRights':continue
            g=call_gate(i)
            if g:ends.setdefault(g,[]).append((fn,k))
    found=[]
    for fn,ins in functions.items():
        for k,i in enumerate(ins):
            if i.op not in (0x1b,0x1f) or call_name(i)!='Function::WaitWarpStart':continue
            gate=call_gate(i)
            if not gate:continue
            proof=traverse(fn,k+1)
            creates=traverse(fn,k+1,collect=True)
            for efn,ek in ends.get(gate,[]):
                proof.extend(traverse(efn,ek+1))
                creates.extend(traverse(efn,ek+1,collect=True))
            if proof:found.append(dict(gate=gate,wait=i.off,function=fn,proof=proof,creates=creates))
    return found

def _call_name(script, ins):
    names={ident:script.names[n] for n,ident,kind in script.syms}
    nm=dis.insn_name(script,ins)
    return nm if nm and not nm.startswith('?') else names.get(ins.args[0])

def _sethp_thou(fn):
    """Nearest placement int and float before this call, stopping at the previous call.
    Thousandths of a fraction in (0, 1]. A non-literal rate is not invented."""
    place=rate=None
    for ins in reversed(fn):
        if ins.op==0x2d and rate is None:
            rate=struct.unpack('<f', struct.pack('<I', ins.args[0] & 0xFFFFFFFF))[0]
        elif ins.op==0x29 and place is None:
            v=int(ins.args[0])
            if v>=0x80000000: v-=1<<32
            if v>0: place=v
        elif ins.op in (0x1b,0x1f,0x3b):
            break
        if place is not None and rate is not None: break
    if place is None or rate is None or not (0<rate<=1.0): return None
    thou=int(round(rate*1000))
    if thou<1 or thou>1000: return None
    return place, thou

def _invade_box(script, k):
    """First integer of the invade call. A packed array's first dword is the box id.
    A variable argument is not guessed."""
    args=dis.call_args(script, k)
    if not args: return 0
    a=args[0]
    if a[:1]=='[' and a[-1:]==']':
        part=a[1:-1].split(',',1)[0]
        if not part: return 0
        v=int(part,16)
        return v if 0<v<0x80000000 else 0
    if a.lstrip('-').isdigit():
        v=int(a)
        return v if v>0 else 0
    return 0

def staged_chain(script):
    """Invade-box RequestID chain plus the pre-final SetHpRate.
    The pre-final fraction is the last Character::SetHpRate after the previous staged
    RequestID and before the last one. One wave has no pre-final. A SetHpRate after the
    last RequestID is the post-final write and is not used.
    boxes[i] is the last literal invade box in that RequestID's function. 0 if the box
    was not a literal."""
    invade=('Function::Wait_InvadeBox_Online','Function::Wait_InvadeBox_HostOnly')
    waves=[];boxes=[];fn=[];invaded=False;box=0;since=None;pre=None
    for k,ins in enumerate(script.insns or []):
        if ins.op==0x3b:
            fn=[];invaded=False;box=0;continue
        fn.append(ins)
        if ins.op not in (0x1b,0x1f): continue
        name=_call_name(script, ins)
        if name in invade:
            invaded=True
            found=_invade_box(script, k)
            if found: box=found
            continue
        # Script order, not one function: each station is its own invade function, and its
        # SetHpRate is after that station's RequestID. The next station's pre-final is that rate.
        if name=='Character::SetHpRate':
            pair=_sethp_thou(fn[:-1])
            if pair: since=pair
            continue
        if name!='WaveBattle::RequestID' or not invaded: continue
        prev=fn[-2] if len(fn)>1 else None
        if not (prev and prev.op==0x29): continue
        if len(waves)>=1: pre=since
        waves.append(int(prev.args[0]));boxes.append(box);since=None;box=0
    if len(waves)<2: pre=None
    return dict(waves=waves, boxes=boxes, hp_thou=pre[1] if pre else 0, hp_place=pre[0] if pre else 0)

def staged_wave_ids(script):
    """WaveBattle::RequestID literals that follow an invade-box wait in the same function, in script order.
    A RequestID in a function with no invade wait is not a station."""
    return staged_chain(script)['waves']

def sequence_members(paths,enemies):
    # Exact same-mission placement joins only; parameter/variable creations are
    # intentionally excluded, and duplicate enemy records are not guessed.
    joined={}
    for path in paths:
        for proof in path['creates']:
            pid=proof['placement']
            records=[r for r in enemies if len(r)==0x138 and struct.unpack_from('<I',r)[0]==pid]
            if len(records)!=1:continue
            chr=struct.unpack_from('<I',records[0],4)[0]
            if not chr:continue
            member=joined.setdefault(pid,dict(placement=pid,chr=chr,proof=[]))
            if proof not in member['proof']:member['proof'].append(proof)
    return [joined[k] for k in sorted(joined)]

LAST_WAVES={}
def generate(lfm):
    cat.LFM=lfm;b=cat.lfm(cat.IDTAB).read_bytes();ids={}
    for i in range(struct.unpack_from('<I',b,4)[0]):
        e=8+i*8;k,o=struct.unpack_from('<Ii',b,e);ids[k]=b[e+o:b.index(b'\0',e+o)].decode()
    b=cat.lfm(cat.MISSIONDB).read_bytes();missions={}
    for i in range(struct.unpack_from('<I',b,4)[0]):
        r=b[8+i*0x198:8+(i+1)*0x198];missions[struct.unpack_from('<H',r,0x158)[0]]=r
    def path(rec,off,archive):
        name=ids.get(struct.unpack_from('<I',rec,off)[0]);p=lfm/archive/name.lstrip('/') if name else None
        return p if p and p.exists() else None
    with (ROOT/'mod/Nioh3BossRematch/Nioh3BossRematch_bosses.tsv').open() as f:rows=list(csv.DictReader((l for l in f if not l.startswith('#')),delimiter='\t'))
    global LAST_WAVES
    out=[];rejected=[];cache={};waves={}
    for row in rows:
        try:mid=int(row['replay_mission_id'],0);placement=int(row['placement_id'])
        except ValueError:continue
        if mid not in missions or row['route'] not in ('R','H','D'):continue
        rec=missions[mid];p=path(rec,0x50,'archive_09');ep=path(rec,0x54,'archive_03')
        if not p or not ep:continue
        b=ep.read_bytes();enemies=[b[8+i*0x138:8+(i+1)*0x138] for i in range(struct.unpack_from('<I',b,4)[0])]
        selected=[r for r in enemies if len(r)==0x138 and struct.unpack_from('<I',r)[0]==placement]
        if len(selected)!=1:continue
        wave=struct.unpack_from('<H',selected[0],0x1a)[0];wave=0 if wave==65535 else wave
        if p not in cache:cache[p]=dis.load(p)
        found=staged_chain(cache[p])
        wave_ids=found['waves']
        if wave_ids and mid not in waves and all(0<=int(x)<=65535 for x in wave_ids) and len(wave_ids)<=8 and 0<=int(found['hp_thou'])<=1000:
            cents=_box_centers(lfm, rec, ids)
            stands=[]
            for box in found['boxes']:
                xyz=cents.get(int(box)) if box else None
                stands.append((int(box), xyz[0], xyz[1], xyz[2]) if xyz else (int(box), 0, 0, 0))
            found['stands']=stands
            waves[mid]=found
        paths=gate_paths(cache[p],placement,wave)
        gates={x['gate'] for x in paths}
        if len(gates)!=1:
            if paths:rejected.append(dict(id=row['id'],reason='multiple linked gates',paths=paths))
            continue
        out.append(dict(id=row['id'],mission=mid,placement=placement,gate=next(iter(gates)),script=str(p.relative_to(lfm)),enemy_table=str(ep.relative_to(lfm)),enemy_wave=wave,paths=paths,members=sequence_members(paths,enemies)))
    LAST_WAVES=waves
    return out,rejected

def render(rows):
    lines=['// Generated by tools/gen_native_entry_gates.py; no manual mission/boss list.',
           '// A literal WaitWarpStart links through the script call graph to selected Create or enemy-record wave.',
           '#include <stdint.h>',
           'typedef struct { uint32_t mission, placement, gate; } NativeEntryGate;',
           'static const NativeEntryGate g_native_entry_gates[] = {']
    for r in rows:lines.append('    {0x%Xu, %du, %du}, // %s'%(r['mission'],r['placement'],r['gate'],r['id']))
    lines+=['};','static uint32_t NativeEntryGateFind(uint32_t mission, uint32_t placement) {',
            '    for (unsigned i = 0; i < sizeof g_native_entry_gates / sizeof *g_native_entry_gates; ++i)',
            '        if (g_native_entry_gates[i].mission == mission && g_native_entry_gates[i].placement == placement) return g_native_entry_gates[i].gate;',
            '    return 0;','}','']
    return '\n'.join(lines)
def render_sequences(rows):
    lines=['// Generated by tools/gen_native_entry_gates.py; exact postgate Create placement + mission enemy joins.',
           '#include <stdint.h>',
           'typedef struct { uint32_t mission, boss, placement, chr; } NativeEntrySequenceMember;',
           'static const NativeEntrySequenceMember g_native_entry_sequence_members[] = {']
    for r in rows:
        for m in r['members']:
            lines.append('    {0x%Xu, %du, %du, %du}, // %s'%(r['mission'],r['placement'],m['placement'],m['chr'],r['id']))
    return '\n'.join(lines+['};',''])

def _box_centers(lfm, rec, ids):
    """PlacementBox id -> rounded centre. Record stride 0xA0, centre at +0x10. Missing file yields nothing."""
    key=struct.unpack_from('<I', rec, 0x5C)[0]
    name=ids.get(key)
    p=lfm/'archive_03'/name.lstrip('/') if name else None
    if not p or not p.exists(): return {}
    b=p.read_bytes(); n=struct.unpack_from('<I', b, 4)[0]
    if n>100000 or 8+n*0xA0>len(b): return {}
    out={}
    for i in range(n):
        o=8+i*0xA0
        bid=struct.unpack_from('<I', b, o)[0]
        x,y,z=struct.unpack_from('<fff', b, o+0x10)
        out[bid]=(int(round(x)), int(round(y)), int(round(z)))
    return out

def _pad8(vals, zero):
    vals=list(vals)+[zero]*(8-len(vals))
    return ', '.join(str(int(v)) for v in vals)

def render_waves(chains):
    lines=['// Generated by tools/gen_native_entry_gates.py; no row id list.',
           '// A station is a literal WaveBattle::RequestID later in the same function as Wait_InvadeBox_Online or Wait_InvadeBox_HostOnly.',
           '// hp_thou is the last Character::SetHpRate before the last station, in thousandths. One station stores 0.',
           '// box/x/y/z are that station invade box and its PlacementBox centre. 0 means the script did not name a literal box.',
           '#include <stdint.h>',
           'typedef struct { uint32_t mission; uint16_t n, hp_thou; uint32_t hp_place; uint16_t wave[8]; uint32_t box[8]; int32_t x[8], y[8], z[8]; } NativeEntryWaveChain;',
           'static const NativeEntryWaveChain g_native_entry_waves[] = {']
    for mid in sorted(chains):
        row=chains[mid]
        ids=list(row['waves'])
        stands=[tuple(s) for s in (row.get('stands') or [])]
        while len(stands)<len(ids): stands.append((0,0,0,0))
        stands=(stands+[(0,0,0,0)]*8)[:8]
        boxp=', '.join('%uu'%int(s[0]) for s in stands)
        lines.append('    {0x%Xu, %du, %uu, %uu, {%s}, {%s}, {%s}, {%s}, {%s}},'%(
            mid,len(ids),int(row['hp_thou']),int(row['hp_place']),_pad8(ids,0), boxp,
            _pad8([s[1] for s in stands],0), _pad8([s[2] for s in stands],0), _pad8([s[3] for s in stands],0)))
    lines+=['};','static const uint16_t* NativeEntryWaves(uint32_t mission, uint16_t* n) {',
            '    for (unsigned i = 0; i < sizeof g_native_entry_waves / sizeof *g_native_entry_waves; ++i)',
            '        if (g_native_entry_waves[i].mission == mission) {',
            '            if (n) *n = g_native_entry_waves[i].n;',
            '            return g_native_entry_waves[i].wave;',
            '        }',
            '    if (n) *n = 0;',
            '    return 0;','}',
            'static int NativeEntryWaveFinal(uint32_t mission, uint16_t* thou, uint32_t* place) {',
            '    for (unsigned i = 0; i < sizeof g_native_entry_waves / sizeof *g_native_entry_waves; ++i)',
            '        if (g_native_entry_waves[i].mission == mission && g_native_entry_waves[i].n > 1 && g_native_entry_waves[i].hp_thou && g_native_entry_waves[i].hp_place) {',
            '            if (thou) *thou = g_native_entry_waves[i].hp_thou;',
            '            if (place) *place = g_native_entry_waves[i].hp_place;',
            '            return 1;',
            '        }',
            '    if (thou) *thou = 0;',
            '    if (place) *place = 0;',
            '    return 0;','}',
            'static int NativeEntryWaveStand(uint32_t mission, unsigned index, uint32_t* box, int* x, int* y, int* z) {',
            '    for (unsigned i = 0; i < sizeof g_native_entry_waves / sizeof *g_native_entry_waves; ++i) {',
            '        const NativeEntryWaveChain* c = &g_native_entry_waves[i];',
            '        if (c->mission != mission || index >= c->n || !c->box[index] || (!c->x[index] && !c->y[index] && !c->z[index])) continue;',
            '        if (box) *box = c->box[index];',
            '        if (x) *x = c->x[index];',
            '        if (y) *y = c->y[index];',
            '        if (z) *z = c->z[index];',
            '        return 1;',
            '    }',
            '    return 0;','}','']
    return '\n'.join(lines)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--lfm',type=Path,required=True);p.add_argument('--check',action='store_true');a=p.parse_args()
    rows,rejected=generate(a.lfm)
    outputs={'mod/Nioh3Training/src/native_entry_sequences_gen.h':render_sequences(rows),'mod/Nioh3BossRematch/src/native_entry_gates_gen.h':render(rows),'mod/Nioh3BossRematch/src/native_entry_waves_gen.h':render_waves(LAST_WAVES),'notes/native-entry-gates-72263.json':json.dumps(dict(rows=rows,rejected=rejected),indent=2)+'\n'}
    for n,t in outputs.items():
        f=ROOT/n
        if a.check:assert f.read_text()==t,n
        else:f.write_text(t)
    print('Generated',len(rows),'gate rows:',','.join(r['id'] for r in rows));print('Rejected ambiguous:',','.join(r['id'] for r in rejected));print('Staged wave missions',len(LAST_WAVES))
