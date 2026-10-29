// padviz_png.h - minimal PNG decoder for the controller-display atlases (padviz_atlas_data.h): 8-bit RGBA, non-interlaced,
// zlib/deflate (stored, fixed and dynamic Huffman blocks), all five scanline filters. Header-only, no CRT allocations
// beyond std::vector, bounds-checked everywhere: a malformed input returns false, never reads or writes out of range.
// Only ever fed our own generated PNGs (tools\gen_padviz_atlas.py writes them with Pillow).
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace padviz_png {

namespace detail {

struct Bits {
    const uint8_t* p;
    size_t n, at = 0;
    uint64_t buf = 0;
    int cnt = 0;
    bool bad = false;
    int Get(int k) {  // k <= 16, LSB first
        while (cnt < k) {
            if (at >= n) { bad = true; return 0; }
            buf |= (uint64_t)p[at++] << cnt;
            cnt += 8;
        }
        const int v = (int)(buf & ((1u << k) - 1));
        buf >>= k;
        cnt -= k;
        return v;
    }
    void Align() { buf >>= (cnt & 7); cnt -= cnt & 7; }
};

struct Huff {
    uint16_t count[16] = {}, sym[320] = {};
    bool Build(const uint8_t* len, int n) {
        memset(count, 0, sizeof(count));
        for (int i = 0; i < n; ++i) ++count[len[i]];
        count[0] = 0;
        int left = 1;
        for (int i = 1; i < 16; ++i) {
            left <<= 1;
            left -= count[i];
            if (left < 0) return false;  // over-subscribed
        }
        uint16_t offs[16] = {};
        for (int i = 1; i < 15; ++i) offs[i + 1] = (uint16_t)(offs[i] + count[i]);
        for (int i = 0; i < n; ++i)
            if (len[i]) sym[offs[len[i]]++] = (uint16_t)i;
        return true;
    }
    int Decode(Bits& b) const {  // canonical decode (zlib puff.c style)
        int code = 0, first = 0, index = 0;
        for (int l = 1; l < 16; ++l) {
            code |= b.Get(1);
            if (b.bad) return -1;
            const int c = count[l];
            if (code - c < first) return sym[index + (code - first)];
            index += c;
            first += c;
            first <<= 1;
            code <<= 1;
        }
        return -1;
    }
};

inline bool Inflate(const uint8_t* src, size_t n, std::vector<uint8_t>& out) {
    static const uint16_t lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const uint8_t lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const uint16_t dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const uint8_t dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    if (n < 2 || (src[0] & 15) != 8 || ((src[0] << 8) | src[1]) % 31) return false;  // zlib header, deflate
    Bits b{src + 2, n - 2};
    for (int last = 0; !last;) {
        last = b.Get(1);
        const int type = b.Get(2);
        if (b.bad) return false;
        if (type == 0) {
            b.Align();
            const unsigned len = (unsigned)b.Get(16), nlen = (unsigned)b.Get(16);
            if (b.bad || (len ^ 0xFFFFu) != nlen) return false;
            for (unsigned k = 0; k < len; ++k) out.push_back((uint8_t)b.Get(8));
            if (b.bad) return false;
            continue;
        }
        if (type == 3) return false;
        Huff lit, dist;
        uint8_t len[320] = {};
        if (type == 1) {
            int i = 0;
            for (; i < 144; ++i) len[i] = 8;
            for (; i < 256; ++i) len[i] = 9;
            for (; i < 280; ++i) len[i] = 7;
            for (; i < 288; ++i) len[i] = 8;
            lit.Build(len, 288);
            for (i = 0; i < 30; ++i) len[i] = 5;
            dist.Build(len, 30);
        } else {
            const int nlen = b.Get(5) + 257, ndist = b.Get(5) + 1, ncode = b.Get(4) + 4;
            if (b.bad || nlen > 286 || ndist > 30) return false;
            uint8_t cl[19] = {};
            for (int i = 0; i < ncode; ++i) cl[order[i]] = (uint8_t)b.Get(3);
            Huff lc;
            if (!lc.Build(cl, 19)) return false;
            for (int i = 0; i < nlen + ndist;) {
                int s = lc.Decode(b);
                if (s < 0) return false;
                if (s < 16) { len[i++] = (uint8_t)s; continue; }
                int rep = 0, val = 0;
                if (s == 16) {
                    if (!i) return false;
                    val = len[i - 1];
                    rep = 3 + b.Get(2);
                } else if (s == 17) rep = 3 + b.Get(3);
                else rep = 11 + b.Get(7);
                if (b.bad || i + rep > nlen + ndist) return false;
                while (rep--) len[i++] = (uint8_t)val;
            }
            if (!lit.Build(len, nlen) || !dist.Build(len + nlen, ndist)) return false;
        }
        for (;;) {
            int s = lit.Decode(b);
            if (s < 0) return false;
            if (s < 256) { out.push_back((uint8_t)s); continue; }
            if (s == 256) break;
            s -= 257;
            if (s >= 29) return false;
            const size_t l = lbase[s] + b.Get(lext[s]);
            const int d = dist.Decode(b);
            if (d < 0 || d >= 30) return false;
            const size_t dd = dbase[d] + b.Get(dext[d]);
            if (b.bad || dd > out.size()) return false;
            const size_t from = out.size() - dd;
            for (size_t k = 0; k < l; ++k) out.push_back(out[from + k]);
        }
    }
    return true;
}

inline uint32_t Be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

}  // namespace detail

// decodes an 8-bit RGBA (colour type 6) or RGB (type 2) PNG into tightly packed RGBA rows
inline bool Decode(const uint8_t* png, size_t n, std::vector<uint8_t>& rgba, int& w, int& h) {
    using namespace detail;
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (n < 33 || memcmp(png, sig, 8)) return false;
    std::vector<uint8_t> z;
    int ctype = -1;
    w = h = 0;
    for (size_t at = 8; at + 12 <= n;) {
        const uint32_t len = Be32(png + at);
        if (len > n - at - 12) return false;
        const uint8_t* t = png + at + 4;
        const uint8_t* d = t + 4;
        if (!memcmp(t, "IHDR", 4)) {
            if (len < 13) return false;
            w = (int)Be32(d);
            h = (int)Be32(d + 4);
            if (d[8] != 8 || (d[9] != 6 && d[9] != 2) || d[10] || d[11] || d[12]) return false;
            ctype = d[9];
        } else if (!memcmp(t, "IDAT", 4)) {
            z.insert(z.end(), d, d + len);
        } else if (!memcmp(t, "IEND", 4)) {
            break;
        }
        at += 12 + (size_t)len;
    }
    if (ctype < 0 || w <= 0 || h <= 0 || w > 8192 || h > 8192) return false;
    const int bpp = ctype == 6 ? 4 : 3;
    const size_t stride = (size_t)w * bpp;
    std::vector<uint8_t> raw;
    raw.reserve((stride + 1) * h);
    if (!Inflate(z.data(), z.size(), raw) || raw.size() < (stride + 1) * h) return false;
    std::vector<uint8_t> cur(stride), prev(stride, 0);
    rgba.assign((size_t)w * h * 4, 0);
    for (int y = 0; y < h; ++y) {
        const uint8_t f = raw[y * (stride + 1)];
        const uint8_t* s = &raw[y * (stride + 1) + 1];
        for (size_t i = 0; i < stride; ++i) {
            const int a = i >= (size_t)bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= (size_t)bpp ? prev[i - bpp] : 0;
            int v = s[i];
            switch (f) {
                case 0: break;
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) >> 1; break;
                case 4: {
                    const int p = a + b - c, pa = p > a ? p - a : a - p, pb = p > b ? p - b : b - p, pc = p > c ? p - c : c - p;
                    v += (pa <= pb && pa <= pc) ? a : pb <= pc ? b : c;
                    break;
                }
                default: return false;
            }
            cur[i] = (uint8_t)v;
        }
        uint8_t* o = &rgba[(size_t)y * w * 4];
        for (int x = 0; x < w; ++x) {
            o[x * 4 + 0] = cur[x * bpp + 0];
            o[x * 4 + 1] = cur[x * bpp + 1];
            o[x * 4 + 2] = cur[x * bpp + 2];
            o[x * 4 + 3] = bpp == 4 ? cur[x * bpp + 3] : 255;
        }
        cur.swap(prev);
    }
    return true;
}

