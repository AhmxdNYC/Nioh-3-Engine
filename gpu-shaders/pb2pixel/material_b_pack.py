"""Private PS05ad one-record pack and exact captured native inputs; never runs GPU."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
sys.dont_write_bytecode=True
PS='05ad17151de4f513fb82c7428874a7f9';VS='70d0ee9feee590f9c48c40b808858e27'
PS_SHA='4173c9dce9ad919b5238426f345de1d1d648b1c47efeaf1ea2c59e01e558dd4b'
VS_SHA='83d64677954ab9c2368b032ea022659353a701bf6a1daf07c37eeaf93a6d55bf'
CAND='efb7895ca386d3e0b43f4d6b61aa20195cf0c0d50c79efc4baedab313b56444c'
FILE='2fd6d3c82b14be2b5cc47e76dd36f0c65a1af394833f2b17737ddfb2fab89326'
JOURNAL='f1845f5f3d61f7f7d59538cb187fef57df433f6e3f3fced9c852b75cec97c40d'
def sha(b):return hashlib.sha256(b).hexdigest()


def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--candidate',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    sys.path[:0]=[str(a.repo/'gpu-shaders/pb2guard'),str(a.repo/'gpu-shaders/materials'),str(a.repo/'gpu-shaders')]
    import pack_check as pack,pipeline_extract as E,dxcom,idrk
    from build_sfb import Group
    game=Path('C:/Program Files (x86)/Steam/steamapps/common/Nioh3')
    gamefile=game/'shader_dx12/0xbc09ad4a.file';raw=gamefile.read_bytes();assert sha(raw)==FILE
    cand=a.candidate.read_bytes();assert sha(cand)==CAND and dxcom.validate_sign(cand)==cand
    g=Group(raw);original=g.dxbc(1);assert sha(original)==PS_SHA and original[4:20].hex()==PS
    op,cp=dxcom.parts(original),dxcom.parts(cand)
    assert all(op.get(k)==cp.get(k) for k in ('ISG1','OSG1','PSV0','SFI0'))
    pack.ORIGINAL,pack.ORIGINAL_SHA=PS,PS_SHA;pack.CANDIDATE,pack.CANDIDATE_SHA=cand[4:20].hex(),CAND
    proof=pack.prove(raw,cand,1);packed=idrk.pack(g.replace({1:cand}),raw);new=Group(packed)
    assert sha(packed)==proof['hypothetical_packed_sha256']
    assert new.replace({1:original})==g.p
    # Independent explicit record/tail witnesses in addition to pack.prove.
    witnesses=[]
    for i in range(len(g.ents)):
        old,now=g.rec(i),new.rec(i)
        if i!=1:assert old==now
        if old[:8]==b'CXDK2000':
            size,n=struct.unpack_from('<II',old,8);size2,n2=struct.unpack_from('<II',now,8)
            assert n==n2 and old[16+size:16+size+n]==now[16+size2:16+size2+n2]
        witnesses.append(dict(slot=i,old_record_sha256=sha(old),new_record_sha256=sha(now)))
    for wrong,slot in ((cand,0),(cand[:-1]+bytes([cand[-1]^1]),1)):
        try:pack.prove(raw,wrong,slot)
        except AssertionError:pass
        else:raise AssertionError('pack negative escaped')
    journal=game/'Nioh3EngineFix_pj/pj_win_dgpu_sm68.bin';data=journal.read_bytes();assert sha(data)==JOURNAL
    roots={};shaders={};selected=[];pos=48
    assert data[:4]==b'N3PJ'
    while pos<len(data):
        assert pos+16<=len(data)
        tag,n,checksum=struct.unpack_from('<4sIQ',data,pos);assert 0<n<=16*1024*1024 and pos+16+n<=len(data)
        r=data[pos+16:pos+16+n]
        if tag in (b'RSIG',b'SHDR'):
            table=roots if tag==b'RSIG' else shaders;k=r[:16].hex()
            assert k not in table or table[k]==(r,checksum)
            table[k]=(r,checksum)
        elif tag==b'PSO1':
            kind,ver,ns,il,fl,ro,z=struct.unpack_from('<BBBBIII',r);assert 32+fl+24*ns<=len(r)
            refs={struct.unpack_from('<I',r,32+fl+24*i)[0]:r[40+fl+24*i:56+fl+24*i].hex() for i in range(ns)}
            if refs.get(8)==VS and refs.get(24)==PS:
                assert (kind,ver,ns,il,fl,ro,len(r))==(1,1,2,0,656,0,736)
                assert E.key(r)[:8]==struct.pack('<Q',checksum)
                selected.append((pos,r))
        pos+=16+n
    assert [o for o,_ in selected]==[16116818,16190406,16195298,16198120]
    a.out.mkdir(parents=True,exist_ok=False);native=a.out/'native';native.mkdir()
    for digest,name,expected in ((VS,'original.dxbc',VS_SHA),(PS,'material.dxbc',PS_SHA)):
        r,c=shaders[digest];assert E.key(r)[:8]==struct.pack('<Q',c)
        assert r[16:20]==b'DXBC' and r[20:36].hex()==digest and sha(r[16:])==expected
        (native/name).write_bytes(r[16:])
    records=[]
    for i,(offset,r) in enumerate(selected):
        root,c=roots[r[16:32].hex()];assert E.key(root)[:8]==struct.pack('<Q',c) and E.key(root[24:])==root[:16]
        assert struct.unpack_from('<I',root,16)[0]==0
        d=r[32:688];(native/f'desc{i}.bin').write_bytes(d);(native/f'root{i}.bin').write_bytes(root[24:])
        records.append(dict(index=i,journal_offset=offset,record_sha256=sha(r),root_sha256=sha(root[24:]),
            descriptor_sha256=sha(d),root_key=root[:16].hex(),rt_formats=list(struct.unpack_from('<8I',d,580))[:struct.unpack_from('<I',d,576)[0]],
            dsv=struct.unpack_from('<I',d,612)[0],samples=list(struct.unpack_from('<II',d,616))))
    (native/'candidate.dxbc').write_bytes(cand)
    src=(a.repo/'gpu-shaders/pb2guard/pipeline_admit.cpp').read_bytes()
    assert sha(src)=='eb23ec8e978e81f2aa51bfd3e369db454acccc9586e50e67fd83a262c07db7a1'
    code=src.decode()
    changes=[('d.pRootSignature=rs;d.PS={ps.data(),ps.size()};','d.pRootSignature=rs;d.VS={vs.data(),vs.size()};'),
        ('auto& blob=which?cand:vs;d.VS={blob.data(),blob.size()};','auto& blob=which?cand:ps;d.PS={blob.data(),blob.size()};'),
        ('for(int i=0;i<8;++i)','for(int i=0;i<4;++i)')]
    for before,after in changes:assert code.count(before)==1;code=code.replace(before,after)
    (native/'pixel_admit.cpp').write_bytes(code.encode())
    (a.out/'candidate.file').write_bytes(packed);(a.out/'original.file').write_bytes(raw)
    result=dict(scope='PRIVATE_PACK_AND_CAPTURED_INPUTS_ONLY',target='shader_dx12/0xbc09ad4a.file',slot=1,
        before_sha256=FILE,after_sha256=sha(packed),candidate_shader_sha256=CAND,candidate_digest=cand[4:20].hex(),
        original_ps_sha256=PS_SHA,original_vs_sha256=VS_SHA,journal_sha256=JOURNAL,records=records,pack_proof=proof,
        independent_record_witnesses=witnesses,original_payload_roundtrip=True,wrong_slot_and_corrupt_candidate_rejected=True,
        helper_source_origin_sha256=sha(src),native_gate='UNRUN',draw_gate='UNRUN',image_gate='UNRUN',performance_gate='UNRUN',
        files={str(f.relative_to(a.out)).replace('\\','/'):sha(f.read_bytes()) for f in sorted(a.out.rglob('*')) if f.is_file()})
    assert gamefile.read_bytes()==raw and journal.read_bytes()==data and a.candidate.read_bytes()==cand
    (a.out/'binding.json').write_text(json.dumps(result,indent=2));print(json.dumps({k:result[k] for k in ('scope','after_sha256','candidate_digest','records')},indent=2))


if __name__=='__main__':main()
