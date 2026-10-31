#!/usr/bin/env python3
"""Compile the actual direct-hide helper against fake memory; never accesses a game."""
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
source = (ROOT / 'mod/Nioh3BossRematch/src/bosslog.cpp').read_text()
helper = source[source.index('// pc #55492: read-only end observation.'):source.index('void FkSlot0Tick(')]
# Native call/SEH are test seams; the controller bodies themselves are compiled unchanged.
helper = helper.replace('((void (*)(uint64_t, uint8_t))(g_base + 0xA550E8))(pchr, h);', 'NativeHide(pchr, h);')
preamble = r'''
#define __try try
#define __except(x) catch (...)
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
template<class T> void put(uint64_t a,T v) {
    const auto* b=reinterpret_cast<const uint8_t*>(&v);
    for(size_t i=0;i<sizeof(v);++i) memory[a+i]=b[i];
}
int writes=0, badByte=-1;
bool failWrite=false, failRead=false, badEndGuard=false;
constexpr uint64_t player=0x10000, other=0x20000;
double Now() { return clockNow; }
void Log(const char*, ...) {}
bool Copy(void* dst, uint64_t addr, size_t n) {
    const uint8_t hide[29]={0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x80,0xB9,0xD4,0x09,0,0,0,
        0x40,0x8A,0xFA,0x48,0x8B,0xD9,0x88,0x91,0x40,0xB4,0,0};
    const uint8_t end[]={0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x57,0x48,0x81,0xEC,0xC0,0,0,0,0x80,0x61,0x10,0xFD};
    const uint8_t begin[]={0x24,0xEF,0x33,0xD2,0x0C,2,0x88,0x41,0x10};
    const uint8_t removal[]={0x48,0x83,0xEC,0x28,0x48,0x8B,0xD1,0x48,0x8B,0x0D,0x52,0x5B,0x48,4,
        0x48,0x85,0xC9,0x74,0x0C,0x48,0x81,0xC1,0x98,0x0B,0,0,0xE8,0x11,0,0,0};
    if(addr==g_base+0xA550E8) {
        assert(n==sizeof(hide) || n==16); memcpy(dst,hide,n);
        if(badByte>=0) static_cast<uint8_t*>(dst)[badByte]^=1;
        return true;
    }
    if(badEndGuard) return false;
    if(addr==g_base+0x1AB4C5C) { assert(n==sizeof(end)); memcpy(dst,end,n); }
    else if(addr==g_base+0x843040) { assert(n==sizeof(begin)); memcpy(dst,begin,n); }
    else if(addr==g_base+0x2BF7E8) { assert(n==sizeof(removal)); memcpy(dst,removal,n); }
    else return false;
    return true;
}
template<class T> bool Rd(uint64_t addr,T& value) {
    if(failRead) return false;
    auto* b=reinterpret_cast<uint8_t*>(&value);
    for(size_t i=0;i<sizeof(value);++i) {
        if(!memory.count(addr+i)) return false;
        b[i]=memory.at(addr+i);
    }
    return true;
}
bool WrBytes(uint64_t addr,const void* src,size_t n) {
    assert(n==1 && memory.count(addr));
    if(failWrite) return false;
    memory[addr]=*static_cast<const uint8_t*>(src); ++writes; return true;
}
'''
preamble += r'''
void NativeHide(uint64_t chr,uint8_t h) {
    if(failWrite) throw 1;
    memory[chr+0xB440]=h; ++writes;
}
'''
main = r'''
int main(int argc,char** argv) {
    assert(argc==2); std::string scenario=argv[1];
    memory[player+0xB440]=0; memory[other+0xB440]=0;
    if(scenario.rfind("native_end",0)==0 || scenario=="native_timer") {
        g_fhMode=1; g_fhUntilEnd=scenario!="native_timer";
        put<uint64_t>(player+0x3A0,0x50000); put<uint8_t>(0x50188,2);
        FormHideTick(player); assert(writes==1);
        clockNow=22; FormHideTick(player);
        if(scenario=="native_timer") { assert(writes==2 && memory[player+0xB440]==0); return 0; }
        assert(writes==1); put<uint8_t>(0x50188,0); clockNow=23;
        if(scenario=="native_end_retry") { failWrite=true; FormHideTick(player); assert(writes==1); failWrite=false; }
        FormHideTick(player); assert(writes==2 && memory[player+0xB440]==0);
        return 0;
    }
    if(scenario.rfind("end_",0)==0) {
        g_fhUntilEnd=1;
        constexpr uint64_t ext=0x50000,mgr=0x60000,buckets=0x70000;
        put(player+0x3A0,ext); put<uint8_t>(ext+0x188,0);
        put<int32_t>(g_base+0x2BF7F2,0x4485B52);
        put(g_base+0x4745348,mgr); put(mgr+0xBA0,buckets);
        for(int i=0;i<16;++i) { put<uint64_t>(buckets+i*16,0); put<uint64_t>(buckets+i*16+8,0); }
        auto sealLive=[&](bool live) {
            put<uint64_t>(buckets,live ? g_fs0CapChr.load()+0xE0 : 0);
            put<uint64_t>(buckets+8,live ? 1 : 0);
            put<uint64_t>(g_fs0CapChr.load()+0xE0,0);
        };
        const bool la=scenario=="end_la" || scenario=="end_la_precedence" || scenario=="end_la_unknown";
        sealLive(true);
        if(la) put<uint8_t>(ext+0x188,2);
        if(scenario=="end_no_signal") sealLive(false);
        if(scenario=="end_bad_guard") badEndGuard=true;
        FormHideDirectTick(player); assert(writes==1);
        clockNow=22; FormHideDirectTick(player);
        if(scenario=="end_no_signal" || scenario=="end_bad_guard") {
            assert(writes==2 && memory[player+0xB440]==0);
            assert(std::string(g_fhEnd.ended)=="unobserved-end-fallback-20s"); return 0;
        }
        assert(writes==1 && FormHideEndHolding(player,g_fs0CapChr));
        clockNow=65; FormHideDirectTick(player); assert(writes==1); // no hidden 20/60 s cap
        if(scenario=="end_la_precedence") {
            sealLive(false); clockNow=66; FormHideDirectTick(player); assert(writes==1);
        }
        if(scenario=="end_la_unknown") memory.erase(player+0x3A0);
        else if(la) put<uint8_t>(ext+0x188,0);
        else if(scenario=="end_table_unknown") memory.erase(buckets+16);
        else if(scenario=="end_table_cycle") { put<uint64_t>(buckets,0x80000); put<uint64_t>(0x80000,0x80000); }
        else if(scenario=="end_table_short") { put<uint64_t>(buckets,0); put<uint64_t>(buckets+8,1); }
        else sealLive(false);
        if(scenario=="end_table_unknown") sealLive(false);
        clockNow=67; FormHideDirectTick(player);
        assert(writes==2 && memory[player+0xB440]==0);
        assert(!FormHideEndHolding(player,g_fs0CapChr));
        const char* why=scenario=="end_la_unknown" ? "LA-state-unreadable" : la ? "LA-ended" :
            (scenario=="end_table_unknown" || scenario=="end_table_cycle" || scenario=="end_table_short") ? "seal-table-unreadable" : "seal-despawn";
        assert(std::string(g_fhEnd.ended)==why); return 0;
    }
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
         'capture_cleared', 'capture_replaced', 'restore_retry',
         'end_la', 'end_la_precedence', 'end_la_unknown', 'end_seal', 'end_no_signal',
         'end_bad_guard', 'end_table_unknown', 'end_table_cycle', 'end_table_short',
         'native_timer', 'native_end', 'native_end_retry']
with tempfile.TemporaryDirectory(prefix='formhide3-test-') as tmp:
    cpp, exe = pathlib.Path(tmp)/'test.cpp', pathlib.Path(tmp)/'test'
    cpp.write_text(preamble + helper + main)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(exe)], check=True)
    for case in cases:
        subprocess.run([str(exe), case], check=True)
        print('PASS', case)
print(f'{len(cases)} hide/restore scenarios passed; MSVC build and in-game visuals remain untested.')
