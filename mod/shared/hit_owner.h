#pragma once
// TASK #142938 (human #142934 'Soul core 1 ... sent out flames ur clearly bugging out'): an attack collider's [obj+8] can be an
// intermediate projectile / effect object rather than the character that fired it. The game's own hit apply (327DEC) resolves the
// attacker through 325F40 -> 325F4C; this is that resolver, pure reads, same rules as Training's FpOwnerChar (live #22356, Hino-enma
// fire projectiles: type-4 object whose +0x260 is the boss). Type = [x] & 0xF: 0 = a character (x itself); 4 = +0x260 (type 0 -> it,
// type 3 -> its +0x1E0, else none); 2 / 6 = [[x+0x240]+0x690] when that is type 0; any other type = none.
#include <cstdint>
typedef bool (*HitOwnerRd)(uint64_t a, uint64_t* out);
static inline uint64_t HitOwnerChar(uint64_t x, HitOwnerRd rd) {
    uint64_t tx = 0;
    if (!x || !rd(x, &tx)) return 0;
    const int t = (int)(tx & 0xF);
    if (t == 0) return x;
    if (t == 4) {
        uint64_t o = 0, to = 0;
        if (rd(x + 0x260, &o) && o && rd(o, &to)) {
            if ((to & 0xF) == 0) return o;
            if ((to & 0xF) == 3) { uint64_t c = 0; return rd(o + 0x1E0, &c) ? c : 0; }
        }
        return 0;
    }
    if (t == 2 || t == 6) {
        uint64_t p = 0, c = 0, tc = 0;
        if (!rd(x + 0x240, &p) || !p || !rd(p + 0x690, &c) || !c || !rd(c, &tc)) return 0;
        return (tc & 0xF) == 0 ? c : 0;
    }
    return 0;
}
// The collider's owner as the hit counters see it: [obj+8] itself when it is one of `mine`, else the character it resolves to
// when THAT is one of `mine`, else 0 (not this form's hit). isMine(actor) is the caller's seal/helper test.
template <class IsMine>
static inline uint64_t HitOwnerMine(uint64_t own, HitOwnerRd rd, IsMine isMine, bool* viaChain) {
    if (viaChain) *viaChain = false;
    if (!own) return 0;
    if (isMine(own)) return own;
    const uint64_t c = HitOwnerChar(own, rd);
    if (c && c != own && isMine(c)) { if (viaChain) *viaChain = true; return c; }
    return 0;
}
