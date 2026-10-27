#define main existing_main
#include "frame_meter_draw_test.c"
#undef main
// Offline mark footprint audit: all 120 positions, 3 sizes, 14 viewport heights.
int main(void) {
 int positions=0,outside=0;
 for(int size=0;size<3;++size)for(int H=600;H<=2160;H+=120){
  Seed();g_pst.v[PS_FRAME_STYLE]=1;g_pst.v[PS_FRAME_SIZE]=size;
  FmRingLayout g=FmRingGeom(H*16/9,H);FmRingBuild(g);
  Canvas c=Make(g.d+20,g.d+20);int center=c.w/2;
  for(int i=0;i<120;++i){
   memset(c.bits,0,(size_t)c.w*c.h*4);
   n3_stroke_layered=1;FmRingCheck(c.dc,center,center,g,i);n3_stroke_layered=0;GdiFlush();
   int seen=0;
   for(int y=0;y<c.h;++y)for(int x=0;x<c.w;++x)if(c.bits[y*c.w+x]&0xffffff){
    ++seen;float r=hypotf(x+.5f-center,y+.5f-center);
    if(r>g.d*.5f || r<g.d*.5f-g.thick)++outside;
   }
   CHECK(seen>0);++positions;
  }Drop(&c);
 }
 CHECK(outside==0);
 for(int size=0;size<3;++size){
  Seed();g_pst.v[PS_FRAME_STYLE]=1;g_pst.v[PS_FRAME_SIZE]=size;
  Canvas c=Make(1920,FmDrawHeight(1920,1080));
  for(int on=0;on<2;++on){
   g_fmp.kipulseFrozen[95]=on;Draw(&c,1080);
   char name[64];snprintf(name,sizeof name,"pulse-%d-%d.bmp",size,on);Save(&c,name);
  }
  Drop(&c);
 }
 printf("PASS: %d mark positions; %d pixels outside PLAYER annulus; 6 production DIB previews saved\n",positions,outside);
 return 0;
}
