// No native APIs. Compile/run only after the live headroom hold ends.
#include "../rematch_menu_bridge.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace rematch_menu_bridge;
int A(uint64_t,uint64_t*) { return Accepted; }
int D(uint64_t) { return Accepted; }
int P(uint64_t,uint64_t,uint32_t) { return Accepted; }
int C(uint64_t,Snapshot*,uint32_t) { return Accepted; }
int V(uint64_t,uint64_t){return Accepted;}
int main() {
 Api a{sizeof(Api),kVersion,kRequiredCapabilities,0,A,D,P,C,V};assert(ValidApi(a));
 auto b=a;b.version++;assert(!ValidApi(b));b=a;b.size--;assert(!ValidApi(b));
 b=a;b.capabilities&=~ExclusiveHost;assert(!ValidApi(b));b=a;b.CopySnapshot=nullptr;assert(!ValidApi(b));
 Snapshot s{};s.size=sizeof(s);s.version=kVersion;s.session=7;s.generation=1;s.workerTickMs=100;s.flags=HostedReady|Open;s.rowCount=1;
 assert(ValidSnapshot(s,7));assert(!ValidSnapshot(s,8));assert(!ValidSnapshot(s,0));
 auto t=s;t.workerTickMs=0;assert(!ValidSnapshot(t,7));t=s;t.generation=0;assert(!ValidSnapshot(t,7));t=s;t.flags=Open;assert(!ValidSnapshot(t,7));
 t=s;t.flags|=0x80000000;assert(!ValidSnapshot(t,7));t=s;t.rowCount=25;assert(!ValidSnapshot(t,7));
 t=s;memset(t.rows[0].text,'x',112);assert(!ValidSnapshot(t,7));
 t=s;memset(t.footer,'x',240);assert(!ValidSnapshot(t,7));t=s;memset(t.toast,'x',160);assert(!ValidSnapshot(t,7));
 t=s;t.rows[0].selected=2;assert(!ValidSnapshot(t,7));
 for (uint32_t phase=1;phase<=3;++phase) {
  t=s;t.coverPhase=phase;t.coverToken=77;t.coverDeadline=90000;
  assert(!ValidSnapshot(t,7)); // unknown/contradictory cannot refresh the host lease
  t.flags|=CoverActive;assert(ValidSnapshot(t,7));
 }
 Queue<2> q;QueuedAction v{};assert(!q.Pop(v));assert(!q.Push({0,1,Accept}));assert(!q.Push({7,1,99}));
 assert(!q.Push({7,1,CloseMenu})); // close requires a separate priority mailbox
 assert(q.Push({7,1,Accept}));assert(q.Push({7,2,Back}));assert(!q.Push({7,3,Down}));
 assert(q.Pop(v)&&v.action==Accept&&v.generation==1);assert(q.Push({7,3,Down}));
 assert(q.Pop(v)&&v.action==Back&&v.generation==2);assert(q.Pop(v)&&v.action==Down&&v.generation==3);assert(!q.Pop(v));
 assert(q.Push({7,4,Accept}));q.Clear();assert(!q.Pop(v));
 puts("PASS ABI bounds/serialized queue fixtures; not runtime handshake/concurrency proof");
}
