#!/usr/bin/env python3
"""Compile the actual direct-hide helper against fake memory; never accesses a game."""
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
source = (ROOT / 'mod/Nioh3BossRematch/src/bosslog.cpp').read_text()
helper = source[source.index('void FormHideDirectTick('):source.index('void FormHideTick(')]
preamble = r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
std::atomic<int> g_fhMode{3};
std::atomic<uint64_t> g_fs0CapChr{0x30000};
double g_fs0CapT=0, clockNow=1;
uint64_t g_base=0x140000000;
std::unordered_map<uint64_t,uint8_t> memory;
int writes=0, badByte=-1;
bool failWrite=false, failRead=false;
constexpr uint64_t player=0x10000, other=0x20000;
double Now() { return clockNow; }
void Log(const char*, ...) {}
bool Copy(void* dst, uint64_t addr, size_t n) {
    assert(addr==g_base+0xA550E8 && n==29);
    const uint8_t bytes[29]={0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x80,0xB9,0xD4,0x09,0,0,0,
        0x40,0x8A,0xFA,0x48,0x8B,0xD9,0x88,0x91,0x40,0xB4,0,0};
    memcpy(dst,bytes,n);
    if(badByte>=0) static_cast<uint8_t*>(dst)[badByte]^=1;
    return true;
}
bool Rd(uint64_t addr,uint8_t& value) {
    if(failRead || !memory.count(addr)) return false;
    value=memory.at(addr); return true;
}
bool WrBytes(uint64_t addr,const void* src,size_t n) {
    assert(n==1 && memory.count(addr));
    if(failWrite) return false;
    memory[addr]=*static_cast<const uint8_t*>(src); ++writes; return true;
}
'''
main = r'''
int main(int argc,char** argv) {
    assert(argc==2); std::string scenario=argv[1];
    memory[player+0xB440]=0; memory[other+0xB440]=0;
    if(scenario=="guard_tail") {
        badByte=28; FormHideDirectTick(player); assert(writes==0); return 0;
    }
    if(scenario=="native_unchanged") {
        for(int m: {0,1,2}) { g_fhMode=m; FormHideDirectTick(player); }
        assert(writes==0); return 0;
    }
    if(scenario=="delay") {
        clockNow=0.49; FormHideDirectTick(player); assert(writes==0);
        clockNow=0.5; FormHideDirectTick(player); assert(writes==1); return 0;
    }
    if(scenario=="write_failure") {
        failWrite=true; FormHideDirectTick(player); failWrite=false;
        clockNow=22; FormHideDirectTick(player); assert(writes==0); return 0;
    }
    if(scenario=="read_failure") {
        failRead=true; FormHideDirectTick(player); assert(writes==0); return 0;
    }
    if(scenario=="saved_one") memory[player+0xB440]=1;
    FormHideDirectTick(player); assert(writes==1 && memory[player+0xB440]==1);
    if(scenario=="player_changed") {
        FormHideDirectTick(other); assert(writes==1 && memory[other+0xB440]==0); return 0;
    }
    if(scenario=="external_change") {
        memory[player+0xB440]=7; clockNow=22; FormHideDirectTick(player);
        assert(writes==1 && memory[player+0xB440]==7); return 0;
    }
    if(scenario=="mode_disabled") g_fhMode=0;
    else if(scenario=="capture_cleared") g_fs0CapChr=0;
    else if(scenario=="capture_replaced") { g_fs0CapChr=0x40000; g_fs0CapT=1; clockNow=2; }
    else clockNow=22;
    if(scenario=="restore_retry") {
        failWrite=true; FormHideDirectTick(player); assert(writes==1);
        failWrite=false;
    }
    FormHideDirectTick(player);
    assert(writes==2 && memory[player+0xB440]==(scenario=="saved_one" ? 1 : 0));
    FormHideDirectTick(player);
    if(scenario=="capture_replaced") {
        assert(writes==3 && memory[player+0xB440]==1);
        clockNow=23; FormHideDirectTick(player);
        assert(writes==4 && memory[player+0xB440]==0);
    } else assert(writes==2);
}
'''
cases = ['guard_tail', 'native_unchanged', 'delay', 'write_failure', 'read_failure',
         'timer', 'saved_one', 'player_changed', 'external_change', 'mode_disabled',
         'capture_cleared', 'capture_replaced', 'restore_retry']
with tempfile.TemporaryDirectory(prefix='formhide3-test-') as tmp:
    cpp, exe = pathlib.Path(tmp)/'test.cpp', pathlib.Path(tmp)/'test'
    cpp.write_text(preamble + helper + main)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(exe)], check=True)
    for case in cases:
        subprocess.run([str(exe), case], check=True)
        print('PASS', case)
print(f'{len(cases)} direct-hide scenarios passed; MSVC build and in-game visuals remain untested.')
