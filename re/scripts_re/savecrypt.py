#!/usr/bin/env python3
"""Nioh 3 SAVEDATA.BIN decrypt / re-encrypt (stdlib only, offline).  See notes/save-format.md.

Cipher (exe v2.02): AES-128 *with a custom S-box* (table at RVA 0x3be2b70, not the standard one),
CTR mode, 32-bit big-endian counter in bytes 12..15 of the IV (writer 0x5b7f4c, CTR 0x5b85e8,
key schedule 0x5b88cc, constants derived by 0xfbac00).  Layout of the file:
  0x000..0x157  header, XORed with a static keystream (two static AES-CTR passes)
     0x49 c8  0x59 b8  0x69 a8  0x79 k98   16-byte per-save random values, each stored
        encrypted: c8,b8 under static (E,D); a8,k98 under key c8 / IV b8
  0x158..end    body = CTR(key a8, IV k98, CTR(key c8, IV b8, plaintext))
Usage:
  savecrypt.py decrypt IN OUT      (OUT = header-decoded + body-decoded, same size)
  savecrypt.py encrypt IN OUT      (inverse; keys/IVs are read from the decoded header)
  savecrypt.py check IN            (decrypt, verify magic + checksum, re-encrypt, compare)
  savecrypt.py scan IN             (decrypt then list top-level tagged records)
"""
import struct, sys

SBOX = bytes.fromhex(
    "1c2f0353a30149daa6cde08a19a704d4061ada4908e2f6b29ee12249ce7b7e5e"
    "a0092a63af49ce707b3c2380fa1747f262626c5910cc299cb54658c74413e738"
    "d5af2783d4d5a09ee3763b8504d9d6986066d47853eaca0e8d565344e2efbda9"
    "9b100aa11393f0430b7c398a47dfd3c50e3431a6ae5ab8e7e63143c0aa0fe082"
    "124cd1df8ba5ac70c53d1b8e93174d794ece63c4330e1457f0d8195b9b6171f2"
    "2b337efd2c0bb62320b9d491199404a430138af1d005ec5eac4ad4d6a5177ff9"
    "e5f60029d7932d5e2cf181a3b7633957c233872da83f02cc08677460d8f0da67"
    "40648755bb7ff210c90314b58066cb91f61f795888bc95c2065fe90932ed9b85")
H = bytes.fromhex
# static constants (identical in every save; captured from exe emulation, see notes)
KEY_E = H("545b5ebdfd7843557c7d7c7274c74491")   # encrypts c8,b8 inside header
IV_D = H("57077871dfece8e48762c77e69f9045d")
HDR_P1 = (H("35311fcdf8524ea279cc51924b5848fe"), H("1bdfdd5727cbce873aeac29ee05b2925"))
HDR_P2 = (H("0e8c95cd6329f6c253ecfa34d5b47f5e"), H("fd4c40a28ece198b7701bc0c14b757bd"))
HDR_LEN = 0x158


def _xt(a):
    a <<= 1
    return (a ^ 0x11b) & 0xff if a & 0x100 else a


_TE = None


def _tables():
    global _TE
    if _TE is None:
        t0, t1, t2, t3 = [], [], [], []
        for x in range(256):
            s = SBOX[x]
            s2 = _xt(s)
            s3 = s2 ^ s
            t0.append((s2 << 24) | (s << 16) | (s << 8) | s3)
            t1.append((s3 << 24) | (s2 << 16) | (s << 8) | s)
            t2.append((s << 24) | (s3 << 16) | (s2 << 8) | s)
            t3.append((s << 24) | (s << 16) | (s3 << 8) | s2)
        _TE = (t0, t1, t2, t3)
    return _TE


def expand_key(key):
    w = list(struct.unpack(">4I", key))
    rc = 1
    for i in range(4, 44):
        t = w[i - 1]
        if i % 4 == 0:
            t = ((t << 8) | (t >> 24)) & 0xffffffff
            t = (SBOX[t >> 24] << 24) | (SBOX[(t >> 16) & 255] << 16) | (SBOX[(t >> 8) & 255] << 8) | SBOX[t & 255]
            t ^= rc << 24
            rc = _xt(rc)
        w.append(w[i - 4] ^ t)
    return w


