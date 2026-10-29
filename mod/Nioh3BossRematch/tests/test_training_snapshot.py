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
uint64_t managerSlot=0, gameMainSlot=0, playModeSlot=0, netSlot=0;
uintptr_t Site(const char* name) {
    ++siteCalls;
    if (!strcmp(name,"g:MissionMgr")) return reinterpret_cast<uintptr_t>(&managerSlot);
    if (!strcmp(name,"g:GameMain")) return reinterpret_cast<uintptr_t>(&gameMainSlot);
    if (!strcmp(name,"g:PlayMode")) return reinterpret_cast<uintptr_t>(&playModeSlot);
    if (!strcmp(name,"g:NetMgr")) return reinterpret_cast<uintptr_t>(&netSlot);
    return 0;
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
    if(!check(Prepare() && siteCalls==4)) return 1;
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
    if(!check(!out.loadingValid && !out.playModeValid && !out.connectionValid &&
              out.loading==0 && out.playMode==0 && out.connection==0 &&
              out.loadingGap && out.playModeGap && out.connectionGap)) return 1;
    auto telemetryPages=static_cast<unsigned char*>(VirtualAlloc(nullptr,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!telemetryPages) return 2;
    gameMainSlot=reinterpret_cast<uint64_t>(telemetryPages+4096-0x58A);
    playModeSlot=reinterpret_cast<uint64_t>(telemetryPages+4096-0x35);
    netSlot=reinterpret_cast<uint64_t>(telemetryPages+4096-0x1DB);
    telemetryPages[4096]=0; memset(telemetryPages+4095,0,4); telemetryPages[4097]=0;
    Pump();
    if(!check(Snapshot(out) && out.loadingValid && out.playModeValid && out.connectionValid &&
              out.loading==0 && out.playMode==0 && out.connection==0 &&
              out.loadingGap && out.playModeGap && out.connectionGap)) return 1;
    Pump();
    if(!check(Snapshot(out) && !out.loadingGap && !out.playModeGap && !out.connectionGap)) return 1;
    auto before=out;
    AcquireSRWLockShared(&g_snapshotLock); Pump(); ReleaseSRWLockShared(&g_snapshotLock);
    if(!check(SnapshotLosses()==1 && Snapshot(out) && out.sequence==before.sequence)) return 1;
    Pump();
    if(!check(Snapshot(out) && out.publicationLosses==1 && out.loadingGap &&
              out.playModeGap && out.connectionGap && out.missionGap)) return 1;
    Pump();
    if(!check(Snapshot(out) && !out.loadingGap && !out.playModeGap && !out.connectionGap && !out.missionGap)) return 1;
    if(!VirtualProtect(telemetryPages+4096,4096,PAGE_NOACCESS,&old)) return 2;
    Pump();
    if(!check(Snapshot(out) && !out.loadingValid && !out.playModeValid && !out.connectionValid &&
              out.loading==0 && out.playMode==0 && out.connection==0)) return 1;
    if(!VirtualProtect(telemetryPages+4096,4096,PAGE_READWRITE,&old)) return 2;
    // Mode straddles telemetryPages; making only the first page unreadable leaves the other fields readable.
    if(!VirtualProtect(telemetryPages,4096,PAGE_NOACCESS,&old)) return 2;
    telemetryPages[4096]=1; telemetryPages[4097]=7; Pump();
    if(!check(Snapshot(out) && out.loadingValid && out.loading==1 && out.connectionValid &&
              out.connection==7 && !out.playModeValid && out.playMode==0 && out.playModeGap)) return 1;
    if(!VirtualProtect(telemetryPages,4096,PAGE_READWRITE,&old)) return 2;
    int32_t mode=-7; memcpy(telemetryPages+4095,&mode,4); Pump();
    if(!check(Snapshot(out) && out.playModeValid && out.playMode==-7 && out.playModeGap)) return 1;
    Pump();
    if(!check(Snapshot(out) && !out.playModeGap)) return 1;
    gameMainSlot=UINT64_MAX-0x100; playModeSlot=UINT64_MAX-0x10; netSlot=UINT64_MAX-0x10;
    Pump();
    if(!check(Snapshot(out) && !out.loadingValid && !out.playModeValid && !out.connectionValid)) return 1;
    gameMainSlot=0; playModeSlot=1; netSlot=0; Pump();
    if(!check(Snapshot(out) && !out.loadingValid && !out.playModeValid && !out.connectionValid)) return 1;
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
    // Disjoint storage for generation witnesses: no accidental scalar aliasing.
    unsigned char loadObject[0x600]{}, modeObject[0x40]{}, netObject[0x200]{};
    gameMainSlot=reinterpret_cast<uint64_t>(loadObject);
    playModeSlot=reinterpret_cast<uint64_t>(modeObject);
    netSlot=reinterpret_cast<uint64_t>(netObject);
    Pump(); Snapshot(out);
    if(!check(out.loadingValid && out.sessionValid && out.loadGeneration>0 &&
              out.sessionGeneration>0 && out.loadingGap && out.sessionGap)) return 1;
    before=out; Pump(); Snapshot(out);
    if(!check(out.loadGeneration==before.loadGeneration && out.sessionGeneration==before.sessionGeneration &&
              !out.loadingGap && !out.sessionGap)) return 1;
    before=out; loadObject[0x58A]=1; Pump(); Snapshot(out);
    if(!check(out.loadGeneration==before.loadGeneration+1 && out.sessionGeneration==before.sessionGeneration &&
              out.missionEpoch==before.missionEpoch && !out.loadingGap)) return 1;
    before=out; loadObject[0x58A]=0; Pump(); Snapshot(out);
    if(!check(out.loadGeneration==before.loadGeneration+1 && out.sessionGeneration==before.sessionGeneration)) return 1;
    before=out; mode=17; memcpy(modeObject+0x34,&mode,4); netObject[0x1DC]=9; Pump(); Snapshot(out);
    if(!check(out.sessionGeneration==before.sessionGeneration+1 && out.loadGeneration==before.loadGeneration &&
              out.sessionValid && !out.sessionGap)) return 1;
    before=out; gameMainSlot=0; playModeSlot=0; Pump(); Snapshot(out);
    if(!check(!out.sessionValid && out.sessionGap && out.sessionGeneration==before.sessionGeneration &&
              !out.loadingValid && out.loadGeneration==before.loadGeneration)) return 1;
    Pump(); Snapshot(out);
    if(!check(out.sessionGeneration==before.sessionGeneration && out.loadGeneration==before.loadGeneration)) return 1;
    gameMainSlot=reinterpret_cast<uint64_t>(loadObject);
    playModeSlot=reinterpret_cast<uint64_t>(modeObject); Pump(); Snapshot(out);
    if(!check(out.sessionValid && out.sessionGap && out.loadingGap &&
              out.sessionGeneration==before.sessionGeneration+1 && out.loadGeneration==before.loadGeneration+1)) return 1;
    before=out; Pump(); Snapshot(out);
    if(!check(!out.sessionGap && !out.loadingGap && out.sessionGeneration==before.sessionGeneration &&
              out.loadGeneration==before.loadGeneration)) return 1;
    AcquireSRWLockShared(&g_snapshotLock); Pump(); ReleaseSRWLockShared(&g_snapshotLock);
    Pump(); Snapshot(out);
    if(!check(out.sessionValid && out.sessionGap && out.loadingGap &&
              out.sessionGeneration==before.sessionGeneration+1 && out.loadGeneration==before.loadGeneration+1)) return 1;
    before=out; Pump(); Snapshot(out);
    if(!check(!out.sessionGap && out.sessionGeneration==before.sessionGeneration && out.loadGeneration==before.loadGeneration)) return 1;
    write(0x1CBD,67064); Pump(); Snapshot(out);
    if(!check(out.sessionGeneration==before.sessionGeneration+1 && out.loadGeneration==before.loadGeneration)) return 1;
    before=out; write(0x1CBD,67065); Pump(); Snapshot(out);
    if(!check(out.sessionGeneration==before.sessionGeneration && out.loadGeneration==before.loadGeneration)) return 1;
    before=out;
    SetPumpLive(false); Pump(); out.mission=123;
    if(!check(!Snapshot(out) && out.sequence==0 && out.mission==0)) return 1;
    SetPumpLive(true);
    if(!check(Snapshot(out) && out.sequence==before.sequence && siteCalls==4)) return 1;
    EntryEvent events[64];
    SetPumpLive(false);
    Create(0,0,0,0,nullptr); // disabled remains passive
    if(!check(Events(events,64)==0 && Counts().create==0)) return 1;
    SetPumpLive(true);
    if(!check(Events(nullptr,64)==0 && Events(events,0)==0)) return 1;
    Create(UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX,nullptr);
    Begin(0,0,0,0,nullptr); End(0,0,0,0,nullptr);
    if(!check(Events(events,64)==3 && events[0].kind==EntryKind::Create &&
              events[1].kind==EntryKind::Begin && events[2].kind==EntryKind::End &&
              events[0].sequence==1 && events[2].sequence==3)) return 1;
    if(!check(events[0].observation.sequence==before.sequence &&
              events[0].observation.sessionGeneration==before.sessionGeneration &&
              events[0].observation.loadGeneration==before.loadGeneration &&
              events[0].monotonicMs>=events[0].observation.monotonicMs)) return 1;
    AcquireSRWLockShared(&g_snapshotLock);
    Create(0,0,0,0,nullptr);
    ReleaseSRWLockShared(&g_snapshotLock);
    if(!check(EventLosses().contention==1 && EventLosses().capacity==0 &&
              Counts().create==2 && Events(events,64)==3)) return 1;
    for(int i=0;i<65;++i) Begin(0,0,0,0,nullptr);
    if(!check(Events(events,64)==64 && events[0].sequence==5 && events[63].sequence==68 &&
              EventLosses().capacity==4 && EventLosses().contention==1)) return 1;
    if(!check(Events(events,2)==2 && events[0].sequence==5 && events[1].sequence==6 &&
              Events(events,64)==64 && Counts().begin==66 && Counts().end==1)) return 1;
    const auto saved=events[63].observation;
    Pump();
    if(!check(Events(events,64)==64 && events[63].observation.sequence==saved.sequence &&
              events[63].observation.sessionGeneration==saved.sessionGeneration)) return 1;
    SetPumpLive(false);
    End(0,0,0,0,nullptr);
    if(!check(Events(events,64)==0 && Counts().end==1 && EventLosses().capacity==4)) return 1;
    VirtualFree(telemetryPages,0,MEM_RELEASE);
    VirtualFree(pages,0,MEM_RELEASE);
    printf("%d actual snapshot checks ALL PASSED\n",checks);
}
'''.replace('@TRAINING@', training.replace('#include "addon.h"',''))
setup = Path(r'C:/nioh3-engine/tools/msvc/setup_x64.bat')
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
