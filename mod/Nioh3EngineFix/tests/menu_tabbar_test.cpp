#define main original_menu_harness_main
#include "menu_harness.cpp"
#undef main
// #44353: tab bar row at the top of every mod menu page (Home + the LB / RB tabs, the current one bracketed in gold).
static int Find(const char* t){for(size_t i=0;i<osd::drawnTexts.size();++i)if(osd::drawnTexts[i]==t)return (int)i;return -1;}
static bool CurrentIs(const char* bracketed){const int i=Find(bracketed);return i>=0&&(osd::drawnTextColor[i]&0xFFFFFFu)==(menu_style::kAccent&0xFFFFFFu);}
int main() {
 using namespace menu;
 // pure layout: everything fits; overflow keeps the current tab with markers and stays inside the columns
 {const int lens[]={4,7,6,8,5};menu_nav::TabSeg s[8];bool l,r;
  CHECK(menu_nav::TabBar(lens,5,2,58,s,l,r)==5&&!l&&!r&&s[0].col==0&&s[1].col==7&&s[4].col+s[4].len<=58);
  for(int cur=0;cur<5;++cur)for(int cols=10;cols<=40;++cols){
   const int k=menu_nav::TabBar(lens,5,cur,cols,s,l,r);bool has=false;
   for(int i=0;i<k;++i){has|=s[i].tab==cur;if(i)CHECK(s[i].tab==s[i-1].tab+1&&s[i].col>=s[i-1].col+s[i-1].len);}
   CHECK(k>=1&&has&&l==(s[0].tab>0)&&r==(s[k-1].tab<4));
   if(k>1||lens[cur]+2+(l?2:0)+(r?2:0)<=cols)CHECK(s[k-1].col+s[k-1].len+(r?2:0)<=cols);}
  CHECK(menu_nav::TabBar(lens,5,4,20,s,l,r)>=1&&l&&!r);}
 g_disabled=0;g_rematchSession=0;g_cfg.layout=0;g_rematchPage=false;
 Open("tabbar");CHECK(g_pages.page==pRoot);
 osd::drawnTexts.clear();osd::drawnTextColor.clear();CHECK(DrawImpl(1920,1080)>0);
 CHECK(CurrentIs("[Home]")&&Find(" Display ")>=0&&Find(" Camera ")>=0&&Find(" Optimize ")>=0&&Find(" Saves ")>=0);
 CHECK((osd::drawnTextColor[Find(" Display ")]&0xFFFFFFu)==(cGray&0xFFFFFFu));
 // the row sits under the title, in LB / RB order, left to right
 CHECK(osd::drawnTextX[Find("[Home]")]<osd::drawnTextX[Find(" Display ")]&&osd::drawnTextX[Find(" Display ")]<osd::drawnTextX[Find(" Saves ")]);
 CHECK(osd::drawnTextY[Find("[Home]")]>osd::drawnTextY[0]);
 g_pressed=N_TAB_NEXT;OnPresentImpl();CHECK(g_pages.page==pDisplay);
 osd::drawnTexts.clear();osd::drawnTextColor.clear();DrawImpl(1920,1080);CHECK(CurrentIs("[Display]")&&Find(" Home ")>=0&&Find("[Home]")<0);
 EnterPage(pOvFrameMeter);osd::drawnTexts.clear();osd::drawnTextColor.clear();DrawImpl(1280,720);CHECK(CurrentIs("[Display]"));  // nested page keeps its tab
 savefiles::testAllowed=true;g_pressed=N_TAB_PREV;OnPresentImpl();CHECK(CurrentTab()==kSaveFilesRow);  // Display -LB-> wraps to the last tab
 osd::drawnTexts.clear();osd::drawnTextColor.clear();DrawImpl(1920,1080);CHECK(CurrentIs("[Saves]"));
 Close("tabbar");
 // hosted Boss Matches / Practice pages show the same row with their tab current
 namespace hf = host_fixture;
 hf::view={};hf::view.size=sizeof(hf::view);hf::view.version=rmb::kVersion;
 hf::view.session=7;hf::view.generation=1;hf::view.flags=rmb::HostedReady;hf::view.workerTickMs=Now();
 g_rematchApi={sizeof(rmb::Api),rmb::kVersion,rmb::kRequiredCapabilities,0,hf::Attach,hf::Detach,hf::Post,hf::Copy,hf::Covered};g_rematchSession=7;
 Open("hosted tabbar");Change(kPracticeRow,0);hf::Complete(true);CHECK(RefreshRematchHost(Now())&&g_rematchPage);
 osd::drawnTexts.clear();osd::drawnTextColor.clear();CHECK(DrawRematchHost(1920,1080)>0);
 CHECK(CurrentIs("[Training]")&&Find(" Boss ")>=0&&Find(" Home ")>=0);
 osd::drawnTexts.clear();osd::drawnTextColor.clear();CHECK(DrawRematchHost(1280,720)>0&&CurrentIs("[Training]"));
 Close("hosted tabbar");
 puts("PASS: tab bar row on flat and hosted pages, current tab bracketed in gold, LB/RB order, overflow window with < > markers");
}
