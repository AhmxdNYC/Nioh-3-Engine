import sys,struct,pickle
d=open(r'C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe','rb').read()
TEXT_END=0x38E128B
ranges=[(0x38E2000,0x38E2000+0xBD1B8C)]  # all of .rdata
lo,hi=ranges[0]
refs={}
mv=memoryview(d)
for a in range(4):
    n=(TEXT_END-0x1000-a)//4
    arr=mv[0x1000+a:0x1000+a+n*4].cast('i')
    base=0x1000+a+4
    for k,disp in enumerate(arr):
        t=base+4*k+disp
        if lo<=t<hi:
            refs.setdefault(t,[]).append(base+4*k-4)
pickle.dump(refs,open('rdata_refs.pkl','wb'))
print(len(refs))
