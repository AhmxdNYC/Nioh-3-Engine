# ld_scripttab.py [regex] - dump the mission-script function table (name ptr, fn ptr pairs) around 0x397e770..0x3990000
import sys, struct, re
D=open(r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe","rb").read()
B=0x7FF74F410000
pat=re.compile(sys.argv[1] if len(sys.argv)>1 else ".", re.I)
def s(r):
    if r<0 or r>=len(D): return None
    x=D[r:r+120].split(b"\0")[0]
    try: t=x.decode("ascii")
    except: return None
    return t if len(t)>2 and all(32<=ord(c)<127 for c in t) else None
for a in range(0x3970000,0x39a0000,8):
    p,f=struct.unpack_from("<QQ",D,a)
    if not (B<=p<B+0x5000000 and B<=f<B+0x3800000): continue
    n=s(p-B)
    if n and "::" in n and pat.search(n):
        print(f"{a:#x} {f-B:#x} {n}")
