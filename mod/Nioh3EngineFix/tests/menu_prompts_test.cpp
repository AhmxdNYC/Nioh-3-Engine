#define main original_menu_harness_main
#include "menu_harness.cpp"
#undef main
// UO open #9: footer prompts follow InputModality() (UO #14), no toggle. Pad: face/d-pad badges + LB RB / View row.
// Keyboard: key caps Arrows / Enter / Esc + PgUp PgDn / menu key, same row count; back on the pad the badges return.
static int Find(const char* t){for(size_t i=0;i<osd::drawnTexts.size();++i)if(osd::drawnTexts[i]==t)return (int)i;return -1;}
static bool Has(const char* t){return Find(t)>=0;}
static void Draw(bool hosted,unsigned W=1920,unsigned H=1080){osd::drawnTexts.clear();osd::drawnTextColor.clear();osd::drawnTextX.clear();osd::drawnTextY.clear();
 if(hosted)menu::DrawRematchHost(W,H);else menu::DrawImpl(W,H);}
static bool PadShown(){return Has("A")&&Has("Select")&&Has("Switch tab")&&Has("First page")&&!Has("Enter")&&!Has("Esc")&&!Has("PgUp")&&!Has("Arrows");}
static bool KeysShown0(){return Has("Arrows")&&Has("Enter")&&Has("Esc")&&Has("PgUp")&&Has("PgDn")&&Has("Select")&&Has("Page")&&!Has("A")&&!Has("Switch tab")&&!Has("First page")&&!Has("Y");}  // row hints "A: open" become Enter caps too
static bool KeysShown(){const bool ok=KeysShown0();if(!ok)for(auto&t:osd::drawnTexts)printf("[%s]",t.c_str());if(!ok)puts("");return ok;}
static float MaxY(){float m=0;for(float y:osd::drawnTextY)m=std::max(m,y);return m;}
int main() {
 using namespace menu;
 // pure helper
 {menu_style::HintItem k[3];
  CHECK(menu_style::KeyPrompts(0,"F10",k)==3&&!strcmp(k[0].keys,"Arrows")&&!strcmp(k[1].keys,"Enter")&&!strcmp(k[1].label,"Select")&&!strcmp(k[2].keys,"Esc")&&!strcmp(k[2].label,"Back"));
  CHECK(menu_style::KeyPrompts(1,"F10",k)==2&&!strcmp(k[0].keys,"PgUp PgDn")&&!strcmp(k[1].keys,"F10")&&!strcmp(k[1].label,"Close"));
  CHECK(menu_style::KeyPrompts(1,"Insert",k)==2&&!strcmp(k[1].keys,"Insert"));
  CHECK(menu_style::KeyPrompts(1,nullptr,k)==1&&menu_style::KeyPrompts(1,"",k)==1&&menu_style::KeyPrompts(1,"None",k)==1);}
 // #105956 row hints: every face-button letter gets its key cap under the keyboard
 CHECK(!strcmp(menu_style::RowKeyCap('A',"F10"),"Enter")&&!strcmp(menu_style::RowKeyCap('B',"F10"),"Esc"));
 CHECK(!strcmp(menu_style::RowKeyCap('Y',"F10"),"F10")&&!strcmp(menu_style::RowKeyCap('Y',"Insert"),"Insert"));
 CHECK(!strcmp(menu_style::RowKeyCap('Y',nullptr),"Esc")&&!strcmp(menu_style::RowKeyCap('Y',"None"),"Esc")&&!strcmp(menu_style::RowKeyCap('Y',""),"Esc"));
 CHECK(menu_style::RowKeyCap('X',"F10")==nullptr);
 // #106005 add-on help text: pad button names become keys under the keyboard, articles stay
 {char b[256];auto kt=[&](const char* in,const char* tk="F10"){menu_style::KeyText(in,b,sizeof b,tk);return std::string(b);};
  CHECK(kt("  Close   A: close the mod menu")=="  Close   Enter: close the mod menu");
  CHECK(kt("  Reset fight   A: refill and return to start")=="  Reset fight   Enter: refill and return to start");
  CHECK(kt("Oni: A opens boss actions. Boss 3 of 9. PgUp/PgDn skips ten rows. LB/RB switches tab. Right: favorite / Left: unfavorite.")
        =="Oni: Enter opens boss actions. Boss 3 of 9. PgUp/PgDn skips ten rows. Right: favorite / Left: unfavorite.");
  CHECK(kt("Mission - route. A start, B back.")=="Mission - route. Enter start, Esc back.");
  CHECK(kt("All meter lanes: ON   A / Left / Right")=="All meter lanes: ON   Enter / Left / Right");
  CHECK(kt("Fight. A: start / B: back.")=="Fight. Enter: start / Esc: back.");
  CHECK(kt("Y: close",nullptr)=="Esc: close"&&kt("Y: close","Insert")=="Insert: close");
  CHECK(kt("A boss with A plan. Beat B-rank. ABBA.")=="A boss with A plan. Beat B-rank. ABBA.");  // articles / words untouched
  CHECK(kt("switch: LB/RB switches tab")=="switch:");
  CHECK(!menu_style::KeyText("no buttons here",b,sizeof b,"F10")&&!strcmp(b,"no buttons here"));
  char sm[8];menu_style::KeyText("A: close the mod menu",sm,sizeof sm,"F10");CHECK(strlen(sm)==7);}
 savefiles::testAllowed=true;g_disabled=0;g_rematchSession=0;g_cfg.layout=0;g_rematchPage=false;
 g_modality=MOD_PAD;
 Open("prompts");CHECK(g_pages.page==pRoot);
 for(unsigned W:{1920u,1280u}){
  g_modality=MOD_PAD;Draw(false,W,W*9/16);CHECK(PadShown());const float padBottom=MaxY();
  g_modality=MOD_KEY;Draw(false,W,W*9/16);CHECK(KeysShown());CHECK(Has(g_cfg.keyName)&&Has("Close"));
  CHECK(MaxY()==padBottom);  // same row count: the panel does not grow or shrink on a switch
  // key caps sit on two rows at the bottom: Enter row above the PgUp row
  CHECK(osd::drawnTextY[Find("Enter")]<osd::drawnTextY[Find("PgUp")]);
  g_modality=MOD_PAD;Draw(false,W,W*9/16);CHECK(PadShown()&&MaxY()==padBottom);}
 // no menu key bound: the PgUp PgDn cap stays, no stray "None" cap
 {const UINT vk=g_cfg.vk;char kn[16];memcpy(kn,g_cfg.keyName,sizeof kn);g_cfg.vk=0;snprintf(g_cfg.keyName,sizeof g_cfg.keyName,"None");
  g_modality=MOD_KEY;Draw(false);CHECK(KeysShown()&&!Has("None")&&Has("PgDn"));
  g_cfg.vk=vk;memcpy(g_cfg.keyName,kn,sizeof kn);}
 // a nested page still follows the modality (Back on B vs Esc)
 EnterPage(pOvFrameMeter);g_modality=MOD_KEY;Draw(false,1280,720);CHECK(KeysShown()&&Has("Back")&&!Has("B")&&!Has("X"));  // #105956: the Back row "B" hint is an Esc cap
 g_modality=MOD_PAD;Draw(false,1280,720);CHECK(PadShown()&&Has("B"));
 // #105956: a page with a Back row (value "B"): pad shows the B badge on the row, the keyboard an Esc cap, never a B letter
 {Close("prompts");Open("prompts");EnterPage(pSaveFiles);CHECK(g_pages.page==pSaveFiles);
  g_modality=MOD_KEY;Draw(false,1280,720);int esc=0;for(auto&t:osd::drawnTexts)esc+=t=="Esc";CHECK(!Has("B")&&esc>=2);
  g_modality=MOD_PAD;Draw(false,1280,720);CHECK(Has("B"));}
 Close("prompts");
 // hosted Boss Matches / Training page footer
 namespace hf = host_fixture;
 hf::view={};hf::view.size=sizeof(hf::view);hf::view.version=rmb::kVersion;
 hf::view.session=7;hf::view.generation=1;hf::view.flags=rmb::HostedReady;hf::view.workerTickMs=Now();
 g_rematchApi={sizeof(rmb::Api),rmb::kVersion,rmb::kRequiredCapabilities,0,hf::Attach,hf::Detach,hf::Post,hf::Copy,hf::Covered};g_rematchSession=7;
 Open("hosted prompts");Change(kPracticeRow,0);hf::Complete(true);CHECK(RefreshRematchHost(Now())&&g_rematchPage);
 g_modality=MOD_PAD;Draw(true);CHECK(Has("A")&&Has("Select")&&Has("Switch tab")&&!Has("Enter")&&!Has("PgUp"));const float hb=MaxY();
 g_modality=MOD_KEY;Draw(true);CHECK(Has("Arrows")&&Has("Enter")&&Has("Esc")&&Has("PgUp")&&!Has("Switch tab")&&!Has("A")&&MaxY()==hb);
 g_modality=MOD_PAD;Draw(true);CHECK(Has("Switch tab")&&!Has("Enter"));
 // #106005: the add-on's rows / footer name keys under the keyboard and pad buttons on the pad
 {const auto keep=hf::view;
  snprintf(hf::view.rows[1].text,sizeof hf::view.rows[1].text,"  Close   A: close the mod menu");hf::view.rowCount=std::max(hf::view.rowCount,2u);
  snprintf(hf::view.footer,sizeof hf::view.footer,"Oni: A opens boss actions. Boss 1 of 2. LB/RB switches tab. Right: favorite.");
  hf::Complete(true);CHECK(RefreshRematchHost(Now()));
  auto any=[](const char* sub){for(auto&t:osd::drawnTexts)if(t.find(sub)!=std::string::npos)return true;return false;};
  g_modality=MOD_KEY;Draw(true);CHECK(any("Enter: close the mod menu")&&any("Enter opens boss actions")&&!any("A: close")&&!any("LB/RB")&&!any("A opens"));
  g_modality=MOD_PAD;Draw(true);CHECK(any("A: close the mod menu")&&any("A opens boss actions")&&any("LB/RB")&&!any("Enter:"));
  hf::view=keep;hf::Complete(true);RefreshRematchHost(Now());}
 Close("hosted prompts");
 g_modality=MOD_PAD;
 puts("PASS: footer prompts follow InputModality (pad badges / key caps, same rows, flat + nested + hosted pages, add-on help text, unbound menu key)");
}
