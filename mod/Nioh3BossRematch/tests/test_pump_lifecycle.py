"""Compile actual startup/activation bodies with a controlled installer; no game."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
main = (ROOT / "src/main.cpp").read_text()
rematch = (ROOT / "src/rematch.cpp").read_text()

def function(text, signature):
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]

prepare = function(rematch, "bool Prepare()")
assert "InstallPump(" not in prepare and "g_live = true" not in prepare
assert prepare.index('L"Enable", 0') < prepare.index('L"SelfTest"')
assert prepare.index('return false;') < prepare.index('g_cfg.hiddenDuels')
assert 'g_prepared = true;' in prepare
assert main.count('InstallPump(') == 1
assert main.index('const bool live = StartRematchPump();') < main.index('Feature("BossLog", "FIGHT")')
# Settings remain available with Rematch off; this must not gate or activate the pump.
assert '\n    ui::Start();' in main
assert main.index('const bool live = StartRematchPump();') < main.index('ui::Start();')
assert 'training::Prepare()' in main

import configparser
config=configparser.ConfigParser()
config.read(ROOT / "Nioh3BossRematch.ini")
assert config.getint('Training','Observe') == 0
assert config.getint('Training','Enable') == 0
assert config.getint('Rematch','Enable') == 0
training = (ROOT / "src/training.cpp").read_text()
assert not any(x in training for x in ('Copy(', 'Wr', 'rematch::', 'sp['))
code = r"""
#include "addon.h"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <initializer_list>
namespace addon {
Image g_img{0,0,0,0,true};
uintptr_t Site(const char*) { return 0; }
int ticks=0, installs=0, prepares=0, fights=0, groups=0, nh=0, failedGroup=0;
bool observe=false, want=false, succeed=false, fightSuccess=true, prepareSuccess=true;
bool g_prepared=false;
std::atomic<bool> g_live{false};
Handler handlers[3]{};
void Tick() { ++ticks; }
void Log(const char*, ...) {}
int IniInt(const wchar_t* sec,const wchar_t* key,int def) {
    if (!wcscmp(sec,L"Training") && !wcscmp(key,L"Observe")) return observe;
    if (!wcscmp(sec,L"Rematch") && !wcscmp(key,L"Enable")) return want;
    return def;
}
bool GroupOk(const char*,const char** why) { ++groups; *why="group failure"; return groups!=failedGroup; }
void OnEntry(const char*,Handler h) { handlers[nh++]=h; }
bool InstallGroup(const char*,const char** why) {
    ++fights; *why="fight failure";
    // Registration is complete before any installer can run a hook.
    if (observe && g_img.knownBuild && !failedGroup && nh!=3) return false;
    for(int i=0;i<nh;++i) handlers[i](0,0,0,0,nullptr);
    return fightSuccess;
}
namespace rematch {
bool Prepare() { ++prepares; const char* why=nullptr; g_prepared=prepareSuccess && InstallGroup("FIGHT",&why); return g_prepared; }
@EXPORTS@
}
}
@TRAINING@
namespace addon {
bool InstallPump(void (*callback)(), const char** why) {
    ++installs; callback();
    for(int i=0;i<nh;++i) handlers[i](0,0,0,0,nullptr);
    *why="pump failure"; return succeed;
}
@STARTUP@
}
int main() {
    using namespace addon;
    int checks=0;
    auto check=[&](bool ok) { ++checks; if (!ok) std::printf("FAIL %d\n",checks); return ok; };
    auto reset=[]() {
        ticks=installs=prepares=fights=groups=nh=0;
        g_prepared=false; g_live=false;
        training::g_prepared=false; training::g_live=false;
        training::g_create=0; training::g_begin=0; training::g_end=0; training::g_pumps=0;
    };
    for(bool obs : {false,true}) for(bool rm : {false,true})
    for(bool fi : {false,true}) for(bool pu : {false,true}) {
        reset(); observe=obs; want=rm; fightSuccess=fi; succeed=pu;
        bool ol=obs && fi && pu, rl=rm && fi && pu;
        if(!check(StartRematchPump()==rl)) return 1;
        if(!check(prepares==(rm ? 1:0) && nh==(obs ? 3:0))) return 1;
        if(!check(installs==((obs || rm) && fi ? 1:0))) return 1;
        if(!check(training::Live()==ol && rematch::Live()==rl && ticks==0)) return 1;
        auto initial=training::Counts();
        if(!check(initial.create==0 && initial.begin==0 && initial.end==0 && initial.pumps==0)) return 1;
        for(int i=0;i<nh;++i) handlers[i](~0ULL,~0ULL,~0ULL,~0ULL,nullptr);
        SharedPump(); SharedPump(); auto c=training::Counts();
        if(!check(c.create==(ol?1U:0U) && c.begin==c.create && c.end==c.create && c.pumps==(ol?2U:0U))) return 1;
        if(!check(ticks==(rl?2:0))) return 1;
        training::SetPumpLive(false); rematch::SetPumpLive(false); SharedPump();
        for(int i=0;i<nh;++i) handlers[i](0,0,0,0,nullptr);
        auto stop=training::Counts();
        if(!check(stop.create==c.create && stop.pumps==c.pumps && ticks==(rl?2:0))) return 1;
    }
    for(bool obs : {false,true}) {
        reset(); observe=obs; want=true; prepareSuccess=false; fightSuccess=true; succeed=true;
        if(!check(!StartRematchPump() && prepares==1 && !rematch::Live() && training::Live()==obs)) return 1;
        SharedPump(); if(!check(ticks==0 && training::Counts().pumps==(obs?1U:0U))) return 1;
    }
    prepareSuccess=true;
    for(int failure : {0,1,2,3}) {
        reset(); observe=true; want=false; g_img.knownBuild=failure!=0; failedGroup=failure;
        if(!check(!StartRematchPump() && !training::Live() && nh==0 && installs==0 && prepares==0)) return 1;
        if(!check(groups==(failure==0 ? 0:failure))) return 1;
    }
    reset(); g_img.knownBuild=true; failedGroup=0; observe=false;
    if(!check(!training::Prepare() && groups==0 && nh==0)) return 1;
    training::SetPumpLive(true);
    if(!check(!training::Live())) return 1;
    observe=true;
    if(!check(training::Prepare() && training::Prepare() && nh==3 && groups==3)) return 1;
    std::printf("%d lifecycle checks ALL PASSED\n",checks);
}
"""
code = code.replace("@EXPORTS@", "\n".join(function(rematch, s) for s in ("bool Live()", "void SetPumpLive(", "void Pump()")))
code = code.replace("@TRAINING@", training.replace('#include "addon.h"', ''))
code = code.replace("@STARTUP@", "\n".join(function(main, s) for s in ("void SharedPump()", "bool StartRematchPump()")))
setup = Path(r"C:/nioh3-engine/tools/msvc/setup_x64.bat")
with tempfile.TemporaryDirectory(prefix="nioh3-pump-") as tmp:
    directory=Path(tmp)
    source=directory / "lifecycle.cpp"
    (directory / "addon.h").write_text((ROOT / "src/addon.h").read_text())
    exe=directory / "lifecycle.exe"
    source.write_text(code)
    command = f'call "{setup}" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /Fo"{directory / "lifecycle.obj"}" /Fe"{exe}" "{source}"'
    result=subprocess.run('cmd /d /s /c "' + command + '"',capture_output=True,text=True)
    assert result.returncode==0, result.stdout + result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    print(result.stdout.strip())
    assert result.returncode==0, result.stderr
print("startup wiring checks ALL PASSED")
