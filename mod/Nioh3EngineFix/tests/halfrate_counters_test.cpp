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
    puts("PASS: OFF counts, ON alternation, multi-call dedup, mixed/absent frames, disable, production log format");
}
