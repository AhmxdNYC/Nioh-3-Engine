"""#31 KiStyleHook guard offline test (#98489). Extracts the real guard + hook from src/training.c,
compiles it against a mock native KiBarStyle (reads [charParam+0x80]) and checks: valid path unchanged,
bad/low/non-canonical pointers skip native, a faulting pointer (crash #31 value 0x9000057B1) is caught,
last good style is kept, log is capped at 5."""
import os, re, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(here, "..", "src", "training.c"), encoding="utf-8").read()
start = src.index("static volatile LONG g_ki_bad;")
fn = src.index("static __attribute__((noinline)) uint8_t KiStyleHook(", start)
end = src.index("\n}\n", fn) + 3
block = src[start:end]
harness = r'''
#include <stdint.h>
#include <stdio.h>
#include <windows.h>
static uint64_t B; static uint64_t Grva(uint64_t r){(void)r;return 0;}
static int logs; static DWORD g_ki_seen; static int g_cfg_goldki=1;
#define Log(...) (++logs)
static int native_calls;
static __attribute__((noinline)) uint8_t MockKiBarStyle(uint64_t p){ ++native_calls; return (uint8_t)(*(volatile int32_t*)(p+0x80) & 1); }
#define GetTickCount() (12345u)
''' + block + r'''
static int fails, n;
#define CHECK(c,m) do{ ++n; if(c) printf("ok   %s\n",m); else { ++fails; printf("FAIL %s\n",m);} }while(0)
int main(void){
  static int32_t obj1[64], obj0[64]; obj1[0x80/4]=1; obj0[0x80/4]=0;
  B=(uint64_t)MockKiBarStyle;
  uint8_t s=KiStyleHook((uint64_t)obj1); CHECK(s==1 && native_calls==1 && g_ki_seen==12345u, "valid style 1: native returned + Ki row stamped");
  s=KiStyleHook((uint64_t)obj0); CHECK(s==0 && native_calls==2, "valid style 0: native returned");
  KiStyleHook((uint64_t)obj1); g_ki_seen=0; int nc=native_calls;
  s=KiStyleHook(0); CHECK(s==1 && native_calls==nc && g_ki_seen==0, "null charParam: native skipped, last style 1 kept, no stamp");
  s=KiStyleHook(0xFFFF); CHECK(s==1 && native_calls==nc, "low pointer 0xFFFF rejected");
  s=KiStyleHook(0x8000000000000000ull); CHECK(s==1 && native_calls==nc, "non-canonical pointer rejected");
  s=KiStyleHook(0x00007FFFFFFF0000ull); CHECK(s==1 && native_calls==nc, "top-of-user boundary rejected");
  s=KiStyleHook(0x9000057B1ull); CHECK(s==1 && native_calls==nc+1, "crash #31 charParam 0x9000057B1: fault caught, last style kept");
  CHECK(g_ki_last==1, "fault does not overwrite last style");
  s=KiStyleHook((uint64_t)obj0); CHECK(s==0 && g_ki_last==0, "valid call after fault works again");
  int l0=logs; for(int i=0;i<10;i++) KiStyleHook(0); CHECK(g_ki_bad>=15, "bad counter counts every reject");
  CHECK(logs-l0<=5 && logs<=6, "guard log capped at 5 lines (+1 first-row line)");
  printf("%d/%d %s\n", n-fails, n, fails?"FAILED":"ALL PASSED"); return fails?1:0;
}
'''
d = tempfile.mkdtemp(prefix="ki31_")
c = os.path.join(d, "t.c"); exe = os.path.join(d, "t.exe")
open(c, "w").write(harness)
r = subprocess.run([sys.executable, "-m", "ziglang", "cc", "-target", "x86_64-windows-gnu", "-O2", "-fms-extensions",
                    "-Xclang", "-fasync-exceptions", c, "-o", exe], capture_output=True, text=True)
if r.returncode: print(r.stdout, r.stderr); sys.exit(1)
sys.exit(subprocess.run([exe]).returncode)
