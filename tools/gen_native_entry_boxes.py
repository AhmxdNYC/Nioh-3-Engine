"""Recover direct entry-box -> GatherWarp -> selected Create paths, with no row list.
Only unique, literal, same-function paths without a door wait are emitted.
The runtime moves to the box; the native script retains GatherWarp and temp guards.
"""
import argparse, csv, json, math, struct, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'re/scripts_re'))
import lfm_disasm as dis
sys.path.insert(0,str(ROOT/'tools'))
import rematch_catalog as cat

def paths(script, placement, wave_id=0):
    names={ident:script.names[n] for n,ident,kind in script.syms}
    result=[];wait=None;gather=None;door=False;fn='';wait_kind=''
    ins=script.insns or []
    for k,i in enumerate(ins):
        if i.op==0x3b:
            wait=gather=None;door=False;fn=names.get(i.args[0],'?')
        if i.op not in (0x1b,0x1f):continue
        name=names.get(i.args[0],'?')
        prev=ins[k-1] if k else None
        if name=='Function::WaitWarpStart':door=True
        if name in ('Function::Wait_InvadeBox_Online','Function::Wait_InvadeBox_HostOnly') and prev and prev.op==0x2c and prev.args[0]==4:
            wait=(prev.args[1],i.off);gather=None;wait_kind=name;wait_boxes=[v for v in prev.args[1:4] if v]
        if name=='Mission::GatherWarp' and wait and k>=2 and prev.op==0x2e and prev.args[0]==4:
            bid=ins[k-2]
            if bid.op==0x29 and (bid.args[0]==wait[0] or wait_kind=='Function::Wait_InvadeBox_HostOnly'):
                gather=(struct.unpack('<4f',struct.pack('<4I',*prev.args[1:])),i.off)
        if name in ('Character::Create','Character::CreateAndWait') and prev and prev.op==0x29 and prev.args[0]==placement:
            if wait and gather and not door:result.append(dict(box=wait[0],wait=wait[1],gather=gather[1],pose=gather[0],create=i.off,function=fn))
        if name=='WaveBattle::RequestID' and wave_id and prev and prev.op==0x29 and prev.args[0]==wave_id:
            if wait and gather and not door and wait_kind=='Function::Wait_InvadeBox_HostOnly':
                result.append(dict(box=wait[0],wait=wait[1],gather=gather[1],pose=gather[0],create=i.off,function=fn,wave=wave_id,wait_boxes=wait_boxes,kind='host-box-wave'))
    return result

def requested_wave_group(script, k, names=None):
    """Group id of RequestID or RequestIDWithWhoWarpSetting. The warp form is
    RequestIDWithWhoWarpSetting(group, true, true, true, true): the group push
    sits behind the four bools, not in the slot immediately before the call."""
    if names is None:
        names={ident:script.names[n] for n,ident,kind in script.syms}
    ins=script.insns or []
    i=ins[k]
    if i.op!=0x1b or names.get(i.args[0]) not in ('WaveBattle::RequestID','WaveBattle::RequestIDWithWhoWarpSetting'):
        return None
    j=k-1
    while j>=0 and ins[j].op in (0x15,0x16):
        j-=1
    if j>=0 and ins[j].op==0x29:
        return ins[j].args[0]
    return None

def wave_members(script, enemy_data, placement):
    names={ident:script.names[n] for n,ident,kind in script.syms}
    requested=set()
    for k in range(len(script.insns or [])):
        group=requested_wave_group(script, k, names)
        if group is not None:
            requested.add(group)
    n=struct.unpack_from('<I',enemy_data,4)[0]
    if 8+n*0x138>len(enemy_data):return []
    records=[enemy_data[8+j*0x138:8+(j+1)*0x138] for j in range(n)]
    selected=[r for r in records if struct.unpack_from('<I',r)[0]==placement]
    if len(selected)!=1:return []
    wave=struct.unpack_from('<H',selected[0],0x1a)[0]
    if not wave or wave not in requested:return []
    return [dict(placement=struct.unpack_from('<I',r)[0],chr=struct.unpack_from('<I',r,4)[0],wave=wave,ordinal=struct.unpack_from('<I',r,0x1c)[0]) for r in records if struct.unpack_from('<H',r,0x1a)[0]==wave]

def inside_xz(q,x,z):
    cx,cy,cz,_,yaw=struct.unpack_from('<5f',q,0x10);ex,ey,ez=struct.unpack_from('<3f',q,0x28)
    angle=math.radians(yaw);dx=x-cx;dz=z-cz
    return abs(dx*math.cos(angle)-dz*math.sin(angle))<ex/2 and abs(dx*math.sin(angle)+dz*math.cos(angle))<ez/2

