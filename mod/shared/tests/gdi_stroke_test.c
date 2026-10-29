#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../gdi_stroke_text.h"
static void SaveBmp(const char* path,const void* bits,int w,int h) {
    BITMAPFILEHEADER f={0}; BITMAPINFOHEADER i={0};
    f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(i);f.bfSize=f.bfOffBits+w*h*4;
    i.biSize=sizeof(i);i.biWidth=w;i.biHeight=-h;i.biPlanes=1;i.biBitCount=32;i.biCompression=BI_RGB;
    FILE* out=fopen(path,"wb"); assert(out);fwrite(&f,sizeof(f),1,out);fwrite(&i,sizeof(i),1,out);fwrite(bits,w*h*4,1,out);fclose(out);
}
int main(int argc,char** argv) {
    setvbuf(stdout,NULL,_IONBF,0);
    enum {W=1000,H=420}; BITMAPINFO bi={0}; void* bits=NULL;
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=W;bi.bmiHeader.biHeight=-H;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(NULL);assert(dc);
    HBITMAP bm=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,NULL,0);assert(bm);
    HGDIOBJ old=SelectObject(dc,bm);unsigned* pixels=(unsigned*)bits;
    SetTextColor(dc,RGB(240,120,60));
    int cases=0;const int sizes[]={12,18,32};
    for(int k=0;k<3;++k) for(int c=32;c<=126;++c) {
        GdiFlush();memset(bits,0,W*H*4);char text[2]={(char)c,0};
        const int px=sizes[k],advance=N3StrokeWidth(px);
        MoveToEx(dc,4,5,NULL);HGDIOBJ pen=GetCurrentObject(dc,OBJ_PEN);
        N3StrokeText(dc,20,20,text,1,px,advance);GdiFlush();
        POINT p;GetCurrentPositionEx(dc,&p);assert(p.x==4&&p.y==5);
        assert(GetCurrentObject(dc,OBJ_PEN)==pen);assert(GetTextColor(dc)==RGB(240,120,60));
        int lit=0;
        for(int y=0;y<H;++y)for(int x=0;x<W;++x) if(pixels[y*W+x]) {
            assert(x>=20&&x<20+advance&&y>=20&&y<20+px);
            assert(((pixels[y*W+x]>>16)&255)<=240 && ((pixels[y*W+x]>>8)&255)<=120 && (pixels[y*W+x]&255)<=60);++lit;
        }
        assert(c==' '?lit==0:lit>0);++cases;
    }
    // Layered text must retain fractional alpha and never bring magenta into the edge.
    GdiFlush();for(int i=0;i<W*H;++i)pixels[i]=0xff00ff;
    n3_stroke_layered=1;SetTextColor(dc,RGB(255,255,255));
    N3StrokeText(dc,20,20,"Smooth",6,18,9);n3_stroke_layered=0;GdiFlush();
    int edges=0;
    for(int i=0;i<W*H;++i)if(pixels[i]>>24) {
        unsigned a=pixels[i]>>24;assert(((pixels[i]>>16)&255)==a && ((pixels[i]>>8)&255)==a && (pixels[i]&255)==a);
        if(a<255)++edges;
    }
    assert(edges>30);
    const DWORD handles=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    for(int i=0;i<1000;++i)N3StrokeText(dc,0,0,"Perf Ki",7,18,9);
    assert(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==handles);
    GdiFlush();for(int i=0;i<W*H;++i)pixels[i]=0x10151e;
    SetTextColor(dc,RGB(245,248,255));
    const char* title="GDI preview: approved angular stroke paths";
    N3StrokeText(dc,20,16,title,(int)strlen(title),28,14);
    const char* lines[]={"PLAYER   BOSS   I-FR   KI DEBT", "Startup 12  Active 13-18  Recovery 20  Total 38  Advantage --", "KEY  Startup  Active  Recovery  Perf Ki  I FR guard", "-Ki 120 res 150 / 200", "Mod menu   Close   Report   Boss Matches   Practice"};
    for(int i=0;i<5;++i) {
        SetTextColor(dc,i==3?RGB(235,110,115):i==2?RGB(183,194,211):RGB(240,243,249));
        N3StrokeText(dc,20,64+i*32,lines[i],(int)strlen(lines[i]),24,12);
    }
    SetTextColor(dc,RGB(220,220,140));
    RECT wrap={20,230,480,290};N3StrokeWrapped(dc,"Standalone footer wraps at words and clips safely within the allotted rectangle.",wrap,18,9);
    SetTextColor(dc,RGB(245,248,255));
    for(int row=0;row<3;++row) { char text[33]={0};for(int j=0;j<32 && 32+row*32+j<=126;++j)text[j]=(char)(32+row*32+j);
      N3StrokeText(dc,20,300+row*32,text,(int)strlen(text),24,12); }
    GdiFlush();if(argc>1)SaveBmp(argv[1],bits,W,H);
    SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);
    printf("PASS: %d glyph/color/clip/DC-state cases; 1000 draws without GDI handle growth\n",cases);
}
