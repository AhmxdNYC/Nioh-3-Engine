"""Extract exact recorded PB2 graphics pipelines. Read-only game access; no device.

Outputs private inputs for pipeline_admit.cpp, never installation artifacts.
"""
import hashlib, json, pathlib, struct, sys

HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE / 'out' / 'pipeline'
VS = '1559714ca496fffe6ddd3c827e1d962e'
PS = '7dcfe3de3f3fcbc199be405eae1596b4'
MASK = (1 << 64) - 1
def rot(x, n):
    x &= MASK
    return ((x << n) | (x >> (64-n))) & MASK
def mix(x):
    x &= MASK; x ^= x >> 33; x = x * 0xff51afd7ed558ccd & MASK
    x ^= x >> 33; x = x * 0xc4ceb9fe1a85ec53 & MASK
    return x ^ (x >> 33)
def key(b):
    # Exact psojournal.cpp Hash128; all C++ uint64 operations wrap.
    a = 0x9e3779b97f4a7c15 ^ len(b); c = 0xc2b2ae3d27d4eb4f + len(b)
    i = 0
    while i + 8 <= len(b):
        w = int.from_bytes(b[i:i+8], 'little'); i += 8
        a ^= rot(w * 0x87c37b91114253d5,31) * 0x4cf5ad432745937f & MASK
        a = (rot(a,27) * 5 + 0x52dce729) & MASK
        c = (rot(c + (w ^ a),33) * 0x9e3779b97f4a7c15 + 0x38495ab5) & MASK
    t = int.from_bytes(b[i:], 'little')
    a ^= rot(t * 0x87c37b91114253d5,31) * 0x4cf5ad432745937f & MASK
    c = (c + (t ^ a)) & MASK
    a = mix(a+c); c = mix(c+a)
    return struct.pack('<QQ',a,c^a)
def sha(b): return hashlib.sha256(b).hexdigest()
def main():
    source = pathlib.Path(sys.argv[1])
    data = source.read_bytes()
    assert data[:4] == b'N3PJ' and struct.unpack_from('<I',data,4)[0] in (1,2)
    chunks=[]; p=48
    while p < len(data):
        assert p+16 <= len(data), 'partial chunk header'
        tag,n,checksum=struct.unpack_from('<4sIQ',data,p)
        assert 0<n<=16*1024*1024 and p+16+n<=len(data), 'partial/invalid chunk'
        chunks.append((p,tag,checksum,data[p+16:p+16+n])); p+=16+n
    shaders={}; roots={}; selected=[]
    for off,tag,checksum,r in chunks:
        if tag in (b'SHDR',b'RSIG'):
            table=shaders if tag==b'SHDR' else roots
            digest=r[:16].hex()
            assert digest not in table or table[digest]==(r,checksum), 'conflicting duplicate dependency'
            table[digest]=(r,checksum)
        elif tag == b'PSO1':
            kind,ver,ns,il,fl,ro,z=struct.unpack_from('<BBBBIII',r)
            assert 32+fl+24*ns<=len(r)
            refs={struct.unpack_from('<I',r,32+fl+24*i)[0]:r[40+fl+24*i:56+fl+24*i].hex() for i in range(ns)}
            if refs.get(8)==VS and refs.get(24)==PS:
                assert kind==ver==1 and il==0 and fl==656 and ro==0 and ns==2 and len(r)==736
                assert key(r)[:8]==struct.pack('<Q',checksum), 'PSO checksum'
                selected.append((off,r,refs))
    assert selected, 'exact pair missing'
    OUT.mkdir(parents=True,exist_ok=True)
    receipt={'journal':str(source),'journal_sha256':sha(data),'header_hex':data[:48].hex(),
             'scope':'Exact selected records/dependencies checksum verified; unrelated chunk checksums not validated', 'records':[]}
    for digest,name in ((VS,'original.dxbc'),(PS,'material.dxbc')):
        r,checksum=shaders[digest]
        assert key(r)[:8]==struct.pack('<Q',checksum) and r[16:20]==b'DXBC' and r[20:36].hex()==digest
        (OUT/name).write_bytes(r[16:]); receipt[name]=sha(r[16:])
    assert receipt['original.dxbc']=='5a7d10634d334e35ed62a5706fa784c905793f8d8a20a60539be9abd0ea970cc'
    candidate=(HERE/'out/multi.dxbc').read_bytes()
    assert sha(candidate)=='0d44ddcd220d87d17d2e38bfff2533550fb2e72595dee71f5037d061466985be'
    (OUT/'candidate.dxbc').write_bytes(candidate); receipt['candidate.dxbc']=sha(candidate)
    for i,(off,r,refs) in enumerate(selected):
        root,checksum=roots[r[16:32].hex()]
        assert key(root)[:8]==struct.pack('<Q',checksum) and key(root[24:])==root[:16]
        assert struct.unpack_from('<I',root,16)[0]==0, 'nonzero root node mask'
        (OUT/f'root{i}.bin').write_bytes(root[24:]); (OUT/f'desc{i}.bin').write_bytes(r[32:688])
        d=r[32:688]
        receipt['records'].append({'index':i,'offset':off,'record_sha256':sha(r),'desc_sha256':sha(d),
            'root_key':root[:16].hex(),'root_sha256':sha(root[24:]),'input_elements':0,
            'topology_type':struct.unpack_from('<I',d,572)[0], 'rt_formats':list(struct.unpack_from('<8I',d,580))[:struct.unpack_from('<I',d,576)[0]],
            'dsv_format':struct.unpack_from('<I',d,612)[0], 'sample':list(struct.unpack_from('<II',d,616))})
    assert source.read_bytes()==data, 'journal changed during extraction; rerun from stable capture'
    (OUT/'binding.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt,indent=2))
if __name__=='__main__': main()
