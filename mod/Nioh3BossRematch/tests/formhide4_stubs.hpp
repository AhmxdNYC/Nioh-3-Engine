#include <atomic>
#include <cstdint>
#include <cstring>
#include <cmath>
struct SRWLOCK { int x; };
#define SRWLOCK_INIT {}
void AcquireSRWLockExclusive(SRWLOCK*); void ReleaseSRWLockExclusive(SRWLOCK*);
template<class T> bool Rd(uint64_t,T&);
bool Copy(void*,uint64_t,size_t); bool WrBytes(uint64_t,const void*,size_t);
void Log(const char*,...); uint64_t FkPlayer(); double Now();
uint64_t g_base; std::atomic<int> g_fhMode,g_fhPress; std::atomic<uint64_t> g_fs0CapChr; double g_fs0CapT;
struct { uint64_t actorGlobal; bool active; } g_fhEnd;
int FormHideSealPresent(uint64_t,uint64_t);
void FormHideEndBegin(uint64_t,uint64_t); const char* FormHideEndReason(uint64_t,uint64_t,double);
bool FormPin(uint64_t,uint64_t); bool FhSealActTick(uint64_t,uint64_t,double,double);
unsigned PadButtons(void*,void*); bool FormHideCodeMatches(uint64_t,const uint8_t*,size_t);
bool InstallCallRetargetAt(uint8_t*,const char*,void*,void**,const char**);
