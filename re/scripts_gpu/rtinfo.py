"""rtinfo.py <ktid> [...]  -- print render-target objects from the RT table dump (system 0x93fb3280)
and also search all render-graph kgk dumps for nodes that reference that hash."""
import sys, re, glob, os
D = r"C:\Users\Ahmad Hamza\nioh3-engine\data\dumps"
txt = open(os.path.join(D, "0x93fb3280.kidsobjdb.txt"), encoding="utf-8", errors="replace").read()
objs = {}
for blk in re.split(r"\n(?=== obj )", txt):
    m = re.match(r"== obj ([0-9a-f]{8})\s+(\S+)", blk)
    if m:
        objs[m.group(1)] = blk
for k in sys.argv[1:]:
    k = k.lower().replace("0x", "").zfill(8)
    b = objs.get(k)
    if not b:
        print("== %s: not in RT table" % k); continue
    lines = b.splitlines()
    keep = [l.strip() for l in lines[1:] if re.match(r"\s+(Width|Height|Depth|PixelFormat|LevelNum|Ratio|Scale|Flag)\b", l)]
    print(lines[0], " | ".join(re.sub(r"\s+", "=", x, 1) for x in keep))
