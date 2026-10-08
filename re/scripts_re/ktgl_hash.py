r"""ktgl / LFM script string hash (boost::hash_combine-style, 32-bit), recovered from the EXE.

    h = 0
    for c in name (signed char):  h = h ^ ((h*64 + (h>>2) - 0x61C88647 + c) & 0xFFFFFFFF)

Source: Nioh3.exe RVA 0x13D170 (script-native / sound-id registration, string branch `iVar1==1`).
Verified against all 3203 (name, hash) pairs in the archive_09 LFM script name tables.
Case-sensitive, no seed.  Used for LFM name tables, Sound BGM ids (PlayBossBGM etc.) and gadget ids.

  py -I re\scripts_re\ktgl_hash.py Name1 Name2 ...              print hashes
  py -I re\scripts_re\ktgl_hash.py --find 6b79c2ca ... < candidates.txt   report which candidate lines hash to the ids
"""
import sys

M = 0xFFFFFFFF


def ktgl_hash(s):
    if isinstance(s, str):
        s = s.encode('utf-8')
    h = 0
    for c in s:
        if c >= 128:
            c -= 256
        h = h ^ ((h * 64 + (h >> 2) - 0x61C88647 + c) & M)
    return h & M


if __name__ == '__main__':
    a = sys.argv[1:]
    if a[:1] == ['--find']:
        want = {int(x, 16) for x in a[1:]}
        for line in sys.stdin:
            t = line.rstrip('\r\n')
            if t and ktgl_hash(t) in want:
                print('%08x\t%s' % (ktgl_hash(t), t))
    else:
        for n in a:
            print('%08x\t%s' % (ktgl_hash(n), n))
