#include <assert.h>
#include <stdio.h>
#include "../src/frame_meter_ki.h"
#include "../src/autobind.h"
int main(void) {
 assert(AbHumanFamily("CP_HUMAN_COMMON")==1 && AbHumanFamily("CP_HUMAN_KATANA")==2 && !AbHumanFamily("CE_BOSS"));
 assert(AbComposedHuman(0,1,1,80,100));
 assert(!AbComposedHuman(1,1,1,100,100) && !AbComposedHuman(0,0,1,100,100) && !AbComposedHuman(0,1,0,100,100) && !AbComposedHuman(0,1,1,79,100) && !AbComposedHuman(0,1,1,0,0));
 assert(FmKiEligible(1,1,1,0,-10,0) && FmKiEligible(1,1,1,0,10,-5));
 assert(!FmKiEligible(1,0,1,0,-10,0) && !FmKiEligible(0,1,1,0,-10,0) && !FmKiEligible(1,1,0,0,-10,0) && !FmKiEligible(1,1,1,1,-10,0));
 assert(!FmKiEligible(1,1,1,0,-0.5f,0) && !FmKiEligible(1,1,1,0,0,0));
 FmMeter m; FmKi k; FmReset(&m); FmKiReset(&k);
 FmKiSample(&k,&m,0,1,0); FmSample(&m,0,1,0,0);
 FmKiSample(&k,&m,20,1,1); FmSample(&m,20,1,1,0);
 assert(FmKiView(&k,&m,0)==0 && FmKiView(&k,&m,1)==1);
 FmKiSample(&k,&m,21,1,0); FmSample(&m,21,1,1,0);
 assert(FmKiView(&k,&m,1)==1);
 FmKiSample(&k,&m,40,1,0); FmSample(&m,40,1,0,0);
 assert(!FmKiView(&k,&m,2));
 FmKiSample(&k,&m,60,2,0); FmSample(&m,60,2,0,0);
 assert(m.haveLast && FmKiView(&k,&m,1) && !FmKiView(&k,&m,2));
 FmReset(&m); FmKiReset(&k); assert(!FmKiView(&k,&m,1));
 for(int i=0;i<800;++i) { FmKiSample(&k,&m,i*17.0,3,i%2); FmSample(&m,i*17.0,3,1,0); }
 assert(k.last==m.lastCell && FmKiView(&k,&m,FM_CELLS-1)==1);
 puts("frame_meter_ki_test: eligibility, composed gate, shared clock/freeze/ring/reset PASS");
}
