// Shared C/C++ GDI stroke text. Original approved paths, no installed/system font.
// Pixel height is a complete 8x16 cell, including bearings and descender space.
// Reads the DC text colour; 4x supersampling, slightly heavier round-capped strokes.
// The destination DC pen, clipping and current position remain untouched.
#pragma once
#include <windows.h>
#include <string.h>
#ifdef _MSC_VER
#pragma comment(lib, "msimg32.lib")
#endif
#include "gdi_stroke_paths.h"
static inline int N3StrokeWidth(int px) { return px > 1 ? (px+1)/2 : 1; }
static inline SIZE N3StrokeMeasure(int count, int px, int advance) {
    SIZE s = {count > 0 ? count * advance : 0, px}; return s;
}
// Only the frame-meter DIB uses per-pixel alpha; other GDI callers keep normal compositing.
static __declspec(thread) int n3_stroke_layered;
static inline void N3StrokeText(HDC dc,int x,int y,const char* text,int count,int px,int advance) {
    if(!text || count<=0 || count>4096 || px<=0 || px>512 || advance<=0 || advance>512) return;
    const int S=4,w=count*advance,h=px,sw=w*S,sh=h*S;
    HDC hi=CreateCompatibleDC(dc),lo=CreateCompatibleDC(dc);
    if(!hi || !lo) { if(hi)DeleteDC(hi);if(lo)DeleteDC(lo);return; }
    BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=sw;bi.bmiHeader.biHeight=-sh;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    unsigned *high=NULL,*low=NULL;
    HBITMAP hb=CreateDIBSection(hi,&bi,DIB_RGB_COLORS,(void**)&high,NULL,0);
    bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;
    HBITMAP lb=CreateDIBSection(lo,&bi,DIB_RGB_COLORS,(void**)&low,NULL,0);
    if(!hb || !lb) { if(hb)DeleteObject(hb);if(lb)DeleteObject(lb);DeleteDC(hi);DeleteDC(lo);return; }
    HGDIOBJ oh=SelectObject(hi,hb),ol=SelectObject(lo,lb);
    memset(high,0,(size_t)sw*sh*4);
    int thick=(int)(px*1.24f*S/16.f+.5f);if(thick<3)thick=3;
    HPEN pen=CreatePen(PS_SOLID,thick,RGB(255,255,255));
    HGDIOBJ op=SelectObject(hi,pen);
    for(int i=0;i<count;++i) {
        unsigned char ch=(unsigned char)text[i];if(ch<32 || ch>126)ch='?';
        const N3StrokeGlyph g=n3_stroke_glyphs[ch-32];
        for(unsigned j=0;j<g.count;++j) {
            const N3StrokeSegment t=n3_stroke_segments[g.first+j];
            const int ax=i*advance*S+(int)((1.1f+5.8f*t.x0)*advance*S/8.f+.5f);
            const int ay=(int)((2.2f+10.f*t.y0)*px*S/16.f+.5f);
            const int bx=i*advance*S+(int)((1.1f+5.8f*t.x1)*advance*S/8.f+.5f);
            const int by=(int)((2.2f+10.f*t.y1)*px*S/16.f+.5f);
            MoveToEx(hi,ax,ay,NULL);LineTo(hi,bx,by);
        }
    }
    GdiFlush();
    const COLORREF color=GetTextColor(dc);
    const unsigned rr=GetRValue(color),gg=GetGValue(color),bb=GetBValue(color);
    DIBSECTION dst={0};
    // Layered callers create top-down DIBs; GetObject normalizes biHeight to positive.
    const int direct=n3_stroke_layered && GetObject(GetCurrentObject(dc,OBJ_BITMAP),sizeof(dst),&dst)==sizeof(dst)
        && dst.dsBm.bmBits && dst.dsBm.bmBitsPixel==32;
    for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx) {
        unsigned sum=0;
        for(int dy=0;dy<S;++dy)for(int dx=0;dx<S;++dx)sum+=high[(yy*S+dy)*sw+xx*S+dx]&255;
        const unsigned a=(sum+8)/16;
        const unsigned src=(a<<24)|(((rr*a+127)/255)<<16)|(((gg*a+127)/255)<<8)|((bb*a+127)/255);
        low[yy*w+xx]=src;
        if(direct && a && x+xx>=0 && x+xx<dst.dsBm.bmWidth && y+yy>=0 && y+yy<dst.dsBm.bmHeight && PtVisible(dc,x+xx,y+yy)) {
            unsigned* d=(unsigned*)((char*)dst.dsBm.bmBits+(y+yy)*dst.dsBm.bmWidthBytes)+(x+xx);
            unsigned v=*d,da=v>>24;
            if(!da) { // GDI fills have no alpha; reconstruct the established panel/key contract.
                const unsigned c=v&0xffffff;
                da=c==0xff00ff?0:c==0x10151e?115:255;
                v=((da<<24)|(((((v>>16)&255)*da+127)/255)<<16)|(((((v>>8)&255)*da+127)/255)<<8)|(((v&255)*da+127)/255));
            }
            unsigned out=0;
            for(int shift=0;shift<=24;shift+=8)out|=((((src>>shift)&255)+(((v>>shift)&255)*(255-a)+127)/255)<<shift);
            *d=out;
        }
    }
    if(!direct) { BLENDFUNCTION blend={AC_SRC_OVER,0,255,AC_SRC_ALPHA}; AlphaBlend(dc,x,y,w,h,lo,0,0,w,h,blend); }
    SelectObject(hi,op);DeleteObject(pen);SelectObject(hi,oh);SelectObject(lo,ol);
    DeleteObject(hb);DeleteObject(lb);DeleteDC(hi);DeleteDC(lo);
}
// Word-wrap within a caller-provided rectangle, including hard breaks and long words.
static inline void N3StrokeWrapped(HDC dc,const char* text,RECT r,int px,int advance) {
    if(!text || px<=0 || advance<=0 || r.right-r.left<advance) return;
    const int saved=SaveDC(dc); if(!saved) return;
    IntersectClipRect(dc,r.left,r.top,r.right,r.bottom);
    const int cols=(r.right-r.left)/advance;
    while(*text && r.top+px<=r.bottom) {
        int n=0,last=-1;
        while(text[n] && text[n]!='\n' && n<cols) { if(text[n]==' ') last=n; ++n; }
        int used=n;
        if(n==cols && text[n] && text[n]!='\n' && last>0) used=last;
        N3StrokeText(dc,r.left,r.top,text,used,px,advance);
        text+=used; while(*text==' ') ++text; if(*text=='\n') ++text;
        r.top+=px+2;
    }
    RestoreDC(dc,saved);
}
// Paint a color-key GDI scene into a per-pixel-alpha window. AA coverage must not
// be blended against magenta (otherwise color-key presentation leaves fringes).
static inline void N3StrokeWindow(HWND wnd,void (*paint)(HDC,int,int)) {
    RECT r;GetClientRect(wnd,&r);const int w=r.right,h=r.bottom;if(w<=0||h<=0)return;
    HDC scr=GetDC(NULL),dc=CreateCompatibleDC(scr);BITMAPINFO bi={0};unsigned* bits=NULL;
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    HBITMAP bm=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,(void**)&bits,NULL,0);
    if(bm) {
        HGDIOBJ old=SelectObject(dc,bm);n3_stroke_layered=1;paint(dc,w,h);n3_stroke_layered=0;GdiFlush();
        for(size_t i=0;i<(size_t)w*h;++i)if(!(bits[i]>>24))bits[i]=(bits[i]&0xffffff)==0xff00ff?0:bits[i]|0xff000000;
        POINT origin={0,0};SIZE size={w,h};BLENDFUNCTION blend={AC_SRC_OVER,0,235,AC_SRC_ALPHA};
        UpdateLayeredWindow(wnd,scr,NULL,&size,dc,&origin,0,&blend,ULW_ALPHA);
        SelectObject(dc,old);DeleteObject(bm);
    }
    DeleteDC(dc);ReleaseDC(NULL,scr);
}
