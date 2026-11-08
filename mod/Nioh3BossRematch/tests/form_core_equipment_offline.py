"""Test the real table mapper and equipped-index reader; optionally verify local extracted game tables."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--game-data', type=Path, help='Optional existing data/extract/lfm directory; read only')
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
out = root / 'build/form-core-equipment-test'
out.mkdir(parents=True, exist_ok=True)
source = r'''
#include "form_core_equipment.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
using namespace formcore;
int checks=0;
void check(bool v){++checks;if(!v){printf("FAIL check %d\n",checks);abort();}}
template<class T>void put(std::vector<uint8_t>& b,size_t off,T v){std::memcpy(b.data()+off,&v,sizeof v);}
std::vector<uint8_t> bytes(const char* path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
 std::vector<uint8_t> items(6*kItemStride),params(3*kParamStride);
 for(int i=0;i<3;++i){
  size_t c=size_t(i)*kItemStride,s=size_t(i+3)*kItemStride,p=size_t(i)*kParamStride;
  put(items,c+0x64,uint32_t(900000000));put(items,c+0x152,uint16_t(100+i));
  put(items,c+0x180,uint16_t(200+i));items[c+0x182]=9;
  put(params,p+0x0C,uint32_t(200+i));put(params,p+0x18,uint32_t(300+i));
  put(items,s+0x152,uint16_t(300+i));put(items,s+0x64,uint32_t(700000000));items[s+0x182]=7;
  put(items,s+0x54,uint32_t(0x123400+i));
 }
 auto map=BuildMap(items,params);check(map.size()==3);
 for(int i=0;i<3;++i)check(Lookup(map,uint16_t(100+i))==uint32_t(0x123400+i));
 check(Lookup(map,0)==0&&Lookup(map,0xFFFF)==0&&Lookup(map,999)==0);
 check(BuildMap({},params).empty()&&BuildMap(items,{}).empty());
 auto bad=items;bad.pop_back();check(BuildMap(bad,params).empty());
 bad=items;put(bad,0x64,uint32_t(700000000));check(Lookup(BuildMap(bad,params),100)==0);
 bad=items;bad[0x182]=7;check(Lookup(BuildMap(bad,params),100)==0);
 bad=items;put(bad,0x152,uint16_t(0));check(BuildMap(bad,params).size()==2);
 bad=items;put(bad,0x180,uint16_t(0));check(BuildMap(bad,params).size()==2);
 bad=params;put(bad,0x18,uint32_t(0x10000));check(BuildMap(items,bad).size()==2);
 bad=items;put(bad,3*kItemStride+0x54,uint32_t(0));check(BuildMap(bad,params).size()==2);
 bad=items;put(bad,3*kItemStride+0x54,uint32_t(0xFFFFFFFF));check(BuildMap(bad,params).size()==2);
 bad=items;put(bad,3*kItemStride+0x64,uint32_t(900000000));check(BuildMap(bad,params).size()==2);
 bad=items;bad[3*kItemStride+0x182]=9;check(BuildMap(bad,params).size()==2);
 // Duplicate core keys, parameter keys and seal keys are rejected, not guessed.
 bad=items;put(bad,kItemStride+0x152,uint16_t(100));check(Lookup(BuildMap(bad,params),100)==0);
 bad=params;put(bad,kParamStride+0x0C,uint32_t(200));check(Lookup(BuildMap(items,bad),100)==0);
 bad=items;put(bad,4*kItemStride+0x152,uint16_t(300));check(Lookup(BuildMap(bad,params),100)==0);
 // Empty slots, maximum count/index, unreadable fields and concurrent slot/count changes.
 const uint64_t base=0x10000000;std::vector<uint8_t> inv(0x23B6C0);
 put(inv,0x927D0,uint64_t(2500));put(inv,0x23B6B8,uint32_t(0));put(inv,0x23B6BC,uint32_t(2499));
 put(inv,0x10,uint16_t(100));put(inv,0x10+2499*0xF0,uint16_t(101));
 int reads=0,changeAt=0;bool fault=false;
 auto read=[&](uint64_t a,auto& v){
  ++reads;if(fault||a<base||a-base+sizeof v>inv.size())return false;
  if(changeAt&&reads==changeAt)put(inv,0x23B6BC,uint32_t(1));
  std::memcpy(&v,inv.data()+a-base,sizeof v);return true;
 };
 uint16_t equipped[2];check(EquippedItems(base,read,equipped)&&equipped[0]==100&&equipped[1]==101);
 check(!EquippedItems(0,read,equipped));fault=true;check(!EquippedItems(base,read,equipped));fault=false;
 put(inv,0x23B6BC,uint32_t(2500));check(!EquippedItems(base,read,equipped));
 put(inv,0x23B6BC,uint32_t(0xFFFFFFFF));check(EquippedItems(base,read,equipped)&&equipped[1]==0);
 put(inv,0x23B6B8,uint32_t(0xFFFFFFFF));put(inv,0x927D0,uint64_t(0));check(EquippedItems(base,read,equipped)&&equipped[0]==0&&equipped[1]==0);
 put(inv,0x927D0,uint64_t(2501));check(!EquippedItems(base,read,equipped));
 put(inv,0x927D0,uint64_t(2500));put(inv,0x23B6B8,uint32_t(0));put(inv,0x23B6BC,uint32_t(2499));
 put(inv,0x10,uint16_t(0));check(!EquippedItems(base,read,equipped));
 put(inv,0x10,uint16_t(0xFFFF));check(!EquippedItems(base,read,equipped));put(inv,0x10,uint16_t(100));
 reads=0;changeAt=6;check(!EquippedItems(base,read,equipped));
 if(argc==3){
  auto it=bytes(argv[1]),pt=bytes(argv[2]);check(it.size()>8&&pt.size()>8);
  check(it.size()==8+size_t(Field<uint32_t>(it.data(),4))*kItemStride);
  check(pt.size()==8+size_t(Field<uint32_t>(pt.data(),4))*kParamStride);
  it.erase(it.begin(),it.begin()+8);pt.erase(pt.begin(),pt.begin()+8);
  auto live=BuildMap(it,pt);check(live.size()==117);
  check(Lookup(live,0xEB60)==0x5129B);check(Lookup(live,0xBD34)==0xA2ACB);check(Lookup(live,0xDFFB)==0x5B402);
  printf("Extracted game tables: %zu mappings; Ibaraki/Maelstrom/Yasha match observed spawn IDs\n",live.size());
 }
 printf("form-core equipment: %d checks PASS (offline; no game)\n",checks);
}
'''
unit = out / 'equipment.cpp'
unit.write_text(source, encoding='utf-8')
exe = out / 'equipment.exe'
setup = Path('C:/Users/Ahmad Hamza/nioh3-engine/tools/msvc/setup_x64.bat')
cmd = f'call "{setup}" >nul && cl /nologo /EHsc /std:c++17 /W4 /I"{root / "src"}" /Fo"{out / "equipment.obj"}" /Fe"{exe}" "{unit}"'
r = subprocess.run('cmd /s /c "' + cmd + '"', capture_output=True, text=True)
print(r.stdout, end='')
print(r.stderr, end='')
r.check_returncode()
command = [str(exe)]
if args.game_data:
    command += [str(next((args.game_data / 'archive_03/2E').glob('D4E1DB3B*'))),
                str(args.game_data / 'archive_03/23/DAC95BA0146EC13EC2F5D25530800A24F092D403B890FE53419274CEACCBAF')]
subprocess.run(command, check=True, timeout=30)
