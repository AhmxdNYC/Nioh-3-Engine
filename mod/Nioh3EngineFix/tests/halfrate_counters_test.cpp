// Exercises the production gates, telemetry and logger without installing hooks.
#include "../src/halfrate.cpp"
#include <cassert>
#include <string>
#include <vector>
std::vector<std::string> lines;
void Capture(const char* format, ...) {
    char b[1024]; va_list ap; va_start(ap,format);
    vsnprintf(b,sizeof b,format,ap); va_end(ap); lines.emplace_back(b);
}
int main() {
    using namespace halfrate;
    g_log=Capture; g_installed=true;
    for (int i=0;i<3;++i) assert(Gate(i,true)==0);
    OnGamePresent();
    for (int i=0;i<3;++i) {
        assert(g_counts[i].recomputedFrames==1 && g_counts[i].reusedFrames==0);
        SetEnabled(i,1);
        assert(Gate(i,true)==0);
    }
    OnGamePresent();
    for (int i=0;i<3;++i) assert(Gate(i,true)==1);
    OnGamePresent();
    // Multiple far/near shadow calls count as one mixed window, not three frames.
    assert(Gate(kShadow,true)==0); assert(Gate(kShadow,true)==1);
    assert(HrShadowGate(0,0)==0);
    OnGamePresent();
    assert(g_counts[0].reusedFrames==2 && g_counts[0].recomputedFrames==3);
    assert(g_counts[0].mixedFrames==1 && g_reuses[0]==2 && g_updates[0]==4);
    assert(g_counts[1].absentFrames==1);
    for (int i=0;i<3;++i) { SetEnabled(i,0); assert(Gate(i,true)==0); }
    OnGamePresent();
    LogLine(); assert(lines.size()==3);
    assert(lines[0].find("HRS enabled=0 frames=5 reused_frames=2 recomputed_frames=4 mixed_frames=1")!=std::string::npos);
    assert(lines[1].find("HRG enabled=0")!=std::string::npos);
    assert(lines[2].find("HRC enabled=0")!=std::string::npos);
    assert(lines[0].find("clock=game_present totals=1")!=std::string::npos);
    for (const auto& line:lines) puts(line.c_str());
    g_installed=false; LogLine(); assert(lines.size()==3);

    // Execute actual camera cave, including stolen prologue and native RCX read.
    auto* page=(uint8_t*)VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    assert(page);
    uint8_t* resume=page+2048;
    const uint8_t tail[]={0x8B,0x41,0x10,0x48,0x83,0xC4,0x78,0xC3};
    memcpy(resume,tail,sizeof tail);
    EmitCam(page,kCamBytes,sizeof kCamBytes,resume);
    float cam[16]={}; cam[4]=1.25f; cam[5]=2.f;cam[6]=3.f;
    FlushInstructionCache(GetCurrentProcess(),page,4096);
    using CameraFn=uint32_t(*)(void*);
    uint32_t want;memcpy(&want,cam+4,4);
    for(int i=0;i<1000;++i) assert(((CameraFn)page)(cam)==want);
    // Test real pass emitter with XMM0 live across helper on forward and skip exits.
    const uint8_t nop[]={0x90}; resume[0]=0xC3;
    using PassFn=float(*)(float);
    SetEnabled(kGi,0);Emit(page,false,kGi,nop,1,resume);
    FlushInstructionCache(GetCurrentProcess(),page,4096);
    for(int i=0;i<1000;++i) assert(((PassFn)page)(1.25f)==1.25f);
    SetEnabled(kGi,1);
    for(int i=0;i<1000;++i) assert(((PassFn)page)(1.25f)==1.25f);

    // Redirect only emitted test helpers to hostile machine code: clobber all
    // volatile GPRs and XMM0..5. Production emitter remains byte-for-byte exercised.
    uint8_t* evil=page+2500; uint8_t* e=evil;
    Put(e,{0x31,0xC9,0x31,0xD2,0x45,0x31,0xC0,0x45,0x31,0xC9,0x45,0x31,0xD2,0x45,0x31,0xDB});
    for(int i=0;i<6;++i) Put(e,{0x66,0x0F,0xEF,(uint8_t)(0xC0+i*9)});
    Put(e,{0xB8}); uint8_t* verdict=e;Put(e,{0,0,0,0,0xC3});
    auto redirect=[&](const void* old) {
        uintptr_t from=(uintptr_t)old,to=(uintptr_t)evil;bool found=false;
        for(int i=0;i<400-8;++i) if(!memcmp(page+i,&from,8)) {memcpy(page+i,&to,8);found=true;break;}
        assert(found);FlushInstructionCache(GetCurrentProcess(),page,4096);
    };
    memcpy(resume,tail,sizeof tail);EmitCam(page,kCamBytes,sizeof kCamBytes,resume);redirect((const void*)HrCamera);
    assert(((CameraFn)page)(cam)==want);
    resume[0]=0xC3;
    for(int skip=0;skip<=1;++skip) {
        *verdict=(uint8_t)skip;Emit(page,false,kGi,nop,1,resume);redirect((const void*)HrPassGate);
        assert(((PassFn)page)(3.75f)==3.75f);
    }
    puts("PASS: hostile helper destroys volatile GPR/SIMD state; camera and both pass exits preserve inputs");
    VirtualFree(page,0,MEM_RELEASE);
    puts("PASS: 3000 executable cave calls: camera RCX/stolen prologue, pass XMM0 OFF/forward/skip");
    puts("PASS: OFF counts, ON alternation, multi-call dedup, mixed/absent frames, disable, production log format");
}
