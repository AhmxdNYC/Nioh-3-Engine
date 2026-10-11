"""Actual observer + actual SEH reads, cached publication and unreadable pages; no game."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
training = (ROOT / "src/training.cpp").read_text()
code = r'''
#include "addon.h"
#include <atomic>
#include <cstdio>
#include <cstring>
namespace addon {
Image g_img{0,0,0,0,true};
bool observe=false;
int siteCalls=0;
uint64_t managerSlot=0;
uintptr_t Site(const char* name) {
    ++siteCalls;
    return !strcmp(name,"g:MissionMgr") ? reinterpret_cast<uintptr_t>(&managerSlot) : 0;
}
void Log(const char*, ...) {}
int IniInt(const wchar_t*,const wchar_t*,int) { return observe; }
bool GroupOk(const char*,const char**) { return true; }
void OnEntry(const char*,Handler) {}
}
@TRAINING@
int main() {
    using namespace addon;
    using namespace addon::training;
    int checks=0;
    auto check=[&](bool ok) { ++checks; if(!ok) printf("FAIL %d\n",checks); return ok; };
    MissionSnapshot out;
    if(!check(!Prepare() && siteCalls==0)) return 1;
    Pump();
    if(!check(!Snapshot(out) && out.sequence==0 && Counts().pumps==0)) return 1;
    observe=true;
    if(!check(Prepare() && siteCalls==1)) return 1;
    SetPumpLive(true);
    if(!check(!Snapshot(out))) return 1;
    uint64_t manager[2]{};
    managerSlot=reinterpret_cast<uint64_t>(manager);
    auto pages=static_cast<unsigned char*>(VirtualAlloc(nullptr,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!pages) return 2;
    auto state=pages+4096-0x28;
    manager[1]=reinterpret_cast<uint64_t>(state);
    auto write=[&](uint32_t mission,uint32_t retry) {
        memcpy(state+0x28,&mission,4); memcpy(state+0x14,&retry,4);
    };
    write(0,0); Pump();
    if(!check(Snapshot(out) && out.missionValid && out.retryValid && out.mission==0 &&
              out.retry==0 && out.missionEpoch==1 && !out.missionGap && out.monotonicMs>0)) return 1;
    write(0x1CBC,67064); Pump();
    if(!check(Snapshot(out) && out.sequence==2 && out.missionEpoch==2 && out.mission==0x1CBC && out.retry==67064)) return 1;
    write(0x1CBC,67065); Pump();
    if(!check(Snapshot(out) && out.missionEpoch==2 && out.retry==67065)) return 1;
    DWORD old=0;
    if(!VirtualProtect(pages+4096,4096,PAGE_NOACCESS,&old)) return 2;
    Pump();
    if(!check(Snapshot(out) && !out.missionValid && out.mission==0 && out.retryValid &&
              out.retry==67065 && out.missionGap && out.missionEpoch==2)) return 1;
    if(!VirtualProtect(pages+4096,4096,PAGE_READWRITE,&old)) return 2;
    Pump();
    if(!check(Snapshot(out) && out.missionValid && out.missionGap && out.missionEpoch==2)) return 1;
    Pump();
    if(!check(Snapshot(out) && !out.missionGap && out.missionEpoch==2)) return 1;
    manager[1]=0;
    Pump();
    if(!check(Snapshot(out) && !out.missionValid && !out.retryValid && out.retry==0 && out.missionGap)) return 1;
    manager[1]=reinterpret_cast<uint64_t>(state);
    write(0x1CBD,0); Pump();
    if(!check(Snapshot(out) && out.missionValid && out.retryValid && out.retry==0 && out.missionEpoch==3 && out.missionGap)) return 1;
    auto before=out;
    AcquireSRWLockShared(&g_snapshotLock); Pump(); ReleaseSRWLockShared(&g_snapshotLock);
    if(!check(SnapshotLosses()==1 && Snapshot(out) && out.sequence==before.sequence)) return 1;
    managerSlot=UINT64_MAX-3; Pump();
    if(!check(Snapshot(out) && !out.missionValid && !out.retryValid)) return 1;
    managerSlot=reinterpret_cast<uint64_t>(manager); manager[1]=UINT64_MAX-0x10; Pump();
    if(!check(Snapshot(out) && !out.missionValid && !out.retryValid)) return 1;
    manager[1]=reinterpret_cast<uint64_t>(state); write(0,0); Pump();
    if(!check(Snapshot(out) && out.missionValid && out.mission==0 && out.missionEpoch==4)) return 1;
    before=out;
    // The converse partial read: retry inaccessible while mission is readable.
    state=pages+4096-0x18; manager[1]=reinterpret_cast<uint64_t>(state);
    write(0x1CBC,67064);
    if(!VirtualProtect(pages,4096,PAGE_NOACCESS,&old)) return 2;
    Pump();
    if(!check(Snapshot(out) && out.missionValid && out.mission==0x1CBC &&
              !out.retryValid && out.retry==0 && out.missionEpoch==5)) return 1;
    if(!VirtualProtect(pages,4096,PAGE_READWRITE,&old)) return 2;
    Pump();
    if(!check(Snapshot(out) && out.retryValid && out.retry==67064 && out.missionEpoch==5)) return 1;
    before=out;
    SetPumpLive(false); Pump(); out.mission=123;
    if(!check(!Snapshot(out) && out.sequence==0 && out.mission==0)) return 1;
    SetPumpLive(true);
    if(!check(Snapshot(out) && out.sequence==before.sequence && siteCalls==1)) return 1;
    VirtualFree(pages,0,MEM_RELEASE);
    printf("%d actual snapshot checks ALL PASSED\n",checks);
}
'''.replace('@TRAINING@', training.replace('#include "addon.h"',''))
setup = Path(r'C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat')
with tempfile.TemporaryDirectory(prefix='nioh3-training-snapshot-') as tmp:
    directory=Path(tmp)
    (directory/'addon.h').write_text((ROOT/'src/addon.h').read_text())
    source=directory/'snapshot.cpp'
    source.write_text(code)
    exe=directory/'snapshot.exe'
    command=f'call "{setup}" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /Fo"{directory / "snapshot.obj"}" /Fe"{exe}" "{source}"'
    result=subprocess.run('cmd /d /s /c "'+command+'"',capture_output=True,text=True)
    assert result.returncode==0, result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    print(result.stdout.strip())
    assert result.returncode==0, result.stderr
