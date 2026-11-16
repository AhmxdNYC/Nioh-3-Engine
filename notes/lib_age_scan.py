"""List bundled third-party library version stamps in a PE file (oldest first by year).
Usage: py -I tools/re/lib_age_scan.py FILE [FILE...]"""
import re, sys
PAT = re.compile(rb"[\x20-\x7e]{6,}")
KEY = re.compile(r"(copyright|\(c\)|version|inflate|deflate|libpng|libjpeg|lua |havok|bink|wwise|fmod|cri |criware|oodle|openssl|zstd|lz4|xxhash|pcre|expat|freetype|harfbuzz|protobuf|rapidjson|mimalloc|tbb|boost|steamworks|sqlite|zlib|png|jpeg|ogg|vorbis|opus|theora|bullet|physx|recast|detour|squish|bc7|ispc|imgui|tinyxml|pugixml|json|curl|ssl|crypt|msvc|mfc)", re.I)
YEAR = re.compile(r"(?<!\d)(19[89]\d|20[0-2]\d)(?!\d)")
for fn in sys.argv[1:]:
    data = open(fn, "rb").read()
    hits = {}
    for m in PAT.finditer(data):
        s = m.group().decode("ascii", "replace").strip()
        if len(s) > 200 or not KEY.search(s): continue
        ys = [int(y) for y in YEAR.findall(s)]
        if not ys and not re.search(r"\d+\.\d+", s): continue
        hits.setdefault(s, min(ys) if ys else 9999)
    print(f"== {fn} ({len(data)} bytes, {len(hits)} stamps)")
    for s, y in sorted(hits.items(), key=lambda kv: (kv[1], kv[0])):
        print(f"{y if y != 9999 else '----'}  {s}")
