#include "formhide4_stubs.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <utility>
static unsigned char playerMem[0xB500], otherMem[0xB500], rootMem[0x400], otherRoot[0x400];
static unsigned char shellMem[0x40], shellRender[0x40], nextRender[0x40], alternateSet[0xA000];
static unsigned nativeAlphaCalls;
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
void alphaWriter(uint64_t, uint8_t flag, uint64_t context){
 assert(flag==7);assert(context);++nativeAlphaCalls;
 put((uint64_t)shellRender+0xC,.75f);
}
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
 // The actual alpha writer may regenerate nonzero alpha even after the outer hold.
 const uint64_t shell=(uint64_t)shellMem, sr=(uint64_t)shellRender, alt=(uint64_t)alternateSet, nr=(uint64_t)nextRender;
 regions.push_back({shell,sizeof shellMem});regions.push_back({sr,sizeof shellRender});regions.push_back({alt,sizeof alternateSet});regions.push_back({nr,sizeof nextRender});
 put(p+0x9F0,p);put(p+0x9F0+0x20+0x9590,shell);put(shell+0x20,sr);put(shell,uint32_t((0x58C2A4u<<4)|3));
 g_fh4AlphaOrig=(void*)&alphaWriter;
 g_fhMode=4;edge=true;tm=15;FormHide4Tick(p);
 HkFh4Alpha(p+0x9F0,7,r);assert(nativeAlphaCalls==1 && !g_fh4Fault && get<float>(sr+0xC)==0);
 // The optional active parts set takes precedence only when its enabled dword is nonzero.
 put(p+0xA950,alt);put(p+0xA95C,uint32_t(1));put(alt+0x9590,shell);put(p+0xA10+0x9590,uint64_t(0));
 HkFh4Alpha(p+0x9F0,7,r);assert(nativeAlphaCalls==2 && !g_fh4Fault && get<float>(sr+0xC)==0);
 put(p+0xA95C,uint32_t(0));HkFh4Alpha(p+0x9F0,7,r);assert(!g_fh4Fault && get<float>(sr+0xC)==.75f); // inactive override ignored
 put(p+0xA95C,uint32_t(1));
 // No owner means pass-through, and native output comes back on mode release.
 put(o+0x9F0,o);HkFh4Alpha(o+0x9F0,7,q);assert(get<float>(sr+0xC)==.75f);
 g_fhMode=0;FormHide4Tick(p);HkFh4Alpha(p+0x9F0,7,r);assert(!g_fh4.owner && get<float>(sr+0xC)==.75f);
 // Unknown FormBody replacements and shared puppet roots must never be clamped.
 g_fhMode=4;edge=true;tm=16;FormHide4Tick(p);put(shell,uint32_t((0x5129Bu<<4)|3));
 HkFh4Alpha(p+0x9F0,7,r);assert(g_fh4Fault && get<float>(sr+0xC)==.75f);FormHide4Tick(p);assert(!g_fh4.owner);
 put(shell,uint32_t((0x295A89u<<4)|3));edge=true;tm=17;FormHide4Tick(p);g_fh4.seal=o;put(o+0x20,r);
 HkFh4Alpha(p+0x9F0,7,r);assert(g_fh4Fault && get<float>(sr+0xC)==.75f);FormHide4Tick(p);assert(!g_fh4.owner);put(o+0x20,q);
 edge=true;tm=18;FormHide4Tick(p);put(shell+0x20,nr);
 HkFh4Alpha(p+0x9F0,7,r);assert(!g_fh4Fault && get<float>(nr+0xC)==0.f); // replacement is resolved fresh, old pointer untouched
 put(shell+0x20,sr);g_fhMode=0;FormHide4Tick(p);
 g_fhMode=4;edge=true;tm=20;FormHide4Tick(p);tm=23;FormHide4Tick(p);assert(!g_fh4.owner && get<float>(r+0x3A8)==.8f);
 edge=true;tm=30;FormHide4Tick(p);put(p+0x20,q);put(q+0x3A8,.7f);FormHide4Tick(p);assert(g_fh4Fault);FormHide4Tick(p);assert(!g_fh4.owner && get<float>(q+0x3A8)==.7f);
 put(p+0x20,r);edge=true;tm=40;FormHide4Tick(p);pl=o;const auto old=get<float>(r+0x3A8);FormHide4Tick(o);assert(!g_fh4.owner && get<float>(r+0x3A8)==old && get<float>(q+0x3A8)==.7f);
 puts("PASS: shell native overwrite, alternate active set, disabled override, unrelated actor, release/recompute, unknown model refusal, shared-root refusal, fresh model lookup (8 shell cases)");
 puts("PASS: press arm, pre/post native update, multiplier reassert, unrelated actor, exact restore with k6 preserved, no-seal timeout, root replacement, player replacement (8 cases)");
}
