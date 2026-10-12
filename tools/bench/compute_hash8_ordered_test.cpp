#include <cstdint>
#include <cstdio>
#include <cstring>
#include <intrin.h>
#include <Windows.h>
extern "C" uint64_t ComputeHash8Ordered(const void*,const void*,uint32_t);
static unsigned forwarded=0;
static uint64_t Original(const void* c,const void* p,uint32_t n){
    if(c||p||n==8)return 0;
    ++forwarded;return 0xabcdef9876543210ull;
}
extern "C" { decltype(&Original) ComputeHash8Original=&Original; }
static uint64_t Expected(uint64_t k,bool crc){
    return crc?_mm_crc32_u32(_mm_crc32_u32(0xffffffffu,uint32_t(k)),uint32_t(k>>32)):
        uint32_t(k)^uint32_t(k>>32);
}
int main(){
    int cpu[4];__cpuid(cpu,1);if(!(cpu[2]&(1<<20)))return 2;
    uint8_t ctx[0x2491]{};uint8_t keys[16]{};uint64_t rng=0x8f179412357abcdeull;
    unsigned checked=0;
    for(unsigned j=0;j<4096;++j){
        rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;
        for(unsigned alignment=0;alignment<8;++alignment){
            std::memcpy(keys+alignment,&rng,8);
            for(unsigned mode=0;mode<3;++mode){
                ctx[0x2490]=uint8_t(mode==2?255:mode);
                if(ComputeHash8Ordered(ctx,keys+alignment,8)!=Expected(rng,mode!=0))return 1;
                ++checked;
            }
        }
    }
    // Non-eight fallback must not touch null context/key; return full original RAX.
    for(uint32_t n=0;n<130;++n)if(n!=8 && ComputeHash8Ordered(nullptr,nullptr,n)!=0xabcdef9876543210ull)return 1;
    // End-of-readable-page input excludes speculative extra key reads.
    SYSTEM_INFO si{};GetSystemInfo(&si);
    auto* pages=static_cast<uint8_t*>(VirtualAlloc(nullptr,si.dwPageSize*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!pages)return 1;DWORD old=0;
    if(!VirtualProtect(pages+si.dwPageSize,si.dwPageSize,PAGE_NOACCESS,&old))return 1;
    std::memcpy(pages+si.dwPageSize-8,&rng,8);
    for(unsigned mode=0;mode<2;++mode){ctx[0x2490]=uint8_t(mode);if(ComputeHash8Ordered(ctx,pages+si.dwPageSize-8,8)!=Expected(rng,mode!=0))return 1;++checked;}
    VirtualFree(pages,0,MEM_RELEASE);
    std::printf("PASS ordered assembly: %u values/alignment/modes/page-boundary; %u untouched-argument fallbacks\n",checked,forwarded);
}
