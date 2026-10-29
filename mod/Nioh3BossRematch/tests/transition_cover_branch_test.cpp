#include "../src/transition_cover.h"
#include <cstdio>
#include <initializer_list>
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){using namespace transition_cover;int checks=0;Model m;
 auto begin=[&](uint32_t mission=0x1DEA,uint32_t start=4095000){m={};return m.Begin(1,2,mission,start,4092708,1000)&&m.Presented(1,1100)&&m.Issue(2);};
 auto sample=[&](uint64_t t,bool loading=false){m.Observe(t,2,true,false,true,true,loading,true,false,true,true,true);};
 // Exact same-attempt alternate first encounter formerly timed out forever.
 for(uint32_t actor:{4092708u,4097963u}){
  CHECK(begin());m.Entry(2,0x1DEA,actor,false);m.Entry(2,0x1DEA,0,true);
  CHECK(m.ActorPlacement()==actor&&m.placement==4092708);
  for(uint64_t t=1200;t<=2000;t+=100)sample(t);
  CHECK(m.phase==Ready);
 }
 // No broad mission/actor/attempt or event-order shortcut.
 CHECK(begin());m.Entry(2,0x1DEA,999,false);m.Entry(2,0x1DEA,0,true);CHECK(!m.bossEntry&&!m.placementEntry);
 m.Entry(1,0x1DEA,4097963,false);m.Entry(2,0x1111,4097963,false);CHECK(!m.placementEntry);
 m.Entry(2,0x1DEA,4097963,false);for(uint64_t t=1200;t<=2000;t+=100)sample(t);CHECK(m.phase==Loading&&!m.bossEntry);
 CHECK(begin(0x1111));m.Entry(2,0x1111,4097963,false);CHECK(!m.placementEntry);
 CHECK(begin(0x1DEA,4095001));m.Entry(2,0x1DEA,4097963,false);CHECK(!m.placementEntry);
 CHECK(begin());m.Entry(2,0x1DEA,4097963,false);m.Entry(2,0x1DEA,4092708,false);CHECK(m.phase==Failed&&m.ActorPlacement()==4097963);
 CHECK(begin(0x1111));m.Entry(2,0x1111,0,true);CHECK(m.bossEntry); // unrelated routes retain legacy event ordering
 // Known native death is terminal, not a readiness delay; ownership remains.
 CHECK(begin());m.PlayerState(1,true,true,false);CHECK(m.Active());
 m.PlayerState(2,false,true,false);m.PlayerState(2,true,false,false);m.PlayerState(2,true,true,true);CHECK(m.Active());
 m.PlayerState(2,true,true,false);CHECK(m.phase==Failed&&m.issued&&m.attempt==2);
 m.Entry(2,0x1DEA,4097963,false);sample(1200);CHECK(m.phase==Failed);
 // A second intro load BEFORE playable encounter is permitted. A reload after
 // observed playable encounter terminates presentation, never auto-requests.
 CHECK(begin());sample(1200,true);m.Entry(2,0x1DEA,4097963,false);sample(1300,true);CHECK(m.Active());
 m.Entry(2,0x1DEA,0,true);sample(1400,true);CHECK(m.Active()&&!m.encounterObserved);
 sample(1500);CHECK(m.encounterObserved&&m.phase==Loading);sample(1600,true);
 CHECK(m.phase==Failed&&m.issued&&m.attempt==2);
 CHECK(begin());m.Entry(2,0x1DEA,4097963,false);m.Entry(2,0x1DEA,0,true);
 sample(1200);m.Observe(1300,2,true,false,false,true,true,true,false,true,true,true);CHECK(m.Active());
 // Fixed deadline and exact existing readiness predicates remain required.
 CHECK(m.deadline==91000);m.Expire(91000);CHECK(m.phase==Failed);
 for(int missing=0;missing<8;++missing){
  CHECK(begin());m.Entry(2,0x1DEA,4097963,false);m.Entry(2,0x1DEA,0,true);
  for(uint64_t t=1200;t<=2000;t+=100)m.Observe(t,2,true,false,missing!=0,missing!=1,missing==2,missing!=3,missing==4,missing!=5,missing!=6,missing!=7);
  CHECK(m.phase==Loading);
 }
 std::printf("PASS %d actual-model branch/death/reload checks\n",checks);
}
