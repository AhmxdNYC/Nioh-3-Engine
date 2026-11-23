"""Offline (#106936, human #92460): the open-world boss list stays parked until the human resumes it.
OpenWorld() needs summonChr; the catalogue parser must not read summon_chr, so no row can show the 'Open world' entry,
dispatch ActOpenWorld, or reach Training FieldArrive (which also refuses chr 0). Fails if someone re-enables the line."""
import pathlib, re, sys
root = pathlib.Path(__file__).resolve().parents[1]
core = (root / "src" / "rematch_core.h").read_text(encoding="utf-8", errors="replace")
rm = (root / "src" / "rematch.cpp").read_text(encoding="utf-8", errors="replace")
tr = (root.parent / "Nioh3Training" / "src" / "training.c").read_text(encoding="utf-8", errors="replace")
fail = []
if not re.search(r"bool OpenWorld\(\) const \{ return route == kX && summonChr && placement; \}", core):
    fail.append("OpenWorld() must still require summonChr")
code = "\n".join(l.split("//", 1)[0] for l in core.splitlines())  # strip // comments
if re.search(r"\bsummonChr\s*=(?!=)", code.replace("uint32_t summonChr = 0;", "")):
    fail.append("rematch_core.h assigns summonChr: open-world list is re-enabled (parked by human #92460)")
rmc = "\n".join(l.split("//", 1)[0] for l in rm.splitlines())
if re.search(r"\bsummonChr\s*=(?!=)", rmc):
    fail.append("rematch.cpp assigns summonChr outside the parser")
for fn in ("bool ActOpenWorld(int row) {",):
    i = rm.find(fn)
    if i < 0 or "OpenWorld()" not in rm[i:i + 300]: fail.append(f"{fn} must refuse non-OpenWorld rows")
m = re.search(r"N3T_API int Nioh3Training_FieldArrive\([^)]*\) \{(.{0,200})", tr, re.S)
if not m or "if (!chr" not in m.group(1): fail.append("Training FieldArrive must refuse chr 0")
tsv = (root / "Nioh3BossRematch_bosses.tsv").read_text(encoding="utf-8", errors="replace").splitlines()
hdr = tsv[0].split("\t")
n = 0
if "summon_chr" in hdr:
    k = hdr.index("summon_chr")
    n = sum(1 for l in tsv[1:] if len(l.split("\t")) > k and l.split("\t")[k].strip() not in ("", "0"))
print(f"catalogue rows with summon_chr (would be open-world if re-enabled): {n}")
if fail:
    print("\n".join("FAIL: " + f for f in fail)); sys.exit(1)
print("PASS open-world list parked: summon_chr not read, OpenWorld() false for every row, FieldArrive refuses chr 0")
