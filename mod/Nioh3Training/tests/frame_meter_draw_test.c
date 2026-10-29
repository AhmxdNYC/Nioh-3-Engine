// The production GDI renderer in a memory DIB: no process, hooks, windows or game access.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/practice_stats.h"
#include "../src/frame_meter.h"
#include "../src/frame_meter_ki.h"
#include "../src/frame_meter_position.h"
static PstCfg g_pst;
static FmMeter g_fm,g_fmp;
static FmKi g_fm_ki;
static int g_fm_show=1,g_fmp_show=1,g_fm_ki_show=1;
static SRWLOCK g_fm_lock=SRWLOCK_INIT;
static const char* g_fm_ki_label="KI DEBT";
static char g_fm_who[48]="Boss A";
#include "../src/frame_meter_draw.h"
static int assertions;
#define CHECK(x) do { ++assertions; if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);} } while(0)
typedef struct {HDC dc;HBITMAP bitmap;HGDIOBJ old;uint32_t* bits;int w,h;} Canvas;
static Canvas Make(int w,int h) {
    Canvas c={0};c.w=w;c.h=h;c.dc=CreateCompatibleDC(0);
    BITMAPINFO b={0};b.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);b.bmiHeader.biWidth=w;b.bmiHeader.biHeight=-h;
    b.bmiHeader.biPlanes=1;b.bmiHeader.biBitCount=32;b.bmiHeader.biCompression=BI_RGB;
    c.bitmap=CreateDIBSection(c.dc,&b,DIB_RGB_COLORS,(void**)&c.bits,0,0);CHECK(c.bitmap&&c.bits);
    c.old=SelectObject(c.dc,c.bitmap);return c;
}
static void Drop(Canvas* c){SelectObject(c->dc,c->old);DeleteObject(c->bitmap);DeleteDC(c->dc);}
static void Draw(Canvas* c,int H) {
    n3_stroke_layered=1;FmPaint(c->dc,c->w,c->h,H);n3_stroke_layered=0;GdiFlush();FmFinalizeAlpha(c->bits,c->w,c->h);
}
static void Save(Canvas* c,const char* name) {
    BITMAPFILEHEADER f={0};BITMAPINFOHEADER b={0};
    f.bfType=0x4d42;f.bfOffBits=sizeof f+sizeof b;f.bfSize=f.bfOffBits+c->w*c->h*4;
    b.biSize=sizeof b;b.biWidth=c->w;b.biHeight=-c->h;b.biPlanes=1;b.biBitCount=32;
    FILE* out=fopen(name,"wb");CHECK(out);fwrite(&f,sizeof f,1,out);fwrite(&b,sizeof b,1,out);fwrite(c->bits,4,(size_t)c->w*c->h,out);fclose(out);
}
static void Seed(void) {
    PstDefaults(&g_pst);g_pst.v[PS_FRAME_METER]=g_pst.v[PS_PLAYER_METER]=g_pst.v[PS_KI_WINDOW]=1;
    FmReset(&g_fm);FmReset(&g_fmp);FmKiReset(&g_fm_ki);
    FmMeter* mm[]={&g_fmp,&g_fm};
    for(int j=0;j<2;++j){FmMeter* m=mm[j];m->haveLast=1;m->frozenN=m->kiwinFrozenN=m->kipulseFrozenN=m->nodmgFrozenN=120;
        m->cur.total=120;m->cur.hits=1;m->cur.startup=20;m->cur.recovery=60;m->cur.done=1;m->cur.ws[0]=20;m->cur.we[0]=60;
        for(int i=0;i<120;++i)m->frozen[i]=(uint8_t)(i<20?FM_STARTUP:i<40?FM_ACTIVE:i<60?FM_ACTIVE_UB:FM_RECOVERY);
        m->frozen[0]|=FM_EDGE;
    }
    for(int i=20;i<60;++i)g_fmp.nodmgFrozen[i]=(uint8_t)(i<30?FMF_NODMG:i<40?FMF_EAA:i<50?FMF_GUARD:FMF_EA7);
    for(int i=80;i<100;++i)g_fmp.kiwinFrozen[i]=(uint8_t)(i<90?1:2);
    g_fmp.kipulseFrozen[95]=1;g_fm_ki.frozenN=120;
    for(int i=45;i<75;++i)g_fm_ki.frozen[i]=1;
}
static int HasColor(uint32_t p,COLORREF c) {
    const unsigned alpha=p>>24;
    if(alpha<=FM_BG_ALPHA)return 0;
    const double coverage=(alpha-FM_BG_ALPHA)/(255.-FM_BG_ALPHA);
    const unsigned bg[]={14,9,7},color[]={GetBValue(c),GetGValue(c),GetRValue(c)};
    for(int i=0;i<3;++i) {
        const double expected=color[i]*coverage+bg[i]*(1.-coverage);
        if(fabs((double)((p>>(i*8))&255)-expected)>2.5)return 0;
    }
    return 1;
}
// Near-white cores of the I-FR run digits, inset so the hole labels and the outer track do not count.
static int InnerWhite(Canvas* c,int H) {
    int cw,bh,lh,pad;FmGeom(c->w,H,&cw,&bh,&lh,&pad);FmRingLayout g=FmRingGeom(c->w,H);
    const int both=(FmPlayerDraw()||FmIFrRow()) && g_pst.v[PS_FRAME_METER]&&g_fm_show;
    const int cx=c->w/2+(both?(g.d+lh)/2:0)*-1,cy=FmReadoutHeight(bh,pad)+g.height/2;
    const float ri=g.d*.5f-(g.thick+g.gap)-g.thick,ro=ri+g.thick;
    int n=0,half=g.d/2+2;
    for(int y=cy-half;y<=cy+half;++y)for(int x=cx-half;x<=cx+half;++x){
        if(x<0||y<0||x>=c->w||y>=c->h)continue;
        const float dx=x+.5f-cx,dy=y+.5f-cy,r=sqrtf(dx*dx+dy*dy);
        if(r<ri+1.f || r>ro-1.f)continue;
        const uint32_t p=c->bits[y*c->w+x];
        if((p>>24)>180 && ((p>>16)&255)>230 && ((p>>8)&255)>230 && (p&255)>230)++n;
    }
    return n;
}
static uint32_t AtLane(Canvas* c,int H,int player,int lane,int cell) {
    int cw,bh,lh,pad;FmGeom(c->w,H,&cw,&bh,&lh,&pad);FmRingLayout g=FmRingGeom(c->w,H);
    const int both=(FmPlayerDraw()||FmIFrRow()) && g_pst.v[PS_FRAME_METER]&&g_fm_show;
    int cx=c->w/2+(both?(player?-1:1)*(g.d+lh)/2:0),cy=FmReadoutHeight(bh,pad)+g.height/2;
    const float radius=g.d*.5f-lane*(g.thick+g.gap)-g.thick*.5f,angle=(cell+.5f)*6.28318530718f/FM_DISPLAY_CELLS;
    POINT p={(LONG)floorf(radius*sinf(angle)),(LONG)floorf(-radius*cosf(angle))};
    CHECK(cx+p.x>=0&&cx+p.x<c->w&&cy+p.y>=0&&cy+p.y<c->h);
    return c->bits[(cy+p.y)*c->w+cx+p.x];
}
static void Pixels(int w,int H,int size) {
    Seed();g_pst.v[PS_FRAME_STYLE]=1;g_pst.v[PS_FRAME_SIZE]=size;Canvas c=Make(w,FmDrawHeight(w,H));Draw(&c,H);
    CHECK(g_fm_ringMesh.pixels && g_fm_ringMesh.pixelN>0);
    int feathered[2]={0};
    for(int i=0;i<g_fm_ringMesh.pixelN;++i){const FmRingPixel p=g_fm_ringMesh.pixels[i];
        CHECK(p.lane<2 && p.cell<FM_DISPLAY_CELLS && p.edgeCell<FM_DISPLAY_CELLS);
        if(p.fill>0 && p.fill<255)++feathered[p.lane];
    }
    CHECK(feathered[0]>30 && feathered[1]>30); // both circular and radial boundaries have real coverage pixels
    FmRingPixel* cached=g_fm_ringMesh.pixels;FmRingBuild(FmRingGeom(w,H));CHECK(cached==g_fm_ringMesh.pixels);
    CHECK(HasColor(AtLane(&c,H,1,0,8),FmColor(FM_STARTUP)));
    CHECK(HasColor(AtLane(&c,H,1,0,85),FM_KIWIN_RGB));
    CHECK(HasColor(AtLane(&c,H,1,0,92),FM_KIPERF_RGB));
    CHECK(HasColor(AtLane(&c,H,0,0,85),FmColor(FM_RECOVERY)));
    CHECK(HasColor(AtLane(&c,H,1,1,20),FM_NODMG_RGB));
    CHECK(HasColor(AtLane(&c,H,1,1,30),FM_EAA_RGB));
    CHECK(HasColor(AtLane(&c,H,1,1,40),FM_GUARD_RGB));
    CHECK(HasColor(AtLane(&c,H,1,1,50),FM_EA7_RGB));
    CHECK(HasColor(AtLane(&c,H,0,1,53),RGB(138,92,255)));
    if(w==1920 && size==1) {
        CHECK(InnerWhite(&c,H)>15); // four "10" run lengths on the inner I-FR ring
        g_pst.v[PS_FRAME_IFR]=0;Draw(&c,H);CHECK(InnerWhite(&c,H)==0);
        g_pst.v[PS_FRAME_IFR]=1;Draw(&c,H);
    }
    if(w==1920) {
        const size_t bytes=(size_t)c.w*c.h*4;
        uint32_t* before=(uint32_t*)malloc(bytes);CHECK(before);memcpy(before,c.bits,bytes);
        g_fmp.kipulseFrozen[95]=0;Draw(&c,H);int changed=0;
        for(int y=0;y<c.h;++y)for(int x=0;x<c.w;++x)if(before[y*c.w+x]!=c.bits[y*c.w+x]) {
            ++changed;CHECK(x<c.w/2); // a perfect press changes PLAYER only, never the boss or legend
        }
        CHECK(changed>0);free(before);g_fmp.kipulseFrozen[95]=1;
    }
    g_pst.v[PS_KI_WINDOW]=0;Draw(&c,H);
    CHECK(HasColor(AtLane(&c,H,1,0,85),FmColor(FM_RECOVERY)));
    CHECK(HasColor(AtLane(&c,H,1,0,92),FM_KIPERF_RGB));
    // Independent visibility: disabled tracks have the shared panel alpha, not a dark ghost ring.
    for(int mask=0;mask<8;++mask) {
        g_pst.v[PS_FRAME_PROW]=!!(mask&1);g_pst.v[PS_FRAME_IFR]=!!(mask&2);g_pst.v[PS_FRAME_KI]=!!(mask&4);
        Draw(&c,H);
        if(mask&3) {
            CHECK((mask&1)?(AtLane(&c,H,1,0,8)>>24)>FM_BG_ALPHA:(AtLane(&c,H,1,0,8)>>24)==FM_BG_ALPHA);
            CHECK((mask&2)?(AtLane(&c,H,1,1,23)>>24)>FM_BG_ALPHA:(AtLane(&c,H,1,1,23)>>24)==FM_BG_ALPHA);
        }
        CHECK((mask&4)?(AtLane(&c,H,0,1,53)>>24)>FM_BG_ALPHA:(AtLane(&c,H,0,1,53)>>24)==FM_BG_ALPHA);
    }
    Seed();g_pst.v[PS_FRAME_STYLE]=1;g_pst.v[PS_FRAME_SIZE]=size;g_pst.v[PS_FRAME_METER]=0;Draw(&c,H);
    CHECK(HasColor(AtLane(&c,H,1,0,8),FmColor(FM_STARTUP))); // lone player centres
    CHECK(!FmKiShown());
    for(size_t i=0;i<(size_t)c.w*c.h;++i) {uint32_t p=c.bits[i],a=p>>24;
        CHECK(((p>>16)&255)<=a && ((p>>8)&255)<=a && (p&255)<=a); // premultiplied contract, no magenta leakage
    }
    const int y=FmPositionY(H,c.h,-1);CHECK(y>=0 && y+c.h<=H);
    Drop(&c);
}
static void CompactPanel(void) {
    Seed();g_pst.v[PS_FRAME_STYLE]=1;
    Canvas c=Make(1920,FmDrawHeight(1920,1080));
    Draw(&c,1080);
    int left=c.w,right=-1,tl=c.w,tr=-1;
    for(int y=0;y<c.h;++y)for(int x=0;x<c.w;++x) {
        const unsigned a=c.bits[y*c.w+x]>>24;
        if(a){if(x<left)left=x;if(x>right)right=x;}
        if(y<20 && a>FM_BG_ALPHA){if(x<tl)tl=x;if(x>tr)tr=x;}
    }
    CHECK(right-left+1<1100); // no inherited 120-cell-wide row background
    CHECK(abs(left+right-(c.w-1))<=1);
    CHECK(tr>=tl && abs(tl+tr-(c.w-1))<12); // header centered on the rings
    g_pst.v[PS_FRAME_READOUT]=0;g_pst.v[PS_FRAME_LEGEND]=0;Draw(&c,1080);
    int both=FmRingContentWidth(1920,1080);
    g_pst.v[PS_FRAME_METER]=0;Draw(&c,1080);
    int one=FmRingContentWidth(1920,1080);CHECK(one<both);
    CHECK((c.bits[(c.h/2)*c.w+(c.w-one)/2-2]>>24)==0);
    printf("COMPACT: default panel %d px; no-header/no-legend two actors %d px, one actor %d px\n",right-left+1,both,one);
    Drop(&c);
}
static double Now(void){LARGE_INTEGER q,f;QueryPerformanceCounter(&q);QueryPerformanceFrequency(&f);return 1000.*q.QuadPart/f.QuadPart;}
static void CacheEquivalence(void) {
    Canvas a=Make(240,60),b=Make(240,60);Seed();g_fm_fontPx=18;
    const COLORREF bg[]={FM_KEY,RGB(16,21,30),FM_KIWIN_RGB};
    for(int fill=0;fill<3;++fill)for(int clipped=0;clipped<2;++clipped)for(int hit=0;hit<2;++hit) {
        Canvas* c[]={&a,&b};
        for(int i=0;i<2;++i){g_pst.v[PS_FRAME_STYLE]=i;FmFill(c[i]->dc,0,0,240,60,bg[fill]);
            int saved=SaveDC(c[i]->dc);if(clipped)IntersectClipRect(c[i]->dc,23,9,118,23);
            n3_stroke_layered=1;FmLabel(c[i]->dc,11,7,"PLAYER 120+",RGB(214,222,234));n3_stroke_layered=0;
            RestoreDC(c[i]->dc,saved);GdiFlush();FmFinalizeAlpha(c[i]->bits,240,60);
        }
        CHECK(!memcmp(a.bits,b.bits,240*60*4));
    }
    CHECK(FmPositionX(1920)==0);Drop(&a);Drop(&b);
}
int main(int argc,char** argv) {
    CHECK(FmRingBlend(0x00ff00ff,0xff0000,128)==0x80800000); // no magenta fringe over transparency
    CHECK(FmRingBlend(0xff000000,0x00ff00,128)==0xff008000);
    CHECK(FmRingBlend(0x0010151e,0xff0000,128)==0xb9830407); // premultiplied shared-panel edge
    CacheEquivalence();
    CompactPanel();
    Seed();g_pst.v[PS_FRAME_STYLE]=1;
    int previous=0;
    for(int size=0;size<3;++size){g_pst.v[PS_FRAME_SIZE]=size;FmRingLayout g=FmRingGeom(1920,1080);
        CHECK(g.d>previous);previous=g.d;CHECK(g.thick>0 && g.gap>0);}
    g_pst.v[PS_FRAME_STYLE]=0;g_pst.v[PS_FRAME_SIZE]=0;int rowHeight=FmDrawHeight(1920,1080);
    g_pst.v[PS_FRAME_SIZE]=2;CHECK(FmDrawHeight(1920,1080)==rowHeight);
    // Side legends must remain inside the panel and outside both rings at each aspect/size.
    Seed();g_pst.v[PS_FRAME_STYLE]=1;
    for(int H=600;H<=2160;H+=120)for(int aspect=0;aspect<2;++aspect)for(int size=0;size<3;++size){
        const int W=H*16/(aspect?10:9);g_pst.v[PS_FRAME_SIZE]=size;
        int cw,bh,lh,pad;FmGeom(W,H,&cw,&bh,&lh,&pad);
        FmRingLayout g=FmRingGeom(W,H);
        CHECK(2*(g.d+lh/2+g.legendW+g.legendGap)<=cw*FM_DISPLAY_CELLS+lh*3+pad*2);
        CHECK(g.height>=9*g.legendPitch+pad*4);
        CHECK(g.legendGap>=8);CHECK(FmLegendHeight(W,H)==0);
        for(int i=0;i<18;++i)CHECK(12+(int)strlen(FmLegendNames()[i])*N3StrokeWidth(g.legendPx)<=g.legendW);
    }
    const int sizes[][2]={{800,600},{1280,720},{1920,1080},{2560,1440},{3840,2160}};
    for(int i=0;i<5;++i)for(int size=0;size<3;++size)Pixels(sizes[i][0],sizes[i][1],size);
    for(int size=0;size<3;++size){Pixels(1280,800,size);Pixels(1920,1200,size);Pixels(2560,1600,size);}
    Seed();const int n=FmLegendCount();CHECK(n==18);
    for(int k=PS_KEY_STARTUP;k<=PS_KEY_IFR;++k){g_pst.v[k]=0;CHECK(FmLegendCount()<n);g_pst.v[k]=1;}
    for(int style=0;style<2;++style){g_pst.v[PS_FRAME_STYLE]=style;
        g_pst.v[PS_FRAME_LEGEND]=0;CHECK(FmLegendHeight(1920,1080)==0);g_pst.v[PS_FRAME_LEGEND]=1;
        g_pst.v[PS_FRAME_READOUT]=0;CHECK(FmReadoutHeight(22,2)==0);g_pst.v[PS_FRAME_READOUT]=1;
    }
    // Visible tail must be identical after 120+ cells, frozen or live.
    g_fmp.frozenN=360;for(int i=0;i<360;++i)g_fmp.frozen[i]=FM_RECOVERY;CHECK(FmVisibleStart(&g_fmp)==240);
    FmReset(&g_fmp);CHECK(FmVisibleStart(&g_fmp)==0);
    for(int i=0;i<400;++i)FmSampleEx(&g_fmp,i*1000./60.,100,1,0,0);
    CHECK(FmVisibleStart(&g_fmp)==240);
    Seed();
    if(argc>1 && !strcmp(argv[1],"--preview-only")) {
        for(int style=0;style<2;++style){g_pst.v[PS_FRAME_STYLE]=style;Canvas c=Make(1920,FmDrawHeight(1920,1080));
            Draw(&c,1080);Save(&c,style?"ring-preview.bmp":"row-preview.bmp");Drop(&c);}
        for(int size=0;size<3;++size){g_pst.v[PS_FRAME_STYLE]=1;g_pst.v[PS_FRAME_SIZE]=size;
            Canvas c=Make(1920,FmDrawHeight(1920,1080));char file[48];snprintf(file,sizeof file,"ring-size-%d.bmp",size);
            Draw(&c,1080);Save(&c,file);Drop(&c);}
        printf("ALL PASSED: %d production renderer assertions; previews saved\n",assertions);return 0;
    }
    if(argc>1 && !strcmp(argv[1],"--check-only")) {
        printf("ALL PASSED: %d production renderer assertions (offline)\n",assertions);return 0;
    }
    // OFF=Row / ON=Ring, CPU paint+GDI flush+alpha only; no game/GPU/presentation claim.
    for(int dynamic=0;dynamic<2;++dynamic)for(int size=0;size<3;++size){const int w=sizes[size+1][0],H=sizes[size+1][1];
        Seed();
        for(int i=0;i<64;++i){free(g_fm_textTiles[i].bits);memset(&g_fm_textTiles[i],0,sizeof g_fm_textTiles[i]);}
        Canvas c[2];double sum[2]={0},max[2]={0};
        for(int s=0;s<2;++s){g_pst.v[PS_FRAME_STYLE]=s;c[s]=Make(w,FmDrawHeight(w,H));Draw(&c[s],H);}
        for(int i=0;i<30;++i)for(int arm=0;arm<2;++arm){const int s=(i&1)?1-arm:arm;g_pst.v[PS_FRAME_STYLE]=s;
            if(dynamic){g_fmp.cur.total=g_fm.cur.total=120+i;g_fmp.cur.recovery=g_fm.cur.recovery=60+i;
                for(int j=0;j<120;++j)g_fmp.frozen[j]=(uint8_t)(j<20+i?FM_STARTUP:j<60?FM_ACTIVE:FM_RECOVERY);
                g_fmp.frozen[0]|=FM_EDGE;g_fm.frozenN=120;
            }
            double t=Now();Draw(&c[s],H);t=Now()-t;if(i>=5){sum[s]+=t;if(t>max[s])max[s]=t;}}
        printf("DRAW %s %dx%d all 4 lanes+legend+readout, 120 cells, 25 samples/arm: OFF Row mean %.3f max %.3f ms; ON Ring mean %.3f max %.3f ms; ratio %.3f\n",dynamic?"changing":"frozen",w,H,sum[0]/25,max[0],sum[1]/25,max[1],sum[1]/sum[0]);
        if(w==1920 && !dynamic){Save(&c[0],"row-preview.bmp");Save(&c[1],"ring-preview.bmp");}
        Drop(&c[0]);Drop(&c[1]);
    }
    printf("ALL PASSED: %d production renderer assertions (offline)\n",assertions);return 0;
}
