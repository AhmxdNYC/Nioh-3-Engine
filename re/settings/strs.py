import re,sys
d=open(r'C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe','rb').read()
out=open(r'C:\Users\Ahmad Hamza\nioh3-engine\re\settings\strings_all.tsv','w',encoding='utf-8')
for m in re.finditer(rb'[\x20-\x7e]{4,}',d):
    out.write(f"{m.start():08X}\tA\t{m.group().decode()}\n")
for m in re.finditer(rb'(?:[\x20-\x7e]\x00){4,}',d):
    out.write(f"{m.start():08X}\tW\t{m.group().decode('utf-16le')}\n")
