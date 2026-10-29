"""Compile production admission, cleanup, tracking and pump bodies with native fault stubs."""
from pathlib import Path
import subprocess
import tempfile
import os, sys
ROOT=Path(__file__).resolve().parents[1]
text=(ROOT/'src/rematch.cpp').read_text()
def fn(sig):
    begin=text.index(sig); end=text.index('{',begin)+1; depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}'); end+=1
    return text[begin:end]
def segment(a,b): return text[text.index(a):text.index(b)]
code=r"""
#include <Windows.h>
#include <atomic>
#include <cstring>
#include <cstdio>
#include "rematch_core.h"
using Row=rcore::Row;
@GLOBALS@
@SNAP@
Snap fixtureSnap;
int IniInt(const wchar_t*,const wchar_t*,int d) { return d; }
const wchar_t* ModDir() { return L""; }
int StageWorld(int*) { return 0; }
void AutoMovesTick() {}
bool TrainingOwnsFight() { return false; }
std::atomic<bool> g_staleBbm{false};
void TfTick(const Snap&,ULONGLONG) {}
ULONGLONG now=20000;
ULONGLONG Ms() { return now; }
Snap ReadSnap() { return fixtureSnap; }
@ATTEMPT@
@GATES@
@EXEC_GLOBALS@
std::vector<Row> g_rows(1);
struct Config { bool hiddenDuels=false,fightNow=true,fightWarp=true,fightSpawn=true; } g_cfg;
struct Spawn { int st=0,reapplied=0; uint32_t point=0; bool landCheck=false; ULONGLONG nextCheck=0; char last[16]="-"; } g_sp;
std::atomic<uint32_t> g_gateWant{0},g_gateChecks{0};
std::atomic<int> g_gateSlot{-1};
std::atomic<bool> g_gateSeen{false};
uint8_t g_gateKey[16]{};
bool g_live=true,g_raOk=true,g_fnowOk=true,g_warpOk=true,g_gateOk=true,g_spawnOk=true,g_exitOk=true,g_emakiOk=true;
const char* g_raWhy="ra",*g_fnowWhy="fight",*g_warpWhy="warp",*g_spawnWhy="spawn",*g_exitWhy="exit",*g_emakiWhy="emaki";
namespace fight { LONG g_active=0,g_fights=0; void ForceEnd(const char*) { g_active=0; } }
std::atomic<bool> g_wantScan{false};
constexpr int cOrange=0,cGreen=1,cRed=2;
int requests=0,returns=0,opens=0,scrolls=0,warps=0,rights=0,writes=0,restores=0;
int outcome=0,g_warpStep=0;
bool canClear=true,restoreOk=true,mapWriteOk=true,mapReadOk=true,mapLie=false,mapReplaced=false;
uint8_t liveMap[8]{};
const char* refused=nullptr;
constexpr uint64_t G_MS=1,G_QUEUE=2,G_CH=3;
uint8_t queue[3*0x58]{};
int reads=0;
bool Copy(void* out,uint64_t address,size_t n) {
    if(address==reinterpret_cast<uint64_t>(liveMap)) { if(!mapReadOk) return false; memcpy(out,liveMap,n); return true; }
    if(address==G_QUEUE) { ++reads; if(outcome==2 && requests+scrolls) return false; memcpy(out,queue,n); return true; }
    return false;
}
template<class T> bool Rd(uint64_t address,T& out) {
    if(address==G_MS) out=100;
    else if(address==108) out=200;
    else if(address==216) out=1;
    else return false;
    return true;
}
void Log(const char*,...) {}
void Result(const char*,...) {}
void ToastF(int,int,const char*,...) {}
void Hex(char* out,size_t,uint64_t,size_t) { out[0]=0; }
void Refuse(const char* why) { refused=why; }
const char* RowName(int) { return "R1CBC"; }
void UnhideRestore(const char*) {}
void UnhideApply() {}
bool SpawnRestore(const char*) { ++restores; if(restoreOk) g_sp.st=0; return restoreOk; }
void SpawnLast(const char* name) { strcpy_s(g_sp.last,name); }
void SpawnApply(const Row&,const char*) { ++writes; g_sp.st=2; }
uint64_t CallWpFind(uint32_t) { return 1234; }
rcore::MapPatch g_map;
rcore::ETrack g_et;
int g_eRow=-1; uint8_t g_eByteSeen=0;

bool ClearedNow(uint16_t,const char** why) { if(!canClear) *why="uncleared"; return canClear; }
void enqueue(uint32_t mission,uint32_t start) {
    if(outcome==6) return;
    if(outcome==3) ++start;
    if(outcome==4) ++mission;
    uint64_t mode=outcome==5?9:2;
    memcpy(queue+4,&mission,4); memcpy(queue+0xC,&start,4); memcpy(queue+0x14,&mode,8);
}
int CallRequestById(uint16_t id,uint8_t,uint32_t start) {
    ++requests;
    if(g_attemptPhase!=aAwaitLoad || !g_attempt.callAttempted || g_attempt.evidence!=aPossible) abort();
    enqueue(id,start?start:67064); return outcome==1?1:0;
}
int CallOpenReplay() { ++opens; return 0; }
int CallReturn() { ++returns; if(g_attemptPhase!=aReturning || !g_attempt.returnAttempted) abort(); return outcome==1?1:0; }
int ReplayMenuBusy() { return 1; }
bool PlayerPos(float* p) { for(int i=0;i<4;++i) p[i]=0; return true; }
int CallWarp(float,float,float,float) { ++warps; return 0; }
void NativeRights(uint32_t,const uint8_t*) { ++rights; }
uintptr_t F_RIGHTSREQ=reinterpret_cast<uintptr_t>(&NativeRights);
struct Scroll { uint64_t ref=123; uint8_t key[12]{}; uint16_t itemId=1; int idx=0; };
struct EmakiTab { uint64_t vb=0; int n=0; uint8_t emptyKey=0; uint32_t count=1; };
bool FindScroll(Scroll&) { return true; }
bool ReadEmakiTab(EmakiTab& t) { t.vb=reinterpret_cast<uint64_t>(liveMap)+(mapReplaced?8:0); t.n=1; return true; }
bool WrGuard(void* dst,const void* src,size_t n) { ++writes; if(!mapWriteOk) return false; if(!mapLie) memcpy(dst,src,n); return true; }
@MAP_RESTORE@
int CallEmakiRequest(uint64_t ref,const uint8_t* key) {
    ++scrolls; if(g_attemptPhase!=aAwaitLoad || !g_attempt.callAttempted) abort();
    enqueue(0xCC96,12345); memcpy(queue+0x30,&ref,8); memcpy(queue+0x20,key,12);
    if(outcome==7) queue[0x30]^=1;
    if(outcome==8) queue[0x20]^=1;
    return outcome==1?1:0;
}
void DoScan(const Snap&) {}
void SelfTestTick(const Snap&,ULONGLONG) {}

@BODIES@
void fresh() {
    // Fixture-only new process lifetime. Production has no reset seam.
    g_attempt=NativeAttempt{}; g_attemptPhase=aNone; g_state=kIdle; g_req=0; g_pend=0;
    g_sp=Spawn{}; g_fight=false; g_fightRow=-1; g_replayIn=false; g_practiceOwned=false; g_activeRow=-1;
    g_gateWant=0; g_gateReq=false; g_warpPending=false; g_fn=kFnIdle;
    g_loadSeen=false; g_replayMs=0; g_loadDoneAt=0; g_exitSince=0; g_wasLoading=false;
    g_qHash=0; g_qChangedAt=0; g_lastTick=20000;
    requests=returns=opens=scrolls=warps=rights=writes=restores=reads=0;
    outcome=0; canClear=restoreOk=mapWriteOk=mapReadOk=true; g_map=rcore::MapPatch{}; mapLie=mapReplaced=false; memset(liveMap,0,8); liveMap[0]=1; refused=nullptr; memset(queue,0,sizeof(queue));
    fixtureSnap=Snap{}; fixtureSnap.gm=fixtureSnap.player=fixtureSnap.msOk=fixtureSnap.queueOk=fixtureSnap.emakiOk=fixtureSnap.onlineOk=fixtureSnap.guestOk=true;
    fixtureSnap.ms28=0x1111; fixtureSnap.ms14=123; now=20000;
    g_rows.resize(1); g_rows[0]=Row{}; g_rows[0].missionId=0x1CBC; g_rows[0].fightStart=67064; g_rows[0].placement=65092;
    g_rows[0].arenaOk=true; g_rows[0].route=rcore::kR;
    strcpy_s(g_rows[0].id,"R1CBC");
}
void start(bool fight=true) { ExecDirect(0,fixtureSnap,now,fight,true); }
void step(ULONGLONG t,bool loading,uint32_t mission,uint32_t retry) {
    now=t; fixtureSnap.st[2]=loading?1:0; fixtureSnap.ms28=mission; fixtureSnap.ms14=retry; Tick();
}
void post(uint32_t kind) { g_req=kind|(1u<<8); Tick(); }
int main(int argc,char** argv) {
    int checks=0;
    auto check=[&](bool ok) { ++checks; if(!ok) printf("FAIL %d phase=%d calls=%d state=%d\n",checks,g_attemptPhase.load(),requests,g_state.load()); return ok; };
    const int scenario=argc>1?atoi(argv[1]):0;
    if(scenario==1) {
        fresh(); post(kReqExit); step(21000,true,0x1111,123); step(22000,false,0x2222,456); step(28001,false,0x2222,456);
        if(!check(g_attemptPhase==aNone && !AttemptStartGate())) return 1;
        fresh(); post(kReqExit); step(21000,true,0x1111,123); step(22000,false,0x2222,456); step(28001,false,0x3333,789);
        if(!check(g_attemptPhase==aReturning && g_attempt.settledAt==28001)) return 1;
        fixtureSnap.gm=false; step(38001,false,0x3333,789); fixtureSnap.gm=true; step(38002,false,0x3333,789);
        if(!check(g_attemptPhase==aReturning && g_attempt.settledAt==38002)) return 1;
        step(44003,false,0x3333,789);
        if(!check(g_attemptPhase==aNone)) return 1;
        printf("%d standalone return regression checks PASS\n",checks); return 0;
    }
    if(scenario==2) {
        fresh(); start(false); step(21000,true,0,0); step(22000,false,0x1CBC,67064); step(26001,false,0x1CBC,67064);
        step(27000,false,0x1111,123); fixtureSnap.gm=false; step(37000,false,0x1111,123);
        fixtureSnap.gm=true; step(37001,false,0x1111,123);
        if(!check(g_attemptPhase==aActive && g_exitSince==37001)) return 1;
        printf("unreadable exit gap regression PASS\n"); return 0;
    }
    if(scenario==3) {
        fresh(); g_attemptPhase=aActive;
        if(!rcore::MapRedirect(g_map,liveMap,1,0,1,2,&WrGuard)) return 2;
        mapWriteOk=false; ToIdle("picture scroll exit",true);
        if(!check(g_attemptPhase==aQuarantined && g_map.active && AttemptStartGate())) return 1;
        mapWriteOk=true;
        if(!check(MapRestoreNow("retry") && !g_map.active && g_attemptPhase==aQuarantined)) return 1;
        for(int fault:{0,1,2,3}) {
            fresh(); g_attemptPhase=aActive;
            if(!rcore::MapRedirect(g_map,liveMap,1,0,1,2,&WrGuard)) return 2;
            const int previousWrites=writes;
            if(fault==0) mapReadOk=false;
            if(fault==1) liveMap[0]^=0x80;
            if(fault==2) mapReplaced=true;
            if(fault==3) mapLie=true;
            ToIdle("map failure",true);
            if(!check(g_attemptPhase==aQuarantined && g_map.active)) return 1;
            if(fault!=3 && !check(writes==previousWrites)) return 1;
            mapReadOk=true; mapReplaced=false; mapLie=false;
            if(fault==1) liveMap[0]^=0x80;
            if(fault==1 || fault==2) {
                if(!check(!MapRestoreNow("address/bytes return") && g_map.conflict && g_map.active)) return 1;
            } else if(!check(MapRestoreNow("retry") && !g_map.active)) return 1;
        }
        fresh(); mapWriteOk=false;
        if(!check(!rcore::MapRedirect(g_map,liveMap,1,0,1,2,&WrGuard) && g_map.active)) return 1;
        mapWriteOk=true;
        if(!check(MapRestoreNow("failed initial write") && !g_map.active)) return 1;
        printf("%d map write/read/conflict ownership regression checks PASS\n",checks); return 0;
    }
    fresh(); canClear=false; start(); if(!check(!requests && g_attemptPhase==aNone)) return 1;
    fresh(); start();
    if(!check(requests==1 && g_attemptPhase==aAwaitLoad && g_attempt.evidence==aMatched && g_fight && g_sp.st==1 && g_attempt.queueSlot==0 && g_attempt.requestedStart==67064)) return 1;
    for(int failure=1;failure<=6;++failure) {
        fresh(); outcome=failure; start();
        if(!check(requests==1 && g_attemptPhase==aQuarantined && g_attempt.callAttempted && g_attempt.mission==0x1CBC && g_attempt.requestedStart==67064 && !g_fight && !g_sp.st)) return 1;
        const uint64_t sequence=g_attempt.sequence;
        const char* reason=g_attempt.reason;
        ToIdle("abort");
        memset(queue,0,sizeof(queue)); outcome=0;
        step(2000000,true,0,0); step(2000010,false,0x1CBC,67064); step(4000000,false,0x1CBC,67064);
        if(!check(g_attemptPhase==aQuarantined && g_attempt.sequence==sequence && g_attempt.reason==reason)) return 1;
        const int called=requests;
        ExecDirect(0,fixtureSnap,now,true,true); ExecOpen(0,fixtureSnap); ExecEmaki(-1,now); post(kReqExit);
        if(!check(requests==called && !opens && !scrolls && !returns && !warps && !rights && !writes)) return 1;
        restoreOk=false; g_sp.st=3; ToIdle("failed restore");
        if(!check(g_attemptPhase==aQuarantined && g_sp.st==3)) return 1;
        restoreOk=true; ToIdle("restored");
        if(!check(g_attemptPhase==aQuarantined && !g_sp.st)) return 1;
    }
    for(bool abortRequest:{false,true}) {
        fresh(); start(); const uint64_t sequence=g_attempt.sequence;
        if(abortRequest) post(kReqAbort); else step(150001,false,0x1111,123);
        if(!check(g_attemptPhase==aQuarantined && g_attempt.evidence==aMatched)) return 1;
        step(160000,true,0,0); step(170000,false,0x1CBC,67064); step(180000,false,0x1CBC,67064);
        if(!check(g_attemptPhase==aQuarantined && g_attempt.sequence==sequence && requests==1 && !warps && !rights && !returns)) return 1;
    }
    fresh(); start(false);
    if(!check(g_attempt.evidence==aMatched && !g_attempt.requestedStart && g_attempt.observedStart==67064 && !g_fight)) return 1;
    // Loading may have no readable player: a valid stop block still witnesses load-up.
    fixtureSnap.player=false; step(21000,true,0x1111,123); fixtureSnap.player=true;
    step(22000,false,0x1CBC,67064); fixtureSnap.st[3]=1; step(24000,false,0x1CBC,67064);
    if(!check(g_attempt.settledAt==0 && !g_loadDoneAt)) return 1;
    fixtureSnap.st[3]=0; step(25000,false,0x1CBC,67064);
    fixtureSnap.msOk=false; step(27000,false,0x1CBC,67064); fixtureSnap.msOk=true;
    step(28000,false,0x1CBC,67064); step(31000,false,0x1CBC,67064);
    if(!check(g_attemptPhase==aAwaitLoad)) return 1;
    step(31001,false,0x1CBC,67064);
    if(!check(g_attemptPhase==aActive && g_replayIn)) return 1;
    // Unknown mission observations and a long load cannot manufacture an operational exit.
    fixtureSnap.msOk=false; step(41000,false,0,0); step(51000,false,0,0);
    if(!check(g_attemptPhase==aActive && g_exitSince==0)) return 1;
    fixtureSnap.msOk=true; step(52000,true,0,0); step(62000,true,0,0);
    if(!check(g_attemptPhase==aActive && g_exitSince==0)) return 1;
    step(63000,false,0x1CBC,67064);
    post(kReqExit); post(kReqExit);
    if(!check(returns==1 && g_attemptPhase==aReturning && g_attempt.returnCompleted)) return 1;
    step(64000,true,0x1CBC,67064); step(65000,false,0x1111,123); step(72000,false,0x1111,123);
    if(!check(g_attemptPhase==aNone && g_state==kIdle)) return 1;
    fresh(); start(false); step(21000,true,0,0); step(22000,false,0x1CBC,67064); step(26001,false,0x1CBC,67064);
    outcome=1; post(kReqExit); post(kReqExit);
    if(!check(returns==1 && g_attemptPhase==aQuarantined && g_attempt.returnAttempted && !g_attempt.returnCompleted)) return 1;
    fresh(); start(); fixtureSnap.onlineOk=false; Tick(); fixtureSnap.onlineOk=true; Tick();
    if(!check(g_attemptPhase==aQuarantined && !g_fight && !warps && !rights && !writes)) return 1;
    // A quarantine remains effective even if UI/tracker flags are inconsistent.
    g_state=kIdle; g_fn=kFnWarped; g_fight=true; g_fightRow=0;
    g_gateWant=1; g_gateDue=1; g_gateSeen=true; g_gateReq=false; g_sp.st=1;
    DoWarp(fixtureSnap,now); FnVerify(fixtureSnap,now); GateTick(fixtureSnap,now); fixtureSnap.st[2]=1; SpawnTick(fixtureSnap,now,false);
    if(!check(!warps && !rights && !writes)) return 1;
    // A different mission's live boss must not satisfy the old replay verifier.
    fresh(); start(); g_fn=kFnArmed; fight::g_active=true;
    fixtureSnap.ms28=0x1111; DoWarp(fixtureSnap,now);
    if(!check(g_fn==kFnArmed && !warps)) return 1;
    fixtureSnap.ms28=g_fightMission; fixtureSnap.ms14=g_fightStart+1;
    DoWarp(fixtureSnap,now);
    if(!check(g_fn==kFnPass && !warps)) return 1;
    // Readable but unproven session modes quarantine; returning to zero is not release.
    for(int32_t mode:{-1,3,INT32_MAX}) {
        fresh(); start(); fixtureSnap.playMode=mode; Tick();
        fixtureSnap.playMode=0; Tick();
        if(!check(g_attemptPhase==aQuarantined && !returns && !warps && !rights && !writes)) return 1;
    }
    for(bool connected:{false,true}) {
        fresh(); start(false); fixtureSnap.online=!connected; fixtureSnap.guest=connected; Tick();
        fixtureSnap.online=fixtureSnap.guest=false; Tick();
        if(!check(g_attemptPhase==aQuarantined && !returns)) return 1;
    }
    for(int failure:{2,3,4,5,6,7,8}) {
        fresh(); outcome=failure; ExecEmaki(-1,now);
        // Explicit start is not requested for Emaki, so its native-resolved variation remains valid.
        if(!check(g_attemptPhase==(failure==3?aAwaitLoad:aQuarantined))) return 1;
        if(failure!=3 && !check(g_attempt.evidence==aPossible)) return 1;
    }
    fresh(); ExecEmaki(-1,now); if(!check(scrolls==1 && g_attempt.evidence==aMatched && g_attempt.scroll)) return 1;
    step(21000,true,0,0); fixtureSnap.emaki=true;
    step(22000,false,0xCC96,12345); step(26001,false,0xCC96,12345);
    if(!check(g_attemptPhase==aActive && g_et.st==rcore::kEIn)) return 1;
    fixtureSnap.emakiOk=false; fixtureSnap.emaki=false;
    step(36000,false,0x1111,123); step(46000,false,0x1111,123);
    if(!check(g_attemptPhase==aActive && !g_et.zeroSince)) return 1;
    fixtureSnap.emakiOk=true; step(47000,false,0x1111,123); step(53000,false,0x1111,123);
    if(!check(g_attemptPhase==aNone && g_state==kIdle)) return 1;
    fresh(); outcome=1; ExecEmaki(-1,now); if(!check(scrolls==1 && g_attemptPhase==aQuarantined)) return 1;
    fresh(); g_rows[0].atFight=true;
    ExecDirect(0,fixtureSnap,now,true,false);
    if(!check(!requests && g_attemptPhase==aNone)) return 1;
    post(kReqShrine);
    if(!check(!requests && g_attemptPhase==aNone)) return 1;
    // Gate-only missions must never fall back to a shrine through Fight now,
    // including raw direct calls and delayed/pipe request dispatch.
    fresh(); g_rows[0].arenaOk=false; g_rows[0].gateId=1;
    ExecDirect(0,fixtureSnap,now,true,true);
    if(!check(!requests && g_attemptPhase==aNone && !g_gateWant && !g_fightWarp)) return 1;
    post(kReqFight);
    if(!check(!requests && g_attemptPhase==aNone && !g_gateWant && !g_fightWarp)) return 1;
    post(kReqShrine);
    if(!check(requests==1 && !g_gateWant && !g_fightWarp)) return 1;
    fresh(); post(kReqPractice);
    if(!check(requests==1 && g_practiceOwned && g_attempt.requestedStart==67064 && !g_fightWarp && !g_sp.st && !g_gateWant && g_fn==kFnIdle)) return 1;
    step(21000,true,0,0); step(22000,false,0x1CBC,67064); step(26001,false,0x1CBC,67064);
    if(!check(!PracticeReturnGate(fixtureSnap) && !warps && !rights && !writes)) return 1;
    post(kReqPracticeExit); if(!check(returns==1 && g_attemptPhase==aReturning)) return 1;
    post(kReqPracticeExit); if(!check(returns==1)) return 1;
    step(27000,true,0x1CBC,67064); step(28000,false,0x1111,123); step(34001,false,0x1111,123);
    if(!check(g_attemptPhase==aNone && !g_practiceOwned)) return 1;
    for(int wrong=0;wrong<7;++wrong) {
        fresh();
        if(wrong==0) strcpy_s(g_rows[0].id,"other");
        if(wrong==1) g_rows[0].missionId=0xC72E;
        if(wrong==2) g_rows[0].fightStart=0;
        if(wrong==3) g_rows[0].placement=1;
        if(wrong==4) g_rows[0].route=rcore::kE;
        if(wrong==5) g_rows[0].atFight=true;
        if(wrong==6) g_rows.push_back(g_rows[0]);
        ExecPractice(0,fixtureSnap,now); if(!check(!requests && !g_practiceOwned)) return 1;
    }
    fresh(); canClear=false; ExecPractice(0,fixtureSnap,now); if(!check(!requests && !g_practiceOwned)) return 1;
    fresh(); fixtureSnap.onlineOk=false; ExecPractice(0,fixtureSnap,now); if(!check(!requests)) return 1;
    fresh(); fixtureSnap.online=true; ExecPractice(0,fixtureSnap,now); if(!check(!requests)) return 1;
    fresh(); fixtureSnap.queueBusy=true; ExecPractice(0,fixtureSnap,now); if(!check(!requests)) return 1;
    fresh(); outcome=1; ExecPractice(0,fixtureSnap,now);
    if(!check(requests==1 && g_practiceOwned && g_attemptPhase==aQuarantined)) return 1;
    post(kReqPracticeExit); if(!check(!returns)) return 1;
    fresh(); start(); step(21000,true,0,0); step(22000,false,0x1CBC,67064); step(26001,false,0x1CBC,67064);
    post(kReqPracticeExit); if(!check(!returns && !g_practiceOwned)) return 1;
    printf("%d actual-source transaction checks PASS\n",checks);
}
"""
# Globals required by gates precede the actual helper bodies.
head=segment('enum ReqKind','const char* RowName(')
# Post is preceded by small unrelated data? All remain scalar request globals.
code=code.replace('@GLOBALS@',head).replace('@SNAP@',segment('struct Snap {','Snap ReadSnap()'))
code=code.replace('@ATTEMPT@',segment('enum AttemptPhase','// Unknown session reads'))
gates='\n'.join(fn(sig) for sig in ('const char* SessionGate(', 'const char* NativeGate(', 'const char* ExitGate(', 'void AttemptObserve(', 'bool AttemptEffectsAllowed(', 'const char* AttemptReturnGate(', 'void AttemptReturning('))
code=code.replace('@GATES@',gates)
code=code.replace('@EXEC_GLOBALS@',segment('uint32_t g_pend =','void Refuse('))
bodies='void* TrainingExport(const char*) { return nullptr; }\nbool XwPending() { return false; }\nint ReturnInfo() { return -1; }\nstd::atomic<bool> g_swappedInPlace{false};\nvoid CoverTick(const Snap&, ULONGLONG) {}\n'+'\n'.join(fn(sig) for sig in ('const char* Gates(', 'void ToIdle(', 'void ExecOpen(', 'void ExecDirect(', 'int PracticeRow(', 'const char* PracticeReturnGate(', 'void ExecPractice(', 'void ExecEmaki(', 'void DoWarp(', 'void FnVerify(', 'void GateTick(', 'void TrackReplay(', 'void TrackEmaki(', 'void SpawnTick(', 'void Tick()'))
code=code.replace('@BODIES@',bodies).replace('@MAP_RESTORE@',fn('bool MapReadGuard(')+'\n'+fn('bool MapRestoreNow('))
setup=Path(r'C:/nioh3-engine/tools/msvc/setup_x64.bat')
with tempfile.TemporaryDirectory(prefix='nioh3-native-attempt-') as tmp:
    directory=Path(tmp); source=directory/'test.cpp'; exe=directory/'test.exe'; source.write_text(code)
    command=f'call "{setup}" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /I"{ROOT / "src"}" /Fo"{directory / "test.obj"}" /Fe"{exe}" "{source}"'
    result=subprocess.run(([sys.executable, '-m', 'ziglang', 'c++', '-target', 'x86_64-windows-gnu', '-std=c++17', '-fms-extensions', '-I'+str(ROOT/'src'), str(source), '-o', str(exe)] if os.environ.get('N3_TEST_ZIG') else 'cmd /d /s /c "'+command+'"'),capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
    results=[]
    for scenario in ('1','2','3','0'):
        result=subprocess.run([str(exe),scenario],capture_output=True,text=True,timeout=10)
        print('scenario '+scenario+': '+result.stdout.strip()); results.append(result)
    assert all(r.returncode==0 for r in results), '\n'.join(r.stderr for r in results)
