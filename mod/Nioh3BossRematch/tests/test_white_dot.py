"""#106000 white-dot Proceed walk: the production LogWhiteDot body with VM-arg fixtures, the harness parser on its exact output,
the site table, and (when the LFM extract is on this machine) every lesson Dojo script reaching the compass native."""
from pathlib import Path
import re, subprocess, sys, tempfile
HERE = Path(__file__).resolve()
BR = HERE.parents[1]; ROOT = HERE.parents[3]
s = (BR / 'src/rematch.cpp').read_text(encoding='utf-8')
def fn(sig):
    a = s.index(sig); e = s.index('{', a) + 1; d = 1
    while d:
        d += (s[e] == '{') - (s[e] == '}'); e += 1
    return s[a:e]
# 1. registration and sites: both compass natives, resolved by name, group FIGHT.
assert 'OnEntry("UI::SetCompassDestinationIndex", &HkDotIndex);' in s and 'OnEntry("UI::SetCompassDestination", &HkDot);' in s
g = (BR / 'src/sites_gen.h').read_text(encoding='utf-8')
for nm, rva in (('UI::SetCompassDestinationIndex', '0x23B7FD0'), ('UI::SetCompassDestination', '0xA3E0C8')):
    assert re.search(r'\{"%s", "FIGHT", kNative, %s, 5, "48 89 5C 24 10".*"%s"\}' % (re.escape(nm), rva, re.escape(nm)), g), nm
# 2. production body: log-only, no g_fight gate (#106298: the lesson dot comes after the win retires the attempt), last three float
#    args, once per attempt per point, row named while in the attempt or for 120 s after it ended.
code = r'''
#include <atomic>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>
using ULONGLONG=uint64_t;
struct Row{char id[8];}; std::vector<Row> g_rows{{"RD759"},{"RA0A1"}};
std::atomic<bool> g_fight; ULONGLONG g_fnFirst; int g_fightRow=1; int g_lastFightRow=-1; ULONGLONG g_lastFightEnd=0; ULONGLONG g_now=0; ULONGLONG Ms(){return g_now;} bool posOk=true; float g_pos[4]={75515,1271,39273,0};
std::vector<std::string> logs;
void Log(const char* f,...){char b[512];va_list a;va_start(a,f);vsnprintf(b,sizeof b,f,a);va_end(a);logs.push_back(b);}
bool PlayerPos(float* p){if(!posOk)return false;memcpy(p,g_pos,16);return true;}
uint32_t A[6]; uint16_t T[6]; int N;
int LessonVmArgs(uint64_t,uint32_t* v,uint16_t* t,int m){for(int i=0;i<N&&i<m;i++){v[i]=A[i];t[i]=T[i];}return N;}
uint32_t F(float f){uint32_t u;memcpy(&u,&f,4);return u;}
void set4(uint32_t first,uint16_t ty0,float x,float y,float z){N=4;A[0]=first;T[0]=ty0;A[1]=F(x);A[2]=F(y);A[3]=F(z);T[1]=T[2]=T[3]=2;}
static constexpr ULONGLONG kDotAfterMs = 120000;
@BODY@
int main(){
 set4(0,1,76494,1270,37765); g_fight=true; g_fnFirst=10; LogWhiteDot(0,"SetCompassDestinationIndex"); assert(logs.size()==1);
 LogWhiteDot(0,"SetCompassDestinationIndex"); assert(logs.size()==1); // same point, same attempt: once
 g_fnFirst=11; LogWhiteDot(0,"SetCompassDestinationIndex"); assert(logs.size()==2); // new attempt logs again
 T[2]=1; LogWhiteDot(0,"SetCompassDestinationIndex"); assert(logs.size()==2); T[2]=2; // a non-float coordinate is not a dot
 N=3; A[0]=F(76135);A[1]=F(3555);A[2]=F(55802);T[0]=T[1]=T[2]=2; g_fightRow=0; posOk=false; LogWhiteDot(0,"SetCompassDestination"); assert(logs.size()==3);
 N=2; LogWhiteDot(0,"SetCompassDestination"); assert(logs.size()==3); // too few args
 for(auto& l:logs) puts(l.c_str());
 // #106134: RA0A1 win retires the attempt (g_fight false, g_fightRow -1) 15 ms before the game sets the lesson dot.
 logs.clear(); posOk=true; g_fight=false; g_fightRow=-1; g_lastFightRow=1; g_lastFightEnd=1000; g_now=1015;
 set4(0,1,76200,1270,37000); LogWhiteDot(0,"SetCompassDestinationIndex"); assert(logs.size()==1);
 assert(logs[0].find("- Fight now row RA0A1")!=std::string::npos);
 LogWhiteDot(0,"SetCompassDestinationIndex"); assert(logs.size()==1); // per-frame repeat of the same point: once
 g_now=1000+120000; set4(0,1,10,20,30); LogWhiteDot(0,"SetCompassDestinationIndex"); assert(logs.size()==2);
 assert(logs[1].find("Fight now row")==std::string::npos); // 120 s after the attempt: logged without a row
 puts(logs[0].c_str()); puts(logs[1].c_str());
}
'''.replace('@BODY@', fn('static void LogWhiteDot('))
with tempfile.TemporaryDirectory() as d:
    p = Path(d); src = p / 'test.cpp'; exe = p / 'test.exe'; src.write_text(code)
    subprocess.run([sys.executable, '-m', 'ziglang', 'c++', '-std=c++17', '-O2', '-UNDEBUG', str(src), '-o', str(exe)], check=True)
    out = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout.splitlines()
