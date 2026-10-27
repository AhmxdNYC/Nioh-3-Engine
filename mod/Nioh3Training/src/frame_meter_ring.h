// Two independent actor histories, each with its own optional inner lane.
// Uses the Row renderer's samples, colours, predicates, checks and legend.
#pragma once
#include <math.h>
typedef struct { int d, thick, gap, height; } FmRingLayout;
static FmRingLayout FmRingGeom(int w, int H) {
    int cw,bh,lh,pad; FmGeom(w,H,&cw,&bh,&lh,&pad);
    const int panelW=cw*FM_DISPLAY_CELLS+lh*3+pad*2;
    FmRingLayout g;
    g.d=H*260/1080; if(g.d<180)g.d=180; if(g.d>512)g.d=512;
    if(g.d>(panelW-lh*2)/2)g.d=(panelW-lh*2)/2;
    if(g.d<64)g.d=64;
    g.thick=bh; if(g.thick>g.d/9)g.thick=g.d/9;
    g.gap=pad*2+2;
    g.height=g.d+pad*4;
    return g;
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
// Coverage masks are built once per geometry, independent of sample colours.
#define FM_RING_AA 4
#define FM_RING_VERTS 8
typedef struct { int x,y,w,h; unsigned char* coverage; POINT outline[FM_RING_VERTS]; } FmRingTile;
static void FmRingTileBuild(FmRingTile* t,const POINT* p) {
    free(t->coverage);memset(t,0,sizeof *t);
    for(int j=0;j<FM_RING_VERTS;++j){t->outline[j].x=(LONG)lround(p[j].x/(double)FM_RING_AA);t->outline[j].y=(LONG)lround(p[j].y/(double)FM_RING_AA);}
    int x0=p[0].x,y0=p[0].y,x1=x0,y1=y0;
    for(int i=1;i<FM_RING_VERTS;++i){if(p[i].x<x0)x0=p[i].x;if(p[i].x>x1)x1=p[i].x;
        if(p[i].y<y0)y0=p[i].y;if(p[i].y>y1)y1=p[i].y;}
    t->x=(int)floor(x0/(double)FM_RING_AA)-1;t->y=(int)floor(y0/(double)FM_RING_AA)-1;
    t->w=(int)ceil(x1/(double)FM_RING_AA)-t->x+1;t->h=(int)ceil(y1/(double)FM_RING_AA)-t->y+1;
    const int w=t->w*FM_RING_AA,h=t->h*FM_RING_AA;
    HDC dc=CreateCompatibleDC(0);if(!dc)return;
    BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    uint32_t* bits=0;HBITMAP bm=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,(void**)&bits,0,0);
    t->coverage=(unsigned char*)malloc((size_t)t->w*t->h);
    if(!bm||!bits||!t->coverage){free(t->coverage);t->coverage=0;if(bm)DeleteObject(bm);DeleteDC(dc);return;}
    HGDIOBJ old=SelectObject(dc,bm),pen=SelectObject(dc,GetStockObject(NULL_PEN)),brush=SelectObject(dc,GetStockObject(WHITE_BRUSH));
    memset(bits,0,(size_t)w*h*4);
    POINT q[FM_RING_VERTS];for(int i=0;i<FM_RING_VERTS;++i){q[i].x=p[i].x-t->x*FM_RING_AA;q[i].y=p[i].y-t->y*FM_RING_AA;}
    Polygon(dc,q,FM_RING_VERTS);GdiFlush();
    for(int y=0;y<t->h;++y)for(int x=0;x<t->w;++x){unsigned n=0;
        for(int dy=0;dy<FM_RING_AA;++dy)for(int dx=0;dx<FM_RING_AA;++dx)n+=(bits[(y*FM_RING_AA+dy)*w+x*FM_RING_AA+dx]&255)!=0;
        t->coverage[y*t->w+x]=(unsigned char)((n*255+8)/16);}
    SelectObject(dc,brush);SelectObject(dc,pen);SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);
}
typedef struct { POINT cell[2][FM_DISPLAY_CELLS][FM_RING_VERTS], stripe[FM_DISPLAY_CELLS][FM_RING_VERTS];
    FmRingTile tile[2][FM_DISPLAY_CELLS], stripeTile[FM_DISPLAY_CELLS];
    POINT edge[2][FM_DISPLAY_CELLS][2], ub[FM_DISPLAY_CELLS][2], mid[FM_DISPLAY_CELLS];
    int d,thick,gap; } FmRingMesh;
