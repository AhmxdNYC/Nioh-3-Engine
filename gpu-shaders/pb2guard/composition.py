"""Input-bound whole-VS compositional certificate; no shader compilation/install.

Structural terms are interned by exact tuples, not hashes. Their equality is
universal syntactic identity under the equal-active-descriptor lemma; finite
wave cases are sensitivity checks, not the source of universal quantification.
"""
import hashlib,json,pathlib,re,struct,sys,types
sys.dont_write_bytecode=True
HERE=pathlib.Path(__file__).resolve().parent;sys.path.insert(0,str(HERE))
import multi_check as M
from prototype import dxcom,Fn,DEF,TOK,OUT,SHA,DIGEST
CANDIDATE_SHA='0d44ddcd220d87d17d2e38bfff2533550fb2e72595dee71f5037d061466985be'
CANDIDATE_DIGEST='11b0311eb9ff27c2db64bd7647d2de5e'
def sha(b):return hashlib.sha256(b).hexdigest()

def validate_blobs(original,candidate):
    assert sha(original)==SHA and original[4:20].hex()==DIGEST
    assert sha(candidate)==CANDIDATE_SHA and candidate[4:20].hex()==CANDIDATE_DIGEST

def bind():
    original=(OUT/'original.dxbc').read_bytes();candidate=(OUT/'multi.dxbc').read_bytes()
    validate_blobs(original,candidate)
    a=dxcom.dumpbin(str(OUT/'original.dxbc'));b=dxcom.dumpbin(str(OUT/'multi.dxbc'))
    assert dxcom.body(a)==dxcom.body((OUT/'original.ll').read_text())
    assert dxcom.body(b)==dxcom.body((OUT/'multi.ll').read_text())
    pa,pb=dxcom.parts(original),dxcom.parts(candidate);assert pa.keys()==pb.keys()
    for k in pa:
        if k not in ('DXIL','HASH','SFI0'):assert pa[k]==pb[k],k
    fa,fb=struct.unpack('<Q',pa['SFI0'])[0],struct.unpack('<Q',pb['SFI0'])[0]
    assert fb==fa|0x4000 and not fa&0x4000
    return a,b

def external_module(a,b):
    def lines(xs):return [x.strip() for x in xs if x.strip() and not x.lstrip().startswith(';')]
    A,B=Fn(a),Fn(b)
    assert lines(A.head)==lines(B.head),'changed input/types/entry signature'
    old=lines(A.tail);new=lines(B.tail)
    wave=[x for x in new if x.startswith('declare ') and ('waveActiveAllEqual.i32' in x or 'waveReadLaneFirst.i32' in x)]
    assert len(wave)==2
    new=[x for x in new if x not in wave]
    expected=[x.replace('!47 = !{i32 0, i64 16}','!47 = !{i32 0, i64 524304}') for x in old]
    assert expected==new,'changed resource/output/semantic metadata or attributes'

def witness(a):
    # Independently select the specified original region; do not invoke rewrite.
    blocks=Fn(a).blocks;xs=dict(blocks)['b35']
    start=next(i for i,x in enumerate(xs) if x.startswith('  %v73 ='))
    region=xs[start:-1];assert len(region)==72 and xs[-1]=='  br label %b176'
    defs={DEF.match(x)[1] for x in region}
    outside=[x for name,ys in blocks for x in (ys[:start]+ys[-1:] if name=='b35' else ys)]
    escaping=defs&{t for x in outside for t in TOK.findall(x)}
    float_defs={DEF.match(x)[1] for x in region if re.search(r'bitcast i32 %\w+ to float$',x)}
    order=[DEF.match(x)[1] for x in region]
    live=[x for x in order if x in escaping and x not in float_defs]+[x for x in order if x in escaping and x in float_defs]
    assert len(live)==13 and len(float_defs&escaping)==4
    return dict(original=region,live_out=live)

class Terms:
    def __init__(self):self.pool={}
    def intern(self,node):
        if node not in self.pool:self.pool[node]=len(self.pool)
        return self.pool[node]
    def execute(self,lines,equal_descriptor):
        env={};reads=[]
        def value(token):return env[token] if token in env else self.intern(('input_or_type',token))
        for line in lines:
            if line.strip().startswith('br '):continue
            dest,rhs=line.strip().split(' = ',1)
            if 'waveReadLaneFirst' in rhs:
                assert equal_descriptor and rhs=='call i32 @dx.op.waveReadLaneFirst.i32(i32 118, i32 %v72)'
                env[dest]=value('%v72');continue
            if 'createHandle' in rhs:
                assert rhs.endswith('i1 true)') or equal_descriptor and rhs.endswith('i1 false)')
                rhs=re.sub(r'i1 (true|false)\)$','i1 DOMAIN_VALID_NONUNIFORM_MARKER)',rhs)
            # Immutable structural application; preserves exact operation,
            # flags, types, constants, masks and all lane-local operands.
            pieces=[];last=0
            for m in TOK.finditer(rhs):pieces.extend((rhs[last:m.start()],('term',value(m[0]))));last=m.end()
            pieces.append(rhs[last:]);node=('instruction',tuple(pieces))
            env[dest]=self.intern(node)
            if 'rawBufferLoad' in rhs or 'cbufferLoadLegacy' in rhs:reads.append(env[dest])
            assert not any(x in rhs for x in ('atomic','bufferStore','sample','waveActive','barrier','discard'))
        return env,reads

