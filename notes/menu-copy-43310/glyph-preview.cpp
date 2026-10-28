#define main original_menu_harness_main
#include "../../mod/Nioh3EngineFix/tests/menu_harness.cpp"
#undef main
int main() {
 using namespace menu;
 for(int style=0;style<2;++style){
  menu_style::g_btnStyle=style;g_open=true;g_rematchPage=false;g_cfg.layout=0;g_pages.Reset();g_sel=kPageRow+pDisplay;
  osd::drawnRects.clear();osd::drawnTexts.clear();osd::drawnTextX.clear();osd::drawnTextY.clear();osd::drawnTextScale.clear();osd::drawnTextColor.clear();
  DrawImpl(1920,1080);
  bool action=false;for(auto& t:osd::drawnTexts){if(t.find("Cross:")!=std::string::npos||t.find("A: open")!=std::string::npos)return 2;action|=t=="open";}
  if(!action || osd::drawnRects.empty()) return 3;
  char path[100];snprintf(path,sizeof path,"notes/menu-copy-43310/glyph-preview-%d.tsv",style);FILE* f=fopen(path,"w");if(!f)return 4;
  for(auto& r:osd::drawnRects)fprintf(f,"R\t%f\t%f\t%f\t%f\t%u\n",r.x,r.y,r.w,r.h,r.color);
  for(size_t i=0;i<osd::drawnTexts.size();++i)fprintf(f,"T\t%f\t%f\t%f\t%u\t%s\n",osd::drawnTextX[i],osd::drawnTextY[i],osd::drawnTextScale[i],osd::drawnTextColor[i],osd::drawnTexts[i].c_str());
  fclose(f);
 }
 puts("PASS: real root menu renders action glyphs in both controller styles without Cross/A action words.");
}
