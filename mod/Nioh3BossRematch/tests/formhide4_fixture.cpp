#include "formhide4_stubs.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <utility>
static unsigned char playerMem[0xB500], otherMem[0xB500], rootMem[0x400], otherRoot[0x400];
static std::vector<std::pair<uint64_t,size_t>> regions;
static uint64_t pl; static double tm; static bool edge; static unsigned nativeParts;
static bool valid(uint64_t a,size_t n) { for(auto r:regions) if(a>=r.first && n<=r.second && a-r.first<=r.second-n)return true;return false; }
bool Copy(void* out,uint64_t a,size_t n){if(!valid(a,n))return false;memcpy(out,(void*)a,n);return true;}
template<class T> bool Rd(uint64_t a,T& v){return Copy(&v,a,sizeof(v));}
bool WrBytes(uint64_t a,const void* v,size_t n){if(!valid(a,n))return false;memcpy((void*)a,v,n);return true;}
template<class T> void put(uint64_t a,T v){assert(WrBytes(a,&v,sizeof(v)));}
template<class T> T get(uint64_t a){T v{};assert(Rd(a,v));return v;}
void AcquireSRWLockExclusive(SRWLOCK*){} void ReleaseSRWLockExclusive(SRWLOCK*){}
void Log(const char*,...){} uint64_t FkPlayer(){return pl;} double Now(){return tm;}
int FormHideSealPresent(uint64_t,uint64_t){return 1;}
void FormHideEndBegin(uint64_t,uint64_t){g_fhEnd.actorGlobal=1;g_fhEnd.active=true;}
const char* FormHideEndReason(uint64_t,uint64_t,double){return nullptr;}
bool FormPin(uint64_t,uint64_t){return true;}
bool FhSealActTick(uint64_t,uint64_t,double,double){bool x=edge;edge=false;return x;}
unsigned PadButtons(void*,void*){return 0;}
bool FormHideCodeMatches(uint64_t,const uint8_t*,size_t){return false;}
bool InstallCallRetargetAt(uint8_t*,const char*,void*,void**,const char**){return false;}
#include "formhide4_extracted.cpp"
void parts(uint64_t chr){++nativeParts;if(chr!=pl)return;uint64_t root=get<uint64_t>(chr+0x20);assert(get<uint8_t>(chr+0xB440)==1);assert(get<float>(chr+0xB444)==1.f);assert(get<float>(root+0x3A8)==0.f);put(root+0x3A8,.6f);}
void multiplier(uint64_t,uint64_t root){put(root+0x3A8,.8f);}
int main(){
 uint64_t p=(uint64_t)playerMem,r=(uint64_t)rootMem,o=(uint64_t)otherMem,q=(uint64_t)otherRoot;
 regions={{p,sizeof playerMem},{r,sizeof rootMem},{o,sizeof otherMem},{q,sizeof otherRoot}};pl=p;put(p+0x20,r);put(o+0x20,q);
 put(p+0xB440,uint8_t(0));put(p+0xB444,.2f);put(r+0x3A8,.9f);put(r+0x248,.65f);
 g_fh4Ready=true;g_fhMode=4;g_fhPress=2;edge=true;tm=10;FormHide4Tick(p);
 assert(g_fh4.owner==p && get<float>(r+0x3A8)==0.f && get<float>(p+0xB444)==1.f);
 g_fh4PartsOrig=(void*)&parts;g_fh4MultiplierOrig=(void*)&multiplier;
 HkFh4Parts(p);assert(nativeParts==1 && get<float>(r+0x3A8)==0.f && g_fh4.multiplier==.6f);
 HkFh4Multiplier(p,r);assert(get<float>(r+0x3A8)==0.f && g_fh4.multiplier==.8f);
 HkFh4Multiplier(o,q);assert(get<float>(q+0x3A8)==.8f);
 g_fhMode=0;FormHide4Tick(p);assert(g_fh4.owner==0 && get<float>(r+0x3A8)==.8f && get<float>(p+0xB444)==.2f && get<uint8_t>(p+0xB440)==0 && get<float>(r+0x248)==.65f);
 g_fhMode=4;edge=true;tm=20;FormHide4Tick(p);tm=23;FormHide4Tick(p);assert(!g_fh4.owner && get<float>(r+0x3A8)==.8f);
 edge=true;tm=30;FormHide4Tick(p);put(p+0x20,q);put(q+0x3A8,.7f);FormHide4Tick(p);assert(g_fh4Fault);FormHide4Tick(p);assert(!g_fh4.owner && get<float>(q+0x3A8)==.7f);
 put(p+0x20,r);edge=true;tm=40;FormHide4Tick(p);pl=o;const auto old=get<float>(r+0x3A8);FormHide4Tick(o);assert(!g_fh4.owner && get<float>(r+0x3A8)==old && get<float>(q+0x3A8)==.7f);
 puts("PASS: press arm, pre/post native update, multiplier reassert, unrelated actor, exact restore with k6 preserved, no-seal timeout, root replacement, player replacement (8 cases)");
}