def ctr(key, iv, data):
    """CTR with 32-bit BE counter at iv[12:16]; returns bytes (xor stream, so enc == dec)."""
    t0, t1, t2, t3 = _tables()
    rk = expand_key(key)
    s0i, s1i, s2i, c = struct.unpack(">3I", iv[:12]) + (struct.unpack(">I", iv[12:16])[0],)
    k = rk
    sb = SBOX
    out = bytearray(len(data))
    n = len(data)
    for off in range(0, n, 16):
        s0 = s0i ^ k[0]; s1 = s1i ^ k[1]; s2 = s2i ^ k[2]; s3 = c ^ k[3]
        for r in range(1, 10):
            b = 4 * r
            a0 = t0[s0 >> 24] ^ t1[(s1 >> 16) & 255] ^ t2[(s2 >> 8) & 255] ^ t3[s3 & 255] ^ k[b]
            a1 = t0[s1 >> 24] ^ t1[(s2 >> 16) & 255] ^ t2[(s3 >> 8) & 255] ^ t3[s0 & 255] ^ k[b + 1]
            a2 = t0[s2 >> 24] ^ t1[(s3 >> 16) & 255] ^ t2[(s0 >> 8) & 255] ^ t3[s1 & 255] ^ k[b + 2]
            a3 = t0[s3 >> 24] ^ t1[(s0 >> 16) & 255] ^ t2[(s1 >> 8) & 255] ^ t3[s2 & 255] ^ k[b + 3]
            s0, s1, s2, s3 = a0, a1, a2, a3
        o0 = ((sb[s0 >> 24] << 24) | (sb[(s1 >> 16) & 255] << 16) | (sb[(s2 >> 8) & 255] << 8) | sb[s3 & 255]) ^ k[40]
        o1 = ((sb[s1 >> 24] << 24) | (sb[(s2 >> 16) & 255] << 16) | (sb[(s3 >> 8) & 255] << 8) | sb[s0 & 255]) ^ k[41]
        o2 = ((sb[s2 >> 24] << 24) | (sb[(s3 >> 16) & 255] << 16) | (sb[(s0 >> 8) & 255] << 8) | sb[s1 & 255]) ^ k[42]
        o3 = ((sb[s3 >> 24] << 24) | (sb[(s0 >> 16) & 255] << 16) | (sb[(s1 >> 8) & 255] << 8) | sb[s2 & 255]) ^ k[43]
        blk = data[off:off + 16]
        ks = struct.pack(">4I", o0, o1, o2, o3)
        if len(blk) == 16:
            out[off:off + 16] = (int.from_bytes(blk, "big") ^ int.from_bytes(ks, "big")).to_bytes(16, "big")
        else:
            out[off:off + len(blk)] = bytes(x ^ y for x, y in zip(blk, ks))
        c = (c + 1) & 0xffffffff
    return bytes(out)


def hdr_stream():
    z = bytes(HDR_LEN)
    a = ctr(HDR_P1[0], HDR_P1[1], z)
    b = ctr(HDR_P2[0], HDR_P2[1], z)
    return bytes(x ^ y for x, y in zip(a, b))


def xor(a, b):
    return bytes(x ^ y for x, y in zip(a, b))


def header_keys(hdr_plain):
    c8 = ctr(KEY_E, IV_D, hdr_plain[0x49:0x59])
    b8 = ctr(KEY_E, IV_D, hdr_plain[0x59:0x69])
    a8 = ctr(c8, b8, hdr_plain[0x69:0x79])
    k98 = ctr(c8, b8, hdr_plain[0x79:0x89])
    return c8, b8, a8, k98


def decrypt(blob):
    hdr = xor(blob[:HDR_LEN], hdr_stream())
    if hdr[:6] not in (b"RNNUSR", b"RNNSYS"):
        raise ValueError("bad magic after header decode: %r" % hdr[:8])
    c8, b8, a8, k98 = header_keys(hdr)
    body = ctr(c8, b8, ctr(a8, k98, blob[HDR_LEN:]))
    return hdr + body


def encrypt(plain):
    hdr = plain[:HDR_LEN]
    c8, b8, a8, k98 = header_keys(hdr)
    body = ctr(a8, k98, ctr(c8, b8, plain[HDR_LEN:]))
    return xor(hdr, hdr_stream()) + body


def checksum(p):
    """Algorithm from notes/amrita-tools.md section 2 (u32 at 0x900194, seed u32 at 0x900190)."""
    M = (1 << 64) - 1
    seed = struct.unpack_from("<I", p, 0x900190)[0]
    acc = 0
    for blk in range(9216):
        base = 0x190 + blk * 1024
        v = struct.unpack_from("<128Q", p, base)
        a = b = 0
        for k in range(32):
            b += v[4 * k] + v[4 * k + 2]
            a += v[4 * k + 1] + v[4 * k + 3]
        acc = ((b + acc + a) & M) ^ seed
    return ((acc // 0xFFFFFFFF) + acc) & 0xffffffff


def scan(p, start=400, limit=100000):
    pos = start
    out = []
    end = 0x900190
    while pos + 8 <= end and len(out) < limit:
        h, n = struct.unpack_from("<II", p, pos)
        if n > end - pos - 8:
            break
        out.append((pos, h, n))
        pos += 8 + n
    return out


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1
    cmd = argv[1]
    data = open(argv[2], "rb").read()
    if cmd == "decrypt":
        open(argv[3], "wb").write(decrypt(data))
    elif cmd == "encrypt":
        open(argv[3], "wb").write(encrypt(data))
    elif cmd == "check":
        p = decrypt(data)
        cs = struct.unpack_from("<I", p, 0x900194)[0]
        print("magic", p[:6], "size", len(p), "savedAt", struct.unpack_from("<I", p, 32)[0],
              "saveCount", struct.unpack_from("<I", p, 40)[0])
        print("stored checksum %08x computed %08x" % (cs, checksum(p)))
        print("re-encrypt byte-exact:", encrypt(p) == data)
    elif cmd == "scan":
        p = decrypt(data)
        for pos, h, n in scan(p):
            print("0x%06x hash %08x size %d" % (pos, h, n))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
