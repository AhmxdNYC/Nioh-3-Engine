# ld_strs.py <file> [minlen] - print printable strings with offsets (script bytecode)
import sys,re
d=open(sys.argv[1],"rb").read(); m=int(sys.argv[2]) if len(sys.argv)>2 else 4
for x in re.finditer(rb"[\x20-\x7e]{%d,}"%m,d): print(f"{x.start():6x} {x.group().decode()}")
