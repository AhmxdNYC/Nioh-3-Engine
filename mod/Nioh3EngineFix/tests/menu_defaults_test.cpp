#define main original_menu_harness_main
#include "menu_harness.cpp"
#undef main
int main() {
 using namespace menu;
 for(int i=0;i<kConfigN;++i)g_configValues.values[(size_t)i]={kConfigSettings[i].initial,false};
 auto cfg=[](const char* section,const char* key){for(int i=0;i<kConfigN;++i)if(!strcmp(kConfigSettings[i].section,section)&&!strcmp(kConfigSettings[i].key,key))return i;return -1;};
 // Shipped values override obsolete catalog hints, including auto values.
 CHECK(!strcmp(DefaultConfigText(cfg("Patches","E1")),"1"));
 CHECK(!strcmp(DefaultConfigText(cfg("Menu","FrameGenToggle")),"0"));
 CHECK(!strcmp(DefaultConfigText(cfg("GpuOpt","Barriers")),"2"));
 // Every page with resettable controls exposes exactly one reset; navigation/action pages expose none.
 int rows[kMenuRowCap];
 for(int layout=0;layout<2;++layout){g_cfg.layout=layout;
  for(int page=0;page<pCount;++page){g_pages.page=page;const int n=PageRows(rows,false);int resets=0;bool settings=false;
   for(int j=3;j<n;++j){settings|=ResettableRow(rows[j]);resets+=rows[j]==kResetPageRow||rows[j]==kCamRowFirst+kCamReset||rows[j]==kVrsRowFirst+vrsmenu::kRowReset;}
   CHECK(resets<2);if(settings&&page!=pRoot)CHECK(resets==1);
  }
 }
 // A page reset applies its live overlay and persisted values without touching other pages.
 g_cfg.layout=0;g_pages.page=pOvController;
 const int pad=cfg("Menu","ControllerDisplay"),input=cfg("Menu","InputDisplay");CHECK(pad>=0&&input>=0);
 g_configValues.values[(size_t)pad]={"1",false};g_configValues.values[(size_t)input]={"1",false};
 Change(kResetPageRow,1);CHECK(!g_configValues.values[(size_t)pad].changed);
 Change(kResetPageRow,0);CHECK(g_configValues.values[(size_t)pad].changed&&g_configValues.values[(size_t)pad].text==DefaultConfigText(pad));
 CHECK(!g_configValues.values[(size_t)input].changed&&g_configValues.values[(size_t)input].text=="1");
 g_pages.page=pOvFps;osd::Overlay().fpsCounter=1;Change(kResetPageRow,0);
 CHECK(osd::Overlay().fpsCounter==atoi(shipped_defaults::Get("Overlay","FpsCounter","0")));
 // Training-backed display settings use the persistent API and shipped values.
 g_pstGet=HarnessPstGet;g_pstSet=HarnessPstSet;g_pages.page=pDisplay;g_psv[12]=10;g_psv[13]=2;
 Change(kResetPageRow,0);CHECK(g_psv[12]==0&&g_psv[13]==0&&g_skyPersist==2);
 g_pages.page=pOvFrameMeter;for(auto& value:g_psv)value=1;Change(kResetPageRow,0);
 CHECK(g_fm==atoi(shipped_defaults::Get("Training.Practice","FrameMeter","0")));
 // Player gets a hosted tab without changing the selected game mode, including LB/RB.
 namespace hf=host_fixture;hf::view={};hf::view.size=sizeof(hf::view);hf::view.version=rmb::kVersion;
 hf::view.session=7;hf::view.generation=1;hf::view.flags=rmb::HostedReady;hf::view.workerTickMs=Now();
 g_rematchApi={sizeof(rmb::Api),rmb::kVersion,rmb::kRequiredCapabilities,0,hf::Attach,hf::Detach,hf::Post,hf::Copy,hf::Covered};g_rematchSession=7;
 Open("Player test");Change(kPracticeRow,0);hf::Complete(true);CHECK(RefreshRematchHost(Now()));
 const int before=g_modeSelection.Reopen(TrainingMode());
 SwitchTab(1);CHECK(hf::posted==rmb::ResumePlayer&&g_hostTab==kPlayerRow&&g_modeSelection.Reopen(TrainingMode())==before);
 hf::Complete(true);CHECK(RefreshRematchHost(Now()));CHECK(CurrentTab()==kPlayerRow);
 osd::drawnTexts.clear();osd::drawnTextColor.clear();DrawRematchHost(1920,1080);bool lit=false;for(const auto& t:osd::drawnTexts)lit|=t=="[Player]";CHECK(lit);
 Change(kPlayerRow,0);CHECK(hf::posted==rmb::OpenPlayer&&g_modeSelection.Reopen(TrainingMode())==before);
 // Optional genuine EngineFix render dump for offline visual review (not game evidence).
 char* dump=nullptr;size_t len=0;if(!_dupenv_s(&dump,&len,"N3_MENU_DEFAULTS_DUMP")&&dump){
  g_rematchPage=false;g_rematchAwait=false;g_pages.Reset();EnterPage(pDisplay);g_sel=kResetPageRow;
  osd::drawnTexts.clear();osd::drawnTextColor.clear();osd::drawnRects.clear();osd::drawnTextX.clear();osd::drawnTextY.clear();osd::drawnTextScale.clear();const float scale=DrawImpl(1920,1080);
  FILE* f=nullptr;if(!fopen_s(&f,dump,"w")&&f){fprintf(f,"S %g %d %d\n",scale,osd::kGlyphW,osd::kGlyphH);
   for(const auto& r:osd::drawnRects)fprintf(f,"R %g %g %g %g %08X\n",r.x,r.y,r.w,r.h,r.color);
   for(size_t i=0;i<osd::drawnTexts.size();++i)fprintf(f,"T %g %g %g %08X %s\n",osd::drawnTextX[i],osd::drawnTextY[i],osd::drawnTextScale[i],osd::drawnTextColor[i],osd::drawnTexts[i].c_str());fclose(f);}
 }free(dump);
 puts("PASS: Player host tab and page-scoped shipped defaults, all pages, persistence, live values, unrelated settings retained");
}
