#include "../src/drainwait.h"
#include <cassert>
#include <thread>
#include <atomic>
#include <cstdio>
struct Ring { alignas(8) uint8_t bytes[0x38]{};
 volatile LONG& w(){return *reinterpret_cast<volatile LONG*>(bytes+0x24);}
 volatile LONG& r(){return *reinterpret_cast<volatile LONG*>(bytes+0x28);}
 volatile LONG& n(){return *reinterpret_cast<volatile LONG*>(bytes+0x30);}
};
static drainwait::State* state;
static Ring* ring;
static meshletwait::WaitFn nativeWait;
static unsigned mode;
static BOOL WINAPI Inject(volatile VOID* p,PVOID expected,SIZE_T size,DWORD timeout) {
 assert(p==&ring->n() && size==4 && timeout==1);
 if(mode==1) {state->Complete(ring->bytes); return nativeWait(p,expected,size,timeout);}
 if(mode==2) return TRUE; // spurious wake
 if(mode==3) {SetLastError(ERROR_INVALID_PARAMETER); return FALSE;}
 if(mode==4) {state->Complete(ring->bytes); InterlockedIncrement(&ring->w()); InterlockedIncrement(&ring->n()); return TRUE;}
 return nativeWait(p,expected,size,timeout);
}
int main(){
 auto kb=GetModuleHandleW(L"kernelbase.dll");
 nativeWait=reinterpret_cast<meshletwait::WaitFn>(GetProcAddress(kb,"WaitOnAddress"));
 auto wake=reinterpret_cast<meshletwait::WakeFn>(GetProcAddress(kb,"WakeByAddressAll"));
 assert(nativeWait&&wake);
 drainwait::State s; s.wait=Inject;s.wake=wake;s.enabled=true;state=&s;
 Ring a;ring=&a;
 assert(!s.Poll(a.bytes)&&!s.rounds);
 a.w()=1; assert(s.Poll(a.bytes)&&!s.rounds); // reserved, not published
 a.r()=1;a.n()=1; mode=2;assert(s.Poll(a.bytes)); // popped != completed
 mode=0;assert(s.Poll(a.bytes)&&s.timeouts==1); // timeout never success
 mode=1;assert(!s.Poll(a.bytes)&&a.n()==0); // completion before kernel enrollment
 a.n()=1;mode=4;assert(s.Poll(a.bytes)&&a.w()==2&&a.n()==1); // new work after old completion
 a.r()=2;mode=3;assert(s.Poll(a.bytes)&&s.failed==1&&s.errors==1);
 LONG rounds=s.rounds;mode=1;assert(s.Poll(a.bytes)&&s.rounds==rounds&&a.n()==1);
 s.Complete(a.bytes);assert(!s.Poll(a.bytes)); // error fallback still full predicate
 s.failed=0;s.wait=nativeWait;
 for(int i=0;i<100;++i){
  a.w()=1;a.r()=0;a.n()=1; std::atomic<bool> entered{false},finished{false};
  std::thread consumer([&]{while(!entered.load()) _mm_pause(); InterlockedExchange(&a.r(),1); s.Complete(a.bytes);});
  entered=true;while(s.Poll(a.bytes)){} finished=true;consumer.join();assert(finished&&a.n()==0);
  // Native shutdown ordering: empty predicate, consumer joined, then clear/reuse.
  a.w()=0;a.r()=0;assert(!s.Poll(a.bytes));
 }
 // Foreign ring completion cannot satisfy this ring's predicate.
 Ring b;a.w()=a.r()=1;a.n()=1;b.n()=1;s.Complete(b.bytes);assert(s.Poll(a.bytes));s.Complete(a.bytes);assert(!s.Poll(a.bytes));
 s.enabled=false;a.n()=1;rounds=s.rounds;assert(s.Poll(a.bytes)&&s.rounds==rounds);s.Complete(a.bytes);assert(!a.n());
 // Exact paired publication helper: failed second-site verification rolls both back.
 uint8_t x=1,y=2;const uint8_t ox=1,oy=2,nx=3,ny=4;
 meshletwait::Site pair[]={{&x,&ox,&nx,1},{&y,&oy,&ny,1}};int calls=0;
 assert(meshletwait::Publish(pair,2,[&]{if(++calls==1)y=9;return true;})==meshletwait::Publication::Unchanged);
 assert(x==1&&y==2&&calls==2);
 puts("RD PASS: native API enrollment, timeout/spurious/error, reservation gap, new publication, 100 completion/reset races, foreign ring, paired rollback");
}
