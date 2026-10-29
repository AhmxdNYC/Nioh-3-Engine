// Ring labels reuse the shared stroke renderer's exact premultiplied glyph pixels.
// Bounded LRU; no persistent GDI handles, no geometry/font changes, no work in game hooks.
#pragma once
#include <stdlib.h>
typedef struct { char text[320]; int px,w,h; COLORREF color; uint64_t stamp; uint32_t* bits; } FmTextTile;
static FmTextTile g_fm_textTiles[64];
static uint64_t g_fm_textStamp;
static int FmCachedLabel(HDC dc,int x,int y,const char* text,int px,COLORREF color) {
    const size_t n=strlen(text);
    if(!n)return 1;
    if(n>=sizeof g_fm_textTiles[0].text || px<=0 || px>128 || !n3_stroke_layered)return 0;
    DIBSECTION dst={0};RECT clip;
    const int clipKind=GetClipBox(dc,&clip);
    if(clipKind==NULLREGION)return 1;
    if(clipKind!=SIMPLEREGION || GetObject(GetCurrentObject(dc,OBJ_BITMAP),sizeof dst,&dst)!=sizeof dst
        || !dst.dsBm.bmBits || dst.dsBm.bmBitsPixel!=32)return 0;
    FmTextTile* tile=0,*victim=&g_fm_textTiles[0];
    for(int i=0;i<64;++i) {
        FmTextTile* t=&g_fm_textTiles[i];
        if(t->bits && t->px==px && t->color==color && !strcmp(t->text,text)){tile=t;break;}
        if(t->stamp<victim->stamp)victim=t;
    }
    if(!tile) {
        const int w=(int)n*N3StrokeWidth(px),h=px;
        HDC source=CreateCompatibleDC(dc);if(!source)return 0;
        BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;
        bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
        uint32_t* pixels=0;HBITMAP bm=CreateDIBSection(source,&bi,DIB_RGB_COLORS,(void**)&pixels,0,0);
        uint32_t* copy=(uint32_t*)malloc((size_t)w*h*4);
        if(!bm || !pixels || !copy){if(bm)DeleteObject(bm);free(copy);DeleteDC(source);return 0;}
        HGDIOBJ old=SelectObject(source,bm);
        for(int i=0;i<w*h;++i)pixels[i]=0x00ff00ff;
        SetTextColor(source,color);N3StrokeText(source,0,0,text,(int)n,px,N3StrokeWidth(px));GdiFlush();
        for(int i=0;i<w*h;++i)copy[i]=pixels[i]==0x00ff00ff?0:pixels[i];
        SelectObject(source,old);DeleteObject(bm);DeleteDC(source);
        tile=victim;free(tile->bits);tile->bits=copy;tile->px=px;tile->w=w;tile->h=h;tile->color=color;
        memcpy(tile->text,text,n+1);
    }
    tile->stamp=++g_fm_textStamp;
    if(clip.left<0)clip.left=0;if(clip.top<0)clip.top=0;
    if(clip.right>dst.dsBm.bmWidth)clip.right=dst.dsBm.bmWidth;
    if(clip.bottom>dst.dsBm.bmHeight)clip.bottom=dst.dsBm.bmHeight;
    // Complete batched GDI fills/lines before touching their destination pixels.
    GdiFlush();
    for(int yy=0;yy<tile->h;++yy)if(y+yy>=clip.top && y+yy<clip.bottom)
        for(int xx=0;xx<tile->w;++xx)if(x+xx>=clip.left && x+xx<clip.right) {
            const uint32_t src=tile->bits[yy*tile->w+xx],a=src>>24;if(!a)continue;
            uint32_t* out=(uint32_t*)((char*)dst.dsBm.bmBits+(y+yy)*dst.dsBm.bmWidthBytes)+x+xx;
            uint32_t v=*out,da=v>>24;
            if(!da) {
                const uint32_t c=v&0xffffff;da=c==0xff00ff?0:c==0x10151e?115:255;
                v=(da<<24)|(((((c>>16)&255)*da+127)/255)<<16)|(((((c>>8)&255)*da+127)/255)<<8)|(((c&255)*da+127)/255);
            }
            uint32_t value=0;
            for(int shift=0;shift<=24;shift+=8)value|=((((src>>shift)&255)+(((v>>shift)&255)*(255-a)+127)/255)<<shift);
            *out=value;
        }
    return 1;
}
