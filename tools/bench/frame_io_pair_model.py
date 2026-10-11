"""Offline counterexample model for the native meshlet read pair; no native IO.

Record ABI: five uint64 fields dst,dstOffset,src,srcOffset,bytes (5C344).
This deliberately compares stock with a tempting eager pair, not an installable
replacement. A discovered semantic mismatch is a NO_GO, never an FPS estimate.
"""
from dataclasses import dataclass,field
import struct

@dataclass(frozen=True)
class CopyRecord:
    dst:int
    dst_offset:int
    src:int
    src_offset:int
    size:int
    def pack(self):
        values=(self.dst,self.dst_offset,self.src,self.src_offset,self.size)
        if any(not isinstance(x,int) or not 0<=x<2**64 for x in values):raise ValueError('uint64 record required')
        if self.dst_offset+self.size>2**64 or self.src_offset+self.size>2**64:raise ValueError('range overflow')
        return struct.pack('<5Q',*values)
    @classmethod
    def unpack(cls,raw):
        if len(raw)!=40:raise ValueError('copy record must be exactly40 bytes')
        value=cls(*struct.unpack('<5Q',raw));value.pack();return value

@dataclass(frozen=True)
class Wrapper:
    identity:int
    source:int
    staging:int
    source_offset:int=0
    staging_offset:int=0

@dataclass
class Case:
    a:Wrapper=field(default_factory=lambda:Wrapper(1,101,201))
    b:Wrapper=field(default_factory=lambda:Wrapper(2,102,202))
    count:int=4
    reread_b:Wrapper|None=None
    reread_count:int|None=None
    failures:frozenset=field(default_factory=frozenset)
    fence_pending:bool=False
    # Neither guarantee has been established for a native hook.
    stable_epoch:bool=True
    lifetime_pin:bool=True

class Machine:
    def __init__(self,case):
        self.case=case;self.trace=[];self.observable=[];self.refs={};self.pending=[];self.states={};self.records=[]
    def retain(self,kind,obj):
        key=(kind,obj);self.refs[key]=self.refs.get(key,0)+1;self.trace.append(('retain',kind,obj))
    def release(self,kind,obj):
        key=(kind,obj)
        if any((kind,obj) in refs for _,refs in self.pending):raise AssertionError('release before fence retirement')
        assert self.refs.get(key,0)>0;self.refs[key]-=1;self.trace.append(('release',kind,obj))
    def prepare(self,label,w,count):
        if self.states.get(w.identity,4)!=4:self.observable.append(('fail',label,'already_mapped'));return None
        if label+'_create' in self.case.failures:self.observable.append(('fail',label,'create'));return None
        record=CopyRecord(w.staging,w.staging_offset,w.source,w.source_offset,count*4);record.pack()
        self.retain('wrapper',w.identity);self.retain('source',w.source);self.retain('staging',w.staging)
        return (label,w,record)
    def retire(self):
        for label,_ in self.pending:self.trace.append(('fence_complete',label))
        self.pending=[]
    def submit(self,items,label):
        self.trace.append(('submit_attempt',label,len(items)))
        if label+'_submit' in self.case.failures:
            self.observable.append(('fail',label,'submit'));return False
        for name,w,rec in items:
            self.records.append(rec);self.trace.append(('copy',name,rec.pack().hex()))
            self.pending.append((name,{('source',w.source),('staging',w.staging)}))
        if label+'_fence' in self.case.failures:
            self.trace.append(('fence_status_unknown',label));return False
        if self.case.fence_pending:
            self.trace.append(('fence_pending',label));return False
        self.retire();return True
    def expose(self,item):
        label,w,rec=item
        assert not self.pending
        if label+'_map' in self.case.failures:self.observable.append(('fail',label,'map'));return False
        self.states[w.identity]=0
        # Exact read range substitutes for bytes only under immutable-source assumption.
        self.observable.append(('map',label,w.identity,rec.src,rec.src_offset,rec.size));return True
    def cleanup(self,items):
        for label,w,_ in reversed(items):
            if self.states.get(w.identity,4)==0:
                self.observable.append(('close',label,w.identity));self.states[w.identity]=4
            self.release('staging',w.staging);self.release('source',w.source);self.release('wrapper',w.identity)
    def finish(self,items):
        if self.pending:
            # GPU ownership persists. Caller must resume after actual completion.
            self.trace.append(('defer_cleanup',len(items)))
        else:
            self.observable.append(('recount',))
            self.cleanup(items)
        return self

def stock(case):
    m=Machine(case);items=[]
    if case.count==0:return m.finish(items)
    a=m.prepare('A',case.a,case.count)
    if a is None:return m.finish(items)
    items.append(a)
    if not m.submit([a],'A') or not m.expose(a):return m.finish(items)
    # The actual caller rereads B and count after first Map returns.
    b=m.prepare('B',case.reread_b or case.b,case.reread_count if case.reread_count is not None else case.count)
    if b is None:return m.finish(items)
    items.append(b)
    if m.submit([b],'B') and m.expose(b):m.observable.append(('consume_pair',))
    return m.finish(items)

def overlap(resource_a,offset_a,size_a,resource_b,offset_b,size_b):
    return resource_a==resource_b and max(offset_a,offset_b)<min(offset_a+size_a,offset_b+size_b)

def eligibility(case):
    reasons=[];a,b=case.a,case.b;n=case.count*4
    if not case.stable_epoch:reasons.append('source_epoch_not_pinned')
    if not case.lifetime_pin:reasons.append('lifetime_not_pinned')
    if case.count<=0:reasons.append('nonpositive_count')
    if a.identity==b.identity:reasons.append('wrapper_alias')
    if case.reread_b is not None and case.reread_b!=b:reasons.append('B_pointer_reread_changes')
    if case.reread_count is not None and case.reread_count!=case.count:reasons.append('count_reread_changes')
    if overlap(a.source,a.source_offset,n,b.source,b.source_offset,n):reasons.append('source_ranges_alias')
    if overlap(a.staging,a.staging_offset,n,b.staging,b.staging_offset,n):reasons.append('staging_ranges_alias')
    for dst in (a,b):
        for src in (a,b):
            if overlap(dst.staging,dst.staging_offset,n,src.source,src.source_offset,n):reasons.append('copy_destination_aliases_source')
    return reasons

def eager_pair(case,guard=True):
    if guard and eligibility(case):return stock(case)
    m=Machine(case);items=[]
    for label,w in [('A',case.a),('B',case.b)]:
        item=m.prepare(label,w,case.count)
        if item is None:return m.finish(items)
        items.append(item)
    if m.submit(items,'pair'):
        if m.expose(items[0]) and m.expose(items[1]):m.observable.append(('consume_pair',))
    return m.finish(items)

def verdict(case):
    a=stock(case);b=eager_pair(case)
    return dict(admission_rejections=eligibility(case),
                semantic_equal=a.observable==b.observable,
                stock_observable=a.observable,pair_observable=b.observable,
                stock_submits=sum(e[0]=='submit_attempt' for e in a.trace),
                pair_submits=sum(e[0]=='submit_attempt' for e in b.trace),
                pending_references=bool(a.pending or b.pending),
                status='NO_GO_EAGER_PAIR' if a.observable!=b.observable else 'CONDITIONAL_MODEL_EQUALITY_ONLY')
