"""Exercise production NavRepeat with staggered diagonal holds (player report #40)."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / 'src/menu.cpp').read_text(encoding='utf-8-sig')
start = source.index('uint32_t NavRepeat(')
body = source[start:source.index('\nvoid OnPresentImpl()', start)]
test = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using ULONGLONG = unsigned long long;
constexpr uint32_t N_UP=1, N_DOWN=2, N_LEFT=4, N_RIGHT=8;
const uint32_t kNavDirs[4]={N_UP,N_DOWN,N_LEFT,N_RIGHT};
uint32_t g_padHeld[4]={}, g_keyHeld=0;
ULONGLONG g_padHeldAt[4]={}, g_keyHeldAt=0, g_rep[4]={};
uint32_t PadNav(uint32_t v){return v;}
uint32_t KeyNav(uint32_t v){return v;}
int checks=0;
void check(bool b){++checks;if(!b){printf("FAIL %d\n",checks);exit(1);}}
void reset(){for(int i=0;i<4;++i){g_padHeld[i]=0;g_padHeldAt[i]=0;g_rep[i]=0;}g_keyHeld=0;g_keyHeldAt=0;}
''' + body + r'''
uint32_t poll(uint32_t pressed,uint32_t held,ULONGLONG t,bool keyboard=false){
 if(keyboard){g_keyHeld=held;g_keyHeldAt=t;}else{g_padHeld[0]=held;g_padHeldAt[0]=t;}
 return NavRepeat(pressed,t);
}
int main(){
 // A diagonal reached at different times must not interleave horizontal edits with scrolling.
 for(bool keyboard:{false,true})for(uint32_t vertical:{N_UP,N_DOWN})for(uint32_t horizontal:{N_LEFT,N_RIGHT}){
  reset();int edits=0,moves=0;
  for(ULONGLONG t=1000;t<6000;t+=16){
   const uint32_t pressed=t==1000?vertical:t==1064?horizontal:0;
   const auto acts=poll(pressed,vertical|(t>=1064?horizontal:0),t,keyboard);
   if(acts&vertical)++moves;else if(acts&horizontal)++edits; // actual host priority
  }
  check(moves>40);check(edits==0);
  // Releasing vertical cannot resurrect the old horizontal hold's repeat timer.
  check(poll(0,horizontal,6016,keyboard)==0);
  poll(0,0,6032,keyboard);
  check(poll(horizontal,horizontal,6048,keyboard)==horizontal);
 }
 // Ordinary horizontal settings still support an edge, 400ms delay, then 90ms repeats.
 for(uint32_t dir:{N_LEFT,N_RIGHT}){
  reset();check(poll(dir,dir,1000)==dir);check(poll(0,dir,1399)==0);
  check(poll(0,dir,1400)==dir);check(poll(0,dir,1489)==0);check(poll(0,dir,1490)==dir);
  check(poll(0,0,1500)==0);check(poll(0,dir,1600)==0); // release requires a fresh edge
 }
 // Vertical direction wins a mixed first edge and suppresses an existing horizontal repeat.
 reset();check(poll(N_DOWN|N_RIGHT,N_DOWN|N_RIGHT,1000)==N_DOWN);
 reset();check(poll(N_RIGHT,N_RIGHT,1000)==N_RIGHT);
 check(poll(N_DOWN,N_DOWN|N_RIGHT,1400)==N_DOWN);
 check(poll(0,N_DOWN|N_RIGHT,1490)==0);
 // Stale pad state must not suppress a new keyboard edit.
 reset();poll(N_DOWN,N_DOWN,1000);
 check(poll(N_RIGHT,N_RIGHT,1400,true)==N_RIGHT);
 printf("navigation axis priority: %d checks PASS (production NavRepeat; offline)\n",checks);
}
'''
test = test.replace('#include <cstdlib>', '#include <cstdlib>\n#include <initializer_list>')
out = root / 'build/nav-repeat-axis-test'
out.mkdir(parents=True, exist_ok=True)
unit = out / 'test.cpp'
unit.write_text(test, encoding='utf-8')
setup = os.environ.get('MSVC_SETUP', 'C:/Users/User/pmsvc/msvc/setup_x64.bat')
exe = out / 'test.exe'
cmd = f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /Fo"{out / "test.obj"}" /Fe"{exe}" "{unit}"'
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
print(r.stdout, end=''); print(r.stderr, end=''); r.check_returncode()
subprocess.run([str(exe)], check=True, timeout=30)