def prove(a,b):
    external_module(a,b);w=witness(a);branches=M.inspect_cfg(b,a,w)
    terms=Terms();original,reads=terms.execute(w['original'],False)
    for tag in ('uniform','mixed'):
        actual,trace=terms.execute(branches[tag],tag=='uniform')
        assert trace==reads,('read expression changed',tag)
        assert all(original[v]==actual[v] for v in w['live_out']),('escaping expression changed',tag)
    outputs=sum('storeOutput' in x for _,xs in Fn(a).blocks for x in xs)
    assert outputs==25
    assert not any('@dx.op.wave' in x for _,xs in Fn(a).blocks for x in xs)
    return dict(status='CONDITIONAL_WHOLE_VS_COMPOSITION_PASS',region_instructions=72,
                escaping_values=13,output_instructions=outputs,symbolic_term_nodes=len(terms.pool),
                ordered_read_terms=len(reads),all_unmodified_blocks_and_outputs_identical=True,
                input_domain='defined original inputs, stable valid read-only resources, conformant active-lane wave semantics',
                whole_shader_execution=False)

def adversarial(a,b):
    w=witness(a);branches=M.inspect_cfg(b,a,w)
    defs={DEF.match(x)[1] for x in w['original']}
    inputs={t for x in w['original'] for t in TOK.findall(x.split(' = ',1)[1]) if t.startswith('%v')}-defs
    words=(0,0x80000000,0x7f800000,0xff800000,0x7fc00001,0x7f800001,0xffc12345,0xffffffff)
    cases=waves=0;old_hashlib=M.hashlib
    try:
        for word in words:
            # Only the suffix interpreter's synthetic read oracle is replaced.
            # No floating conversion is performed on these raw words.
            M.hashlib=types.SimpleNamespace(sha256=lambda data:types.SimpleNamespace(digest=lambda:struct.pack('<I',word)*8))
            for width in (1,4,32,64,128):
                for mask in set((1,1<<(width-1),(1<<width)-1,sum(1<<i for i in range(0,width,2)))):
                    active=[i for i in range(width) if mask>>i&1]
                    for want_mixed in (False,True):
                        descriptors={i:(i%16 if want_mixed else 3) for i in active}
                        if want_mixed and len(active)>1:descriptors[active[-1]]=(descriptors[active[0]]+1)%16
                        equal=len(set(descriptors.values()))==1;first=descriptors[active[0]];waves+=1
                        assert not equal or all(descriptors[i]==first for i in active)
                        for lane in active:
                            env={v:(lane*37+3)&0xffffffff for v in inputs};env['%v72']=descriptors[lane];env['%v47']=lane
                            orig,reads=M.execute(w['original'],env,first,7)
                            actual,trace=M.execute(branches['uniform' if equal else 'mixed'],env,first,7)
                            assert reads==trace and all(orig[v]==actual[v] for v in w['live_out']);cases+=1
    finally:M.hashlib=old_hashlib
    return dict(waves=waves,active_lane_cases=cases,raw_word_patterns=[hex(x) for x in words],
                sparse_last_lane_and_mixed_descriptors=True,float_bits_compared_without_float_conversion=True)

def main():
    a,b=bind();result=prove(a,b);result['adversarial']=adversarial(a,b);rejected=[]
    for name,bad in M.cfg_faults((OUT/'multi.ll').read_text()).items():
        try:prove(a,bad)
        except (AssertionError,KeyError,AttributeError):rejected.append(name)
        else:raise AssertionError('accepted mutation '+name)
    normalized=Fn(b);at=next(i for i,(_,xs) in enumerate(normalized.blocks) if any('waveActiveAllEqual' in x for x in xs))
    for name,needle,change in [('read_mask','rawBufferLoad',lambda x:re.sub(r'i8 (\d+)',lambda m:'i8 '+str(int(m[1])^1),x,count=1)),
                               ('raw_bitcast','bitcast i32',lambda x:re.sub(r'bitcast i32 %\w+','bitcast i32 0',x,count=1)),
                               ('overflow_flag','add nsw i32',lambda x:x.replace('add nsw i32','add i32',1))]:
        f=Fn(b);xs=f.blocks[at+1][1];j=next(i for i,x in enumerate(xs) if needle in x);xs[j]=change(xs[j]);bad=f.emit()
        try:prove(a,bad)
        except (AssertionError,KeyError,AttributeError):rejected.append(name)
        else:raise AssertionError('accepted mutation '+name)
    original=(OUT/'original.dxbc').read_bytes();candidate=(OUT/'multi.dxbc').read_bytes()
    for name,oa,cb in [('original_bytes',original[:-1]+bytes([original[-1]^1]),candidate),
                       ('candidate_bytes',original,candidate[:-1]+bytes([candidate[-1]^1]))]:
        try:validate_blobs(oa,cb)
        except AssertionError:rejected.append(name)
        else:raise AssertionError('accepted binding mutation '+name)
    for name,bad in [('resource_metadata',b.replace('i32 16, i32 11','i32 15, i32 11',1)),
                     ('entry_feature',b.replace('i64 524304','i64 524305',1))]:
        assert bad!=b,name
        try:prove(a,bad)
        except (AssertionError,KeyError,AttributeError):rejected.append(name)
        else:raise AssertionError('accepted mutation '+name)
    result.update(original_sha256=SHA,candidate_sha256=CANDIDATE_SHA,actual_source_mutations_rejected=rejected,
                  checker_sha256=sha(pathlib.Path(__file__).read_bytes()),cfg_checker_sha256=sha(pathlib.Path(M.__file__).read_bytes()))
    (OUT/'composition.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
if __name__=='__main__':main()