static FmRingMesh g_fm_ringMesh;
static POINT FmPolar(float r,float a) {
    POINT p={(LONG)lroundf(r*sinf(a)),(LONG)lroundf(-r*cosf(a))}; return p;
}
static POINT FmPolarFine(float r,float a) {return FmPolar(r*FM_RING_AA,a);}
static void FmRingSector(POINT* p,float ri,float ro,float a,float b) {
    for(int j=0;j<4;++j){p[j]=FmPolarFine(ro,a+(b-a)*j/3.f);p[4+j]=FmPolarFine(ri,b-(b-a)*j/3.f);}
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
            FmRingSector(p,ri,ro,a+inset,b-inset);
            FmRingTileBuild(&m->tile[lane][i],p);
            m->edge[lane][i][0]=FmPolar(ri-1,a);m->edge[lane][i][1]=FmPolar(ro+1,a);
            if(!lane) {
                POINT* s=m->stripe[i];FmRingSector(s,ro-g.thick/3.f,ro,a+inset,b-inset);
                FmRingTileBuild(&m->stripeTile[i],s);
                m->ub[i][0]=FmPolar((ro+ri)/2,a+inset);m->ub[i][1]=FmPolar((ro+ri)/2,b-inset);
                m->mid[i]=FmPolar((ro+ri)/2,(a+b)/2);
            }
        }
    }
}
static void FmRingRaw(HDC dc,const FmRingTile* t,int cx,int cy,COLORREF color) {
    POINT p[FM_RING_VERTS];for(int i=0;i<FM_RING_VERTS;++i){p[i].x=t->outline[i].x+cx;p[i].y=t->outline[i].y+cy;}
    SetDCBrushColor(dc,color);Polygon(dc,p,FM_RING_VERTS);
}
static void FmRingPoly(HDC dc,const FmRingTile* t,int cx,int cy,COLORREF color) {
    if(!t->coverage || !n3_stroke_layered){FmRingRaw(dc,t,cx,cy,color);return;}
    DIBSECTION dst={0};RECT clip;
    const int kind=GetClipBox(dc,&clip);
    if(kind==NULLREGION)return;
    if(kind!=SIMPLEREGION || GetObject(GetCurrentObject(dc,OBJ_BITMAP),sizeof dst,&dst)!=sizeof dst
       || !dst.dsBm.bmBits || dst.dsBm.bmBitsPixel!=32){FmRingRaw(dc,t,cx,cy,color);return;}
    if(clip.left<0)clip.left=0;if(clip.top<0)clip.top=0;
    if(clip.right>dst.dsBm.bmWidth)clip.right=dst.dsBm.bmWidth;
    if(clip.bottom>dst.dsBm.bmHeight)clip.bottom=dst.dsBm.bmHeight;
    const uint32_t rgb=(GetRValue(color)<<16)|(GetGValue(color)<<8)|GetBValue(color);
    GdiFlush(); // finish prior GDI fills before direct premultiplied DIB writes
    for(int y=0;y<t->h;++y){int yy=cy+t->y+y;if(yy<clip.top||yy>=clip.bottom)continue;
        for(int x=0;x<t->w;++x){int xx=cx+t->x+x;if(xx<clip.left||xx>=clip.right)continue;
            const unsigned a=t->coverage[y*t->w+x];if(!a)continue;
            uint32_t* out=(uint32_t*)((char*)dst.dsBm.bmBits+yy*dst.dsBm.bmWidthBytes)+xx;
            uint32_t v=*out,da=v>>24,c=v&0xffffff;
            if(!da){da=c==0xff00ff?0:c==0x10151e?115:255;v=da<<24;
                for(int shift=0;shift<24;shift+=8)v|=((((c>>shift)&255)*da+127)/255)<<shift;}
            uint32_t value=(a+(((v>>24)*(255-a)+127)/255))<<24;
            for(int shift=0;shift<24;shift+=8)value|=(((((rgb>>shift)&255)*a+127)/255+(((v>>shift)&255)*(255-a)+127)/255)<<shift);
            *out=value;
        }
    }
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
static void FmRingActor(HDC dc,int cx,int cy,FmRingLayout g,const FmMeter* m,const FmKi* ki,
                        int player,int outer,int inner) {
    const int start=FmVisibleStart(m);
    const FmRingMesh* mesh=&g_fm_ringMesh;
    HGDIOBJ oldBrush=SelectObject(dc,GetStockObject(DC_BRUSH));
    HGDIOBJ oldPen=SelectObject(dc,GetStockObject(NULL_PEN));
    for(int i=0;i<FM_DISPLAY_CELLS;++i) {
        const uint8_t c=FmViewCell(m,start+i);
        const int win=player?FmShowKiWin(m,start+i):0;
        if(outer) {
            FmRingPoly(dc,&mesh->tile[0][i],cx,cy,win==2?FM_KIPERF_RGB:win?FM_KIWIN_RGB:FmColor(c&0x7F));
            // The existing Row fallback: I-FR lane OFF leaves the gold no-damage stripe on PLAYER.
            if(player && !g_pst.v[PS_FRAME_IFR] && (FmViewNoDmg(m,start+i)&FMF_NODMG))
                FmRingPoly(dc,&mesh->stripeTile[i],cx,cy,FM_NODMG_RGB);
        }
        if(inner) FmRingPoly(dc,&mesh->tile[1][i],cx,cy,player?FmIFrColor(FmViewNoDmg(m,start+i)):
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
    if(outer)FmRingCounts(dc,m,start,cx,cy,g,player);
    if(outer && player)for(int i=0;i<FM_DISPLAY_CELLS;++i)if(FmViewKiPulse(m,start+i)) {
        int cw=(int)((g.d-g.thick)*3.14159265f/FM_DISPLAY_CELLS);if(cw<6)cw=6;
        FmCheck(dc,cx+mesh->mid[i].x-cw/2,cy+mesh->mid[i].y-g.thick/2,cw,g.thick);
    }
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
}
static int FmRings(HDC dc,int w,int H,int oy,const FmMeter* player,const FmMeter* boss,const FmKi* ki) {
    int cw,bh,lh,pad;FmGeom(w,H,&cw,&bh,&lh,&pad);
    const int panelW=cw*FM_DISPLAY_CELLS+lh*3+pad*2,left=(w-panelW)/2;
    const FmRingLayout g=FmRingGeom(w,H);FmRingBuild(g);
    const int hasPlayer=FmPlayerDraw()||FmIFrRow(),hasBoss=g_pst.v[PS_FRAME_METER]&&g_fm_show;
    FmFill(dc,left,oy,panelW,g.height,RGB(16,21,30));
    SetBkMode(dc,TRANSPARENT);
    const int cy=oy+g.height/2;
    if(hasPlayer)FmRingActor(dc,hasBoss?w/2-(g.d+lh)/2:w/2,cy,g,player,ki,1,FmPlayerDraw(),FmIFrRow());
    if(hasBoss)FmRingActor(dc,hasPlayer?w/2+(g.d+lh)/2:w/2,cy,g,boss,ki,0,1,FmKiShown());
    return g.height;
}
