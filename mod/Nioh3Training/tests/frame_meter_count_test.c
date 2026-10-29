#include <assert.h>
#include <stdio.h>
#include "../src/frame_meter_count.h"
int main(void) {
    HDC dc=CreateCompatibleDC(NULL); assert(dc); int cases=0;
    const int widths[]={1,2,3,4,6,10,14,22,46}, heights[]={10,16,24,48};
    for(int wi=0;wi<9;++wi) for(int hi=0;hi<4;++hi) for(int n=1;n<=360;++n) {
        char text[16]; snprintf(text,sizeof text,"%d",n);
        RECT cell={37,19,37+widths[wi],19+heights[hi]},box;
        N3CountFont font=FmFitCount(dc,text,cell,28,&box);
        if(font.px) {
            SIZE z=N3StrokeMeasure((int)strlen(text),font.px,font.advance);
            assert(box.right-box.left==z.cx && box.bottom-box.top==z.cy);
            assert(box.left>=cell.left && box.right<=cell.right);
            assert(box.top>=cell.top && box.bottom<=cell.bottom);
            int dx=(box.left-cell.left)-(cell.right-box.right);
            int dy=(box.top-cell.top)-(cell.bottom-box.bottom);
            assert(dx>=-1 && dx<=1 && dy>=-1 && dy<=1);
        } else assert(widths[wi]<6); // Only extremely narrow cells may have no fitting font.
        ++cases;
    }
    { RECT cell={0,0,10,14},a,b; N3CountFont fa=FmFitCount(dc,"1",cell,12,&a),fb=FmFitCount(dc,"18",cell,12,&b);
      assert(fa.px&&fb.px&&a.top==b.top&&a.bottom==b.bottom); }
    DeleteDC(dc); printf("frame_meter_count_test: %d stroke-font centering cases PASS\n",cases);
    return 0;
}
