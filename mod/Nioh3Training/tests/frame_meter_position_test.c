#include <assert.h>
#include <stdio.h>
#include "../src/frame_meter_position.h"
int main(void) {
    /* Adding a Ki row leaves the bottom edge fixed above the player HUD. */
    assert(FmPositionY(1080,76,-1)+76 == 877);
    assert(FmPositionY(1080,94,-1)+94 == 877);
    assert(FmPositionY(2160,188,-1)+188 == 1755);
    assert(FmPositionX(1920)==0 && FmPositionX(3840)==0);
    const int widths[]={1280,1600,1920,2560,3440,3840};
    const int heights[]={720,900,1080,1440,1440,2160};
    for(int i=0;i<6;i++) {
        int w=widths[i],h=heights[i],lh=h*20/1080,pad=h*2/1080;
        if(lh<15)lh=15;if(pad<2)pad=2;
        int cw=FmCellWidth(w,lh*3,pad,120),panel=cw*120+lh*3+pad*2;
        int left=(w-cw*120-lh*3)/2-pad,right=left+panel;
        assert(cw>=1 && panel<=w*60/100);
        assert(left>=w*20/100 && right<=w-w*20/100);
        assert(left-(w-right)>=-1 && left-(w-right)<=1);
        printf("%dx%d: panel %d..%d width %d cell %d\n",w,h,left,right,panel,cw);
    }
    /* Explicit old percentages retain their location. */
    assert(FmPositionY(1080,94,10)==108);
    assert(FmPositionY(2160,188,10)==216);
    assert(FmPositionY(1080,94,0)==0);
    assert(FmPositionY(100,120,-1)==0);
    assert(FmPositionY(100,120,90)==0);
    for(int h=240; h<=4320; h+=120)
      for(int rows=18; rows<=200; rows+=18) {
        int y=FmPositionY(h,rows,-1);
        assert(y>=0 && y+rows<=h);
      }
    puts("frame_meter_position_test PASS: HUD anchor, optional rows, 1080p/4K, saved overrides, bounds");
}
