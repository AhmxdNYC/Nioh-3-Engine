import ctypes as C, ctypes.wintypes as W, struct, sys, subprocess
k=C.WinDLL('kernel32',use_last_error=True)
class MBI(C.Structure):
    _fields_=[('Base',C.c_uint64),('Alloc',C.c_uint64),('AP',W.DWORD),('pad',W.DWORD),('Size',C.c_uint64),('State',W.DWORD),('Prot',W.DWORD),('Type',W.DWORD),('pad2',W.DWORD)]
k.VirtualQueryEx.argtypes=[W.HANDLE,C.c_uint64,C.POINTER(MBI),C.c_size_t]
k.ReadProcessMemory.argtypes=[W.HANDLE,C.c_uint64,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)]
pid=int(subprocess.check_output('powershell -NoProfile -c "(Get-Process Nioh3).Id"',shell=True).split()[0])
h=k.OpenProcess(0x410,0,pid)
def regions():
    a=0;m=MBI()
    while k.VirtualQueryEx(h,a,C.byref(m),C.sizeof(m)):
        if m.State==0x1000 and m.Prot in(4,2,0x40,0x20): yield m.Base,m.Size
        a=m.Base+m.Size
        if a>=0x7fffffffffff:break
def rd(a,n):
    b=C.create_string_buffer(n);g=C.c_size_t()
    if k.ReadProcessMemory(h,a,b,n,C.byref(g)):return b.raw[:g.value]
    return b''
pat=struct.pack('<I',0x5129B)
hits=[]
for base,size in regions():
    if size>256<<20:continue
    d=rd(base,size)
    i=d.find(pat)
    while i>=0:
        if i>=0x54 and i+0x14<=len(d) and struct.unpack_from('<I',d,i+0x10)[0]==700000000:
            hits.append((base,size,i-0x54));print('HIT row',hex(base+i-0x54),'region',hex(base),hex(size))
        i=d.find(pat,i+1)
open(sys.argv[1] if len(sys.argv)>1 else 'hits.txt','w').write('\n'.join('%x %x %x'%x for x in hits))
for base,size,off in hits[:1]:
    d=rd(base,size)
    # dump region in 416-B grid aligned to hit
    start=off%416
    n=(len(d)-start)//416
    out=open('C:/nioh3-re/t81523/rows.tsv','w')
    for r in range(n):
        w=struct.unpack_from('<104I',d,start+r*416)
        out.write('%d\t%x\t'%(r,base+start+r*416)+'\t'.join('%x'%x for x in w)+'\n')
    out.close();print('rows',n,'start',hex(base+start))
