#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <vector>
#include "../src/autocont_thunk.h"
struct Image { uint64_t result,gpr[6],simd[12]; };
struct Code {
 std::vector<uint8_t> b;
 void E(std::initializer_list<uint8_t> x){b.insert(b.end(),x);}
 void P(uint64_t x){for(int i=0;i<8;++i)b.push_back(uint8_t(x>>(i*8)));}
 void Imm(uint8_t rex,uint8_t op,uint64_t x){E({rex,op});P(x);}
};
int main(){
 auto page=(uint8_t*)VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(page);
 uint64_t align=99;
 // Permission callback intentionally destroys every allowed volatile GPR/SIMD.
 Code callback;callback.E({0x48,0x89,0xE0,0x83,0xE0,0x0F});callback.Imm(0x49,0xBB,(uintptr_t)&align);callback.E({0x49,0x89,0x03});
 for(uint8_t i=0;i<6;++i)callback.E({0x66,0x0F,0xEF,uint8_t(0xC0+i*9)});
 callback.Imm(0x48,0xB9,99);callback.Imm(0x48,0xBA,99);
 for(uint8_t i=0;i<4;++i)callback.Imm(0x49,uint8_t(0xB8+i),99);
 callback.E({0xB0,0,0xC3});const size_t permissionByte=callback.b.size()-2;
 memcpy(page+0x500,callback.b.data(),callback.b.size());
 page[0x600]=0xB0;page[0x601]=0;page[0x602]=0xC3; // original leaf declines
 const size_t size=autocont_thunk::Build(page,(uintptr_t)(page+0x600),(uintptr_t)(page+0x500));assert(size<autocont_thunk::kSwitch);
 memcpy(page+autocont_thunk::kUnwind,autocont_thunk::kUnwindBytes,sizeof(autocont_thunk::kUnwindBytes));
 RUNTIME_FUNCTION runtime{0,static_cast<DWORD>(size),static_cast<DWORD>(autocont_thunk::kUnwind)};
 assert(RtlAddFunctionTable(&runtime,1,reinterpret_cast<DWORD64>(page)));
 size_t afterCall=0;for(size_t i=0;i+1<size;++i)if(page[i]==0xFF&&page[i+1]==0xD0){afterCall=i+2;break;}assert(afterCall);
 alignas(16) uint64_t stack[64]{};stack[0xC8/8]=0x123456789;
 CONTEXT context{};context.ContextFlags=CONTEXT_FULL;context.Rsp=reinterpret_cast<DWORD64>(stack);context.Rip=reinterpret_cast<DWORD64>(page+afterCall);
 void* handler=nullptr;DWORD64 establisher=0;
 RtlVirtualUnwind(UNW_FLAG_NHANDLER,reinterpret_cast<DWORD64>(page),context.Rip,&runtime,&context,&handler,&establisher,nullptr);
 assert(context.Rip==0x123456789 && context.Rsp==reinterpret_cast<DWORD64>(stack)+0xD0);
 Code caller;caller.E({0x53,0x48,0x83,0xEC,0x20,0x48,0x89,0xCB}); // preserve RBX, output pointer
 caller.Imm(0x48,0xB9,11);caller.Imm(0x48,0xBA,12);
 for(uint8_t i=0;i<4;++i)caller.Imm(0x49,uint8_t(0xB8+i),13+i);
 for(uint8_t i=0;i<6;++i){caller.Imm(0x48,0xB8,21+i);caller.E({0x66,0x48,0x0F,0x6E,uint8_t(0xC0+i*8)});}
 caller.Imm(0x48,0xB8,(uintptr_t)page);caller.E({0xFF,0xD0,0x0F,0xB6,0xC0,0x48,0x89,0x03});
 caller.E({0x48,0x89,0x4B,8,0x48,0x89,0x53,16});
 for(uint8_t i=0;i<4;++i)caller.E({0x4C,0x89,uint8_t(0x43+i*8),uint8_t(24+i*8)});
 for(uint8_t i=0;i<6;++i){caller.E({0xF3,0x0F,0x7F,uint8_t(0x83+i*8)});const uint32_t d=56+i*16;for(int j=0;j<4;++j)caller.b.push_back(uint8_t(d>>(j*8)));}
 caller.E({0x48,0x83,0xC4,0x20,0x5B,0xC3});memcpy(page+0x700,caller.b.data(),caller.b.size());
 for(int user=0;user<2;++user)for(int allowed=0;allowed<2;++allowed){
  page[autocont_thunk::kSwitch]=uint8_t(user);page[0x500+permissionByte]=uint8_t(allowed);align=99;
  *reinterpret_cast<uint32_t*>(page+autocont_thunk::kCount)=0;FlushInstructionCache(GetCurrentProcess(),page,4096);
  Image image{};reinterpret_cast<void(*)(Image*)>(page+0x700)(&image);
  assert(image.result==uint64_t(user||allowed));assert(align==uint64_t(user?99:8));
  for(int i=0;i<6;++i){assert(image.gpr[i]==uint64_t(11+i));assert(image.simd[i*2]==uint64_t(21+i));assert(image.simd[i*2+1]==0);}
  assert(*reinterpret_cast<uint32_t*>(page+autocont_thunk::kCount)==uint32_t(user||allowed));
 }
 assert(RtlDeleteFunctionTable(&runtime));VirtualFree(page,0,MEM_RELEASE);puts("PASS actual emitted thunk: all preference/permission branches, callback stack alignment, six GPR and six XMM registers preserved");
}
