"""Generate exact native WaitWarpStart gates from mission scripts and enemy records.
No row allowlist: link the post-wait call graph to a selected Create, or to the
wave ID encoded in that selected enemy's record. Ambiguous gates are rejected.
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
    functions={};current=None
    for ins in script.insns or []:
        if ins.op==0x3b:current=names.get(ins.args[0]);functions[current]=[]
        elif current is not None:functions[current].append(ins)
    pool=strings(script) if pool is None else pool
    def traverse(fn,begin=0,seen=None,collect=False):
        seen=set() if seen is None else seen
        if fn in seen:return []
        seen=seen|{fn};out=[];ins=functions.get(fn,[])
        for k in range(begin,len(ins)):
            i=ins[k]
            if i.op not in (0x1b,0x1f):continue
            name=names.get(i.args[0]);prev=ins[k-1] if k else None
            literal=prev.args[0] if prev and prev.op==0x29 else None
            if name in ('Character::Create','Character::CreateAndWait') and literal is not None and (collect or literal==placement):
                out.append(dict(kind='create',function=fn,offset=i.off,placement=literal))
            if not collect and wave and name=='WaveBattle::RequestID' and literal==wave:
                out.append(dict(kind='enemy-record-wave',function=fn,offset=i.off,wave=wave,placement=placement))
            target=name if i.op==0x1f else None
            if name=='Script::Create' and prev and prev.op==0x2f and prev.args[0]<len(pool):target=pool[prev.args[0]]
            if target in functions:out.extend(traverse(target,0,seen,collect))
        return out
    found=[]
    for fn,ins in functions.items():
        for k,i in enumerate(ins):
            if i.op not in (0x1b,0x1f) or names.get(i.args[0])!='Function::WaitWarpStart':continue
            prev=ins[k-1] if k else None
            if not prev or prev.op!=0x29 or not prev.args[0]:continue
            proof=traverse(fn,k+1)
            if proof:found.append(dict(gate=prev.args[0],wait=i.off,function=fn,proof=proof,creates=traverse(fn,k+1,collect=True)))
    return found

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
    out=[];rejected=[];cache={}
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
        paths=gate_paths(cache[p],placement,wave)
        gates={x['gate'] for x in paths}
        if len(gates)!=1:
            if paths:rejected.append(dict(id=row['id'],reason='multiple linked gates',paths=paths))
            continue
        out.append(dict(id=row['id'],mission=mid,placement=placement,gate=next(iter(gates)),script=str(p.relative_to(lfm)),enemy_table=str(ep.relative_to(lfm)),enemy_wave=wave,paths=paths,members=sequence_members(paths,enemies)))
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

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--lfm',type=Path,required=True);p.add_argument('--check',action='store_true');a=p.parse_args()
    rows,rejected=generate(a.lfm)
    outputs={'mod/Nioh3Training/src/native_entry_sequences_gen.h':render_sequences(rows),'mod/Nioh3BossRematch/src/native_entry_gates_gen.h':render(rows),'notes/native-entry-gates-72263.json':json.dumps(dict(rows=rows,rejected=rejected),indent=2)+'\n'}
    for n,t in outputs.items():
        f=ROOT/n
        if a.check:assert f.read_text()==t,n
        else:f.write_text(t)
    print('Generated',len(rows),'gate rows:',','.join(r['id'] for r in rows));print('Rejected ambiguous:',','.join(r['id'] for r in rejected))
