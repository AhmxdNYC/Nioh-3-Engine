"""Reproduce v4 restore ownership loss from pinned actual bodies; no game/build deployment."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
REVISION = "c297a20"
SOURCE = "mod/Nioh3BossRematch/src/rematch.cpp"


def body(text, signature):
    start = text.index(signature)
    opening = text.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main():
    source = subprocess.run(
        ["git", "-C", str(ROOT), "show", f"{REVISION}:{SOURCE}"],
        check=True, capture_output=True, text=True,
    ).stdout
    code = r'''
#include <cstdint>
#include <cstdio>
#include <cstring>
using ULONGLONG = unsigned long long;
@STRUCT@ g_sp;
constexpr uint64_t record = 0x10000;
float position[4]; uint32_t recordFlags;
bool readablePosition=true, readableFlags=true;
bool writablePosition=true, writableFlags=true;
uint64_t found=record;
int writeCalls=0, checks=0;
uint64_t CallWpFind(uint32_t) { return found; }
bool Copy(void* dst, uint64_t address, size_t size) {
    if (address != record+0x40 || size != 16 || !readablePosition) return false;
    memcpy(dst,position,16); return true;
}
bool Rd(uint64_t address, uint32_t& flags) {
    if (address != record+8 || !readableFlags) return false;
    flags=recordFlags; return true;
}
bool WrBytes(uint64_t address, const void* src, size_t size) {
    ++writeCalls;
    if (address==record+0x40 && size==16 && writablePosition) {
        memcpy(position,src,16); return true;
    }
    if (address==record+8 && size==4 && writableFlags) {
        memcpy(&recordFlags,src,4); return true;
    }
    return false;
}
void Log(const char*, ...) {}
@WRITE@
@RESTORE@
bool check(bool ok, const char* why) {
    ++checks; if (!ok) std::printf("FAIL: %s\n",why); return ok;
}
void reset() {
    g_sp={}; g_sp.st=2; g_sp.rec=record; g_sp.point=67064; g_sp.landCheck=true;
    for (int i=0;i<4;++i) { g_sp.orig[i]=float(i); g_sp.mine[i]=float(i+10); }
    g_sp.origFlags=0x24; g_sp.mineFlags=0x20;
    memcpy(position,g_sp.mine,16); recordFlags=g_sp.mineFlags;
    found=record; readablePosition=readableFlags=writablePosition=writableFlags=true;
    writeCalls=0;
}
int main() {
    reset(); SpawnRestore("success");
    if (!check(!memcmp(position,g_sp.orig,16) && recordFlags==g_sp.origFlags,"success restores bytes") ||
        !check(g_sp.st==0 && g_sp.rec==0 && !g_sp.landCheck && writeCalls==2,"success releases state")) return 1;
    for (int failure=0; failure<5; ++failure) {
        reset();
        if (failure==0) writablePosition=false;
        if (failure==1) writableFlags=false;
        if (failure==2) readablePosition=false;
        if (failure==3) readableFlags=false;
        if (failure==4) found=0; // transient missing lookup, not proof of destruction
        SpawnRestore("injected failure");
        if (!check(g_sp.st==0 && g_sp.rec==0,"failure drops bookkeeping") ||
            !check(recordFlags==g_sp.mineFlags,"override flag remains") ||
            !check(!memcmp(position,failure==1 ? g_sp.orig : g_sp.mine,16),"expected residual coordinates") ||
            !check(writeCalls==(failure==0 ? 1 : failure==1 ? 2 : 0),"expected write attempts")) return 1;
        const int previousCalls=writeCalls;
        writablePosition=writableFlags=readablePosition=readableFlags=true; found=record;
        SpawnRestore("retry after recovery");
        if (!check(writeCalls==previousCalls && recordFlags==g_sp.mineFlags,"recovered retry cannot restore")) return 1;
    }
    for (int foreign=0;foreign<3;++foreign) {
        reset();
        if (foreign==0) position[0]=100.0f;
        if (foreign==1) recordFlags=0x99;
        if (foreign==2) found=record+0x100; // different lookup identity
        float before[4]; memcpy(before,position,16); const uint32_t beforeFlags=recordFlags;
        SpawnRestore("foreign control");
        if (!check(writeCalls==0 && !memcmp(before,position,16) && recordFlags==beforeFlags,"foreign control not written") ||
            !check(g_sp.st==0 && g_sp.rec==0,"foreign control releases state")) return 1;
    }
    std::printf("%d checks PASS; 5 failure witnesses + success/3 foreign controls; repair NOT implemented\n",checks);
}
'''
    for tag, signature in (("STRUCT", "struct Spawn {"), ("WRITE", "bool SpawnWrite("), ("RESTORE", "void SpawnRestore(")):
        code = code.replace(f"@{tag}@", body(source, signature))
    setup = ROOT / "tools/msvc/setup_x64.bat"
    with tempfile.TemporaryDirectory(prefix="nioh3-restore-audit-") as tmp:
        directory = Path(tmp)
        cpp, exe = directory / "audit.cpp", directory / "audit.exe"
        cpp.write_text(code, encoding="utf-8")
        command = f'call "{setup}" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /Fo"{directory / "audit.obj"}" /Fe"{exe}" "{cpp}"'
        compile_result = subprocess.run('cmd /d /s /c "' + command + '"', capture_output=True, text=True, timeout=60)
        if compile_result.returncode:
            raise RuntimeError(compile_result.stdout + compile_result.stderr)
        result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
        print(f"Pinned actual-body audit {REVISION}: {SOURCE}")
        print(result.stdout.strip())
        if result.returncode:
            raise RuntimeError(result.stderr or "behavioral assertion failed")


if __name__ == "__main__":
    main()
