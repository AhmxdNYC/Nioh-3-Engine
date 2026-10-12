#include <cstdint>
#include <cstdio>
#include <intrin.h>
// Exact length=8 specialization of the retained1199A4 arithmetic. No PSOs,
// resources, cache entries, locks or game memory are retained or accessed.
static uint32_t Stock(uint64_t key, bool crc) {
    uint32_t words[2]={uint32_t(key),uint32_t(key>>32)};
    uint32_t h=crc?0xffffffffu:0;
    for(unsigned i=0;i<2;++i)h=crc?_mm_crc32_u32(h,words[i]):h^words[i];
    return h;
}
static uint32_t Fast(uint64_t key,bool crc) {
    return crc?uint32_t(_mm_crc32_u64(0xffffffffu,key)):uint32_t(key)^uint32_t(key>>32);
}
int main(){
    int cpu[4];__cpuid(cpu,1);if(!(cpu[2]&(1<<20))){std::puts("SKIP: SSE4.2 unavailable");return 2;}
    unsigned cases=0,seedMut=0,upperMut=0,orderMut=0;
    uint64_t rng=0x78197abcdfe01234ull;
    for(unsigned i=0;i<65536+66;++i){
        rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;
        const uint64_t key=i<64?(uint64_t(1)<<i):i==64?0:i==65?~uint64_t(0):rng;
        for(unsigned mode=0;mode<2;++mode){
            const bool crc=mode!=0;
            const auto expected=Stock(key,crc);
            if(expected!=Fast(key,crc)){std::printf("FAIL case %u\n",i);return 1;}
            ++cases;
            if(crc){
                seedMut+=expected!=uint32_t(_mm_crc32_u64(0,key));
                upperMut+=expected!=_mm_crc32_u32(0xffffffffu,uint32_t(key));
                orderMut+=expected!=_mm_crc32_u32(_mm_crc32_u32(0xffffffffu,uint32_t(key>>32)),uint32_t(key));
            }
        }
    }
    if(!seedMut||!upperMut||!orderMut)return 1;
    std::printf("PASS %u hardware arithmetic cases; wrong-seed, missing-upper-half, reversed-order mutations rejected\n",cases);
    return 0;
}
