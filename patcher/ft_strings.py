import re, sys
img = open(r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe","rb").read()
pat = sys.argv[1].encode()
flags = re.I
for m in re.finditer(rb"[\x20-\x7e]{4,}", img[0x38e2000:0x4e28000]):
    if re.search(pat, m.group(), flags):
        print(hex(m.start()+0x38e2000), m.group()[:120].decode())
# utf16
for m in re.finditer(rb"(?:[\x20-\x7e]\x00){4,}", img[0x38e2000:0x4e28000]):
    s = m.group().decode("utf-16le")
    if re.search(sys.argv[1], s, flags):
        print(hex(m.start()+0x38e2000), "W", s[:120])
