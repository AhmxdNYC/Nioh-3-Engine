import pickle
d=open(r'C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe','rb').read()
refs=pickle.load(open('rdata_refs.pkl','rb'))
def cstr(a):
    j=d.index(b'\0',a); return d[a:j].decode('latin1')
def islea(p):
    # p = disp position; check REX 8D modrm
    return d[p-3] in (0x48,0x4C) and d[p-2]==0x8D and (d[p-1]&0xC7)==0x05
s=0x3A9A5D0; e=0x3A9AAB0
for a in sorted(refs):
    if s<=a<e and 32<=d[a]<127 and d[a-1]==0:
        rs=[r for r in refs[a] if islea(r)]
        print(f"{a:08X} {cstr(a)!r:30} {[hex(r) for r in rs]}")
