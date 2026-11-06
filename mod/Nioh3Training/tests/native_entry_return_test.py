"""Offline regression for the real native-entry handshake and return hook; no game."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
tr = (ROOT / "mod/Nioh3Training/src/training.c").read_text()
br = (ROOT / "mod/Nioh3BossRematch/src/rematch.cpp").read_text()

def function(src, signature):
    start = src.index(signature)
    begin = src.index("{", start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (src[end] == "{") - (src[end] == "}")
        end += 1
    return src[start:end]

# Mutation entry points must all stop for the sticky Training owner.
# #71926: GateTick is the exception: the door right moves nothing, and only runs while Training is still waiting (route 1).
assert "routeFn(g_fightMission) != 1" in function(br, "void GateTick(")
for name in ("DoWarp", "SpawnTick"):
    body = function(br, f"void {name}(")
    assert body.split("{", 1)[1].lstrip().startswith("if (g_trainingDestination) return;"), name
verify = function(br, "void FnVerify(")
assert verify.index("if (g_trainingDestination)") < verify.index("fight::g_fights >")
assert "!g_trainingDestination && warp" in br
assert "g_trainingDestination = false; g_trainingRoute = -1;" in function(br, "void ToIdle(")
assert "now - g_en.tWarp >= (g_en.moved ? 15000u : 30000u)" in tr
assert "dx * dx + dy * dy + dz * dz" in function(tr, "static __attribute__((noinline)) void MonTick(")
assert "if (g_en.confirmed) return;" in function(tr, "static void GateTick(")

stub = r"""
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#define N3T_API
#define __builtin_return_address(x) ((void*)0x1234)
static uintptr_t B;
struct Row { uint32_t mission; const char* id; } row{123, "test"};
struct { int active, confirmed, route; Row* row; } g_en;
static void EntryCoverBegin(Row* r) { (void)r; }
static void EntryCoverEnd(const char* why) { (void)why; }
struct { int active; } g_sp, g_tr;
struct { int active, respawnAt; } g_vf;
struct { int up, session, tried; } g_gate;
struct { int active, want; } g_rh;
struct { int valid; } g_home;
static int g_gate_again, g_cfg_gate=1, g_cfg_gate_always=0;
static int stopped, finished, used, returned, masked, alive=1;
static const uint32_t* passed; static uint8_t flag;
static void Log(const char*, ...) {}
static void TrainStop(const char*) { ++stopped; g_tr.active=0; }
static int PlayerDeadNow() { return 0; }
enum { FL_LOSS, FL_QUIT };
static int FightFinish(int) { ++finished; return 1; }
static int GateBossAlive() { return alive; }
static int GateUse() { ++used; return 1; }
static void Mask(int,int) { ++masked; }
static auto p_osdMask = Mask;
static void RealReturn(const uint32_t* p, uint8_t f) { ++returned; passed=p; flag=f; }
static auto o_reqret=RealReturn;
"""
stub += r"""
using DWORD = unsigned long;
struct { uint32_t placement, chr; int world; } g_ar;
static uint64_t wrapper[4], record[2], target=0x10000, player=0x20000, tracked;
static int world=1, live=1, deleted;
static DWORD tick=1000;
static int CurWorld() { return world; }
static uint64_t FindWrapper(uint32_t) { return (uint64_t)wrapper; }
static int Rd64(uint64_t a, uint64_t* out) { memcpy(out,(void*)a,8); return 1; }
static uint64_t PlayerChar() { return player; }
static int InSp(uint64_t a) { return a==tracked; }
static int Actors(uint64_t* out,int) { if(live) { out[0]=target; return 1; } return 0; }
static DWORD GetTickCount() { tick+=1000; return tick; }
static uintptr_t Grva(uintptr_t x) { return x; }
static void DeleteActor(uint64_t a) { assert(a==target); ++deleted; }
"""
stub += function(tr, "static int ArenaNativeDuplicate(")
stub += function(tr, "N3T_API int Nioh3Training_NativeEntryRoute(")
stub += function(tr, "N3T_API void Nioh3Training_NativeEntryResult(")
stub += function(tr, "static void h_reqret(")
stub += r"""
int main() {
    uint32_t args=789;
    g_en={1,0,1,&row};
    assert(Nioh3Training_NativeEntryRoute(123)==1);
    assert(Nioh3Training_NativeEntryRoute(999)==0);
    Nioh3Training_NativeEntryResult(999,1); assert(!g_en.confirmed);
    Nioh3Training_NativeEntryResult(123,1); assert(g_en.confirmed);
    g_en.active=0; g_en.route=2; // completion must not give BR destination ownership back
    assert(Nioh3Training_NativeEntryRoute(123)==2);
    for (int route: {1,2,3,4,5}) {
        g_en={1,1,route,&row}; g_tr.active=1; g_gate={1,1,1};
        g_sp.active=1; g_vf={1,1}; g_rh.want=1; g_home.valid=1; g_gate_again=1;
        int before=returned;
        h_reqret(&args,7);
        assert(returned==before+1 && passed==&args && flag==7 && used==0);
        assert(!g_en.active && !g_en.confirmed && !g_en.route);
        assert(!g_sp.active && !g_vf.active && !g_vf.respawnAt && !g_tr.active);
        assert(!g_gate.up && !g_gate.session && !g_gate.tried && !g_gate_again);
        assert(!g_rh.want && !g_home.valid);
    }
    assert(stopped==5 && masked==5);
    g_en={0,1,2,&row}; g_tr.active=0;
    h_reqret(&args,9); assert(returned==6 && finished==1);
    g_en={1,0,1,&row}; Nioh3Training_NativeEntryResult(123,0);
    assert(!g_en.active && !g_en.confirmed && !g_en.route);
    g_tr.active=1; g_gate.up=1; alive=0;
    h_reqret(&args,3); assert(used==1 && returned==6); // ordinary Training still uses its gate
    g_gate.up=0; h_reqret(&args,4); assert(returned==7);
    B=(uintptr_t)&DeleteActor-0x2BF7E8;
    g_ar={77,88,1}; wrapper[0]=target; wrapper[3]=(uint64_t)record;
    ((uint32_t*)record)[0]=77; ((uint32_t*)record)[1]=88;
    tracked=target; assert(!ArenaNativeDuplicate() && !deleted); // never delete our dynamic actor
    tracked=0; player=target; assert(!ArenaNativeDuplicate() && !deleted);
    player=0x20000; world=2; assert(!ArenaNativeDuplicate() && !deleted);
    world=1; ((uint32_t*)record)[0]=99; assert(!ArenaNativeDuplicate() && !deleted);
    ((uint32_t*)record)[0]=77; ((uint32_t*)record)[1]=99; assert(!ArenaNativeDuplicate() && !deleted);
    ((uint32_t*)record)[1]=88; live=0; assert(!ArenaNativeDuplicate() && !deleted);
    live=1; assert(ArenaNativeDuplicate() && deleted==1); // exact live native placement only
    puts("PASS: exact-placement dedupe rejects tracked/player/wrong-world/wrong-record/stale actors");
    puts("PASS: ownership handshake, all five native-route exits, cleanup, ordinary Training return");
}
"""
stub = "#include <initializer_list>\n" + stub
with tempfile.TemporaryDirectory(prefix="n3-native-exit-") as folder:
    path = Path(folder)
    source = path / "test.cpp"
    exe = path / "test.exe"
    source.write_text(stub)
    subprocess.run(["cl", "/nologo", "/EHsc", "/std:c++17", str(source), f"/Fe{exe}"], cwd=path, check=True)
    subprocess.run([str(exe)], cwd=path, check=True)
print("PASS: BR mutation guards, 30-second fallback and 3D distance source checks")
