"""Two bound C-buffer index preparations; no floating-point or resource motion."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import sys
sys.dont_write_bytecode=True
SHA='da83f00e00883f3a4824f75276461de0efd2e452c364be54b1f64265f940839c'
BODY='b552d22e52afdd5fdc330a4c8b47c2ed92a68980f2198296020546fc9b646695'
FILE='817056c5d554e20d10d0bfb5a933f11b7d49cceb2f504c80773a7f028214bfdf'
SITES=((325,326,333,334,50),(2031,2032,2033,2034,3))
def sha(x):return hashlib.sha256(x).hexdigest()


def rewrite(ll,S,dxcom):
    assert sha(dxcom.body(ll).encode())==BODY
    f=S.Fn(ll);original=[(b,list(lines)) for b,lines in f.blocks];edits=[]
    for input_id,shift_id,or_id,add_id,constant in SITES:
        shift=f'  %v{shift_id} = shl i32 %v{input_id}, 1'
        old_or=f'  %v{or_id} = or i32 %v{shift_id}, 1'
        old_add=f'  %v{add_id} = add i32 %v{or_id}, {constant}'
        new_add=f'  %v{add_id} = add i32 %v{shift_id}, {constant+1}'
        blocks=[(b,lines) for b,lines in f.blocks if old_or in lines];assert len(blocks)==1
        b,lines=blocks[0];assert shift in lines and old_add in lines
        assert lines.index(shift)<lines.index(old_or) and lines.index(old_add)==lines.index(old_or)+1
        users=[line for _,ls in f.blocks for line in ls if f'%v{or_id}' in S.TOK.findall(line.split(' = ',1)[-1])]
        assert users==[old_add]
        # The shift's lifetime ends at the same instruction position as before
        # (or one instruction later if it had no later use); no block hoist.
        lines[lines.index(old_add)]=new_add;lines.remove(old_or)
        edits.append(dict(block=b,removed=old_or,before=old_add,after=new_add))
    # Exact edit-set check: no silent change to any remaining instruction.
    expected=[]
    for b,lines in original:
        for e in edits:
            if e['block']==b:
                lines=[e['after'] if x==e['before'] else x for x in lines if x!=e['removed']]
        expected.append((b,lines))
    assert expected==[(b,lines) for b,lines in f.blocks]
    return f.emit(),edits


def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--assemble',action='store_true');a=p.parse_args()
    sys.path[:0]=[str(a.repo/'gpu-shaders/materials'),str(a.repo/'gpu-shaders')]
    import dxcom,sfb_rewrite as S
    from build_sfb import Group
    game=Path('C:/Program Files (x86)/Steam/steamapps/common/Nioh3/shader_dx12/0xabb677e6.file')
    raw=game.read_bytes();assert sha(raw)==FILE
    binary=Group(raw).dxbc(3);assert sha(binary)==SHA
    a.out.mkdir(parents=True,exist_ok=True);src=a.out/'original.dxbc';src.write_bytes(binary)
    ll=dxcom.dumpbin(str(src));new,edits=rewrite(ll,S,dxcom)
    rng=random.Random(0xa11ff9e5);values=list(range(65536))+[rng.getrandbits(32) for _ in range(4096)]+[0xffffffff,0x80000000,0x7fffffff]
    for x in values:
        y=(x<<1)&0xffffffff;assert y&1==0
        for c in (50,3):assert ((y|1)+c)&0xffffffff==(y+c+1)&0xffffffff
    # Wrong constant gives a concrete failing address even with input zero.
    assert (((0<<1)|1)+50)!=((0<<1)+50)
    (a.out/'fold-index.ll').write_bytes(new.encode())
    result=dict(source_file_sha256=FILE,slot=3,shader_sha256=SHA,body_sha256=BODY,
        candidate_body_sha256=sha(dxcom.body(new).encode()),edits=edits,integer_cases=len(values),
        wrong_constant_negative=True,resource_float_control_instructions_unchanged=True,
        codegen='PENDING_MANUAL_GAME_SESSION',native='UNADMITTED')
    if a.assemble:
        candidate=dxcom.ll_to_container(new);dst=a.out/'fold-index.dxbc';dst.write_bytes(candidate)
        assert dxcom.body(dxcom.dumpbin(str(dst)))==dxcom.body(new)
        op,cp=dxcom.parts(binary),dxcom.parts(candidate)
        assert all(op.get(k)==cp.get(k) for k in ('ISG1','OSG1','PSV0','SFI0'))
        result['candidate_sha256']=sha(candidate)
    assert game.read_bytes()==raw
    (a.out/'fold-index.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))


if __name__=='__main__':main()
