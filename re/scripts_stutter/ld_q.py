# ld_q.py qwords <rva> <n>   - dump qwords from re\Nioh3.dump.exe (raw offset == RVA)
import sys, struct
D=open(r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe","rb").read()
cmd=sys.argv[1]
if cmd=="qwords":
    a=int(sys.argv[2],16); n=int(sys.argv[3])
    for i in range(n):
        v=struct.unpack_from("<Q",D,a+8*i)[0]
        print(f"{a+8*i:#x}: {v:#018x}")
elif cmd=="bytes":
    a=int(sys.argv[2],16); n=int(sys.argv[3]); print(D[a:a+n].hex(" "))
elif cmd=="str":
    a=int(sys.argv[2],16); print(D[a:a+200].split(b"\0")[0])
