#include "../src/transition_cover.h"
#include <cstdio>
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    using namespace transition_cover;int checks=0;Model m;
    auto begin=[&](){m={};return m.Begin(7,2,0x1CBC,67064,65092,1000);};
    CHECK(begin());CHECK(!m.Issue(1));CHECK(!m.Presented(6,1100));CHECK(!m.issued);
    CHECK(m.Presented(7,1100));CHECK(m.Issue(1));CHECK(!m.Issue(2));
    m.Entry(0,0x1CBC,65092,false);CHECK(!m.placementEntry);
    m.Entry(1,0x9999,65092,false);CHECK(!m.placementEntry);
    m.Entry(1,0x1CBC,999,false);CHECK(!m.placementEntry);
    m.Entry(1,0x1CBC,65092,false);m.Entry(1,0x1CBC,0,true);
    CHECK(m.placementEntry&&m.bossEntry);
    // Each missing current predicate resets settling; no global-fight shortcut.
    for(int missing=0;missing<9;++missing){
        CHECK(begin());CHECK(m.Presented(7,1100)&&m.Issue(1));
        m.Entry(1,0x1CBC,65092,false);m.Entry(1,0x1CBC,0,true);
        for(uint64_t t=1200;t<4000;t+=100)m.Observe(t,1,true,false,missing!=0,missing!=1,missing==2,missing!=3,missing==4,missing!=5,missing!=6,missing!=7);
        CHECK(m.phase==(missing==8?Ready:Loading));
    }
    CHECK(begin());CHECK(m.Presented(7,1100)&&m.Issue(1));
    m.Entry(1,0x1CBC,65092,false);m.Entry(1,0x1CBC,0,true);
    m.Observe(1200,1,true,false,true,true,false,true,false,true,true,true);
    m.Observe(1300,1,true,false,true,true,false,true,false,true,true,true);CHECK(m.settledAt==1300);
    m.Observe(2000,1,true,false,true,true,false,true,false,true,true,true);CHECK(!m.settledAt&&m.phase==Loading);
    m.Observe(2100,1,true,false,true,true,false,true,false,true,true,true);
    m.Observe(2500,1,true,false,true,true,false,true,false,true,true,true);CHECK(m.phase==Loading);
    m.Observe(2600,1,true,false,true,true,false,true,false,true,true,true);CHECK(m.phase==Ready);
    CHECK(begin());m.Expire(6000);CHECK(m.phase==Failed&&!m.issued);
    CHECK(begin());CHECK(m.Presented(7,1100)&&m.Issue(1));m.Expire(91000);CHECK(m.phase==Failed&&m.issued&&m.attempt==1);
    CHECK(begin());m.Cancel(6);CHECK(m.Active());m.Cancel(7);CHECK(m.phase==Cancelled&&!m.issued);
    CHECK(begin());CHECK(m.Presented(7,1100)&&m.Issue(1));m.Cancel(7);CHECK(m.phase==Cancelled&&m.issued&&m.attempt==1);
    CHECK(begin());CHECK(m.Presented(7,1100)&&m.Issue(1));m.Observe(1200,1,false,true,true,true,false,true,false,true,true,true);CHECK(m.phase==Failed&&m.attempt==1);
    std::printf("PASS %d transition cover state checks; no native runtime claim\n",checks);
}
