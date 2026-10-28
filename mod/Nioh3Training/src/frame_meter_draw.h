// Shared production renderer, also exercised by the offline DIB checker. No game reads.
#pragma once
// ---- drawing (own thread) ----
#include "frame_meter_count.h"
#define FM_DISPLAY_CELLS 120 // SF6-sized pips; retain the full 360-cell sampler history.
#define FM_KEY RGB(255, 0, 255)
static int g_fm_fontPx = 12;
static COLORREF FmColor(uint8_t st) {
    switch (st) {
    case FM_NEUTRAL: return RGB(9,12,17);
    case FM_PENDING: return RGB(105,117,134);
    case FM_STARTUP: return RGB(80,210,72);
    case FM_ACTIVE: return RGB(255,142,91);
    case FM_ACTIVE_UB: return RGB(155,18,39);
    case FM_GRAB: return RGB(205,210,220);
    case FM_RECOVERY: return RGB(35,157,220);
    default: return RGB(5,7,10);
    }
}
static void FmFill(HDC dc, int x, int y, int w, int h, COLORREF c) { RECT r = {x, y, x + w, y + h}; HBRUSH b = CreateSolidBrush(c); FillRect(dc, &r, b); DeleteObject(b); }
static int FmGeom(int W, int H, int* cw, int* barH, int* lineH, int* pad) {
    const float k = H > 0 ? (float)H / 1080.0f : 1.0f;
    *lineH = (int)(26*k); if (*lineH<22) *lineH=22;
    *pad = (int)(2*k); if (*pad<2) *pad=2;
    *cw = FmCellWidth(W,*lineH*3,*pad,FM_DISPLAY_CELLS);
    *barH = (int)(22*k); if (*barH<20) *barH=20;
    return *pad*2 + *barH;
}
static int FmReadoutHeight(int barH, int pad) { return g_pst.v[PS_FRAME_READOUT] ? barH+pad : 0; }
static int FmKiShown(void) { return g_pst.v[PS_FRAME_KI] && g_pst.v[PS_FRAME_METER] && g_fm_show && g_fm_ki_show; }
static int FmPlayerRow(void) { return (g_pst.v[PS_PLAYER_METER] || g_pst.v[PS_KI_WINDOW]) && g_fmp_show; }
static int FmIFrRow(void) { return g_pst.v[PS_FRAME_IFR] && FmPlayerRow(); }  // human #37322: separate i-frame row under the PLAYER row
static int FmPlayerDraw(void) { return FmPlayerRow() && g_pst.v[PS_FRAME_PROW]; }  // #38180: PLAYER row can be hidden while the I-FR row stays
static int FmPanels(void) { return (g_pst.v[PS_FRAME_METER] && g_fm_show ? 1 : 0) + (FmPlayerDraw() ? 1 : 0) + FmIFrRow() + FmKiShown(); }
#define FM_KIWIN_RGB RGB(16,44,168)
#define FM_KIPERF_RGB RGB(64,232,214)
#define FM_EAA_RGB RGB(255,120,40)   // I-FR row: ctrl+EAA set (many specials)
#define FM_GUARD_RGB RGB(70,200,120) // ctrl+EA5 (guard stance)
#define FM_EA7_RGB RGB(220,110,225)  // ctrl+EA7
#define FM_NODMG_RGB RGB(255,206,48)  // #35176 i-frame stripe (gold), top third of the cell
// #38492: teal (perfect Ki Pulse window, mark 2) always shows on the PLAYER row; dark blue (mark 1, whole window) only with KiWindow ON
static int FmShowKiWin(const FmMeter* m, int i) { const int v = FmViewKiWinRaw(m, i); return v == 1 && !g_pst.v[PS_KI_WINDOW] ? 0 : v; }
// #38505: rounded corners for the whole meter stack (all rows share one x extent). Paints the colour key (transparent) outside a
// circle of radius r in each corner, one FillRect per corner row.
static int FmRadius(int H) { const int r = (int)(6.0f * (H > 0 ? (float)H / 1080.0f : 1.0f) + 0.5f); return r < 4 ? 4 : r; }
static void FmRoundCorners(HDC dc, int x, int y, int w, int h, int r) {
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    HBRUSH key = CreateSolidBrush(FM_KEY);
    for (int i = 0; i < r; ++i) {
        const float dy = (float)r - ((float)i + 0.5f);
        const int in = (int)((float)r - sqrtf((float)(r * r) - dy * dy) + 0.5f);
        if (in <= 0) continue;
        RECT a = {x, y + i, x + in, y + i + 1}, b = {x + w - in, y + i, x + w, y + i + 1};
        RECT c = {x, y + h - 1 - i, x + in, y + h - i}, d = {x + w - in, y + h - 1 - i, x + w, y + h - i};
        FillRect(dc, &a, key); FillRect(dc, &b, key); FillRect(dc, &c, key); FillRect(dc, &d, key);
    }
    DeleteObject(key);
}
static COLORREF FmIFrColor(int bits) {
    return bits&FMF_NODMG ? FM_NODMG_RGB : bits&FMF_EAA ? FM_EAA_RGB : bits&FMF_EA7 ? FM_EA7_RGB : bits&FMF_GUARD ? FM_GUARD_RGB : FmColor(FM_EMPTY);
}
#include "frame_meter_text_cache.h"
static void FmLabel(HDC dc, int x, int y, const char* text, COLORREF color) {
    if(g_pst.v[PS_FRAME_STYLE] && FmCachedLabel(dc,x,y,text,g_fm_fontPx,color))return;
    SetTextColor(dc,color); N3StrokeText(dc,x,y,text,(int)strlen(text),g_fm_fontPx,N3StrokeWidth(g_fm_fontPx));
}
static void FmCheck(HDC dc, int x, int y, int w, int h) {
    // #38492: check mark ~25% smaller than the full cell, centred
    const int cx = x + w / 2, cy = y + h / 2, sw = (w * 3) / 4, sh = (h * 3) / 4;
    x = cx - sw / 2; y = cy - sh / 2; w = sw; h = sh;
    const int thick = h / 3 > 3 ? h / 3 : 3;
    const int x0 = x + w / 5, y0 = y + h / 2, x1 = x + w / 2, y1 = y + (h * 4) / 5, x2 = x + (w * 4) / 5, y2 = y + h / 5;
    HPEN dark = CreatePen(PS_SOLID, thick + 2, RGB(8, 10, 16));
    HPEN light = CreatePen(PS_SOLID, thick, RGB(245, 248, 255));
    HGDIOBJ old = SelectObject(dc, dark);
    MoveToEx(dc, x0, y0, 0); LineTo(dc, x1, y1); LineTo(dc, x2, y2);
    SelectObject(dc, light);
    MoveToEx(dc, x0, y0, 0); LineTo(dc, x1, y1); LineTo(dc, x2, y2);
    SelectObject(dc, old); DeleteObject(dark); DeleteObject(light);
}
static void FmPanel(HDC dc, int w, int h, int H, int oy, const FmMeter* mp, const char* who, const char* side, int kiStripe, int noDmg) {
    const FmMeter m=*mp;
    int cw,barH,lineH,pad; const int ph=FmGeom(w,H,&cw,&barH,&lineH,&pad); (void)h;
    const int px=barH-2>9?barH-2:9;
    g_fm_fontPx=px;
    int used=FM_CELLS;
    while(used>0 && (FmViewCell(&m,used-1)&0x7F)==FM_EMPTY) --used;
    const int viewStart=used>FM_DISPLAY_CELLS?used-FM_DISPLAY_CELLS:0;
    const int barW=cw*FM_DISPLAY_CELLS,gutter=lineH*3,x0=(w-barW-gutter)/2+gutter,by=oy+pad;
    FmFill(dc,x0-gutter-pad,oy,barW+gutter+pad*2,ph,RGB(16,21,30));
    SetBkMode(dc,TRANSPARENT);
    // Clip long actor names/freeze hints to the strip instead of spilling outside the panel.
    int saved=SaveDC(dc); IntersectClipRect(dc,x0-gutter,oy,x0+barW,oy+ph);
    (void)who; // Compact default has no actor/title line.
    FmLabel(dc,x0-gutter,by,side,RGB(240,243,249));
    FmFill(dc,x0-1,by-1,barW+2,barH+2,RGB(41,50,65));
    for(int i=0;i<FM_DISPLAY_CELLS;++i) {
        const uint8_t c=FmViewCell(&m,viewStart+i);
        const int win=kiStripe ? FmShowKiWin(&m,viewStart+i) : 0;
        FmFill(dc,x0+i*cw,by,cw>3?cw-2:cw>1?cw-1:cw,barH,win==2?FM_KIPERF_RGB:win?FM_KIWIN_RGB:FmColor(c&0x7F));
        if(!win && (c&0x7F)==FM_ACTIVE_UB) FmFill(dc,x0+i*cw,by+barH/2,cw>3?cw-2:1,1,RGB(255,210,216));
        if(c&FM_EDGE) FmFill(dc,x0+i*cw,by-2,1,barH+4,RGB(235,240,248));
        if(noDmg && (FmViewNoDmg(&m,viewStart+i)&FMF_NODMG)) FmFill(dc,x0+i*cw,by,cw>3?cw-2:cw>1?cw-1:cw,barH/3>2?barH/3:2,FM_NODMG_RGB);
    }
    // Count each visible colour run; action boundaries and the Ki window also start a new run.
    // Centre within its LAST coloured cell, excluding the separator.
    for(int first=0;first<FM_CELLS;) {
        const uint8_t st=FmViewCell(&m,first)&0x7F;
        const int win=kiStripe ? FmShowKiWin(&m,first) : 0;
        int end=first+1;
        while(end<FM_CELLS) {
            const uint8_t next=FmViewCell(&m,end);
            const int nwin=kiStripe ? FmShowKiWin(&m,end) : 0;
            if((next&0x7F)!=st || (next&FM_EDGE) || nwin!=win) break;
            ++end;
        }
        if(st!=FM_EMPTY && end>viewStart && end<=viewStart+FM_DISPLAY_CELLS) {
            char n[12]; snprintf(n,sizeof n,"%d",end-first);
            const int visibleFirst=first>viewStart?first:viewStart;
            const int right=x0+(end-viewStart)*cw;
            const int wanted=(int)strlen(n)*N3StrokeWidth(px);
            const int available=(end-visibleFirst)*cw;
            const int countW=wanted<available?wanted:available;
            const int left=right-countW;
            const int fillW=cw>3?cw-2:cw>1?cw-1:cw;
            RECT cell={left,by,right-(cw-fillW),by+barH},textBox;
            N3CountFont font=FmFitCount(dc,n,cell,px,&textBox);
            if(font.px) {
                int clip=SaveDC(dc); IntersectClipRect(dc,cell.left,cell.top,cell.right,cell.bottom);
                for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx)
                    if(dx || dy) { SetTextColor(dc,RGB(2,3,5)); N3StrokeText(dc,textBox.left+dx,textBox.top+dy,n,(int)strlen(n),font.px,font.advance); }
                SetTextColor(dc,RGB(255,255,255)); N3StrokeText(dc,textBox.left,textBox.top,n,(int)strlen(n),font.px,font.advance);
                RestoreDC(dc,clip);
            }
        }
        first=end;
    }
    if(kiStripe) {
        for(int i=0;i<FM_DISPLAY_CELLS;++i) if(FmViewKiPulse(&m,viewStart+i))
            FmCheck(dc,x0+i*cw,by,cw>3?cw-2:cw,barH);
    }
    RestoreDC(dc,saved);
}
// Shared legend below both rows; wraps at smaller resolutions.
// KEY entries (names/colours in FmLegend): which are listed = what the meter can show now AND the per-group KEY toggles (#38180)
static int FmLegendUse(int use[18]) {
    for(int i=0;i<11;++i) use[i]=1;
    use[11]=g_pst.v[PS_KI_WINDOW]?1:0; use[12]=FmKiShown()?1:0; use[13]=FmPlayerRow()?1:0; use[14]=FmPlayerDraw()?1:0; use[15]=use[16]=use[17]=FmIFrRow()?1:0;
    static const struct { int i, key; } grp[] = {{0,PS_KEY_STARTUP},{1,PS_KEY_ACTIVE},{2,PS_KEY_RECOVERY},{3,PS_KEY_UNBLOCK},{4,PS_KEY_GRAB},{5,PS_KEY_OTHER},{6,PS_KEY_OTHER},{7,PS_KEY_OTHER},
        {8,PS_KEY_TIMING},{9,PS_KEY_TIMING},{10,PS_KEY_TIMING},{11,PS_KEY_KI},{12,PS_KEY_KI},{14,PS_KEY_KI},{13,PS_KEY_IFR},{15,PS_KEY_IFR},{16,PS_KEY_IFR},{17,PS_KEY_IFR}};
    for(int k=0;k<18;++k) if(!g_pst.v[grp[k].key]) use[grp[k].i]=0;
    int n=0; for(int i=0;i<18;++i) n+=use[i]; return n;
}
static int FmLegendCount(void) { int u[18]; return FmLegendUse(u); }
static int FmLegendHeight(int w,int H) {
    if(!g_pst.v[PS_FRAME_LEGEND] || g_pst.v[PS_FRAME_STYLE]) return 0;
    int cw,bh,lh,pad; FmGeom(w,H,&cw,&bh,&lh,&pad);
    const int n=FmLegendCount();
    if(n<=0) return 0;
    int cols=cw*FM_DISPLAY_CELLS/(lh*7); if(cols<1) cols=1; if(cols>n) cols=n;
    return ((n+cols-1)/cols)*lh+pad*2;
}
static const char** FmLegendNames(void) {
    static const char* names[]={"Startup","Active","Recovery","Unblockable","Grab","No hit","Pending","Empty","Action start","Total frames","Stun/adv --","Ki window","Ki debt","I FR","Perf Ki","I FR special","I FR guard","I FR other"};
    return names;
}
static void FmLegendColors(COLORREF colors[18]) {
    COLORREF values[]={FmColor(FM_STARTUP),FmColor(FM_ACTIVE),FmColor(FM_RECOVERY),FmColor(FM_ACTIVE_UB),FmColor(FM_GRAB),FmColor(FM_NEUTRAL),FmColor(FM_PENDING),FmColor(FM_EMPTY),RGB(235,240,248),RGB(214,222,234),RGB(145,159,181),FM_KIWIN_RGB,RGB(138,92,255),FM_NODMG_RGB,FM_KIPERF_RGB,FM_EAA_RGB,FM_GUARD_RGB,FM_EA7_RGB};
    memcpy(colors,values,sizeof values);
}
static void FmLegend(HDC dc,int w,int H,int y) {
    if(!g_pst.v[PS_FRAME_LEGEND] || FmLegendCount()<=0) return;
    int cw,bh,lh,pad; FmGeom(w,H,&cw,&bh,&lh,&pad);
    const int bw=cw*FM_DISPLAY_CELLS,gutter=lh*3,x=(w-bw-gutter)/2+gutter;
    const char** names=FmLegendNames(); COLORREF colors[18]; FmLegendColors(colors);
    int use[18]; FmLegendUse(use);
    int idx[18], n=0; for(int i=0;i<18;++i) if(use[i]) idx[n++]=i;
    int cols=bw/(lh*7); if(cols<1) cols=1; if(cols>n) cols=n;
    const int pitch=bw/cols;
    FmFill(dc,x-gutter-pad,y,bw+gutter+pad*2,FmLegendHeight(w,H),RGB(16,21,30));
    SetBkMode(dc,TRANSPARENT);
    FmLabel(dc,x-gutter,y+pad,"KEY",RGB(145,159,181));
    for(int i=0;i<n;++i) {
        const int xx=x+(i%cols)*pitch,yy=y+pad+(i/cols)*lh;
        FmFill(dc,xx,yy+3,8,8,colors[idx[i]]); FmLabel(dc,xx+12,yy,names[idx[i]],RGB(183,194,211));
    }
}
#include "frame_meter_ring.h"
static void FmPaint(HDC dc, int w, int h, int H) {
    static FmMeter m,mp; static FmKi ki; int showKi; char who[48]; AcquireSRWLockShared(&g_fm_lock);
    m=g_fm; mp=g_fmp; ki=g_fm_ki; showKi=FmKiShown(); snprintf(who,sizeof who,"%s",g_fm_who); ReleaseSRWLockShared(&g_fm_lock);
    FmFill(dc,0,0,w,h,FM_KEY);
    int cw,barH,lineH,pad; const int ph=FmGeom(w,H,&cw,&barH,&lineH,&pad);
    // Clip long readouts/key labels as well as cells to the centered panel bounds.
    int panelW=cw*FM_DISPLAY_CELLS+lineH*3+pad*2;
    const int ring=g_pst.v[PS_FRAME_STYLE];
    if(ring) panelW=FmRingContentWidth(w,H);
    int panelLeft=(w-panelW)/2;
    const int panelClip=SaveDC(dc);
    g_fm_fontPx=barH-2>9?barH-2:9;
    int oy=FmReadoutHeight(barH,pad);
    if(oy) {
        const FmSum* q=FmPlayerRow() ? &mp.cur : &m.cur;
        char start[24]="--",active[192]="--",recovery[24]="--",total[24]="--",text[320];
        if(q->hits) {
            snprintf(start,sizeof start,"%d",q->startup);
            snprintf(total,sizeof total,"%d%s",q->total,q->done?"":"+");
            snprintf(recovery,sizeof recovery,"%d%s",q->recovery,q->done?"":"+");
            int used=0; active[0]=0;
            for(int i=0;i<q->hits && i<FM_MAXW && used<(int)sizeof active-24;++i)
                used+=snprintf(active+used,sizeof active-used,"%s%d-%d",i?",":"",q->ws[i]+1,q->we[i]>q->ws[i]?q->we[i]:q->ws[i]+1);
        }
        // Separate header above both pip rows. Top actor's active windows are 1-based.
        // Independent snapshots cannot measure advantage.
        snprintf(text,sizeof text,"Startup %s  Active %s  Recovery %s  Total %s  Advantage --",start,active,recovery,total);
        int bw=cw*FM_DISPLAY_CELLS,gutter=lineH*3,x=(w-bw-gutter)/2+gutter;
        if(ring) {
            const int limit=cw*FM_DISPLAY_CELLS+lineH*3;
            while(g_fm_fontPx>9 && (int)strlen(text)*N3StrokeWidth(g_fm_fontPx)>limit) --g_fm_fontPx;
            const int textW=(int)strlen(text)*N3StrokeWidth(g_fm_fontPx);
            int need=textW+pad*4; if(need>limit+pad*2)need=limit+pad*2;
            if(panelW<need)panelW=need;
            panelLeft=(w-panelW)/2; x=(w-textW)/2;
        }
        IntersectClipRect(dc,panelLeft,0,panelLeft+panelW,h);
        FmFill(dc,panelLeft,0,panelW,oy,RGB(16,21,30));
        FmLabel(dc,x,0,text,RGB(245,248,255));
    }
    IntersectClipRect(dc,panelLeft,0,panelLeft+panelW,h);
    if(ring) { oy+=FmRings(dc,w,H,oy,panelW,&mp,&m,&ki); }
    else {
    if(FmPlayerDraw()) { FmPanel(dc,w,h,H,oy,&mp,"PLAYER","PLAYER",1,!g_pst.v[PS_FRAME_IFR]); oy+=ph; }
    if(FmIFrRow()) {  // gold cells = the same no-damage marks the stripe used, in their own lane aligned with the PLAYER row
        int used=FM_CELLS; while(used>0 && (FmViewCell(&mp,used-1)&0x7F)==FM_EMPTY) --used;
        const int start=used>FM_DISPLAY_CELLS?used-FM_DISPLAY_CELLS:0;
        const int bw=cw*FM_DISPLAY_CELLS,gutter=lineH*3,x=(w-bw-gutter)/2+gutter,by=oy+pad;
        FmFill(dc,x-gutter-pad,oy,bw+gutter+pad*2,ph,RGB(16,21,30));
        SetBkMode(dc,TRANSPARENT);
        FmLabel(dc,x-gutter,by,"I-FR",RGB(240,243,249));
        FmFill(dc,x-1,by-1,bw+2,barH+2,RGB(41,50,65));
        for(int i=0;i<FM_DISPLAY_CELLS;++i) FmFill(dc,x+i*cw,by,cw>3?cw-2:cw>1?cw-1:cw,barH,FmIFrColor(FmViewNoDmg(&mp,start+i)));
        oy+=ph;
    }
    if(g_pst.v[PS_FRAME_METER] && g_fm_show) { FmPanel(dc,w,h,H,oy,&m,who,"BOSS",0,0); oy+=ph; }
    if(showKi) {
        int used=FM_CELLS; while(used>0 && (FmViewCell(&m,used-1)&0x7F)==FM_EMPTY) --used;
        const int start=used>FM_DISPLAY_CELLS?used-FM_DISPLAY_CELLS:0;
        const int bw=cw*FM_DISPLAY_CELLS,gutter=lineH*3,x=(w-bw-gutter)/2+gutter,by=oy+pad;
        FmFill(dc,x-gutter-pad,oy,bw+gutter+pad*2,ph,RGB(16,21,30));
        SetBkMode(dc,TRANSPARENT);
        FmLabel(dc,x-gutter,by,g_fm_ki_label,RGB(240,243,249));
        FmFill(dc,x-1,by-1,bw+2,barH+2,RGB(41,50,65));
        for(int i=0;i<FM_DISPLAY_CELLS;++i) FmFill(dc,x+i*cw,by,cw>3?cw-2:cw>1?cw-1:cw,barH,FmKiView(&ki,&m,start+i)?RGB(138,92,255):FmColor(FM_EMPTY));
        oy+=ph;
    }
    } // Row style
    if(!g_pst.v[PS_FRAME_STYLE]) FmLegend(dc,w,H,oy);
    FmRoundCorners(dc,panelLeft,0,panelW,oy+FmLegendHeight(w,H),FmRadius(H));
    RestoreDC(dc,panelClip);
}

#define FM_BG_ALPHA 0x73
static void FmFinalizeAlpha(uint32_t* px,int w,int h) {
        const uint32_t key = 0x00FF00FFu;  // FM_KEY RGB(255,0,255) as 0x00RRGGBB
        const uint32_t bg = 0x0010151Eu;   // RGB(16,21,30), the panel fill
        for (size_t i = 0, n = (size_t)w * (size_t)h; i < n; ++i) {
            const uint32_t c = px[i] & 0x00FFFFFFu;
            if (px[i] >> 24) continue; // AA text already composited in premultiplied linear coverage.
            if (c == key) px[i] = 0;
            else if (c == bg) {  // premultiplied
                const uint32_t rr = (((c >> 16) & 255) * FM_BG_ALPHA + 127) / 255, gg = (((c >> 8) & 255) * FM_BG_ALPHA + 127) / 255, bb = ((c & 255) * FM_BG_ALPHA + 127) / 255;
                px[i] = ((uint32_t)FM_BG_ALPHA << 24) | (rr << 16) | (gg << 8) | bb;
            } else px[i] = c | 0xFF000000u;
        }
}
