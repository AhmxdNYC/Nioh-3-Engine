"""Regression: quaternion-shaped list metadata must never become a facing write target."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'src/bosslog.cpp').read_text()
a=s.index('static bool FormFaceWrite(')
b=s.index('// #82256:',a)
production=s[a:b]
assert 'FormFaceFindQuats' not in s and 'FormFaceWriteQuats' not in s and 'g_faceQ' not in s
pre=r'''
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <utility>
std::vector<std::pair<uint64_t,size_t>> writes;
bool Copy(void* out,uint64_t a,size_t n){memcpy(out,(void*)a,n);return true;}
template<class T> bool Rd(uint64_t a,T& out){return Copy(&out,a,sizeof(out));}
bool WrBytes(uint64_t a,const void* in,size_t n){writes.push_back({a,n});memcpy((void*)a,in,n);return true;}
int checks=0;void check(bool b){++checks;if(!b){printf("FAIL %d\n",checks);exit(1);}}
'''
post=r'''
int main(){
 alignas(16) unsigned char chr[0x400]={},root[0x800]={};
 uint64_t cap=(uint64_t)chr,rp=(uint64_t)root;memcpy(chr+0x20,&rp,8);
 // A plausible heap pointer has low bits that are also a finite float.
 uint64_t head=0x000001b83d600000ULL;memcpy(root+0x458,&head,8);
 float q[4]={0,0,0,0};memcpy(&q[3],root+0x458,4);q[1]=std::sqrt(1-q[3]*q[3]);
 memcpy(root+0x44c,q,12); // preserve head bytes
 float yaw=-2*std::atan2(q[1],q[3])*57.29578f;
 // Exact old admission conditions admit this metadata as a negative-sign quaternion.
 check(std::isfinite(q[0])&&std::isfinite(q[1])&&std::isfinite(q[2])&&std::isfinite(q[3]));
 check(std::fabs(q[0])<=.02f&&std::fabs(q[2])<=.02f&&std::fabs(q[1])>=.05f&&std::fabs(q[3])>=.02f);
 check(std::fabs(q[1]*q[1]+q[3]*q[3]-1)<=.01f);
 check(std::fabs(-2*std::atan2(q[1],q[3])*57.29578f-yaw)<.7f);
 unsigned char before[sizeof(root)];memcpy(before,root,sizeof(root));
 float h=-(-125.9f)*.5f*.01745329f,yw[2]={std::sin(h),std::cos(h)};
 // Historical q+4/q+12 stores corrupt both list-header fields.
 memcpy(root+0x450,&yw[0],4);memcpy(root+0x458,&yw[1],4);
 uint64_t changed=0;memcpy(&changed,root+0x458,8);check(changed!=head);
 memcpy(root,before,sizeof(root));
 float matrix[12]={1,0,0,0,0,1,0,0,0,0,1,0};memcpy(chr+0x210,matrix,sizeof(matrix));
 for(int swap=0;swap<10;++swap){
   // Reused actor storage, fresh model root, rapid ten-session facing updates.
   unsigned char other[0x800];memcpy(other,before,sizeof(other));
   uint64_t active=(uint64_t)(swap%2?other:root);memcpy(chr+0x20,&active,8);
   for(int frame=0;frame<60;++frame){
     writes.clear();check(FormFaceWrite(cap,0x120,matrix));
     check(!memcmp((void*)active,before,sizeof(root)));
     check(writes.size()==2);
     for(auto w:writes)check((w.first==cap+0x120||w.first==cap+0x210)&&w.second==48);
   }
 }
 check(!memcmp(root,before,sizeof(root)));
 printf("face-list regression: %d checks PASS; old writer corrupts head, fixed writer preserves 10 reused sessions / 600 updates\n",checks);
}
'''
out=root/'build/face-list-test';out.mkdir(parents=True,exist_ok=True)
unit=out/'test.cpp';unit.write_text(pre+production+post)
setup=Path('C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat');exe=out/'test.exe'
cmd=f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /Fo"{out / "test.obj"}" /Fe"{exe}" "{unit}"'
r=subprocess.run('cmd /s /c "'+cmd+'"',capture_output=True,text=True);print(r.stdout,end='');print(r.stderr,end='');r.check_returncode()
subprocess.run([str(exe)],check=True,timeout=30)
