#include "../src/savefiles.cpp"
#include <cstdlib>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 using namespace savefiles;
 Gate g;Sample s;s.known=s.readable=s.title=true;s.loading=false;s.mode=1;
 CHECK(!g.Update(s,100));for(int t=200;t<1100;t+=100)CHECK(!g.Update(s,t));CHECK(g.Update(s,1100));
 s.title=false;CHECK(!g.Update(s,1200));s.title=true;CHECK(!g.Update(s,1300));
 CHECK(!g.Update(s,2500)); // stale sampling never preserves admission
 s.loading=true;CHECK(!g.Update(s,2600));s.loading=false;s.known=false;CHECK(!g.Update(s,2700));
 base=(uint8_t*)VirtualAlloc(nullptr,0x4800000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(base);
 uintptr_t manager[8]{},mode[2]{},stack[1]{};unsigned char gm[0x600]{};int sm[8]{};
 *(uintptr_t*)(base+0x4709B50)=(uintptr_t)manager;
 *(uintptr_t*)(base+0x4759910)=(uintptr_t)gm;*(uintptr_t*)(base+0x45C8480)=(uintptr_t)sm;
 manager[0]=(uintptr_t)stack;manager[1]=(uintptr_t)(stack+1);stack[0]=(uintptr_t)mode;
 mode[1]=(uintptr_t)base+0x44C91D0;known=true;
 auto a=Read();CHECK(a.known&&a.readable&&a.title&&!a.loading);
 mode[1]=(uintptr_t)base+0x44C91E0;CHECK(!Read().title); // TitleMode is NOT TitleMenuMode
 mode[1]=(uintptr_t)base+0x44C91F0;CHECK(!Read().title); // movie
 mode[1]=(uintptr_t)base+0x44C91D0;gm[0x58A]=1;CHECK(Read().loading);
 manager[1]=manager[0];CHECK(!Read().readable);manager[1]=manager[0]+5000;CHECK(!Read().readable);
 known=false;CHECK(!Read().readable);VirtualFree(base,0,MEM_RELEASE);base=nullptr;
 puts("ALL PASSED: title gate stability, stale/loading/unknown denial, exact top-mode identity and malformed stack reads");return 0;
}
