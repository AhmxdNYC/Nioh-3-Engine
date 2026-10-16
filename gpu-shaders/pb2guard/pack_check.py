"""Offline PB2 single-record pack proof. Never writes packed/game shader files."""
import hashlib,json,pathlib,struct,sys
sys.dont_write_bytecode=True
HERE=pathlib.Path(__file__).resolve().parent
sys.path[:0]=[str(HERE.parent),str(HERE.parent/'materials')]
import idrk
from build_sfb import Group
ORIGINAL='1559714ca496fffe6ddd3c827e1d962e'
CANDIDATE='11b0311eb9ff27c2db64bd7647d2de5e'
ORIGINAL_SHA='5a7d10634d334e35ed62a5706fa784c905793f8d8a20a60539be9abd0ea970cc'
CANDIDATE_SHA='0d44ddcd220d87d17d2e38bfff2533550fb2e72595dee71f5037d061466985be'
GAME=pathlib.Path(r'C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12')
def sha(b):return hashlib.sha256(b).hexdigest()

def prove(raw,candidate,slot=10):
    assert sha(candidate)==CANDIDATE_SHA and candidate[4:20].hex()==CANDIDATE
    group=Group(raw)
    assert group.dxbc(slot)[4:20].hex()==ORIGINAL
    assert sha(group.dxbc(slot))==ORIGINAL_SHA
    assert [k for k in range(len(group.ents)) if group.dxbc(k)[4:20].hex()==ORIGINAL]==[slot]
    payload=group.replace({slot:candidate})
    packed=idrk.pack(payload,raw);result=Group(packed)
    assert result.p==payload and len(result.ents)==len(group.ents)
    assert result.dxbc(slot)==candidate
    changed=[k for k in range(len(group.ents)) if result.rec(k)!=group.rec(k)]
    assert changed==[slot]
    # The rewritten CXDK record retains its complete auxiliary tail.
    oldrec,newrec=group.rec(slot),result.rec(slot)
    if oldrec[:8]==b'CXDK2000':
        osize,otail=struct.unpack_from('<II',oldrec,8)
        nsize,ntail=struct.unpack_from('<II',newrec,8)
        assert otail==ntail
        assert oldrec[16+osize:16+osize+otail]==newrec[16+nsize:16+nsize+ntail]
    # Only payload sizes and table offset/size entries may change in header.
    a,b=bytearray(group.p[:group.base]),bytearray(result.p[:result.base])
    assert group.base==result.base
    for start,end in ((8,12),(0xb4,0xb8),(group.tab,group.tab+8*len(group.ents))):
        a[start:end]=bytes(end-start);b[start:end]=bytes(end-start)
    assert a==b
    assert idrk.header(raw)[4:]==idrk.header(packed)[4:]
    records=[dict(slot=k,original_sha256=sha(group.rec(k)),result_sha256=sha(result.rec(k))) for k in range(len(group.ents))]
    return dict(original_file_sha256=sha(raw),hypothetical_packed_sha256=sha(packed),
                original_file_bytes=len(raw),hypothetical_packed_bytes=len(packed),
                payload_sha256=sha(payload),slot=slot,records=records,changed_records=changed,
                auxiliary_tail_and_unrelated_records_unchanged=True)

def main():
    candidate=(HERE/'out/multi.dxbc').read_bytes()
    files={};snapshots={}
    for name in ('0xa79af848.file','0xf00d5c8c.file'):
        raw=(GAME/name).read_bytes();snapshots[name]=raw
        files[name]=prove(raw,candidate)
        try:prove(raw,candidate,9)
        except AssertionError:pass
        else:raise AssertionError('wrong slot accepted')
    corrupt=bytearray(candidate);corrupt[-1]^=1
    try:prove(next(iter(snapshots.values())),bytes(corrupt))
    except AssertionError:pass
    else:raise AssertionError('changed candidate accepted')
    ps={}
    for name in ('0x46f41041.file','0xb4751b0f.file','0xdf8c9879.file'):
        raw=(GAME/name).read_bytes();snapshots[name]=raw;group=Group(raw)
        assert group.dxbc(1)[4:20].hex()=='7dcfe3de3f3fcbc199be405eae1596b4'
        ps[name]=dict(file_sha256=sha(raw),record=1,record_sha256=sha(group.rec(1)))
    for name,raw in snapshots.items():assert (GAME/name).read_bytes()==raw
    assert (HERE/'out/multi.dxbc').read_bytes()==candidate
    receipt=dict(status='OFFLINE_SINGLE_RECORD_PACK_BINDING_PASS',source_original_sha256=ORIGINAL_SHA,
                 candidate_sha256=CANDIDATE_SHA,files=files,associated_pixel_shaders=ps,
                 wrong_slot_and_corrupt_candidate_refused=True,game_files_unchanged=True,
                 candidate_unchanged=True,packed_files_written=False,
                 note='Two independent alternatives, not permission to install both; actual draw permutation unresolved.')
    (HERE/'out/pack-binding.json').write_text(json.dumps(receipt,indent=2))
    print(receipt['status'])
    for name,row in files.items():print(name,'slot',row['slot'],'records',len(row['records']),row['hypothetical_packed_sha256'])
if __name__=='__main__':main()
