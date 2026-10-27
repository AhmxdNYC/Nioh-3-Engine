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
static uint32_t DibColor(COLORREF c){return ((uint32_t)GetRValue(c)<<16)|((uint32_t)GetGValue(c)<<8)|GetBValue(c);}
static uint32_t AtLane(Canvas* c,int H,int player,int lane,int cell) {
    int cw,bh,lh,pad;FmGeom(c->w,H,&cw,&bh,&lh,&pad);FmRingLayout g=FmRingGeom(c->w,H);
    const int both=(FmPlayerDraw()||FmIFrRow()) && g_pst.v[PS_FRAME_METER]&&g_fm_show;
    int cx=c->w/2+(both?(player?-1:1)*(g.d+lh)/2:0),cy=FmReadoutHeight(bh,pad)+g.height/2;
    POINT p=FmPolar(g.d*.5f-lane*(g.thick+g.gap)-g.thick*.5f,(cell+.5f)*6.28318530718f/FM_DISPLAY_CELLS);
    CHECK(cx+p.x>=0&&cx+p.x<c->w&&cy+p.y>=0&&cy+p.y<c->h);
    return c->bits[(cy+p.y)*c->w+cx+p.x];
}
static void Pixels(int w,int H) {
    Seed();g_pst.v[PS_FRAME_STYLE]=1;Canvas c=Make(w,FmDrawHeight(w,H));Draw(&c,H);
    CHECK((AtLane(&c,H,1,0,8)&0xffffff)==DibColor(FmColor(FM_STARTUP)));
    CHECK((AtLane(&c,H,1,0,85)&0xffffff)==DibColor(FM_KIWIN_RGB));
    CHECK((AtLane(&c,H,1,0,92)&0xffffff)==DibColor(FM_KIPERF_RGB));
    CHECK((AtLane(&c,H,0,0,85)&0xffffff)==DibColor(FmColor(FM_RECOVERY)));
    CHECK((AtLane(&c,H,1,1,23)&0xffffff)==DibColor(FM_NODMG_RGB));
    CHECK((AtLane(&c,H,1,1,33)&0xffffff)==DibColor(FM_EAA_RGB));
    CHECK((AtLane(&c,H,1,1,43)&0xffffff)==DibColor(FM_GUARD_RGB));
    CHECK((AtLane(&c,H,1,1,53)&0xffffff)==DibColor(FM_EA7_RGB));
    CHECK((AtLane(&c,H,0,1,53)&0xffffff)==DibColor(RGB(138,92,255)));
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
    CHECK((AtLane(&c,H,1,0,85)&0xffffff)==DibColor(FmColor(FM_RECOVERY)));
    CHECK((AtLane(&c,H,1,0,92)&0xffffff)==DibColor(FM_KIPERF_RGB));
    // Independent visibility: disabled tracks have the shared panel alpha, not a dark ghost ring.
    for(int mask=0;mask<8;++mask) {
        g_pst.v[PS_FRAME_PROW]=!!(mask&1);g_pst.v[PS_FRAME_IFR]=!!(mask&2);g_pst.v[PS_FRAME_KI]=!!(mask&4);
        Draw(&c,H);
        if(mask&3) {
            CHECK((AtLane(&c,H,1,0,8)>>24)==((mask&1)?255:FM_BG_ALPHA));
            CHECK((AtLane(&c,H,1,1,23)>>24)==((mask&2)?255:FM_BG_ALPHA));
        }
        CHECK((AtLane(&c,H,0,1,53)>>24)==((mask&4)?255:FM_BG_ALPHA));
    }
    Seed();g_pst.v[PS_FRAME_STYLE]=1;g_pst.v[PS_FRAME_METER]=0;Draw(&c,H);
    CHECK((AtLane(&c,H,1,0,8)&0xffffff)==DibColor(FmColor(FM_STARTUP))); // lone player centres
    CHECK(!FmKiShown());
    for(size_t i=0;i<(size_t)c.w*c.h;++i) {uint32_t p=c.bits[i],a=p>>24;
        CHECK(((p>>16)&255)<=a && ((p>>8)&255)<=a && (p&255)<=a); // premultiplied contract, no magenta leakage
    }
    const int y=FmPositionY(H,c.h,-1);CHECK(y>=0 && y+c.h<=H);
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
    CacheEquivalence();
    const int sizes[][2]={{800,600},{1280,720},{1920,1080},{2560,1440},{3840,2160}};
    for(int i=0;i<5;++i)Pixels(sizes[i][0],sizes[i][1]);
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
