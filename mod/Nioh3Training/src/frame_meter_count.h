// GDI-only count placement, shared by the overlay and its offline Windows test.
#pragma once
#include <windows.h>
#include <string.h>
static HFONT FmFitCount(HDC dc, const char* text, RECT cell, int maxPx, RECT* ink) {
    const int w=cell.right-cell.left,h=cell.bottom-cell.top;
    if(w<1 || h<1) return NULL;
    for(int px=maxPx;px>=1;--px) {
        const int chars=(int)strlen(text);
        const int width=chars>0 ? w/chars : 0;
        HFONT font=CreateFontW(-px,width,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
        if(!font) continue;
        HGDIOBJ old=SelectObject(dc,font); SIZE z={0};
        const BOOL measured=GetTextExtentPoint32A(dc,text,(int)strlen(text),&z);
        SelectObject(dc,old);
        if(measured && z.cx>0 && z.cy>0 && z.cx<=w && z.cy<=h) {
            ink->left=cell.left+(w-z.cx)/2; ink->top=cell.top+(h-z.cy)/2;
            ink->right=ink->left+z.cx; ink->bottom=ink->top+z.cy;
            return font; // One/two digits share the same selected font size and baseline. Caller draws with this exact font and deletes it afterwards.
        }
        DeleteObject(font);
    }
    return NULL; // Subpixel cells cannot contain readable text; never spill into a neighbour.
}
