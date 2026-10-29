#include "../src/transition_cover.h"
#include <cstdio>
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){using namespace transition_cover;int checks=0;Model m;
 auto begin=[&](uint32_t mission=0x1A5E,uint32_t start=5021112,uint32_t placement=5027806){
  m={};return m.Begin(8,1,mission,start,placement,1000)&&m.Presented(8,1100)&&m.Issue(9);};
 CHECK(begin());m.Entry(9,0x1A5E,0,true);
 // Actual dojo trace: new BossBattle, exact current actor/algo/life/control,
 // but no Character::Create event. The former model stayed opaque until death.
 for(uint64_t t=1200;t<=2000;t+=100)m.Observe(t,9,true,false,true,true,false,true,false,true,true,true);
 CHECK(m.phase==Ready&&m.placementEntry&&m.ActorPlacement()==5027806);
 for(int missing=0;missing<10;++missing){
  CHECK(begin());if(missing!=0)m.Entry(9,0x1A5E,0,true);
  for(uint64_t t=1200;t<=2000;t+=100)m.Observe(t,missing==1?8:9,missing!=2,false,
      missing!=3,missing!=4,missing==5,missing!=6,missing==7,missing!=8,true,missing!=9);
  CHECK(!m.placementEntry&&m.phase!=Ready);
 }
 CHECK(begin());m.Entry(8,0x1A5E,0,true);CHECK(!m.bossEntry);
 m.Entry(9,0x1A5F,0,true);CHECK(!m.bossEntry);
 for(int changed=0;changed<3;++changed){
  CHECK(begin(changed==0?0x1A5F:0x1A5E,changed==1?5021113:5021112,changed==2?5027807:5027806));
  m.Entry(9,m.mission,0,true);
  for(uint64_t t=1200;t<=2000;t+=100)m.Observe(t,9,true,false,true,true,false,true,false,true,true,true);
  CHECK(m.phase==Loading&&!m.placementEntry);
 }
 CHECK(begin());m.Entry(9,0x1A5E,0,true);
 for(uint64_t t=1200;t<=2000;t+=100)m.Observe(t,9,true,false,true,true,false,true,false,true,false,true);
 CHECK(!m.placementEntry&&m.phase!=Ready); // control absent is never a readiness substitute
 CHECK(begin());m.Entry(9,0x1A5E,0,true);
 m.Observe(1200,9,true,false,true,true,false,true,false,true,true,true);
 m.Observe(1300,9,true,false,true,true,false,true,false,true,true,true);CHECK(m.phase==Loading);
 m.Observe(1400,9,true,false,true,true,false,true,false,true,true,false);CHECK(m.settledAt==0);
 m.Observe(1500,9,true,false,true,true,false,true,false,true,true,true);
 m.Observe(1900,9,true,false,true,true,false,true,false,true,true,true);CHECK(m.phase==Loading);
 m.Observe(2000,9,true,false,true,true,false,true,false,true,true,true);CHECK(m.phase==Ready);
 CHECK(begin());m.Entry(9,0x1A5E,0,true);m.Cancel(8);
 m.Observe(1200,9,true,false,true,true,false,true,false,true,true,true);CHECK(m.phase==Cancelled&&!m.placementEntry);
 CHECK(begin());m.Entry(9,0x1A5E,0,true);m.PlayerState(9,true,true,false);
 m.Observe(1200,9,true,false,true,true,false,true,false,true,true,true);CHECK(m.phase==Failed&&m.issued&&m.attempt==9);
 std::printf("PASS %d dojo native-model regression checks\n",checks);
}
