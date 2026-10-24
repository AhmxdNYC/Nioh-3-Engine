"""Per-action unblockable flag + owned hitbox rows/groups from an archive_01 character action file (static, read-only).

  py -I tools/chr_hit_groups.py <archive_01 file>   -> TSV on stdout

great_attack = action-row qword[+0] bit 45 (u16 +0x04 bit 0x2000). The action-start code RVA 0x4F6155..0x4F617A copies it into
state bitset (+0x1060) bit 11; hit setup RVA 0x2B4FEE..0x2B5000 copies that into hit record +0x13C bit 10, which is exactly
what script Collision::Damage::Hit::IsAttackGreatAttack (RVA 0x23C7EA0) returns. See notes/yoritomo-hit-source-22305.md.
bits40_43 = row qword[+0] bits 40..43 -> hit +0x13C bits 0,1(IsAttackPowerAttack),3,2 (conditions 0x2D,0x2E,0x32,0x33).
Hitbox rows: sub-table 0 packed records (u16 count + indexes) -> sub-table 2 rows (192 B). +0x10 = physical power
(base damage = power * 0.01 * 500, RVA 0x23C78A1), +0x12 = element power, +0x14 = type. A "group" is a run of consecutive owned
rows with equal (power10, power12, type14): CANDIDATE hit count only (not proven).
"""
import struct
import sys


def main(path):
    d = open(path, 'rb').read()
    st = [struct.unpack_from('<II', d, 4 + 8 * i) for i in range(16)]
    o0, s0 = st[0]
    o1, s1 = st[1]
    o2 = st[2][0]
    na = s1 // 0x150
    sizes = struct.unpack_from('<%dH' % na, d, o0 + 64)
    p = o0 + s0 - sum(sizes)
    print('action\tmotion\tgreat_attack\tbits40_43\tn_rows\tn_groups\tgroups(power10/power12/type14 x rows)')
    for r in range(na):
        cnt = struct.unpack_from('<H', d, p)[0]
        idx = struct.unpack_from('<%dH' % cnt, d, p + 2)
        p += sizes[r]
        ro = o1 + r * 0x150
        aid, mot = struct.unpack_from('<H', d, ro + 0xC)[0], struct.unpack_from('<H', d, ro + 0x20)[0]
        q0 = struct.unpack_from('<Q', d, ro)[0]
        atk = any(struct.unpack_from('<H', d, ro + x)[0] != 0xFFFF for x in (0x13C, 0x142))
        if not atk and not cnt:
            continue
        groups = []
        for i in idx:
            k = struct.unpack_from('<3H', d, o2 + i * 192 + 0x10)
            if groups and groups[-1][0] == k:
                groups[-1][1] += 1
            else:
                groups.append([k, 1])
        print('0x%X\t%d\t%d\t%s\t%d\t%d\t%s' % (
            aid, mot, q0 >> 45 & 1, ''.join(str((q0 >> k) & 1) for k in (40, 41, 42, 43)), cnt, len(groups),
            ' '.join('%d/%d/%dx%d' % (g[0] + (g[1],)) for g in groups)))


if __name__ == '__main__':
    main(sys.argv[1])
