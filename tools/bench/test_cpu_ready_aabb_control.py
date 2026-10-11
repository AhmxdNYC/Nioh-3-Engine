"""Offline reproduction of the stable1.7.61 AABB acknowledgement trap.

Compile actual selected menu/LiveCtl functions with an isolated host. Only the
experimental arm of Change and the experimental Value dispatch are included;
the full visibility/availability guards, key lookup, RemoteValue, RunRemote,
ExpValue and ApplyOne are used. No game access, ASI build or source modification.
MSVC: py -I tools/bench/test_cpu_ready_aabb_control.py --setup <setup_x64.bat>
"""
import argparse
import hashlib
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "mod/Nioh3EngineFix/src"


def block(text, marker):
    start = text.index(marker)
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--setup", "--vcvars", dest="setup", type=pathlib.Path, required=True)
    args = parser.parse_args()
    if not args.setup.is_file():
        parser.error("MSVC environment setup batch is missing")
    menu = (SRC / "menu.cpp").read_text(encoding="utf-8")
    header = (SRC / "menu.h").read_text(encoding="utf-8")
    live = (SRC / "livectl.cpp").read_text(encoding="utf-8")
    live_header = (SRC / "livectl.h").read_text(encoding="utf-8")
    change = block(block(menu, "void Change(int id,"), "if (IsExp(id))")
    # AABB uses the default availability/visibility arms and bool Remote branch.
    # These are actual functions, rather than a fake menu or ACK implementation.
    pieces = [
        block(menu, "enum Id ") + ";",
        block(header, "enum ExpId ") + ";",
        block(header, "struct ExpFix ") + ";",
        block(menu, "struct RemoteReq ") + ";",
        block(live_header, "struct StartInfo ") + ";",
        menu[menu.index("const wchar_t* const kExpIniKey["):menu.index("struct ItemDef")],
    ]
    functions = [block(menu, name) for name in (
        "bool IsExp(", "bool IsEp(", "bool Visible(", "bool ReadOnly(",
        "bool Available(", "bool LiveBool(", "void ExpValue(",
        "void NormKey(", "int FindItem(", "bool RemoteValue(",
    )]
    run_remote = block(menu, "void RunRemote(")
    live_functions = [block(live, name) for name in (
        "std::string Trim(", "std::string NoSpaces(", "bool ReplyValue(", "bool ApplyOne(",
    )]
    selected = "\n".join(pieces + functions + [change, run_remote] + live_functions)
    print("Selected actual source paths SHA256", hashlib.sha256(selected.encode()).hexdigest())
    for name in ("menu.cpp", "menu.h", "livectl.cpp", "livectl.h", "dllmain.cpp", "caves.asm"):
        print(name, hashlib.sha256((SRC / name).read_bytes()).hexdigest())
    prelude = r'''
#include <windows.h>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
namespace gpuopt {
enum class State { Off, Active, Paused };
State GetState() { return State::Off; }
bool Batching() { return false; }
bool Rewrite(int) { return false; }
}
namespace extraperf {
constexpr int kNumPresets = 3;
bool AnyAvailable() { return false; }
bool Available(int) { return false; }
int Level(int) { return 0; }
int MatchPreset() { return 0; }
void ApplyPreset(int) {}
void SetLevel(int, int) {}
}
namespace osd {
struct OverlaySettings { int maxLatency=0; bool fpsCounter=false, frameTime=false, speed=false; };
OverlaySettings& Overlay() { static OverlaySettings o; return o; }
}
'''
    host = r'''
struct Host {
    ExpFix exp[kNumExp];
    bool wine=false, knownBuild=true; uintptr_t base=1;
    volatile uint32_t *vrrEnable=nullptr, *w3Enable=nullptr, *w3UseSleep=nullptr,
        *r1Timeout=nullptr, *l1Enable=nullptr, *fgEnable=nullptr, *wkEnable=nullptr,
        *autoContinue=nullptr, *tgCap=nullptr;
    volatile long *tsInDispUs=nullptr, *tgState=nullptr;
} g_host;
int g_expIni[kNumExp]={}, g_expIni0[kNumExp]={}; bool g_expAuto[kNumExp]={};
bool g_fgOk=false, g_dirty=false;
int g_bootWorkerCap=0, g_bootP12=0, g_bootL1Wine=0, g_bootFgWine=0;
struct { int pauseHint=0; } g_cfg;
struct { const char* plain=""; const char* tech=""; } kItems[kCount];
StartInfo g_si;
'''
    adapters = r'''
void Value(int id, char* buf, size_t n, uint32_t& color) {
    assert(IsExp(id)); ExpValue(id, buf, n, color, 1, 2, 3, 4);
}
void LogItem(int) {}
'''
    tests = r'''
int Transport(const char* key,int set,int value,char* reply,size_t n,unsigned) {
    RemoteReq q{}; strcpy_s(q.key, key); q.set=set; q.value=value;
    RunRemote(q); strcpy_s(reply,n,q.reply); return q.rc;
}
void Reset(int ini,volatile uint32_t* liveSwitch,bool applied) {
    g_host.exp[kExpAABB]={liveSwitch,ini,applied};
    g_expIni[kExpAABB]=liveSwitch?(*liveSwitch!=0):ini;
    g_expIni0[kExpAABB]=ini;
}
bool Apply(const char* want,std::string& rb,std::string& why) {
    rb.clear(); why.clear(); return ApplyOne("AABB",want,rb,why);
}
int main() {
    kItems[kXAABB]={"Faster bounding boxes","(AABB)"}; g_si.remote=&Transport;
    volatile uint32_t aabb=0;
    std::string rb,why;
    Reset(0,nullptr,false);
    assert(Apply("1",rb,why) && rb=="1" && !g_host.exp[kExpAABB].applied);
    assert(aabb==0 && why.find("restart")!=std::string::npos);
    printf("REPRO uninstalled: ack success AABB=%s; reply %s; no live cave\n",rb.c_str(),why.c_str());
    // AABB=2 sets host.ini=1; failed/skipped installation leaves pointer null.
    Reset(1,nullptr,false);
    assert(Apply("0",rb,why) && rb=="0" && aabb==0);
    assert(Apply("1",rb,why) && rb=="1" && aabb==0);
    assert(why.find("failed")!=std::string::npos);
    printf("REPRO skipped install: ack success AABB=%s; reply %s; no live cave\n",rb.c_str(),why.c_str());
    aabb=0; Reset(1,&aabb,true);
    volatile uint32_t dq=0,pf=0;
    g_host.exp[kExpDQ]={&dq,0,true}; g_host.exp[kExpP1E7]={&pf,0,true};
    g_expIni[kExpMA]=0;
    for(const char* want:{"0","1","1","0","0","1","0"}) {
        assert(Apply(want,rb,why) && rb==want && aabb==unsigned(atoi(want)));
        assert(why.find("restart")==std::string::npos && why.find("(ini)")==std::string::npos);
        assert(dq==0 && pf==0 && g_expIni[kExpMA]==0);
    }
    printf("PASS installed/OFF: 7 exact live set/readbacks incl. no-ops; DQ/P1E7/MA unchanged\n");
    assert(!ApplyOne("not-a-row","1",rb,why));
    g_si.remote=nullptr; assert(!Apply("1",rb,why));
    printf("PASS missing row/transport fail closed; ALL PASSED (source-path host test, no game)\n");
}
'''
    code = "\n".join([prelude, *pieces, host, *functions,
                        "void Change(int id,int) { if(!Available(id)) return; " + change + " }",
                        adapters, run_remote, *live_functions, tests])
    build_root = ROOT / "mod/Nioh3EngineFix/build/cpu-ready-control"
    build_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=build_root) as temporary:
        folder = pathlib.Path(temporary)
        cpp, exe = folder / "control.cpp", folder / "control.exe"
        cpp.write_text(code, encoding="utf-8")
        command = f'call "{args.setup}" >nul && cl /nologo /Od /MT /EHsc /std:c++17 /W4 "{cpp}" /Fo"{folder / "control.obj"}" /Fe"{exe}" /link kernel32.lib'
        # A quoted command passed as one Windows argv item acquires backslash
        # escapes from list2cmdline; cmd.exe does not interpret those as quotes.
        # Run an explicit relative batch path so current-directory executable
        # lookup policy cannot block it; paths with spaces stay inside the file.
        batch = folder / "build_control.cmd"
        batch.write_text("@echo off\n" + command + "\n", encoding="utf-8", newline="\r\n")
        result = subprocess.run(["cmd.exe", "/d", "/c", ".\\" + batch.name], cwd=folder,
                                capture_output=True, text=True)
        if result.returncode:
            print(result.stdout[-5000:], result.stderr[-1000:])
            return result.returncode
        result = subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True)
        print(result.stdout, end="")
        if result.returncode:
            print(result.stderr)
        return result.returncode


if __name__ == "__main__":
    sys.exit(main())
