"""Dump a per-character action table from LFM archive_01 (static, read-only).

Format (found 2026-10-19, notes/yoritomo-move-table-search.md): archive_01 file = u32 n=16, then 16 (offset,size)
sub-tables. Sub-table 1 = action records, 336 (0x150) bytes each:
  +0x0C u16 action id (the id the native action state reports, e.g. 0x4E53)
  +0x20 u16 motion number (attack motions 1000-1199, movement < 100, hit reactions 30000+; shared by variants)
  +0xF0 u32 action object hash = an entry of the boss's MotorCharacterSetting CharacterActionObjectNameHashArray
Other fields (flags +0x06, +0x10, +0x1A ...) are not decoded.

  py -I tools/chr_action_table.py <archive_01 file>            -> TSV: row, action_id, motion, flags06, flags10, obj_hash
"""
import struct
import sys

REC = 0x150


def records(path):
    d = open(path, 'rb').read()
    n = struct.unpack_from('<I', d, 0)[0]
    assert n == 16, 'not a 16-sub-table archive_01 file'
    off, size = struct.unpack_from('<II', d, 12)
    assert size and size % REC == 0, 'sub-table 1 is not 336-byte records'
    for r in range(size // REC):
        o = off + r * REC
        yield (r, struct.unpack_from('<H', d, o + 0x0C)[0], struct.unpack_from('<H', d, o + 0x20)[0],
               struct.unpack_from('<H', d, o + 0x06)[0], struct.unpack_from('<H', d, o + 0x10)[0],
               struct.unpack_from('<I', d, o + 0xF0)[0])


if __name__ == '__main__':
    print('row\taction_id\tmotion\tflags06\tflags10\tobj_hash')
    for r, a, m, f6, f10, h in records(sys.argv[1]):
        print('%d\t0x%X\t%d\t0x%04X\t0x%04X\t%08x' % (r, a, m, f6, f10, h))
