// Two independent actor histories, each with its own optional inner lane.
// Uses the Row renderer's samples, colours, predicates, checks and legend.
#pragma once
#include <math.h>
typedef struct { int d, thick, gap, height, legendPx, legendW, legendPitch, legendGap; } FmRingLayout;
static FmRingLayout FmRingGeom(int w, int H) {
    int cw,bh,lh,pad; FmGeom(w,H,&cw,&bh,&lh,&pad);
    const int panelW=cw*FM_DISPLAY_CELLS+lh*3+pad*2;
    FmRingLayout g;
    const int size=PstClamp(PS_FRAME_SIZE,g_pst.v[PS_FRAME_SIZE]);
    const int diameter[]={160,210,260},minimum[]={128,154,180};
    g.d=H*diameter[size]/1080; if(g.d<minimum[size])g.d=minimum[size]; if(g.d>512)g.d=512;
    g.legendPx=H*12/1080; if(g.legendPx<9)g.legendPx=9;
    g.legendPitch=g.legendPx+H*10/1080; if(g.legendPitch<15)g.legendPitch=15;
    g.legendGap=H*10/1080; if(g.legendGap<8)g.legendGap=8;
    g.legendW=g_pst.v[PS_FRAME_LEGEND] && FmLegendCount()>0 ? 12+12*N3StrokeWidth(g.legendPx) : 0;
    const int reserve=g.legendW ? 2*(g.legendW+g.legendGap) : 0;
    if(g.d>(panelW-lh*2-reserve)/2)g.d=(panelW-lh*2-reserve)/2;
    if(g.d>(panelW-lh*2)/2)g.d=(panelW-lh*2)/2;
    if(g.d<64)g.d=64;
    g.thick=bh; if(g.thick>g.d/9)g.thick=g.d/9;
    g.gap=pad*2+2;
    g.height=g.d+pad*4;
    if(g.legendW && g.height<9*g.legendPitch+pad*4)g.height=9*g.legendPitch+pad*4;
    return g;
}
static int FmRingContentWidth(int w,int H) {
    int cw,bh,lh,pad; FmGeom(w,H,&cw,&bh,&lh,&pad);
    const FmRingLayout g=FmRingGeom(w,H);
    const int actors=(FmPlayerDraw()||FmIFrRow())+(g_pst.v[PS_FRAME_METER]&&g_fm_show);
    return actors*g.d+(actors>1?lh:0)+(g.legendW?2*(g.legendW+g.legendGap):0)+pad*4;
}
static int FmDrawHeight(int w,int H) {
    int cw,bh,lh,pad; const int rowH=FmGeom(w,H,&cw,&bh,&lh,&pad);
    return (g_pst.v[PS_FRAME_STYLE] ? FmRingGeom(w,H).height : rowH*FmPanels())
        +FmReadoutHeight(bh,pad)+FmLegendHeight(w,H);
}
static int FmVisibleStart(const FmMeter* m) {
    int used=FM_CELLS;
    while(used>0 && (FmViewCell(m,used-1)&0x7F)==FM_EMPTY)--used;
    return used>FM_DISPLAY_CELLS?used-FM_DISPLAY_CELLS:0;
}
// Trigonometry only when resolution changes, not per cell per redraw.
typedef struct { short x,y; unsigned char lane,cell,fill,stripe,ub,edgeCell,edge; } FmRingPixel;
typedef struct { POINT cell[2][FM_DISPLAY_CELLS][4], stripe[FM_DISPLAY_CELLS][4];
    POINT edge[2][FM_DISPLAY_CELLS][2], ub[FM_DISPLAY_CELLS][2], mid[FM_DISPLAY_CELLS];
    int d,thick,gap,pixelN; FmRingPixel* pixels; } FmRingMesh;
