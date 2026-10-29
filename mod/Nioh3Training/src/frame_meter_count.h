// GDI stroke-count placement, shared by the overlay and its offline test.
#pragma once
#include "../../shared/gdi_stroke_text.h"
typedef struct { int px,advance; } N3CountFont;
static N3CountFont FmFitCount(HDC dc,const char* text,RECT cell,int maxPx,RECT* ink) {
    N3CountFont font={0,0}; (void)dc;
    const int w=cell.right-cell.left,h=cell.bottom-cell.top,n=(int)strlen(text);
    if(w<1 || h<1 || n<1 || maxPx<1) return font;
    font.px=maxPx<h?maxPx:h;
    font.advance=N3StrokeWidth(font.px);
    if(font.advance>w/n) font.advance=w/n;
    if(font.advance<1) { font.px=0; return font; }
    const int width=n*font.advance;
    ink->left=cell.left+(w-width)/2; ink->right=ink->left+width;
    ink->top=cell.top+(h-font.px)/2; ink->bottom=ink->top+font.px;
    return font;
}