def generate(lfm):
    cat.LFM=lfm
    b=cat.lfm(cat.IDTAB).read_bytes(); ids={}
    for i in range(struct.unpack_from('<I',b,4)[0]):
        e=8+i*8;k,o=struct.unpack_from('<Ii',b,e);ids[k]=b[e+o:b.index(b'\0',e+o)].decode()
    def table(rec,off,archive):
        name=ids.get(struct.unpack_from('<I',rec,off)[0])
        if not name:return None
        p=lfm/archive/name.lstrip('/')
        return p if p.exists() else None
    b=cat.lfm(cat.MISSIONDB).read_bytes();missions={}
    for i in range(struct.unpack_from('<I',b,4)[0]):
        r=b[8+i*0x198:8+(i+1)*0x198];missions[struct.unpack_from('<H',r,0x158)[0]]=r
    result=[];waves=[]
    with (ROOT/'mod/Nioh3BossRematch/Nioh3BossRematch_bosses.tsv').open() as f:
        catalogue=list(csv.DictReader((x for x in f if not x.startswith('#')),delimiter='\t'))
    for r in catalogue:
        try:mid=int(r['replay_mission_id'],0);placement=int(r['placement_id'])
        except ValueError:continue
        if mid not in missions or r['route'] not in ('R','H','D'):continue
        rec=missions[mid];p=table(rec,0x50,'archive_09');bp=table(rec,0x5c,'archive_03');ep=table(rec,0x54,'archive_03')
        if not p or not bp:continue
        script=dis.load(p);members=wave_members(script,ep.read_bytes(),placement) if ep else []
        if members:waves.append(dict(id=r['id'],mission=mid,script=str(p.relative_to(lfm)),enemy_table=str(ep.relative_to(lfm)),members=members))
        matches=paths(script,placement,members[0]['wave'] if members else 0)
        if len(matches)!=1:continue
        m=matches[0];t=bp.read_bytes();boxes=[]
        for j in range(struct.unpack_from('<I',t,4)[0]):
            q=t[8+j*0xa0:8+(j+1)*0xa0]
            if struct.unpack_from('<I',q)[0] in m.get('wait_boxes',[m['box']]):boxes.append(q)
        if m.get('kind')=='host-box-wave':
            boxes=[q for q in boxes if inside_xz(q,m['pose'][0],m['pose'][2])]
            boxes.sort(key=lambda q:m['wait_boxes'].index(struct.unpack_from('<I',q)[0]))
            if not boxes:continue
            boxes=boxes[:1];m['box']=struct.unpack_from('<I',boxes[0])[0]
        if len(boxes)!=1:continue
        q=boxes[0];x,y,z,_,yaw=struct.unpack_from('<5f',q,0x10);ex,ey,ez=struct.unpack_from('<3f',q,0x28)
        floor=m['pose'][1]+50
        if not (ex>100 and ez>100 and y-ey/2<floor<y+ey/2):continue
        if m.get('kind')=='host-box-wave':x,z=m['pose'][0],m['pose'][2]
        result.append(dict(id=r['id'],mission=mid,placement=placement,script=str(p.relative_to(lfm)),box_table=str(bp.relative_to(lfm)),target=[x,floor,z,m['pose'][3]],**m))
    return result,waves

def render(rows,waves=()):
    s=['// Generated by tools/gen_native_entry_boxes.py from original mission script/box records.',
       '// Observation/movement only: no temp, story or clear writes.',
       'typedef struct { const char* id; uint32_t placement, box; float target[4]; } NativeEntryBox;',
       'static const NativeEntryBox g_native_entry_boxes[] = {']
    for r in rows:s.append('    {"%s", %du, %du, {%s}},'%(r['id'],r['placement'],r['box'],', '.join('%.6ff'%v for v in r['target'])))
    s += ['};','typedef struct { const char* id; uint32_t placement, chr, wave; } NativeEntryWaveMember;',
          'static const NativeEntryWaveMember g_native_entry_wave_members[] = {']
    for r in waves:
        for m in r['members']:s.append('    {"%s", %du, %du, %du},' % (r['id'],m['placement'],m['chr'],m['wave']))
    return '\n'.join(s+['};',''])
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--lfm',type=Path,required=True);p.add_argument('--check',action='store_true');a=p.parse_args()
    rows,waves=generate(a.lfm)
    outputs={'mod/Nioh3Training/src/native_entry_boxes_gen.h':render(rows,waves),'notes/native-entry-boxes-72188.json':json.dumps(rows,indent=2)+'\n','notes/native-entry-waves-72263.json':json.dumps(waves,indent=2)+'\n'}
    for n,t in outputs.items():
        path=ROOT/n
        if a.check:assert path.read_text()==t,n
        else:path.write_text(t)
    print('Direct box candidates:',', '.join(r['id'] for r in rows))
