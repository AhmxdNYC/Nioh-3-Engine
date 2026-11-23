"""Compile the actual runtime guards (roster dedupe, Ibaraki native duplicate) against mocked game state, without launching a game."""
from pathlib import Path
import subprocess
import sys
import tempfile

source = (Path(__file__).resolve().parents[1] / "src/training.c").read_text()


def function(signature):
    start = source.index(signature + " {")
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


fixture = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef uint32_t DWORD;
#define MAXROWS 16
#define Log(...) ((void)0)
static struct { uint32_t mission, chr; char id[16], name[64]; } g_sum[MAXROWS];
static int g_nsum, g_nroster;
static struct { uint32_t placement, chr; int world; } g_ar;
static uint64_t wrapper[4], actor=123, player=456, owned, B;
static uint32_t record[2];
static int world=7, live=1, deletes;
static DWORD tick=1000;
static int CurWorld(void) { return world; }
static uint64_t FindWrapper(uint32_t id) { return id==record[0] ? (uint64_t)wrapper : 0; }
static int Rd64(uint64_t address, uint64_t* out) { *out=*(uint64_t*)address; return 1; }
static uint64_t PlayerChar(void) { return player; }
static int InSp(uint64_t a) { return a==owned; }
static int Actors(uint64_t* out, int cap) { assert(cap); out[0]=actor; return live; }
static DWORD GetTickCount(void) { return tick; }
static void Delete(uint64_t a) { assert(a==actor); ++deletes; }
static uint64_t Grva(uint32_t r) { assert(r==0x2BF7E8); return (uint64_t)&Delete; }
'''
for signature in (
    "static void AddSummonRow(uint32_t mission, uint32_t chr, const char* id, const char* name)",
    "static int ArenaNativeDuplicate(void)",
):
    fixture += function(signature) + "\n"
fixture += r'''
int main(void) {
    AddSummonRow(0, 91, "S91", "Boss"); AddSummonRow(0, 91, "S91", "Boss");
    assert(g_nsum==1 && g_nroster==1);
    AddSummonRow(1234, 91, "R1234", "Boss");
    assert(g_nsum==2 && g_nroster==1); // mission alias retained, roster deduplicated
    g_ar.placement=record[0]=51; g_ar.chr=record[1]=61; g_ar.world=7;
    wrapper[0]=actor; wrapper[3]=(uint64_t)record;
    assert(ArenaNativeDuplicate() && deletes==1); // native already present before dynamic spawn
    assert(ArenaNativeDuplicate() && deletes==1); // deferred deletion: hold spawn, no repeated delete
    live=0; assert(!ArenaNativeDuplicate()); // safe to create after deletion completes
    live=1; tick+=500; assert(ArenaNativeDuplicate() && deletes==2); // late native respawn
    owned=actor; assert(!ArenaNativeDuplicate() && deletes==2); // explicit A/B summon untouched
    owned=0; world=8; assert(!ArenaNativeDuplicate()); // return/other world untouched
    world=7; record[1]=62; assert(!ArenaNativeDuplicate()); // other character untouched
    record[1]=61; player=actor; assert(!ArenaNativeDuplicate()); // player untouched
    player=456; g_ar.placement=0; assert(!ArenaNativeDuplicate()); // raw/summon-here has no placement
    puts("PASS: roster dedupe, pre-spawn/late native duplicates, explicit slots/world/player guards");
}
'''
with tempfile.TemporaryDirectory() as d:
    path = Path(d)
    (path / "guard.c").write_text(fixture)
    exe = path / "guard.exe"
    subprocess.run([sys.executable, "-m", "ziglang", "cc", "-O2", "-UNDEBUG", str(path / "guard.c"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=30)
