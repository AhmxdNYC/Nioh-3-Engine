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
assert 'rematch::Start()' not in main
assert 'const bool live = StartRematchPump();' in main
assert 'if (live) ui::Start();' in main

code = r"""
#include <atomic>
#include <cstdio>
#include <initializer_list>
namespace addon {
int ticks=0, installs=0, prepares=0;
bool want=false, succeed=false;
bool g_prepared=false;
std::atomic<bool> g_live{false};
void Tick() { ++ticks; }
void Log(const char*, ...) {}
namespace rematch {
bool Prepare() { ++prepares; g_prepared=want; return want; }
@EXPORTS@
}
bool InstallPump(void (*callback)(), const char** why) {
    ++installs;
    callback(); // hook can run before Init publishes live; must remain passive
    if (ticks) return false;
    *why="injected failure";
    return succeed;
}
@STARTUP@
}
int main() {
    using namespace addon;
    int checks=0;
    auto check=[&](bool ok) { ++checks; if (!ok) { std::printf("FAIL %d\n",checks); return false; } return true; };
    for (bool enabled : {false,true}) for (bool installed : {false,true}) {
        want=enabled; succeed=installed; g_prepared=false; g_live=false;
        ticks=installs=prepares=0;
        if (!check(StartRematchPump()==(enabled && installed))) return 1;
        if (!check(prepares==1 && installs==(enabled ? 1 : 0))) return 1;
        if (!check(rematch::Live()==(enabled && installed) && ticks==0)) return 1;
        SharedPump(); SharedPump();
        if (!check(ticks==((enabled && installed) ? 2 : 0))) return 1;
        rematch::SetPumpLive(false); SharedPump();
        if (!check(!rematch::Live() && ticks==((enabled && installed) ? 2 : 0))) return 1;
    }
    g_prepared=false; rematch::SetPumpLive(true); SharedPump();
    if (!check(!rematch::Live() && ticks==2)) return 1;
    std::printf("%d lifecycle checks ALL PASSED\n",checks);
}
"""
code = code.replace("@EXPORTS@", "\n".join(function(rematch, s) for s in ("bool Live()", "void SetPumpLive(", "void Pump()")))
code = code.replace("@STARTUP@", "\n".join(function(main, s) for s in ("void SharedPump()", "bool StartRematchPump()")))
setup = Path(r"C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat")
with tempfile.TemporaryDirectory(prefix="nioh3-pump-") as tmp:
    directory=Path(tmp)
    source=directory / "lifecycle.cpp"
    exe=directory / "lifecycle.exe"
    source.write_text(code)
    command = f'call "{setup}" >nul && cl /nologo /std:c++17 /EHsc /W4 /WX /Fo"{directory / "lifecycle.obj"}" /Fe"{exe}" "{source}"'
    result=subprocess.run('cmd /d /s /c "' + command + '"',capture_output=True,text=True)
    assert result.returncode==0, result.stdout + result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    print(result.stdout.strip())
    assert result.returncode==0, result.stderr
print("startup wiring checks ALL PASSED")
