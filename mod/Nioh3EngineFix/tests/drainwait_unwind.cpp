#include <windows.h>
#include <cassert>
#include <cstdio>
#include <initializer_list>
extern "C" void caveRDWake();
extern "C" void RDComplete(void*){}
int main(){
 alignas(16) DWORD64 stack[128]{};
 // Native entry E; current RSP=E-0x58, saved RBP=E-8; saved RBX/RDI=E+8/+16.
 DWORD64 E=(DWORD64)&stack[64];*(DWORD64*)(E-8)=0x1111;*(DWORD64*)(E+8)=0x2222;*(DWORD64*)(E+16)=0x3333;*(DWORD64*)E=0x12345678;
 for(unsigned off: {0u,3u,8u,15u}){
  CONTEXT c{};c.Rip=(DWORD64)&caveRDWake+off;c.Rsp=E-0x58;c.Rbp=E-8;c.Rbx=0xaaaa;c.Rdi=0xbbbb;
  DWORD64 base=0;auto f=RtlLookupFunctionEntry(c.Rip,&base,nullptr);assert(f);
  void* data=nullptr;DWORD64 estab=0;
  RtlVirtualUnwind(UNW_FLAG_NHANDLER,base,c.Rip,f,&c,&data,&estab,nullptr);
  assert(c.Rip==0x12345678&&c.Rsp==E+8&&c.Rbp==0x1111&&c.Rbx==0x2222&&c.Rdi==0x3333);
 }
 puts("RD inherited-frame RtlVirtualUnwind PASS at entry, call, post-call and tail jump");
}
