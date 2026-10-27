#pragma once
#include <cstdint>
namespace savefiles {
// Exact top-mode identity is required; boot/logo/movie and absent player are insufficient.
struct Sample { bool known=false, readable=false, title=false, loading=true; uintptr_t manager=0, mode=0; int saveState=-1; };
struct Gate {
    uint64_t since=0,last=0;uintptr_t mode=0;
    bool Update(const Sample& s,uint64_t now) {
        const bool good=s.known&&s.readable&&s.title&&!s.loading;
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
