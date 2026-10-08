"""boss_map_all.py - offline boss per mission (notes/boss-duel-flow.md s.7). Read-only, stdlib.
Mission master (archive_03 52/BD29D455) rec +0x50 = mission script file id, +0x54..+0x68 = 6 placement-set file ids
(+0x54 = enemy table = set slot +0xB0), +0x158 u16 = mission id (= set+0x118). File id -> LFM path through the
id table archive_08 AE/50B70535 (what fills the [0x475A1A0] map). Boss = first enemy record with byte +0x9C & 4.
Usage: py -I re/scripts_re/boss_map_all.py [out.csv]   (needs data/extract/lfm and data/lfm_list.tsv)"""
import struct,os,csv,collections,sys
REPO=r'C:\Users\Ahmad Hamza\nioh3-engine'
L=REPO+r'\data'
LFM=L+r'\extract\lfm'
# 1. LFM id table (archive_08 AE/50B70535...): u32 0, u32 n, n x {u32 fileId, i32 rel->"XX/hash"}
d=open(os.path.join(LFM,'archive_08','AE','50B70535D0EDD19523951019C533295915AE7D4F65FE37E351E09D3858A8C5'),'rb').read()
n=struct.unpack_from('<I',d,4)[0]; E={}
for i in range(n):
    e=8+i*8; k,o=struct.unpack_from('<Ii',d,e); E[k]=d[e+o:d.index(b'\0',e+o)].decode('latin1')
arch={}
for l in open(L+r'\lfm_list.tsv'):
    p=l.split('\t'); arch[p[5].strip('/')]=p[0]
def fpath(k):
    v=E.get(k)
    if v is None or v not in arch: return None
    return os.path.join(LFM,'archive_'+arch[v],*v.split('/'))
def enemy_table(p,rs=0x138):
    t=open(p,'rb').read(); assert t[:4]==b'\x00\x22\x04\x20'; c=struct.unpack_from('<I',t,4)[0]; pool=8+c*rs; out=[]
    for j in range(c):
        r=t[8+j*rs:8+(j+1)*rs]; no=struct.unpack_from('<I',r,0x134)[0]
        out.append((struct.unpack_from('<I',r,0)[0],struct.unpack_from('<I',r,4)[0],r[0x9c],t[pool+no:t.index(b'\0',pool+no)].decode('latin1')))
    return out
md=open(os.path.join(LFM,'archive_03','52','BD29D45533CFACBFA2D6EC80A3B9F40E07CBF9183727358B0B3C3ABE3E4FC2'),'rb').read()
mi={int(r['idx']):r for r in csv.DictReader(open(REPO+r'\notes\missions.csv'))}
prev={r['script']:r for r in csv.DictReader(open(REPO+r'\notes\boss-map-duelflow.csv'))}
rows=[];stat=collections.Counter()
for i in range(struct.unpack_from('<I',md,4)[0]):
    b=8+i*0x198
    ids=struct.unpack_from('<8I',md,b+0x50); key=struct.unpack_from('<H',md,b+0x158)[0]
    sp=fpath(ids[0]); script=E[ids[0]].replace('/','')[2:14] if sp else ''
    ok=all(fpath(x) for x in ids[1:7])
    m=mi.get(i,{})
    row=dict(script=script,mission_id=key,placement_file='',boss_placement_id='',boss_char_id='',boss_name='',
             mission_idx=i,mission_name=m.get('name_en',''),mission_class=m.get('class',''),script_file_id=hex(ids[0]),
             enemy_file_id=hex(ids[1]),boss_flags='',enemy_records=0,boss_candidates=0,prev_duelflow_boss='',agrees_prev='',note='')
    if not sp or not ok:
        row['note']='file ids %s not in LFM id table: set never built (0x1E2E720 check), not shipped'%('|'.join(hex(x) for x in ids if x)) ; stat['unshipped']+=1
    else:
        row['placement_file']='/'+E[ids[1]]
        T=enemy_table(fpath(ids[1])); row['enemy_records']=len(T)
        C=[r for r in T if r[2]&4]; row['boss_candidates']=len(C)
        if C:
            pid,cid,fl,nm=C[0]; row.update(boss_placement_id=pid,boss_char_id=cid,boss_name=nm,boss_flags=hex(fl)); stat['boss']+=1
        else:
            row['note']='no flag-4 record: BossPlacementIdForMission falls back (DataMgr+0x668 scan / 0x3A)'; stat['noboss']+=1
        p=prev.get(script)
        if p:
            row['prev_duelflow_boss']=p['boss_placement_id']+'/'+p['confidence']
            if p['boss_placement_id']: row['agrees_prev']='yes' if p['boss_placement_id']==str(row['boss_placement_id']) else 'NO'
    rows.append(row)
print(stat, collections.Counter(r['agrees_prev'] for r in rows), collections.Counter((r['agrees_prev'],r['prev_duelflow_boss'].split('/')[-1]) for r in rows if r['prev_duelflow_boss']))
print('scripts unique',len({r['script'] for r in rows if r['script']}),'keys unique',len({r['mission_id'] for r in rows}))
out=sys.argv[1] if len(sys.argv)>1 else REPO+r'\notes\boss-map-all.csv'
w=csv.DictWriter(open(out,'w',newline=''),fieldnames=list(rows[0].keys())); w.writeheader()
for r in sorted(rows,key=lambda r:(r['script']=='',r['script'],r['mission_idx'])): w.writerow(r)