// a decoded sprite atlas: premultiplied RGBA (filtering and mips stay correct at sprite edges) + box-filtered mip chain down to
// 8 texels, all levels tightly packed (level l at px[off[l]], max(1, w >> l) x max(1, h >> l))
struct Atlas {
    std::vector<uint8_t> px;
    int w = 0, h = 0, levels = 0;
    size_t off[12] = {};
};
inline bool DecodeAtlas(const uint8_t* png, size_t n, Atlas& A) {
    std::vector<uint8_t> rgba;
    int w = 0, h = 0;
    if (!Decode(png, n, rgba, w, h)) return false;
    for (size_t i = 0; i < rgba.size(); i += 4) {
        const unsigned a = rgba[i + 3];
        for (int c = 0; c < 3; ++c) rgba[i + c] = (uint8_t)((rgba[i + c] * a + 127) / 255);
    }
    size_t total = 0;
    int lw = w, lh = h, levels = 0;
    while (levels < 12) {
        A.off[levels++] = total;
        total += (size_t)lw * lh * 4;
        if (lw <= 8 || lh <= 8) break;
        lw = lw / 2 > 1 ? lw / 2 : 1;
        lh = lh / 2 > 1 ? lh / 2 : 1;
    }
    A.px.resize(total);
    memcpy(A.px.data(), rgba.data(), rgba.size());
    lw = w;
    lh = h;
    for (int l = 1; l < levels; ++l) {
        const int nw = lw / 2 > 1 ? lw / 2 : 1, nh = lh / 2 > 1 ? lh / 2 : 1;
        const uint8_t* s = &A.px[A.off[l - 1]];
        uint8_t* d = &A.px[A.off[l]];
        for (int y = 0; y < nh; ++y)
            for (int x = 0; x < nw; ++x) {
                const int x0 = 2 * x < lw ? 2 * x : lw - 1, x1 = 2 * x + 1 < lw ? 2 * x + 1 : lw - 1;
                const int y0 = 2 * y < lh ? 2 * y : lh - 1, y1 = 2 * y + 1 < lh ? 2 * y + 1 : lh - 1;
                for (int c = 0; c < 4; ++c) {
                    const unsigned v = s[((size_t)y0 * lw + x0) * 4 + c] + s[((size_t)y0 * lw + x1) * 4 + c] +
                                       s[((size_t)y1 * lw + x0) * 4 + c] + s[((size_t)y1 * lw + x1) * 4 + c];
                    d[((size_t)y * nw + x) * 4 + c] = (uint8_t)((v + 2) / 4);
                }
            }
        lw = nw;
        lh = nh;
    }
    A.w = w;
    A.h = h;
    A.levels = levels;
    return true;
}

}  // namespace padviz_png
