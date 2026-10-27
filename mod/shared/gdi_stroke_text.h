// Shared C/C++ GDI stroke text. Original approved paths, no installed/system font.
// Pixel height is a complete 8x16 cell, including bearings and descender space.
// Reads the DC text colour; restores pen, clipping and current position on return.
#pragma once
#include <windows.h>
#include <string.h>
#include "gdi_stroke_paths.h"
static inline int N3StrokeWidth(int px) { return px > 1 ? (px+1)/2 : 1; }
static inline SIZE N3StrokeMeasure(int count, int px, int advance) {
    SIZE s = {count > 0 ? count * advance : 0, px}; return s;
}
static inline void N3StrokeText(HDC dc,int x,int y,const char* text,int count,int px,int advance) {
    if(!text || count<=0 || px<=0 || advance<=0) return;
    const int saved=SaveDC(dc); if(!saved) return;
    IntersectClipRect(dc,x,y,x+count*advance,y+px);
    int thick=(px*106+800)/1600; if(thick<1) thick=1;
    HPEN pen=CreatePen(PS_SOLID,thick,GetTextColor(dc));
    if(!pen) { RestoreDC(dc,saved); return; }
    SelectObject(dc,pen);
    for(int i=0;i<count;++i) {
        unsigned char ch=(unsigned char)text[i]; if(ch<32 || ch>126) ch='?';
        const N3StrokeGlyph g=n3_stroke_glyphs[ch-32];
        for(unsigned j=0;j<g.count;++j) {
            const N3StrokeSegment s=n3_stroke_segments[g.first+j];
            // Exactly the atlas transform: x=1.1+5.8*u, y=2.2+10*v.
            const int ax=x+i*advance+(int)((1.1f+5.8f*s.x0)*advance/8.f+.5f);
            const int ay=y+(int)((2.2f+10.f*s.y0)*px/16.f+.5f);
            const int bx=x+i*advance+(int)((1.1f+5.8f*s.x1)*advance/8.f+.5f);
            const int by=y+(int)((2.2f+10.f*s.y1)*px/16.f+.5f);
            MoveToEx(dc,ax,ay,NULL); LineTo(dc,bx,by);
            // GDI excludes a line's end pixel. Retain collapsed punctuation at small sizes.
            SetPixelV(dc,bx,by,GetTextColor(dc));
        }
    }
    RestoreDC(dc,saved); DeleteObject(pen);
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
