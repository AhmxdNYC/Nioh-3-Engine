# Walk core item id -> seal chr spawn id from the live game, starting only at the exe global. Read-only.
exec(open(__file__.replace('core_chain.py','core_chain_scanlib.py')).read().split('pat=struct')[0])
import sys
ps=C.WinDLL('psapi');arr=(C.c_uint64*1024)();need=W.DWORD()
ps.EnumProcessModulesEx.argtypes=[W.HANDLE,C.c_void_p,W.DWORD,C.POINTER(W.DWORD),W.DWORD]
ps.EnumProcessModulesEx(h,arr,C.sizeof(arr),C.byref(need),3); exe=arr[0]
Q=lambda a:struct.unpack('<Q',rd(a,8))[0]; D=lambda a:struct.unpack('<I',rd(a,4))[0]; H=lambda a:struct.unpack('<H',rd(a,2))[0]
gd=Q(exe+0x45B9E30)
def table(off,stride):
    hdr=Q(Q(gd+off)); return hdr+8, D(hdr+4), stride
IT=table(0x68,0x1A0); SC=table(0x118,0x54)
print('exe %x GameData %x items %x n=%d  soulcores %x n=%d'%(exe,gd,IT[0],IT[1],SC[0],SC[1]))
items={H(IT[0]+i*0x1A0+0x152):IT[0]+i*0x1A0 for i in range(IT[1])}
sc={D(SC[0]+i*0x54+0xC):SC[0]+i*0x54 for i in range(SC[1])}
for a in sys.argv[1:]:
    core=int(a,16); r=items[core]; cat=D(r+0x64); key=H(r+0x180); s=sc.get(key)
    yang=D(s+0x18); yin=D(s+0x1C); sr=items[yang]
    print('core %X row %x cat %d key(+0x180) %X -> sc row %x yang seal %X yin %X -> seal row %x cat %d +0x54 = %X'%(core,r,cat,key,s,yang,yin,sr,D(sr+0x64),D(sr+0x54)))
n=0
for i in range(IT[1]):
    r=IT[0]+i*0x1A0
    if D(r+0x64)==900000000:
        s=sc.get(H(r+0x180))
        if s: n+=1
print('cores with a soul-core row: %d'%n)
