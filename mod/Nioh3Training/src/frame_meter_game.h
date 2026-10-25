// Practice frame meter (SF6 style), game side. Included by training.c after frame_probe_game.h. State machine: frame_meter.h (tests/frame_meter_test.c).
// Input: the FRAMEPROBE signals only (frame_probe_game.h): the boss action id seen by the pump, open hit windows (attack collision still in its pool
// slot) and hits created since the last tick, with the GreatAttack bit (hit +0x13C bit 10 = unblockable, notes/yoritomo-hit-source-22305.md).
// No game memory is read or written beyond what the probe already does. Generic: the first live Training boss slot, no per-boss table.
// Clock: GAME time (user #22575): the game's own step per update tick from FpGameStep (frame_probe_game.h), so one cell is 1/60 s of game time at
// any frame rate; it also stops while the mod menu pauses the game (MENU PAUSE), the F9 freeze or a load.
// Drawing: a click-through topmost layered GDI window over the game (the same fallback renderer as the Rematch standalone menu and the fade window),
// so no Present hook and no other ASI is involved. Hidden while the mod menu is open, the game is not the foreground window or no data yet.
// Ini (Nioh3Training.ini [Practice]): FrameMeter = 0/1 (default 0, the Practice hub row 'Frame meter'), FrameMeterY = top of the meter in % of the
// screen height; -1 (default) anchors above the lower player HUD, slightly left.
#include "frame_meter.h"
#include "frame_meter_ki.h"
#include "frame_meter_position.h"
static __attribute__((noinline)) int NkmRead(uint64_t a,float* cur,float* mx,float* k60,int* dead);
static int FmHumanBoss(uint64_t actor,uint32_t chr); // definition after automatic binding, no per-boss list
static const char* const g_fm_ki_label="KI DEBT";
static FmKi g_fm_ki; static volatile LONG g_fm_ki_show;
static volatile LONG g_fz_hold, g_fz_over, g_fz_frames, g_fz_steps; static int g_fz_vk_freeze, g_fz_vk_step;  // practice_freeze_game.h (Practice freeze + frame advance)
static const char* FzKeyName(int vk, char* b, int n);
static FmMeter g_fm, g_fmp; static volatile LONG g_fmp_show; static SRWLOCK g_fm_lock = SRWLOCK_INIT;
static volatile LONG g_fm_ver, g_fm_show, g_fm_thread; static int g_fm_slot = -1; static double g_fm_clock, g_fm_last; static int g_cfg_fm_y = -1;
static HWND g_fm_wnd, g_fm_game; static uint64_t g_fm_actor; static uint32_t g_fm_chr; static char g_fm_who[48] = "Boss A";
// Label of the followed boss: "Boss B - <name>" (name from the arena table row of its character id, when there is one).
static void FmWho(int slot, uint32_t chr, char* out, int n) {
    const char* nm = 0;
    for (int i = 0; i < g_nrows && chr; ++i) if (g_rows[i].chr == chr && g_rows[i].name[0]) { nm = g_rows[i].name; break; }
    if (nm) snprintf(out, n, "Boss %c - %s", 'A' + slot, nm); else snprintf(out, n, "Boss %c", 'A' + slot);
}
static DWORD WINAPI FmThread(LPVOID p);
static void FmLoadIni(const char* ini) {
    g_cfg_fm_y = (int)GetPrivateProfileIntA("Practice", "FrameMeterY", -1, ini); if (g_cfg_fm_y < -1 || g_cfg_fm_y > 90) g_cfg_fm_y = -1;
    Log("FRAME METER: [Practice] FrameMeter %d, PlayerFrameMeter %d, KiWindow %d, FrameMeterY %d (-1 = above player HUD; otherwise top percent)", g_pst.v[PS_FRAME_METER], g_pst.v[PS_PLAYER_METER], g_pst.v[PS_KI_WINDOW], g_cfg_fm_y);
}
// Player KiTick K+0x74 (status = [actor+0x3A0], K = status+0x98). Read only. 1 while the 30-tick window is counting down.
static __attribute__((noinline)) int FmPlayerKiOpen(uint64_t actor) {
    uint64_t st = 0; float kt = -1.f;
    if (!actor || !Rd64((uintptr_t)actor + 0x3A0, &st) || !st) return 0;
    __try { kt = *(volatile float*)(st + 0x98 + 0x74); } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return FmKiWindowOpen(kt);
}
// KiTick +0x5C and current Ki. Read only. 1 on the tick a pulse lands (see FmKiPulseLanded).
static __attribute__((noinline)) int FmPlayerKiLanded(uint64_t actor) {
    static uint64_t who = 0;
    static int have = 0;
    static float prevKt = 0.f, prevKi = 0.f;
    uint64_t st = 0;
    float kt = 0.f, ki = 0.f;
    int landed = 0;
    if (!actor || actor != who) { who = actor; have = 0; }
    if (!actor || !Rd64((uintptr_t)actor + 0x3A0, &st) || !st) { have = 0; return 0; }
    __try {
        kt = *(volatile float*)(st + 0x98 + 0x5C);
        ki = *(volatile float*)(st + 0xAC);
    } __except (EXCEPTION_EXECUTE_HANDLER) { have = 0; return 0; }
    if (have) landed = FmKiPulseLanded(prevKt, kt, prevKi, ki);
    prevKt = kt; prevKi = ki; have = 1;
    return landed;
}
// FpTick (game thread, FpLock held), once per pump tick.
static void FmFeed(double gdt) {
    const int wantB = g_pst.v[PS_FRAME_METER], wantP = g_pst.v[PS_PLAYER_METER] || g_pst.v[PS_KI_WINDOW];
    if ((!wantB && !wantP) || !g_pst_live) { g_fm_show = 0; g_fmp_show = 0; g_fm_ki_show = 0; g_fm_slot = -1; g_fm_last = 0; return; }
    if (!g_fm_thread && InterlockedExchange(&g_fm_thread, 1) == 0) { HANDLE t = CreateThread(0, 0, FmThread, 0, 0, 0); if (t) CloseHandle(t); else Log("FRAME METER: draw thread failed (%lu)", GetLastError()); }
    double dt = gdt;
    if (dt < 0) dt = 0; if (dt > 150) dt = 150;
    const uint8_t* gs = GameState();
    if (((g_menu_frozen || g_dbg_frozen) && !g_fz_over) || (gs && gs[2])) dt = 0;  // a Practice frame step runs one frame through every stop request
    g_fm_clock += dt;
    // player bar (user #22796): same sampler, same clock, fed from the player's g_fp entry
    if (!wantP) g_fmp_show = 0;
    else if (g_fp[FP_PL].valid) {
        int active = g_fp[FP_PL].pulse, ub = g_fp[FP_PL].pulseUb, grab = g_fp[FP_PL].pulseGrab;
        for (int w = 0; w < FP_WIN; ++w) if (g_fpw[w].used && g_fpw[w].slot == FP_PL && g_fpw[w].act == g_fp[FP_PL].run.act) { active = 1; if (g_fpw[w].great) ub = 1; if (g_fpw[w].grab) grab = 1; }
        AcquireSRWLockExclusive(&g_fm_lock);
        FmSampleEx(&g_fmp, g_fm_clock, g_fp[FP_PL].run.act, active, ub, grab);
        if (g_pst.v[PS_KI_WINDOW]) {
            FmMarkKiWin(&g_fmp, g_fm_clock, FmPlayerKiOpen(g_fp[FP_PL].a));
            FmMarkKiPulse(&g_fmp, g_fm_clock, FmPlayerKiLanded(g_fp[FP_PL].a));
        }
        ReleaseSRWLockExclusive(&g_fm_lock);
        g_fmp_show = 1;
    }
    g_fp[FP_PL].pulse = g_fp[FP_PL].pulseUb = g_fp[FP_PL].pulseGrab = 0;
    if (!wantB) { g_fm_ki_show = 0; g_fm_show = 0; g_fm_slot = -1; for (int i = 0; i < N3T_SLOTS; ++i) g_fp[i].pulse = g_fp[i].pulseUb = g_fp[i].pulseGrab = 0; InterlockedIncrement(&g_fm_ver); return; }
    int valid[N3T_SLOTS], busy[N3T_SLOTS];
    for (int i = 0; i < N3T_SLOTS; ++i) {
        valid[i] = g_fp[i].valid; busy[i] = g_fp[i].valid && (g_fp[i].pulse || g_fp[i].pulseUb);
        for (int w = 0; w < FP_WIN && valid[i] && !busy[i]; ++w) if (g_fpw[w].used && g_fpw[w].slot == i && g_fpw[w].act == g_fp[i].run.act) busy[i] = 1;
    }
    const int s = FmPickSlot(g_fm_slot, N3T_SLOTS, valid, busy);  // user #22918: the boss that is attacking, not always the first slot
    if (s < 0) { g_fm_ki_show = 0; g_fm_slot=-1; for (int i = 0; i < N3T_SLOTS; ++i) g_fp[i].pulse = g_fp[i].pulseUb = g_fp[i].pulseGrab = 0; InterlockedIncrement(&g_fm_ver); return; }  // no boss: the last picture stays
    int active = g_fp[s].pulse, ub = g_fp[s].pulseUb, grab = g_fp[s].pulseGrab;
    for (int i = 0; i < N3T_SLOTS; ++i) g_fp[i].pulse = g_fp[i].pulseUb = g_fp[i].pulseGrab = 0;
    // an open window of this boss is active only while it belongs to the CURRENT action: a window opened by the previous action (its hitbox still
    // open when the action id changes) is closed for the meter, so an idle/move action after an attack gets no fake active/recovery run
    for (int w = 0; w < FP_WIN; ++w) if (g_fpw[w].used && g_fpw[w].slot == s && g_fpw[w].act == g_fp[s].run.act) { active = 1; if (g_fpw[w].great) ub = 1; if (g_fpw[w].grab) grab = 1; }
    AcquireSRWLockExclusive(&g_fm_lock);
    if (s != g_fm_slot || g_fp[s].a != g_fm_actor || g_fp[s].chr != g_fm_chr) {  // another slot, or a new boss (retry / respawn / swap) in the same slot: fresh meter
        FmReset(&g_fm); FmKiReset(&g_fm_ki); g_fm_ki_show=0; g_fm_slot = s; g_fm_actor = g_fp[s].a; g_fm_chr=g_fp[s].chr;
        FmWho(s, g_fp[s].chr, g_fm_who, sizeof g_fm_who);
        Log("FRAME METER: following %s (chr %u)", g_fm_who, g_fp[s].chr);
    }
    float kiCur=0,kiMax=0,kiSecondary=0; int kiDead=0;
    const int kiRead=g_pst.v[PS_FRAME_KI] && NkmRead(g_fp[s].a,&kiCur,&kiMax,&kiSecondary,&kiDead);
    const int kiNegative=kiRead && !kiDead && NkmNeg(kiCur,kiSecondary,0)<0.0f;
    g_fm_ki_show=FmKiEligible(g_pst.v[PS_FRAME_KI],FmHumanBoss(g_fp[s].a,g_fp[s].chr),kiRead,kiDead,kiCur,kiSecondary);
    FmKiSample(&g_fm_ki,&g_fm,g_fm_clock,g_fp[s].run.act,kiNegative);
    FmSampleEx(&g_fm, g_fm_clock, g_fp[s].run.act, active, ub, grab);
    ReleaseSRWLockExclusive(&g_fm_lock);
    InterlockedIncrement(&g_fm_ver); g_fm_show = 1;
}
// ---- drawing (own thread) ----
#include "frame_meter_count.h"
#define FM_DISPLAY_CELLS 120 // SF6-sized pips; retain the full 360-cell sampler history.
#define FM_KEY RGB(255, 0, 255)
static HFONT g_fm_font; static int g_fm_fontPx;
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
    *lineH = (int)(20*k); if (*lineH<15) *lineH=15;
    *pad = (int)(2*k); if (*pad<2) *pad=2;
    *cw = FmCellWidth(W,*lineH*3,*pad,FM_DISPLAY_CELLS);
    *barH = (int)(14*k); if (*barH<10) *barH=10;
    return *pad*2 + *barH;
}
static int FmReadoutHeight(int barH, int pad) { return g_pst.v[PS_FRAME_READOUT] ? barH+pad : 0; }
static int FmKiShown(void) { return g_pst.v[PS_FRAME_KI] && g_pst.v[PS_FRAME_METER] && g_fm_show && g_fm_ki_show; }
static int FmPlayerRow(void) { return (g_pst.v[PS_PLAYER_METER] || g_pst.v[PS_KI_WINDOW]) && g_fmp_show; }
static int FmPanels(void) { return (g_pst.v[PS_FRAME_METER] && g_fm_show ? 1 : 0) + (FmPlayerRow() ? 1 : 0) + FmKiShown(); }
#define FM_KIWIN_RGB RGB(16,44,168)
static void FmLabel(HDC dc, int x, int y, const char* text, COLORREF color) {
    SetTextColor(dc,color); TextOutA(dc,x,y,text,(int)strlen(text));
}
static void FmCheck(HDC dc, int x, int y, int w, int h) {
    const int thick = h / 5 > 2 ? h / 5 : 2;
    const int x0 = x + w / 5, y0 = y + h / 2, x1 = x + w / 2, y1 = y + (h * 4) / 5, x2 = x + (w * 4) / 5, y2 = y + h / 5;
    HPEN dark = CreatePen(PS_SOLID, thick + 2, RGB(8, 10, 16));
    HPEN light = CreatePen(PS_SOLID, thick, RGB(245, 248, 255));
    HGDIOBJ old = SelectObject(dc, dark);
    MoveToEx(dc, x0, y0, 0); LineTo(dc, x1, y1); LineTo(dc, x2, y2);
    SelectObject(dc, light);
    MoveToEx(dc, x0, y0, 0); LineTo(dc, x1, y1); LineTo(dc, x2, y2);
    SelectObject(dc, old); DeleteObject(dark); DeleteObject(light);
}
static void FmPanel(HDC dc, int w, int h, int H, int oy, const FmMeter* mp, const char* who, const char* side, int kiStripe) {
    const FmMeter m=*mp;
    int cw,barH,lineH,pad; const int ph=FmGeom(w,H,&cw,&barH,&lineH,&pad); (void)h;
    const int px=barH-2>9?barH-2:9;
    if (!g_fm_font || g_fm_fontPx!=px) {
        if(g_fm_font) DeleteObject(g_fm_font);
        g_fm_fontPx=px; g_fm_font=CreateFontW(-px,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
    }
    int used=FM_CELLS;
    while(used>0 && (FmViewCell(&m,used-1)&0x7F)==FM_EMPTY) --used;
    const int viewStart=used>FM_DISPLAY_CELLS?used-FM_DISPLAY_CELLS:0;
    const int barW=cw*FM_DISPLAY_CELLS,gutter=lineH*3,x0=(w-barW-gutter)/2+gutter,by=oy+pad;
    FmFill(dc,x0-gutter-pad,oy,barW+gutter+pad*2,ph,RGB(16,21,30));
    HGDIOBJ of=SelectObject(dc,g_fm_font); SetBkMode(dc,TRANSPARENT);
    // Clip long actor names/freeze hints to the strip instead of spilling outside the panel.
    int saved=SaveDC(dc); IntersectClipRect(dc,x0-gutter,oy,x0+barW,oy+ph);
    (void)who; // Compact default has no actor/title line.
    FmLabel(dc,x0-gutter,by,side,RGB(240,243,249));
    FmFill(dc,x0-1,by-1,barW+2,barH+2,RGB(41,50,65));
    for(int i=0;i<FM_DISPLAY_CELLS;++i) {
        const uint8_t c=FmViewCell(&m,viewStart+i);
        const int win=kiStripe && FmViewKiWin(&m,viewStart+i);
        FmFill(dc,x0+i*cw,by,cw>3?cw-2:cw>1?cw-1:cw,barH,win?FM_KIWIN_RGB:FmColor(c&0x7F));
        if(!win && (c&0x7F)==FM_ACTIVE_UB) FmFill(dc,x0+i*cw,by+barH/2,cw>3?cw-2:1,1,RGB(255,210,216));
        if(c&FM_EDGE) FmFill(dc,x0+i*cw,by-2,1,barH+4,RGB(235,240,248));
    }
    // Count each visible colour run; action boundaries and the Ki window also start a new run.
    // Centre within its LAST coloured cell, excluding the separator.
    for(int first=0;first<FM_CELLS;) {
        const uint8_t st=FmViewCell(&m,first)&0x7F;
        const int win=kiStripe && FmViewKiWin(&m,first);
        int end=first+1;
        while(end<FM_CELLS) {
            const uint8_t next=FmViewCell(&m,end);
            const int nwin=kiStripe && FmViewKiWin(&m,end);
            if((next&0x7F)!=st || (next&FM_EDGE) || nwin!=win) break;
            ++end;
        }
        if(st!=FM_EMPTY && end>viewStart && end<=viewStart+FM_DISPLAY_CELLS) {
            char n[12]; snprintf(n,sizeof n,"%d",end-first);
            const int left=x0+(end-viewStart-1)*cw;
            const int fillW=cw>3?cw-2:cw>1?cw-1:cw;
            RECT cell={left,by,left+fillW,by+barH},textBox;
            HFONT font=FmFitCount(dc,n,cell,px,&textBox);
            if(font) {
                HGDIOBJ normal=SelectObject(dc,font);
                int clip=SaveDC(dc); IntersectClipRect(dc,cell.left,cell.top,cell.right,cell.bottom);
                for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx)
                    if(dx || dy) FmLabel(dc,textBox.left+dx,textBox.top+dy,n,RGB(2,3,5));
                FmLabel(dc,textBox.left,textBox.top,n,
                    RGB(255,255,255));
                RestoreDC(dc,clip); SelectObject(dc,normal); DeleteObject(font);
            }
        }
        first=end;
    }
    if(kiStripe) {
        for(int i=0;i<FM_DISPLAY_CELLS;++i) if(FmViewKiPulse(&m,viewStart+i))
            FmCheck(dc,x0+i*cw,by,cw>3?cw-2:cw,barH);
    }
    RestoreDC(dc,saved); SelectObject(dc,of);
}
// Shared legend below both rows; wraps at smaller resolutions.
static int FmLegendCount(void) { return 11 + (g_pst.v[PS_KI_WINDOW] ? 1 : 0) + FmKiShown(); }
static int FmLegendHeight(int w,int H) {
    if(!g_pst.v[PS_FRAME_LEGEND]) return 0;
    int cw,bh,lh,pad; FmGeom(w,H,&cw,&bh,&lh,&pad);
    const int n=FmLegendCount();
    int cols=cw*FM_DISPLAY_CELLS/(lh*6); if(cols<1) cols=1; if(cols>n) cols=n;
    return ((n+cols-1)/cols)*lh+pad*2;
}
static void FmLegend(HDC dc,int w,int H,int y) {
    if(!g_pst.v[PS_FRAME_LEGEND]) return;
    int cw,bh,lh,pad; FmGeom(w,H,&cw,&bh,&lh,&pad);
    const int bw=cw*FM_DISPLAY_CELLS,gutter=lh*3,x=(w-bw-gutter)/2+gutter;
    const char* names[]={"Startup","Active","Recovery","Unblockable","Grab","No hit","Pending","Empty","Action start","Total frames","Stun/adv --","Ki Pulse","Ki debt"};
    COLORREF colors[]={FmColor(FM_STARTUP),FmColor(FM_ACTIVE),FmColor(FM_RECOVERY),FmColor(FM_ACTIVE_UB),FmColor(FM_GRAB),FmColor(FM_NEUTRAL),FmColor(FM_PENDING),FmColor(FM_EMPTY),RGB(235,240,248),RGB(214,222,234),RGB(145,159,181),FM_KIWIN_RGB,RGB(138,92,255)};
    int use[13]; for(int i=0;i<11;++i) use[i]=1; use[11]=g_pst.v[PS_KI_WINDOW]?1:0; use[12]=FmKiShown()?1:0;
    int idx[13], n=0; for(int i=0;i<13;++i) if(use[i]) idx[n++]=i;
    int cols=bw/(lh*6); if(cols<1) cols=1; if(cols>n) cols=n;
    const int pitch=bw/cols;
    FmFill(dc,x-gutter-pad,y,bw+gutter+pad*2,FmLegendHeight(w,H),RGB(16,21,30));
    HGDIOBJ old=SelectObject(dc,g_fm_font); SetBkMode(dc,TRANSPARENT);
    FmLabel(dc,x-gutter,y+pad,"KEY",RGB(145,159,181));
    for(int i=0;i<n;++i) {
        const int xx=x+(i%cols)*pitch,yy=y+pad+(i/cols)*lh;
        FmFill(dc,xx,yy+3,8,8,colors[idx[i]]); FmLabel(dc,xx+12,yy,names[idx[i]],RGB(183,194,211));
    }
    SelectObject(dc,old);
}
static void FmPaint(HDC dc, int w, int h, int H) {
    static FmMeter m,mp; static FmKi ki; int showKi; char who[48]; AcquireSRWLockShared(&g_fm_lock);
    m=g_fm; mp=g_fmp; ki=g_fm_ki; showKi=FmKiShown(); snprintf(who,sizeof who,"%s",g_fm_who); ReleaseSRWLockShared(&g_fm_lock);
    FmFill(dc,0,0,w,h,FM_KEY);
    int cw,barH,lineH,pad; const int ph=FmGeom(w,H,&cw,&barH,&lineH,&pad);
    // Clip long readouts/key labels as well as cells to the centered panel bounds.
    const int panelW=cw*FM_DISPLAY_CELLS+lineH*3+pad*2;
    const int panelLeft=(w-cw*FM_DISPLAY_CELLS-lineH*3)/2-pad;
    const int panelClip=SaveDC(dc);
    IntersectClipRect(dc,panelLeft,0,panelLeft+panelW,h);
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
        FmFill(dc,x-gutter-pad,0,bw+gutter+pad*2,oy,RGB(16,21,30));
        HFONT font=CreateFontW(-(barH-2),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
        HGDIOBJ old=SelectObject(dc,font); SetBkMode(dc,TRANSPARENT);
        FmLabel(dc,x,0,text,RGB(245,248,255)); SelectObject(dc,old); DeleteObject(font);
    }
    if(FmPlayerRow()) { FmPanel(dc,w,h,H,oy,&mp,"PLAYER","PLAYER",g_pst.v[PS_KI_WINDOW]); oy+=ph; }
    if(g_pst.v[PS_FRAME_METER] && g_fm_show) { FmPanel(dc,w,h,H,oy,&m,who,"BOSS",0); oy+=ph; }
    if(showKi) {
        int used=FM_CELLS; while(used>0 && (FmViewCell(&m,used-1)&0x7F)==FM_EMPTY) --used;
        const int start=used>FM_DISPLAY_CELLS?used-FM_DISPLAY_CELLS:0;
        const int bw=cw*FM_DISPLAY_CELLS,gutter=lineH*3,x=(w-bw-gutter)/2+gutter,by=oy+pad;
        FmFill(dc,x-gutter-pad,oy,bw+gutter+pad*2,ph,RGB(16,21,30));
        HGDIOBJ old=SelectObject(dc,g_fm_font); SetBkMode(dc,TRANSPARENT);
        FmLabel(dc,x-gutter,by,g_fm_ki_label,RGB(240,243,249));
        FmFill(dc,x-1,by-1,bw+2,barH+2,RGB(41,50,65));
        for(int i=0;i<FM_DISPLAY_CELLS;++i) FmFill(dc,x+i*cw,by,cw>3?cw-2:cw>1?cw-1:cw,barH,FmKiView(&ki,&m,start+i)?RGB(138,92,255):FmColor(FM_EMPTY));
        SelectObject(dc,old); oy+=ph;
    }
    FmLegend(dc,w,H,oy);
    RestoreDC(dc,panelClip);
}
static BOOL CALLBACK FmFind(HWND h, LPARAM lp) { (void)lp;
    DWORD pid; GetWindowThreadProcessId(h, &pid); if (pid != GetCurrentProcessId() || !IsWindowVisible(h) || h == g_fm_wnd || h == g_fd_wnd || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r; GetClientRect(h, &r); if (r.right < 400 || r.bottom < 300) return TRUE;
    g_fm_game = h; return FALSE;
}
static int g_fm_H;  // game client height (font / bar scale)
static LRESULT CALLBACK FmProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT r; GetClientRect(h, &r);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, r.right, r.bottom); HGDIOBJ ob = SelectObject(mem, bmp);
        __try { FmPaint(mem, r.right, r.bottom, g_fm_H); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
static DWORD WINAPI FmThread(LPVOID p) {
    (void)p;
    WNDCLASSEXW wc = {sizeof wc}; wc.lpfnWndProc = FmProc; wc.hInstance = GetModuleHandleW(0); wc.lpszClassName = L"Nioh3TrainingFrameMeter";
    RegisterClassExW(&wc);
    g_fm_wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP, 0, 0, 100, 100, 0, 0, wc.hInstance, 0);
    if (!g_fm_wnd) { Log("FRAME METER: overlay window failed (%lu), meter not drawn", GetLastError()); return 0; }
    SetLayeredWindowAttributes(g_fm_wnd, FM_KEY, 235, LWA_COLORKEY | LWA_ALPHA);
    Log("FRAME METER: overlay window ready");
    int shown = 0; LONG lastVer = -1; RECT last = {0};
    for (;;) {
        Sleep(33);
        MSG msg; while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        if (!g_fm_game || !IsWindow(g_fm_game)) { g_fm_game = 0; EnumWindows(FmFind, 0); }
        const int want = (g_pst.v[PS_FRAME_METER] || g_pst.v[PS_PLAYER_METER] || g_pst.v[PS_KI_WINDOW]) && g_pst_live && FmPanels() && g_fm_game && !IsIconic(g_fm_game) && GetForegroundWindow() == g_fm_game && !GateMenuOpen();
        if (!want) { if (shown) { ShowWindow(g_fm_wnd, SW_HIDE); shown = 0; } continue; }
        RECT cr; GetClientRect(g_fm_game, &cr); POINT o = {0, 0}; ClientToScreen(g_fm_game, &o);
        int cw, barH, lineH, pad; g_fm_H = cr.bottom;
        const int rowH = FmGeom(cr.right, cr.bottom, &cw, &barH, &lineH, &pad);
        const int hh = rowH * FmPanels() + FmReadoutHeight(barH,pad) + FmLegendHeight(cr.right,cr.bottom);
        const int top = o.y + FmPositionY(cr.bottom, hh, g_cfg_fm_y);
        const int left = o.x + (g_cfg_fm_y < 0 ? FmPositionX(cr.right) : 0);
        RECT want_r = {left, top, left + cr.right, top + hh};
        if (!shown || memcmp(&want_r, &last, sizeof last)) {
            SetWindowPos(g_fm_wnd, HWND_TOPMOST, want_r.left, want_r.top, want_r.right - want_r.left, hh, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            last = want_r; shown = 1; lastVer = -1;
        }
        const LONG v = g_fm_ver; if (v != lastVer) { lastVer = v; InvalidateRect(g_fm_wnd, 0, FALSE); }
    }
}