assert len(out) == 5, out
assert out[3].startswith('Rematch: white dot 76200 1270 37000 (SetCompassDestinationIndex) - player') and out[3].endswith('- Fight now row RA0A1'), out[3]
assert out[4].startswith('Rematch: white dot 10 20 30 ') and 'row' not in out[4], out[4]
out = out[:3]
assert out[0] == 'Rematch: white dot 76494 1270 37765 (SetCompassDestinationIndex) - player 75515 1271 39273, 18.0 m flat - Fight now row RA0A1', out[0]
# 3. the harness parser reads exactly what the mod writes, newest per row, no stored coordinates.
sys.path.insert(0, str(ROOT / 'tools/testmatrix'))
import white_dot
logs = ['[13:00:00.000] ' + l for l in out] + ['[13:00:01.000] Rematch: lesson group 50701 checked, teacher fight not started - Fight now row RA0A1']
assert white_dot.target(logs, 'RA0A1')[:3] == (76494, 1270, 37765)
assert white_dot.target(logs, 'RD759')[:4] == (76135, 3555, 55802, 'SetCompassDestination')
assert white_dot.target(logs)[4] == 'RD759' and white_dot.target(logs, 'R9BE4') is None
walk = (ROOT / 'tools/testmatrix/white_dot_walk.ps1').read_text(encoding='utf-8')
assert 'white_dot.py' in walk and not re.search(r'\b7[56]\d{3}\b', walk), 'walk script must not carry Dojo coordinates'
# 4. universal source: every lesson Dojo script's entry reaches the compass native (needs the LFM extract; skipped otherwise).
LESSONS = {'RF5E5': 'A96CE28F8E68', 'RD759': 'E7923507DECC', 'R26F3': '23B6AC585823', 'RA0A1': '0AD2EFD76D68'}
lfm = ROOT / 'data/extract/lfm'
if not lfm.is_dir():
    lfm = Path(r'C:\Users\Ahmad Hamza\nioh3-engine\data\extract\lfm')
checked = 0
if lfm.is_dir():
    sys.path.insert(0, str(ROOT / 're/scripts_re')); import lfm_disasm
    for row, pre in LESSONS.items():
        path = next(lfm.glob('archive_*/*/%s*' % pre), None)
        assert path, row
        sc = lfm_disasm.load(str(path)); fn_at = {}; entry = None
        for i, ins in enumerate(sc.insns):
            if ins.op == 0x3B: fn_at[ins.args[0]] = i; entry = ins.args[0]
        seen, stack, hit = set(), [entry], False
        while stack and not hit:
            sym = stack.pop()
            if sym in seen or sym not in fn_at: continue
            seen.add(sym)
            for ins in sc.insns[fn_at[sym] + 1:]:
                if ins.op == 0x3B: break
                if ins.op == 0x1B and lfm_disasm.insn_name(sc, ins) in ('UI::SetCompassDestinationIndex', 'UI::SetCompassDestination'): hit = True
                elif ins.op == 0x1F: stack.append(ins.args[0])
        assert hit, '%s entry never sets the compass' % row
        checked += 1
print('PASS white dot: log-only hook (no g_fight gate, row kept 120 s after the attempt, float xyz, once per attempt), parser = mod output, no stored dots; %d/4 lesson scripts reach the compass native' % checked)
