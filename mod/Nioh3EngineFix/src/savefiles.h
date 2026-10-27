#pragma once
#include <cstdint>
namespace savefiles {
// Native evidence #41309: TitleMenuMode state3 includes PRESS ANY BUTTON.
// No runtime/INI override until a distinct main-menu predicate is proven.
constexpr bool kMainMenuScreenVerified = false;
// Exact top-mode identity is required; boot/logo/movie and absent player are insufficient.
struct Sample { bool known=false, readable=false, title=false, loading=true; uintptr_t manager=0, mode=0; int saveState=-1, titleState=-1; bool queueEmpty=false, messagesClear=false; int sequenceState=-1; };
inline bool Ready(const Sample& s) { return s.known&&s.readable&&s.title&&!s.loading&&(s.titleState==2||s.titleState==3)&&s.queueEmpty&&s.messagesClear; }
struct Gate {
    uint64_t since=0,last=0;uintptr_t mode=0;
    bool Update(const Sample& s,uint64_t now) {
        const bool good=Ready(s);
        if(!good || (last && (now<last || now-last>500)) || mode!=s.mode){since=0;mode=s.mode;}
        last=now;
        if(!good)return false;
        if(!since)since=now;
        return now>=since&&now-since>=1000;
    }
};
void Configure(uint8_t* base,bool known,const wchar_t* ini,void (*log)(const char*,...));
void Poll();
bool Allowed();
bool Queue(int slot,bool backup);
const char* Note();
}
