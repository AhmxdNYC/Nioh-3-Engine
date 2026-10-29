#include "../src/transition_native_read.h"
#include <map>
#include <cstdio>
#include <cstdlib>
namespace nr=transition_native_read;
struct Memory {
    std::map<uint64_t,uint8_t> bytes;
    size_t calls=0,fail=0;
    uint64_t watch=0,mutation=0,value=0; unsigned hits=0; size_t width=0;
    void fill(uint64_t p,size_t n) { for(size_t i=0;i<n;++i)bytes[p+i]=0; }
    template<class T> void put(uint64_t p,T v) { auto b=reinterpret_cast<const uint8_t*>(&v); for(size_t i=0;i<sizeof(v);++i)bytes[p+i]=b[i]; }
    static bool read(void* ctx,uint64_t p,void* out,size_t n) {
        auto& m=*static_cast<Memory*>(ctx);
        if(++m.calls==m.fail)return false;
        if(p==m.watch && ++m.hits==2) for(size_t i=0;i<m.width;++i)m.bytes[m.mutation+i]=uint8_t(m.value>>(8*i));
        for(size_t i=0;i<n;++i)if(!m.bytes.count(p+i))return false;
        for(size_t i=0;i<n;++i)static_cast<uint8_t*>(out)[i]=m.bytes[p+i];
        return true;
    }
};
constexpr uint64_t B=0x10000000,G=B+0x45BBD58,A=B+0x45B9E40;
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
Memory control() {
    Memory m; m.put(G,uint64_t(0x1000));m.fill(0x1000,0x38);
    m.put(0x1018,uint64_t(0x2000));m.put(0x1020,uint64_t(3));
    m.put(0x2000,uint16_t(0x01));m.put(0x2002,uint16_t(0x48));m.put(0x2004,uint16_t(0x90));return m;
}
Memory actor() {
    Memory m;m.put(A,uint64_t(0x3000));m.put(0x3000,uint64_t(0x4000));m.put(0x4048,uint64_t(0x5000));
    m.put(0x5000,uint64_t(0x6000));m.put(0x5008,uint64_t(3));
    for(unsigned i=0;i<3;++i){m.put(0x6000+i*16,uint32_t(10+i*10));m.put(0x6008+i*16,uint64_t(0x7000));}
    m.put(0x7000,uint64_t(0x8000));m.put(0x83B0,uint64_t(0x9000));m.put(0x9050,uint64_t(0xA000));m.put(0xB574,int32_t(1));return m;
}
Memory living() {
    auto m=actor();m.put(0x8000,uint32_t(0x123400));m.put(0x83A0,uint64_t(0xC000));m.put(0xC048,int32_t(0));
    m.put(0x9008,uint64_t(0xD000));m.put(0xD000,uint64_t(0xE000));m.put(0xE0C0,B+0x2E9934);
    m.put(0xD0E8,uint64_t(0xF000));m.put(0xF020,uint64_t(0x10000));m.put(0x10018,uint32_t(0));
    m.put(0xD118,uint64_t(0x11000));m.put(0x11020,uint64_t(0x12000));m.put(0x12018,uint32_t(0));
    m.put(0xD0A8,uint64_t(0));m.put(0x88D4,int32_t(0));return m;
}
int main() {
    auto c=control();auto cr=nr::ReadControl(B,Memory::read,&c);CHECK(cr.known&&cr.clear&&cr.manager==0x1000);
    const size_t cc=c.calls;
    for(size_t i=1;i<=cc;++i){auto m=control();m.fail=i;CHECK(!nr::ReadControl(B,Memory::read,&m).known);}
    for(uint64_t count:{uint64_t(0),uint64_t(257),UINT64_MAX}){auto m=control();m.put(0x1020,count);CHECK(!nr::ReadControl(B,Memory::read,&m).known);}
    for(uint16_t pair:{uint16_t(0x47),uint16_t(0x01),uint16_t(0x248)}){auto m=control();m.put(0x2002,pair);CHECK(!nr::ReadControl(B,Memory::read,&m).known);}
    c=control();c.put(0x2002,uint16_t(0x148));cr=nr::ReadControl(B,Memory::read,&c);CHECK(cr.known&&!cr.clear);
    c=control();c.put(0x1034,uint32_t(4));cr=nr::ReadControl(B,Memory::read,&c);CHECK(cr.known&&!cr.clear);
    c=control();c.put(0x1000,uint64_t(0x2500));c.put(0x1008,uint64_t(0x2510));c.fill(0x2500,16);c.put(0x2508,uint8_t(0x48));cr=nr::ReadControl(B,Memory::read,&c);CHECK(cr.known&&!cr.clear);
    for(uint64_t end:{uint64_t(0x24ff),uint64_t(0x2501),uint64_t(0x3510)}){auto m=c;m.put(0x1008,end);CHECK(!nr::ReadControl(B,Memory::read,&m).known);}
    for(uint64_t p:{G,uint64_t(0x1018),uint64_t(0x1020),uint64_t(0x1030),uint64_t(0x1034),uint64_t(0x2003)}){
        auto m=control();m.watch=G;m.mutation=p;m.value=1;m.width=1;CHECK(!nr::ReadControl(B,Memory::read,&m).known);
    }
    auto a=actor();auto ar=nr::ReadActor(B,20,Memory::read,&a);CHECK(ar.known&&ar.present&&ar.algoReady&&ar.live==0x8000);
    const size_t ac=a.calls;
    for(size_t i=1;i<=ac;++i){auto m=actor();m.fail=i;CHECK(!nr::ReadActor(B,20,Memory::read,&m).known);}
    for(uint32_t id:{0u,15u,40u}){auto m=actor();auto r=nr::ReadActor(B,id,Memory::read,&m);CHECK(!r.present&&!r.algoReady);CHECK(r.known==(id!=0));}
    for(int32_t active:{0,2,-1}){auto m=actor();m.put(0xB574,active);auto r=nr::ReadActor(B,20,Memory::read,&m);CHECK(r.known&&r.present&&!r.algoReady);}
    for(uint64_t p:{uint64_t(0x6018),uint64_t(0x7000),uint64_t(0x83B0),uint64_t(0x9050)}){
        auto m=actor();m.put(p,uint64_t(0));auto r=nr::ReadActor(B,20,Memory::read,&m);CHECK(r.known&&!r.algoReady);
    }
    for(uint64_t p:{A,uint64_t(0x3000),uint64_t(0x4048),uint64_t(0x5000),uint64_t(0x5008),uint64_t(0x6010),uint64_t(0x6018),uint64_t(0x7000),uint64_t(0x83B0),uint64_t(0x9050),uint64_t(0xB574)}){
        auto m=actor();m.watch=A;m.mutation=p;m.value=255;m.width=1;CHECK(!nr::ReadActor(B,20,Memory::read,&m).known);
    }
    a=actor();a.put(0x5008,uint64_t(65537));CHECK(!nr::ReadActor(B,20,Memory::read,&a).known);
    a=actor();a.put(0x5000,UINT64_MAX-8);CHECK(!nr::ReadActor(B,20,Memory::read,&a).known);
    CHECK(!nr::ReadActor(UINT64_MAX,20,Memory::read,&a).known);
    CHECK(!nr::ReadControl(UINT64_MAX,Memory::read,&c).known);
    auto l=living();auto expected=nr::ReadActor(B,20,Memory::read,&l);l.calls=0;
    auto life=nr::ReadLife(B,20,expected,Memory::read,&l);CHECK(life.known&&life.notDead);
    const size_t lc=l.calls;
    for(size_t i=1;i<=lc;++i){auto m=living();m.fail=i;CHECK(!nr::ReadLife(B,20,expected,Memory::read,&m).known);}
    for(uint64_t p:{uint64_t(0xC048),uint64_t(0x88D4)}){auto m=living();m.put(p,int32_t(1));auto r=nr::ReadLife(B,20,expected,Memory::read,&m);CHECK(r.known&&!r.notDead);}
    l=living();l.put(0xD0A8,uint64_t(1)<<46);life=nr::ReadLife(B,20,expected,Memory::read,&l);CHECK(life.known&&!life.notDead);
    // Decompiler case0x2C under a biased switch is NOT the actual condition2C.
    l=living();l.put(0xE068,uint32_t(0x100));life=nr::ReadLife(B,20,expected,Memory::read,&l);CHECK(life.known&&life.notDead);
    for(uint64_t p:{uint64_t(0x10018),uint64_t(0x12018)}){auto m=living();m.put(p,uint32_t(0x100));m.put(0xD0A8,uint64_t(1)<<46);m.put(0x88D4,int32_t(1));auto r=nr::ReadLife(B,20,expected,Memory::read,&m);CHECK(r.known&&r.notDead);}
    l=living();l.put(0xC048,int32_t(1));l.put(0x10018,uint32_t(0x100));life=nr::ReadLife(B,20,expected,Memory::read,&l);CHECK(life.known&&!life.notDead);
    l=living();l.put(0xD118,uint64_t(0));life=nr::ReadLife(B,20,expected,Memory::read,&l);CHECK(life.known&&life.notDead);
    l=living();l.put(0x88D4,int32_t(-1));life=nr::ReadLife(B,20,expected,Memory::read,&l);CHECK(life.known&&life.notDead);
    for(uint64_t p:{uint64_t(0x83A0),uint64_t(0x9008),uint64_t(0xD000),uint64_t(0xE0C0),uint64_t(0xD0E8),uint64_t(0xF020),uint64_t(0x11020)}){auto m=living();m.put(p,uint64_t(0));CHECK(!nr::ReadLife(B,20,expected,Memory::read,&m).known);}
    l=living();l.put(0x8000,uint32_t(0x930FE7));CHECK(!nr::ReadLife(B,20,expected,Memory::read,&l).known);
    l=living();l.put(0x8000,uint32_t(0x930FE7));l.put(0x1000C,uint16_t(0x1234));life=nr::ReadLife(B,20,expected,Memory::read,&l);CHECK(life.known&&life.notDead);
    l=living();l.put(0x8000,uint32_t(0x930FE7));l.put(0x1000C,uint16_t(0x4E52));CHECK(!nr::ReadLife(B,20,expected,Memory::read,&l).known);
    l.put(0x10018,uint32_t(0x100));life=nr::ReadLife(B,20,expected,Memory::read,&l);CHECK(life.known&&life.notDead);
    l.put(0xC048,int32_t(1));life=nr::ReadLife(B,20,expected,Memory::read,&l);CHECK(life.known&&!life.notDead);
    l=living();l.put(0x8000,uint32_t(0x930FE7));l.put(0x1000C,uint16_t(0x1234));l.watch=A;l.mutation=0x1000C;l.value=0x4E52;l.width=2;CHECK(!nr::ReadLife(B,20,expected,Memory::read,&l).known);
    for(uint64_t p:{uint64_t(0xC048),uint64_t(0xD0A8),uint64_t(0x10018),uint64_t(0x12018),uint64_t(0x88D4),uint64_t(0xE0C0),uint64_t(0x6018),uint64_t(0x7000)}){
        auto m=living();m.watch=A;m.mutation=p;m.value=255;m.width=1;CHECK(!nr::ReadLife(B,20,expected,Memory::read,&m).known);
    }
    l=living();auto wrong=expected;wrong.live++;CHECK(!nr::ReadLife(B,20,wrong,Memory::read,&l).known);
    l=living();CHECK(!nr::ReadLife(B,10,wrong,Memory::read,&l).known);
    l=living();CHECK(!nr::ReadLife(B,10,expected,Memory::read,&l).known);
    auto player=[](){Memory m;m.put(B+0x4745348,uint64_t(0x1000));m.put(0x1000+0xCD*0x18,uint64_t(0x5000));m.put(0x53A0,uint64_t(0x6000));m.put(0x6048,int32_t(0));return m;};
    auto pm=player();auto pr=nr::ReadPlayer(B,Memory::read,&pm);CHECK(pr.known&&pr.notDead&&pr.live==0x5000);
    for(size_t i=1;i<=8;++i){auto m=player();m.fail=i;CHECK(!nr::ReadPlayer(B,Memory::read,&m).known);}
    pm=player();pm.put(0x6048,int32_t(1));pr=nr::ReadPlayer(B,Memory::read,&pm);CHECK(pr.known&&!pr.notDead);
    for(uint64_t p:{B+0x4745348,uint64_t(0x1000+0xCD*0x18),uint64_t(0x53A0),uint64_t(0x6048)}){auto m=player();m.watch=B+0x4745348;m.mutation=p;m.value=255;m.width=1;CHECK(!nr::ReadPlayer(B,Memory::read,&m).known);}
    std::printf("PASS %u passive native reader checks\n",checks);
}