static FmRingMesh g_fm_ringMesh;
static float FmCoverage(float distance) { return distance<=-.5f?0.f:distance>=.5f?1.f:distance+.5f; }
static float FmMin(float a,float b){return a<b?a:b;}
static unsigned char FmCoverageByte(float a){return (unsigned char)(a*255.f+.5f);}
static POINT FmPolar(float r,float a) {
    POINT p={(LONG)lroundf(r*sinf(a)),(LONG)lroundf(-r*cosf(a))}; return p;
}
static void FmRingBuild(FmRingLayout g) {
    FmRingMesh* m=&g_fm_ringMesh;
    if(m->d==g.d && m->thick==g.thick && m->gap==g.gap)return;
    m->d=g.d; m->thick=g.thick; m->gap=g.gap;
    const float step=6.28318530718f/FM_DISPLAY_CELLS;
    for(int lane=0;lane<2;++lane) {
        const float ro=g.d*.5f-lane*(g.thick+g.gap),ri=ro-g.thick;
        const float inset=.45f/ri;
        for(int i=0;i<FM_DISPLAY_CELLS;++i) {
            const float a=i*step,b=(i+1)*step;
            POINT* p=m->cell[lane][i];
            p[0]=FmPolar(ro,a+inset);p[1]=FmPolar(ro,b-inset);
            p[2]=FmPolar(ri,b-inset);p[3]=FmPolar(ri,a+inset);
            m->edge[lane][i][0]=FmPolar(ri-1,a);m->edge[lane][i][1]=FmPolar(ro+1,a);
            if(!lane) {
                POINT* s=m->stripe[i];s[0]=p[0];s[1]=p[1];
                s[2]=FmPolar(ro-g.thick/3.f,b-inset);s[3]=FmPolar(ro-g.thick/3.f,a+inset);
                m->ub[i][0]=FmPolar((ro+ri)/2,a+inset);m->ub[i][1]=FmPolar((ro+ri)/2,b-inset);
                m->mid[i]=FmPolar((ro+ri)/2,(a+b)/2);
            }
        }
    }
    // Analytic signed-distance coverage: circular arcs and straight radial separators
    // get a one-pixel feather. Cache only annulus pixels; no trigonometry in redraws.
    free(m->pixels);m->pixels=0;m->pixelN=0;
    const int half=(g.d+1)/2+2,side=half*2+1;
    m->pixels=(FmRingPixel*)malloc((size_t)side*side*sizeof(FmRingPixel));
    if(!m->pixels)return; // existing GDI mesh remains an allocation-failure fallback
    for(int y=-half;y<=half;++y)for(int x=-half;x<=half;++x) {
        const float xx=x+.5f,yy=y+.5f,r=sqrtf(xx*xx+yy*yy);
        int lane=-1;
        for(int k=0;k<2;++k){const float ro=g.d*.5f-k*(g.thick+g.gap),ri=ro-g.thick;
            if(r>=ri-1.5f && r<=ro+1.5f){lane=k;break;}}
        if(lane<0)continue;
        const float ro=g.d*.5f-lane*(g.thick+g.gap),ri=ro-g.thick;
        float a=atan2f(xx,-yy);if(a<0)a+=6.28318530718f;
        int cell=(int)(a/step);if(cell>=FM_DISPLAY_CELLS)cell=FM_DISPLAY_CELLS-1;
        const float phase=a/step-cell,angular=FmMin(phase,1.f-phase)*step*r;
        const float angularCoverage=FmCoverage(angular-.45f);
        FmRingPixel p;p.x=(short)x;p.y=(short)y;p.lane=(unsigned char)lane;p.cell=(unsigned char)cell;
        p.fill=FmCoverageByte(FmCoverage(FmMin(r-ri,ro-r))*angularCoverage);
        p.stripe=FmCoverageByte(FmCoverage(FmMin(r-(ro-g.thick/3.f),ro-r))*angularCoverage);
        p.ub=FmCoverageByte(FmCoverage(.5f-fabsf(r-(ro+ri)*.5f))*angularCoverage);
        p.edgeCell=(unsigned char)(phase<.5f?cell:(cell+1)%FM_DISPLAY_CELLS);
        p.edge=FmCoverageByte(FmCoverage(.5f-angular)*FmCoverage(FmMin(r-(ri-1),ro+1-r)));
        if(p.fill || (!lane && (p.edge || p.stripe || p.ub)))m->pixels[m->pixelN++]=p;
    }
}
static uint32_t FmRingRgb(COLORREF c) {return ((uint32_t)GetRValue(c)<<16)|((uint32_t)GetGValue(c)<<8)|GetBValue(c);}
static uint32_t FmRingBlend(uint32_t dst,uint32_t rgb,unsigned a) {
    if(!a)return dst;
    if(a==255)return 0xff000000u|rgb;
    unsigned da=dst>>24;
    if(!da){const uint32_t c=dst&0xffffff;da=c==0xff00ff?0:c==0x10151e?115:255;
        dst=(da<<24)|(((((c>>16)&255)*da+127)/255)<<16)|(((((c>>8)&255)*da+127)/255)<<8)|(((c&255)*da+127)/255);}
    uint32_t result=(a+(da*(255-a)+127)/255)<<24;
    for(int shift=0;shift<24;shift+=8) {
        const unsigned source=(((rgb>>shift)&255)*a+127)/255;
        const unsigned background=(((dst>>shift)&255)*(255-a)+127)/255;
        result|=(source+background)<<shift;
    }
    return result;
}
static int FmRingPixels(HDC dc,int cx,int cy,const FmMeter* m,const FmKi* ki,int start,int player,int outer,int inner) {
    DIBSECTION dib={0};RECT clip;
    if(!n3_stroke_layered || !g_fm_ringMesh.pixels || GetClipBox(dc,&clip)!=SIMPLEREGION
        || GetObject(GetCurrentObject(dc,OBJ_BITMAP),sizeof dib,&dib)!=sizeof dib || !dib.dsBm.bmBits || dib.dsBm.bmBitsPixel!=32)return 0;
    uint32_t palette[2][FM_DISPLAY_CELLS];unsigned char stripe[FM_DISPLAY_CELLS],ub[FM_DISPLAY_CELLS],edge[FM_DISPLAY_CELLS];
    for(int i=0;i<FM_DISPLAY_CELLS;++i){const uint8_t c=FmViewCell(m,start+i);const int win=player?FmShowKiWin(m,start+i):0;
        palette[0][i]=FmRingRgb(win==2?FM_KIPERF_RGB:win?FM_KIWIN_RGB:FmColor(c&0x7F));
        palette[1][i]=FmRingRgb(player?FmIFrColor(FmViewNoDmg(m,start+i)):FmKiView(ki,m,start+i)?RGB(138,92,255):FmColor(FM_EMPTY));
        stripe[i]=(unsigned char)(player && !g_pst.v[PS_FRAME_IFR] && (FmViewNoDmg(m,start+i)&FMF_NODMG));
        ub[i]=(unsigned char)((c&0x7F)==FM_ACTIVE_UB && !win);edge[i]=(unsigned char)!!(c&FM_EDGE);
    }
    GdiFlush();
    for(int i=0;i<g_fm_ringMesh.pixelN;++i){const FmRingPixel p=g_fm_ringMesh.pixels[i];
        if(p.lane?!inner:!outer)continue;
        const int x=cx+p.x,y=cy+p.y;
        if(x<clip.left || x>=clip.right || y<clip.top || y>=clip.bottom || x<0 || x>=dib.dsBm.bmWidth || y<0 || y>=dib.dsBm.bmHeight)continue;
        uint32_t* dst=(uint32_t*)((char*)dib.dsBm.bmBits+y*dib.dsBm.bmWidthBytes)+x;
        uint32_t v=FmRingBlend(*dst,palette[p.lane][p.cell],p.fill);
        if(!p.lane){
            if(stripe[p.cell])v=FmRingBlend(v,FmRingRgb(FM_NODMG_RGB),p.stripe);
            if(ub[p.cell])v=FmRingBlend(v,FmRingRgb(RGB(255,210,216)),p.ub);
            if(edge[p.edgeCell])v=FmRingBlend(v,FmRingRgb(RGB(235,240,248)),p.edge);
        }
        *dst=v;
    }
    return 1;
}
static void FmRingPoly(HDC dc,const POINT* points,int cx,int cy,COLORREF color) {
    POINT p[4];for(int j=0;j<4;++j){p[j].x=points[j].x+cx;p[j].y=points[j].y+cy;}
    SetDCBrushColor(dc,color);Polygon(dc,p,4);
}
static void FmRingLine(HDC dc,const POINT* p,int cx,int cy,COLORREF color) {
    SetDCPenColor(dc,color);MoveToEx(dc,p[0].x+cx,p[0].y+cy,0);LineTo(dc,p[1].x+cx,p[1].y+cy);
}
static void FmRingCounts(HDC dc,const FmMeter* m,int start,int cx,int cy,FmRingLayout g,int player) {
    const int oldPx=g_fm_fontPx;
    for(int first=0;first<FM_CELLS;) {
        const uint8_t st=FmViewCell(m,first)&0x7F;
        const int win=player?FmShowKiWin(m,first):0;
        int end=first+1;
        while(end<FM_CELLS) {
            const uint8_t next=FmViewCell(m,end);
            if((next&0x7F)!=st || (next&FM_EDGE) || (player?FmShowKiWin(m,end):0)!=win)break;
            ++end;
        }
        if(st!=FM_EMPTY && end>start && end<=start+FM_DISPLAY_CELLS) {
            char text[12];snprintf(text,sizeof text,"%d",end-first);
            int px=g.thick-3,chars=(int)strlen(text);
            // Horizontal stroke digits fit inside the radial track even at 3/9 o'clock.
            while(px>6 && chars*N3StrokeWidth(px)>g.thick-2)--px;
            const int visible=first>start?first:start;
            const float radius=(g.d-g.thick)*.5f;
            const float step=6.28318530718f/FM_DISPLAY_CELLS;
            const float arc=(end-visible)*step*radius;
            if(chars*N3StrokeWidth(px)<=arc && px>=6) {
                const float offset=(chars*N3StrokeWidth(px)*.5f+1)/radius;
                const POINT p=FmPolar(radius,(end-start)*step-offset);
                const int x=cx+p.x-chars*N3StrokeWidth(px)/2,y=cy+p.y-px/2;
                g_fm_fontPx=px;
                FmLabel(dc,x+1,y+1,text,RGB(2,3,5));FmLabel(dc,x,y,text,RGB(255,255,255));
            }
        }
        first=end;
    }
    g_fm_fontPx=oldPx;
}
// Same priority as FmIFrColor. A zero kind is an empty I-FR cell and gets no number.
static int FmIfrKind(int bits) {
    return bits&FMF_NODMG ? 1 : bits&FMF_EAA ? 2 : bits&FMF_EA7 ? 3 : bits&FMF_GUARD ? 4 : 0;
}
// Run length of one I-FR colour, on the inner player lane. Independent of action cells and FM_EDGE.
static void FmRingIfrCounts(HDC dc,const FmMeter* m,int start,int cx,int cy,FmRingLayout g) {
    const int oldPx=g_fm_fontPx;
    const float radius=g.d*.5f-(g.thick+g.gap)-g.thick*.5f;
    const float step=6.28318530718f/FM_DISPLAY_CELLS;
    for(int first=0;first<FM_CELLS;) {
        const int kind=FmIfrKind(FmViewNoDmg(m,first));
        int end=first+1;
        while(end<FM_CELLS && FmIfrKind(FmViewNoDmg(m,end))==kind)++end;
        if(kind && end>start && end<=start+FM_DISPLAY_CELLS) {
            char text[12];snprintf(text,sizeof text,"%d",end-first);
            int px=g.thick-3,chars=(int)strlen(text);
            while(px>6 && chars*N3StrokeWidth(px)>g.thick-2)--px;
            const int visible=first>start?first:start;
            const float arc=(end-visible)*step*radius;
            if(chars*N3StrokeWidth(px)<=arc && px>=6) {
                const float offset=(chars*N3StrokeWidth(px)*.5f+1)/radius;
                const POINT p=FmPolar(radius,(end-start)*step-offset);
                const int x=cx+p.x-chars*N3StrokeWidth(px)/2,y=cy+p.y-px/2;
                g_fm_fontPx=px;
                FmLabel(dc,x+1,y+1,text,RGB(2,3,5));FmLabel(dc,x,y,text,RGB(255,255,255));
            }
        }
        first=end;
    }
    g_fm_fontPx=oldPx;
}
// Upright check proportions follow track thickness, not the very narrow 1/120 arc.
// Thin antialiased strokes keep a tick readable rather than a tall white blob.
static float FmCheckDistance(float x,float y,float ax,float ay,float bx,float by) {
    const float dx=bx-ax,dy=by-ay;
    float t=((x-ax)*dx+(y-ay)*dy)/(dx*dx+dy*dy);if(t<0)t=0;if(t>1)t=1;
    return hypotf(x-ax-t*dx,y-ay-t*dy);
}
static void FmRingCheck(HDC dc,int cx,int cy,FmRingLayout g,int cell) {
    const POINT mid=g_fm_ringMesh.mid[cell];
    const float size=g.thick*.65f,stroke=size*.12f>1.f?size*.12f:1.f;
    DIBSECTION dib={0};RECT clip;
    if(!n3_stroke_layered || GetClipBox(dc,&clip)!=SIMPLEREGION ||
       GetObject(GetCurrentObject(dc,OBJ_BITMAP),sizeof dib,&dib)!=sizeof dib || !dib.dsBm.bmBits || dib.dsBm.bmBitsPixel!=32){
        int box=(int)size;FmCheck(dc,cx+mid.x-box/2,cy+mid.y-box/2,box,box);return;
    }
    const int half=g.thick/2+2;
    GdiFlush();
    for(int y=-half;y<=half;++y)for(int x=-half;x<=half;++x){
        const int xx=cx+mid.x+x,yy=cy+mid.y+y;
        if(xx<clip.left||xx>=clip.right||yy<clip.top||yy>=clip.bottom||
           xx<0||xx>=dib.dsBm.bmWidth||yy<0||yy>=dib.dsBm.bmHeight)continue;
        const float px=x+.5f,py=y+.5f;
        const float distance=FmMin(FmCheckDistance(px,py,-size*.38f,0,-size*.10f,size*.28f),
                                   FmCheckDistance(px,py,-size*.10f,size*.28f,size*.40f,-size*.34f));
        // Restrict every mark pixel to its actor's outer track, even on the diagonal.
        const float r=hypotf(mid.x+px,mid.y+py);
        if(r<g.d*.5f-g.thick || r>g.d*.5f)continue;
        const unsigned dark=FmCoverageByte(FmCoverage(stroke*.5f+1.f-distance));
        if(!dark)continue;
        const unsigned light=FmCoverageByte(FmCoverage(stroke*.5f-distance));
        uint32_t* out=(uint32_t*)((char*)dib.dsBm.bmBits+yy*dib.dsBm.bmWidthBytes)+xx;
        *out=FmRingBlend(FmRingBlend(*out,0x080a10,dark),0xf5f8ff,light);
    }
}
static void FmRingActor(HDC dc,int cx,int cy,FmRingLayout g,const FmMeter* m,const FmKi* ki,
                        int player,int outer,int inner) {
    const int start=FmVisibleStart(m);
    const FmRingMesh* mesh=&g_fm_ringMesh;
    if(!FmRingPixels(dc,cx,cy,m,ki,start,player,outer,inner)) {
    HGDIOBJ oldBrush=SelectObject(dc,GetStockObject(DC_BRUSH));
    HGDIOBJ oldPen=SelectObject(dc,GetStockObject(NULL_PEN));
    for(int i=0;i<FM_DISPLAY_CELLS;++i) {
        const uint8_t c=FmViewCell(m,start+i);
        const int win=player?FmShowKiWin(m,start+i):0;
        if(outer) {
            FmRingPoly(dc,mesh->cell[0][i],cx,cy,win==2?FM_KIPERF_RGB:win?FM_KIWIN_RGB:FmColor(c&0x7F));
            // The existing Row fallback: I-FR lane OFF leaves the gold no-damage stripe on PLAYER.
            if(player && !g_pst.v[PS_FRAME_IFR] && (FmViewNoDmg(m,start+i)&FMF_NODMG))
                FmRingPoly(dc,mesh->stripe[i],cx,cy,FM_NODMG_RGB);
        }
        if(inner) FmRingPoly(dc,mesh->cell[1][i],cx,cy,player?FmIFrColor(FmViewNoDmg(m,start+i)):
            FmKiView(ki,m,start+i)?RGB(138,92,255):FmColor(FM_EMPTY));
    }
    SelectObject(dc,GetStockObject(DC_PEN));
    if(outer)for(int i=0;i<FM_DISPLAY_CELLS;++i) {
        const uint8_t c=FmViewCell(m,start+i);
        if(c&FM_EDGE)FmRingLine(dc,mesh->edge[0][i],cx,cy,RGB(235,240,248));
        if((c&0x7F)==FM_ACTIVE_UB && !(player && FmShowKiWin(m,start+i)))
            FmRingLine(dc,mesh->ub[i],cx,cy,RGB(255,210,216));
    }
    SelectObject(dc,oldPen);SelectObject(dc,oldBrush);
    }
    if(outer)FmRingCounts(dc,m,start,cx,cy,g,player);
    if(inner && player)FmRingIfrCounts(dc,m,start,cx,cy,g);
    if(outer && player)for(int i=0;i<FM_DISPLAY_CELLS;++i)if(FmViewKiPulse(m,start+i)) {
        FmRingCheck(dc,cx,cy,g,i);
    }
    const int oldLabelPx=g_fm_fontPx;
    if(g_fm_fontPx>g.d/12)g_fm_fontPx=g.d/12;
    const int px=g_fm_fontPx;
    const char* actor=player?"PLAYER":"BOSS";
    FmLabel(dc,cx-(int)strlen(actor)*N3StrokeWidth(px)/2,cy-px,actor,RGB(240,243,249));
    if(g_pst.v[PS_FRAME_READOUT]) {
        char text[32];snprintf(text,sizeof text,"%d%s F",m->cur.total,m->cur.done?"":"+");
        FmLabel(dc,cx-(int)strlen(text)*N3StrokeWidth(px)/2,cy,text,RGB(214,222,234));
    }
    if(inner) {
        const char* label=player?"I-FR":g_fm_ki_label;
        FmLabel(dc,cx-(int)strlen(label)*N3StrokeWidth(px)/2,cy+px,label,RGB(145,159,181));
    }
    g_fm_fontPx=oldLabelPx;
}
static void FmRingLegend(HDC dc,int x,int cy,FmRingLayout g,int side) {
    if(!g.legendW)return;
    int use[18]; FmLegendUse(use);
    const char** names=FmLegendNames();COLORREF colors[18];FmLegendColors(colors);
    const int old=g_fm_fontPx;g_fm_fontPx=g.legendPx;
    int n=0;for(int i=side*9;i<side*9+9;++i)n+=use[i];
    int y=cy-n*g.legendPitch/2;
    for(int i=side*9;i<side*9+9;++i)if(use[i]){
        FmFill(dc,x,y+2,8,8,colors[i]);
        FmLabel(dc,x+12,y,names[i],RGB(183,194,211));
        y+=g.legendPitch;
    }
    g_fm_fontPx=old;
}
static int FmRings(HDC dc,int w,int H,int oy,int panelW,const FmMeter* player,const FmMeter* boss,const FmKi* ki) {
    int cw,bh,lh,pad;FmGeom(w,H,&cw,&bh,&lh,&pad);
    const int left=(w-panelW)/2;
    const FmRingLayout g=FmRingGeom(w,H);FmRingBuild(g);
    const int hasPlayer=FmPlayerDraw()||FmIFrRow(),hasBoss=g_pst.v[PS_FRAME_METER]&&g_fm_show;
    FmFill(dc,left,oy,panelW,g.height,RGB(16,21,30));
    SetBkMode(dc,TRANSPARENT);
    const int cy=oy+g.height/2;
    if(hasPlayer)FmRingActor(dc,hasBoss?w/2-(g.d+lh)/2:w/2,cy,g,player,ki,1,FmPlayerDraw(),FmIFrRow());
    if(hasBoss)FmRingActor(dc,hasPlayer?w/2+(g.d+lh)/2:w/2,cy,g,boss,ki,0,1,FmKiShown());
    if(g.legendW){
        const int outer=(hasPlayer && hasBoss)?g.d+lh/2:g.d/2;
        FmRingLegend(dc,w/2-outer-g.legendGap-g.legendW,cy,g,0);
        FmRingLegend(dc,w/2+outer+g.legendGap,cy,g,1);
    }
    return g.height;
}
