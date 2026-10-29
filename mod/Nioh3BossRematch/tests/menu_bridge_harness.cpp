// Actual provider callbacks/worker with a small UI model. No game/window/thread.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include "../../rematch_menu_bridge.h"
namespace fixture {
std::atomic<bool> g_open{false}, g_hostRequested{false};
std::atomic<uint32_t> g_padEdges{0};
std::atomic<int> g_toggleOwner{-1}, g_owner{-1};
std::atomic<unsigned> g_latchEpoch{0};
HWND g_wnd = nullptr;
int calls = 0, model = 0;
void Toggle(bool open) { g_open = open; }
namespace rematch {
struct UiRow { char text[112]; uint32_t color; bool sel; };
void UiMove(int n) { model += n; }
void UiChange(int n) { model += n; }
bool UiAct() { ++calls; return true; }
bool UiActHosted(){return UiAct();}
struct CoverInfo{uint64_t token,deadline;uint32_t phase;char reason[160];};
CoverInfo cover{};uint64_t acknowledged=0,cancelled=0;bool coverReadable=true;
bool CoverSnapshot(CoverInfo& out){if(!coverReadable)return false;out=cover;return true;}
void CoverPresented(uint64_t token){acknowledged=token;}
void CoverCancel(uint64_t token){cancelled=token;}
int testCalls=0;bool testAllow=true;
bool UiTestFight(const char*,uint64_t& token,char* reason,size_t n){
 ++testCalls;if(!testAllow){snprintf(reason,n,"normal gate refused");return false;}
 token=++cover.token;cover.phase=1;snprintf(reason,n,"awaiting actual covered Present");return true;
}
bool UiBack() { return true; }
bool UiTakeHostMenu() { return false; }
void UiClearHostMenu() {}
void UiOpenAt(int) {}  // user #13223 first-page direct opens (OpenRematch / OpenPractice); the fixture model has no pages
int UiFill(UiRow* rows, int, char* footer, size_t size) {
    snprintf(rows[0].text, sizeof(rows[0].text), "model %d", model);
    rows[0].color = 123; rows[0].sel = true; snprintf(footer, size, "native rewards"); return 1;
}
bool Toast(char* out, size_t, uint32_t&) { out[0] = 0; return false; }
}
void GateReqTick() {} // no native gate requests in this provider fixture
#include "../src/menu_bridge_impl.h"
}
#define CHECK(c) do { ++checks; if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
int main() {
    using namespace fixture;
    int checks = 0; uint64_t session = 0, other = 0;
    CHECK(BridgeAttach(1, &session) == mb::Unavailable);
    g_bridgeAvailable = true;
    CHECK(BridgeAttach(1, &session) == mb::Accepted && session && g_hostRequested);
    CHECK(BridgeAttach(1, &other) == mb::Accepted && other == session);
    CHECK(BridgeAttach(2, &other) == mb::Busy);
    CHECK(BridgePost(session, 1, mb::OpenMenu) == mb::Busy); // attach's initial close barrier
    HostedTick();
    mb::Snapshot s{}; s.size = sizeof(s); s.version = mb::kVersion;
    CHECK(BridgeSnapshot(session, &s, sizeof(s)) == mb::Accepted && mb::ValidSnapshot(s, session) && (s.flags & mb::HostedReady));
    AcquireSRWLockExclusive(&g_bridgeLock);
    CHECK(BridgeSnapshot(session, &s, sizeof(s)) == mb::Busy);
    CHECK(BridgePost(session, s.generation, mb::Accept) == mb::Busy);
    ReleaseSRWLockExclusive(&g_bridgeLock);
    CHECK(BridgePost(session, s.generation, mb::OpenMenu) == mb::Accepted);
    HostedTick(); CHECK(g_open);
    CHECK(BridgeSnapshot(session, &s, sizeof(s)) == mb::Accepted && (s.flags & mb::Open));
    CHECK(BridgePost(session, s.generation, mb::Accept) == mb::Accepted);
    ++model; // async model change after display/queue but before worker dispatch
    HostedTick(); CHECK(calls == 0 && g_open);
    CHECK(BridgeSnapshot(session, &s, sizeof(s)) == mb::Accepted);
    for (int i = 0; i < 32; ++i) CHECK(BridgePost(session, s.generation, mb::Down) == mb::Accepted);
    CHECK(BridgePost(session, s.generation, mb::Down) == mb::Busy);
    CHECK(BridgePost(session, 0, mb::CloseMenu) == mb::Accepted);
    HostedTick(); CHECK(!g_open && !g_bridgeQueue.count && !calls);
    CHECK(BridgeSnapshot(session, &s, sizeof(s)) == mb::Accepted);
    CHECK(BridgePost(session, s.generation, mb::OpenMenu) == mb::Accepted);
    HostedTick(); CHECK(BridgeSnapshot(session, &s, sizeof(s)) == mb::Accepted);
    CHECK(BridgePost(session, s.generation, mb::Accept) == mb::Accepted);
    HostedTick(); CHECK(calls == 1 && !g_open);
    CHECK(BridgeSnapshot(session, &s, sizeof(s)) == mb::Accepted && (s.flags & mb::CloseRequested));
    CHECK(BridgePost(session, s.generation - 1, mb::OpenMenu) == mb::StaleGeneration);
    rematch::cover.token=77;rematch::cover.deadline=GetTickCount64()+90000;rematch::cover.phase=1;
    HostedTick();
    CHECK(BridgeSnapshot(session,&s,sizeof(s))==mb::Accepted && (s.flags&mb::CoverActive));
    // Ally token1: one failed try-lock must not publish fresh inactive phase3.
    rematch::cover.phase=3;HostedTick();
    g_bridgeSnapshot.workerTickMs=GetTickCount64()-50;
    CHECK(BridgeSnapshot(session,&s,sizeof(s))==mb::Accepted);
    const auto known=s;
    rematch::coverReadable=false;HostedTick();
    CHECK(BridgeSnapshot(session,&s,sizeof(s))==mb::Accepted);
    CHECK((s.flags&mb::CoverActive) && s.coverPhase==3 && s.coverToken==77);
    CHECK(s.workerTickMs==known.workerTickMs);
    CHECK(s.generation==known.generation);
    for(int i=0;i<3;++i)HostedTick();
    CHECK(BridgeSnapshot(session,&s,sizeof(s))==mb::Accepted && s.workerTickMs==known.workerTickMs);
    rematch::coverReadable=true;HostedTick();
    CHECK(BridgeSnapshot(session,&s,sizeof(s))==mb::Accepted && s.workerTickMs>=known.workerTickMs && (s.flags&mb::CoverActive));
    CHECK(BridgePresentCover(session+1,77)==mb::StaleSession && !rematch::acknowledged);
    CHECK(BridgePresentCover(session,78)==mb::StaleGeneration && !rematch::acknowledged);
    CHECK(BridgePresentCover(session,77)==mb::Accepted && rematch::acknowledged==77);
    CHECK(BridgePost(session,0,mb::CloseMenu)==mb::Accepted);
    rematch::acknowledged=0;
    CHECK(BridgePresentCover(session,77)==mb::Unavailable && !rematch::acknowledged);
    HostedTick();CHECK(rematch::cancelled==77);
    CHECK(BridgeDetach(session) == mb::Accepted);
    CHECK(BridgePost(session, s.generation, mb::Accept) == mb::StaleSession);
    HostedTick(); CHECK(!g_open && g_hostRequested);
    CHECK(BridgeAttach(2, &other) == mb::Accepted && other != session);
    HostedTick();rematch::cover.phase=0;HostedTick();
    char reply[512]{};
    BridgeTestFight("R1DEA",reply,sizeof(reply));CHECK(strstr(reply,"disabled")&&rematch::testCalls==0);
    g_hostedFightTest=true;
    BridgeTestFight("R1DEA extra",reply,sizeof(reply));CHECK(strstr(reply,"exact row")&&rematch::testCalls==0);
    BridgeTestFight("R1DEA",reply,sizeof(reply));CHECK(g_hostedTest.state==1&&strstr(reply,"queued")&&rematch::testCalls==0);
    const auto firstSerial=g_hostedTest.serial;
    BridgeTestFight("R1CBC",reply,sizeof(reply));CHECK(g_hostedTest.serial==firstSerial&&strstr(reply,"pending"));
    HostedTick();CHECK(g_hostedTest.state==2&&g_hostedTest.token==rematch::cover.token&&rematch::testCalls==1);
    CHECK(rematch::acknowledged==0); // queue/worker does not fabricate presentation
    HostedTick();CHECK(rematch::testCalls==1);
    BridgeTestFight("",reply,sizeof(reply));CHECK(strstr(reply,"state=2")&&strstr(reply,"row=R1DEA"));
    rematch::cover.phase=0;HostedTick();
    BridgeTestFight("R1DEA",reply,sizeof(reply));CHECK(g_hostedTest.state==1);
    CHECK(BridgePost(other,0,mb::CloseMenu)==mb::Accepted);HostedTick();CHECK(g_hostedTest.state==3&&rematch::testCalls==1);
    BridgeTestFight("R1DEA",reply,sizeof(reply));CHECK(g_hostedTest.state==1);
    g_hostedTest.deadline=GetTickCount64();HostedTick();CHECK(g_hostedTest.state==3&&rematch::testCalls==1);
    BridgeTestFight("R1DEA",reply,sizeof(reply));CHECK(g_hostedTest.state==1);
    g_bridgeSnapshot.workerTickMs=GetTickCount64()-1500;HostedTick();CHECK(g_hostedTest.state==3&&rematch::testCalls==1);
    BridgeTestFight("R1DEA",reply,sizeof(reply));CHECK(g_hostedTest.state==1);
    CHECK(BridgeDetach(other)==mb::Accepted);HostedTick();CHECK(g_hostedTest.state==3&&rematch::testCalls==1);
    CHECK(BridgeAttach(3,&other)==mb::Accepted);HostedTick();
    rematch::testAllow=false;BridgeTestFight("R1DEA",reply,sizeof(reply));HostedTick();CHECK(g_hostedTest.state==3&&rematch::testCalls==2);
    HostedTick();CHECK(rematch::testCalls==2);
    puts("PASS actual provider attach/queue/worker/close/stale model fixtures; no native game exercised");
    printf("%d checks\n", checks);
}
