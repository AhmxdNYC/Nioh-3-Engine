#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include "../src/vrs_selective.h"
#define CHECK(x) do { if(!(x)) {printf("FAIL %d: %s\n",__LINE__,#x); return 1;} } while(0)
int main() {
    FILE* file=nullptr; CHECK(fopen_s(&file,"tests/vrs_capture_5174.tsv","r")==0);
    char line[512]; unsigned long long total=0,tagged=0,eight=0; unsigned rows=0;
    while(fgets(line,sizeof line,file)) {
        if(line[0]=='#') continue;
        unsigned n,rt[8],old; unsigned long long calls;
        CHECK(sscanf_s(line,"%u %u %u %u %u %u %u %u %u %llu %x",&n,&rt[0],&rt[1],&rt[2],&rt[3],&rt[4],&rt[5],&rt[6],&rt[7],&calls,&old)==11);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d{}; d.NumRenderTargets=n; d.SampleDesc.Count=1;
        if(!(old&vrsselect::DepthOnly)) d.PS={(void*)1,1};
        for(unsigned i=0;i<8;++i) d.RTVFormats[i]=(DXGI_FORMAT)rt[i];
        d.BlendState.RenderTarget[0].BlendEnable=(old&vrsselect::Blend)!=0;
        if(old&vrsselect::Samples) d.SampleDesc.Count=4;
        // The capture lacks these PSO fields. Exercise all combinations rather
        // than invent a measured value: admission must be independent of them.
        bool admitted=false;
        for(unsigned depth=0;depth<2;++depth) for(unsigned cull=1;cull<=3;++cull)
        for(unsigned gs=0;gs<2;++gs) for(unsigned fmt=0;fmt<3;++fmt) {
            const DXGI_FORMAT depthFmt[]={DXGI_FORMAT_UNKNOWN,DXGI_FORMAT_D32_FLOAT,DXGI_FORMAT_D32_FLOAT_S8X24_UINT};
            d.DepthStencilState.DepthEnable=depth; d.RasterizerState.CullMode=(D3D12_CULL_MODE)cull;
            d.GS.BytecodeLength=gs; d.DSVFormat=depthFmt[fmt];
            auto m=vrsselect::Describe(d,false);
            if(old&vrsselect::Unknown) m.reject|=vrsselect::Unknown;
            bool pass=!vrsselect::RejectDraw(m,!(old&vrsselect::Binding),n,false);
            if(!depth && cull==1 && !gs && !fmt) admitted=pass;
            CHECK(admitted==pass);
        }
        total+=calls; if(admitted) {tagged+=calls; if(n==8) eight+=calls;}
        ++rows;
    }
    fclose(file);
    CHECK(rows==27 && total==13297664 && tagged==5406166 && eight==1274599);
    printf("CAPTURE REPLAY: %llu / %llu calls admitted (%llu%%); eight-target %llu. Policy replay of captured histogram, NOT measured live tagging or speed.\n",tagged,total,tagged*100/total,eight);
    return 0;
}
