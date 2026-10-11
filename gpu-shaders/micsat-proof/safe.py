"""Isolated MIC SAT candidate: retain header guards, predicate corner addresses.

No staging/install. Uses the existing rewrite only as a captured intermediate;
the existing rewriter and shader sources are never modified on disk.
"""
import pathlib,re,sys
sys.dont_write_bytecode=True
HERE=pathlib.Path(__file__).resolve().parent
sys.path[:0]=[str(HERE.parent/'meshcull'),str(HERE.parent/'materials')]
import micsat
from sfb_rewrite import Fn

class Capture(Fn):
    instance=None
    def emit(self):
        Capture.instance=self
        return super().emit()

def rewrite(text):
    original=dict(Fn(text).blocks)
    saved=micsat.Fn;micsat.Fn=Capture
    try:_,info=micsat.rewrite(text)
    finally:micsat.Fn=saved
    fn=Capture.instance;blocks=dict(fn.blocks);count=0
    for h0,head,n in info:
        # The original h0 successor with T[2] remains present after folding.
        b1=next(b for b in re.findall(r'label %(\w+)',original[h0][-1])
                if any('bufferLoad.i32' in x and ', i32 2, i32 undef)' in x for x in original[b]))
        blocks[h0][:]=original[h0]
        blocks[b1][:]=original[b1]
        # Restore guarded T[1] and its extract at their original position.
        src=original[head]
        at=next(i for i,x in enumerate(src) if 'bufferLoad.i32' in x and ', i32 1, i32 undef)' in x)
        blocks[head][at:at]=src[at:at+2]
        lines=blocks[head]
        # Exactly three load results feed selects; the fourth corner remains
        # unconditional as in the original. T[0] was already read on every
        # incoming path, making index0 the fallback under the same validity
        # contract without requiring a new OOB-read guarantee.
        selections=[]
        for line in lines:
            m=re.fullmatch(r'\s*%\w+ = select i1 (%\w+), i32 (%\w+|0), i32 (%\w+|0)',line)
            if m and ((m[2]=='0') != (m[3]=='0')):selections.append(m)
        assert len(selections)==3
        for m in selections:
            value=m[3] if m[2]=='0' else m[2]
            extract=next(x for x in lines if x.strip().startswith(value+' = extractvalue '))
            loaded=re.search(r'ResRet.i32 (%\w+), 0$',extract)[1]
            at=next(i for i,x in enumerate(lines) if x.strip().startswith(loaded+' = call '))
            load=lines[at]
            idx=re.search(r'Handle %\w+, i32 (%\w+|\d+), i32 undef\)$',load)[1]
            name='%safe_idx'+str(count)
            yes,no=('0',idx) if m[2]=='0' else (idx,'0')
            lines[at:at+1]=['  '+name+' = select i1 '+m[1]+', i32 '+yes+', i32 '+no,
                           load.replace(', i32 '+idx+', i32 undef)',', i32 '+name+', i32 undef)')]
            count+=1
    assert count==9
    return fn.emit(),info

if __name__=='__main__':
    candidate,info=rewrite(pathlib.Path(sys.argv[1]).read_text())
    pathlib.Path(sys.argv[2]).write_text(candidate)
    print('SAFE_INDEX_DRAFT',info)
