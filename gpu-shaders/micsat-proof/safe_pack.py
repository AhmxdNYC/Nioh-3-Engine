"""Pinned offline single-record MIC pack + restore artifact. Never installs."""
import hashlib,json,pathlib,struct,sys
sys.dont_write_bytecode=True
HERE=pathlib.Path(__file__).resolve().parent;OUT=HERE/'out'/'safe'/'pack'
sys.path[:0]=[str(HERE),str(HERE.parent),str(HERE.parent/'materials')]
import idrk,safe_check
from build_sfb import Group
NAME='0x17ad25bb.file'
GAME=pathlib.Path(r'C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12')
FILE_SHA='1c89c8e765015265b57781f913e9e7eab881790fdc82a3301844837b41337fdf'
ORIGINAL_SHA='3566a0efaaeac7724d0354c6a79bd0007ff15db33629f23b0555a324a80aeffc'
CANDIDATE_SHA='9a64454f4f89d6571c01cbaf38229aca08ec66e2f0a206fc969a77a9e4409c66'
ORIGINAL_DIGEST='e31c389f6d4a38de7b1b784b87c710d2'
CANDIDATE_DIGEST='ed9a06c39e23b641d422411cc4f76144'
def sha(b):return hashlib.sha256(b).hexdigest()

def validate_result(raw,candidate,packed,slot=0):
    assert sha(raw)==FILE_SHA and sha(candidate)==CANDIDATE_SHA
    assert candidate[4:20].hex()==CANDIDATE_DIGEST
    before,after=Group(raw),Group(packed)
    assert sha(before.dxbc(slot))==ORIGINAL_SHA and before.dxbc(slot)[4:20].hex()==ORIGINAL_DIGEST
    assert [i for i in range(len(before.ents)) if before.dxbc(i)[4:20].hex()==ORIGINAL_DIGEST]==[slot]
    assert len(before.ents)==len(after.ents) and after.dxbc(slot)==candidate
    assert [i for i in range(len(before.ents)) if before.rec(i)!=after.rec(i)]==[slot]
    a,b=before.rec(slot),after.rec(slot)
    assert a[:8]==b[:8]==b'CXDK2000'
    oldlen,oldtail=struct.unpack_from('<II',a,8);newlen,newtail=struct.unpack_from('<II',b,8)
    assert oldtail==newtail
    assert a[16+oldlen:16+oldlen+oldtail]==b[16+newlen:16+newlen+newtail]
    assert not any(a[16+oldlen+oldtail:]) and not any(b[16+newlen+newtail:])
    assert before.p[before.base+before.blobs:]==after.p[after.base+after.blobs:]
    assert before.base==after.base and before.tab==after.tab
    x,y=bytearray(before.p[:before.base]),bytearray(after.p[:after.base])
    for start,end in ((8,12),(0xb4,0xb8),(before.tab,before.tab+8*len(before.ents))):
        x[start:end]=bytes(end-start);y[start:end]=bytes(end-start)
    assert x==y and idrk.header(raw)[4:]==idrk.header(packed)[4:]
    assert idrk.header(packed)[1]==len(packed) and idrk.header(packed)[2]==len(packed)-0x38
    # Inverse replacement must restore the complete decompressed original,
    # including metadata, every other record, and padding.
    assert after.replace({slot:before.dxbc(slot)})==before.p
    return dict(record_count=len(before.ents),slot=slot,original_payload_record_offset=before.base+before.ents[slot][0],
                original_payload_dxbc_offset=before.base+before.ents[slot][0]+16,original_record_sha256=sha(a),
                candidate_record_sha256=sha(b),auxiliary_tail_bytes=oldtail,
                records=[dict(slot=i,original_sha256=sha(before.rec(i)),candidate_sha256=sha(after.rec(i))) for i in range(len(before.ents))],
                payload_sha256=sha(after.p),candidate_file_sha256=sha(packed),candidate_file_bytes=len(packed))

def prove(raw,candidate,slot=0):
    assert sha(raw)==FILE_SHA and sha(candidate)==CANDIDATE_SHA
    before=Group(raw)
    assert sha(before.dxbc(slot))==ORIGINAL_SHA
    packed=idrk.pack(before.replace({slot:candidate}),raw)
    return packed,validate_result(raw,candidate,packed,slot)

def main():
    safe_check.validate()
    raw=(GAME/NAME).read_bytes();candidate=(HERE/'out/safe/candidate.dxbc').read_bytes()
    packed,receipt=prove(raw,candidate);checks=[]
    def rejected(name,fn):
        try:fn()
        except (AssertionError,ValueError,IndexError,struct.error):checks.append(name)
        else:raise AssertionError('accepted negative: '+name)
    rejected('wrong_slot',lambda:prove(raw,candidate,1))
    changed=bytearray(raw);changed[-1]^=1
    rejected('original_full_file_changed',lambda:prove(bytes(changed),candidate))
    changed_candidate=bytearray(candidate);changed_candidate[-1]^=1
    rejected('candidate_changed',lambda:prove(raw,bytes(changed_candidate)))
    after=Group(packed)
    changed_payload=bytearray(after.p);changed_payload[16]^=1
    rejected('unrelated_header_changed',lambda:validate_result(raw,candidate,idrk.pack(bytes(changed_payload),raw)))
    assert len(after.ents)>1
    changed_payload=bytearray(after.p);offset,size=after.ents[1];changed_payload[after.base+offset+size-1]^=1
    rejected('other_record_changed',lambda:validate_result(raw,candidate,idrk.pack(bytes(changed_payload),raw)))
    rec=after.rec(0);n,tail=struct.unpack_from('<II',rec,8)
    assert tail>0
    changed_payload=bytearray(after.p);changed_payload[after.base+after.ents[0][0]+16+n]^=1
    rejected('target_auxiliary_tail_changed',lambda:validate_result(raw,candidate,idrk.pack(bytes(changed_payload),raw)))
    assert (GAME/NAME).read_bytes()==raw and (HERE/'out/safe/candidate.dxbc').read_bytes()==candidate
    OUT.mkdir(exist_ok=True)
    for label,content in [('original',raw),('candidate',packed)]:
        directory=OUT/label;directory.mkdir(exist_ok=True);(directory/NAME).write_bytes(content)
        assert (directory/NAME).read_bytes()==content
    assert (GAME/NAME).read_bytes()==raw
    receipt.update(status='OFFLINE_MIC_SAFE_PACK_PASS',original_file_sha256=FILE_SHA,
                   original_dxbc_sha256=ORIGINAL_SHA,candidate_dxbc_sha256=CANDIDATE_SHA,
                   original_digest=ORIGINAL_DIGEST,candidate_digest=CANDIDATE_DIGEST,
                   negative_checks=checks,game_file_unchanged=True,inverse_payload_round_trip=True,
                   original_artifact=str(OUT/'original'/NAME),candidate_artifact=str(OUT/'candidate'/NAME),
                   harness_sha256=sha(pathlib.Path(__file__).read_bytes()),
                   idrk_sha256=sha(pathlib.Path(idrk.__file__).read_bytes()))
    (OUT/'receipt.json').write_text(json.dumps(receipt,indent=2))
    print(json.dumps({k:v for k,v in receipt.items() if k!='records'}))
if __name__=='__main__':main()
