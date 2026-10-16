#include <Windows.h>
#include <intrin.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
using Hash=uint64_t(*)(const void*,const void*,uint32_t);
extern "C" uint64_t ComputeHash8Ordered(const void*,const void*,uint32_t);
extern "C" { Hash ComputeHash8Original=nullptr; }
struct alignas(64) Context {uint8_t bytes[0x24c0]{};};
static Context contexts[2];
static uint64_t keys[32];
static volatile uint64_t sink=0;
__declspec(noinline) static uint64_t Calls(Hash fn,unsigned count,unsigned mode,unsigned mask){
    uint64_t sum=0;
    for(unsigned i=0;i<count;++i){
        const unsigned c=mode==2?(i&1):mode;
        sum+=fn(contexts[c].bytes,&keys[i&mask],8);
    }
    return sum;
}
static double Measure(Hash fn,unsigned count,unsigned mode,unsigned mask,LONGLONG frequency){
    LARGE_INTEGER a{},b{};QueryPerformanceCounter(&a);
    const uint64_t value=Calls(fn,count,mode,mask);
    QueryPerformanceCounter(&b);sink=value;
    return double(b.QuadPart-a.QuadPart)*1e9/double(frequency)/count;
}
int main(int argc,char** argv){
    if(argc!=2)return 1;
    int cpu[4];__cpuid(cpu,1);if(!(cpu[2]&(1<<20))){std::puts("SSE4.2 missing");return 2;}
    std::ifstream input(argv[1],std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
    if(bytes.size()!=164)return 1;
    void* code=VirtualAlloc(nullptr,bytes.size(),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!code)return 1;
    std::memcpy(code,bytes.data(),bytes.size());DWORD previous=0;
    if(!VirtualProtect(code,bytes.size(),PAGE_EXECUTE_READ,&previous)||!FlushInstructionCache(GetCurrentProcess(),code,bytes.size()))return 1;
    const Hash native=reinterpret_cast<Hash>(code);ComputeHash8Original=native;
    contexts[1].bytes[0x2490]=1;uint64_t rng=0x356781efa39876bcull;
    for(auto& k:keys){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;k=rng;}
    unsigned checked=0;
    for(unsigned j=0;j<4096;++j){
        rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;
        uint8_t raw[16]{};
        for(unsigned align=0;align<8;++align){
            std::memcpy(raw+align,&rng,8);
            for(unsigned c=0;c<2;++c){
                if(native(contexts[c].bytes,raw+align,8)!=ComputeHash8Ordered(contexts[c].bytes,raw+align,8))return 1;
                ++checked;
            }
        }
    }
    std::printf("checks,%u\n",checked);
    char brand[49]{};for(unsigned k=0;k<3;++k){__cpuid(cpu,int(0x80000002u+k));std::memcpy(brand+k*16,cpu,16);}
    std::printf("cpu,%s\n",brand);
    LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
    constexpr unsigned count=5000000;
    for(unsigned mode=0;mode<3;++mode)for(unsigned k=0;k<2;++k){
        const unsigned mask=k?31:0;
        sink=Calls(native,10000,mode,mask);sink=Calls(&ComputeHash8Ordered,10000,mode,mask);
        for(unsigned sample=0;sample<9;++sample){
            double a,b;
            if(sample&1){b=Measure(&ComputeHash8Ordered,count,mode,mask,frequency.QuadPart);a=Measure(native,count,mode,mask,frequency.QuadPart);}
            else {a=Measure(native,count,mode,mask,frequency.QuadPart);b=Measure(&ComputeHash8Ordered,count,mode,mask,frequency.QuadPart);}
            std::printf("sample,%u,%u,%u,%.6f,%.6f\n",mode,mask,sample,a,b);
        }
    }
    VirtualFree(code,0,MEM_RELEASE);
}
