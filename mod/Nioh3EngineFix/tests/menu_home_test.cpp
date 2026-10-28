#define main original_menu_harness_main
#include "menu_harness.cpp"
#undef main
int main() {
 using namespace menu;
 g_disabled=0;g_rematchSession=0;g_open=false;g_cfg.holdMask=XINPUT_GAMEPAD_START;g_cfg.gesture=1;g_cfg.tapGapMs=300;g_cfg.tapWindowMs=900;
 for(auto& p:g_pad)p={};g_toggleReq=0;g_lastPresent=Now();
 auto feed=[](WORD buttons){XINPUT_STATE st{};st.Gamepad.wButtons=buttons;g_lastPresent=Now();PadFeed(0,&st,false);return st.Gamepad.wButtons;};
 for(int tap=0;tap<3;++tap){feed(XINPUT_GAMEPAD_START);feed(0);}
 CHECK(g_toggleReq==1); // closed-menu triple tap still requests opening
 g_toggleReq=0;Open("home test");EnterPage(pDisplay);EnterPage(pOvFrameMeter);
 // #44181: Start no longer returns to the first page; View / Create does
 CHECK(feed(XINPUT_GAMEPAD_START)==0);CHECK(!(g_pressed & N_HOME));feed(0);
 CHECK(!TryPadHome((uint32_t)InterlockedExchange(&g_pressed,0)));CHECK(g_pages.page==pOvFrameMeter);
 CHECK(feed(XINPUT_GAMEPAD_BACK)==0);CHECK(g_pressed & N_HOME);
 CHECK(TryPadHome((uint32_t)InterlockedExchange(&g_pressed,0)));
 CHECK(g_open && g_pages.page==pRoot);feed(0);
 for(int tap=0;tap<3;++tap){feed(XINPUT_GAMEPAD_START);feed(0);}
 CHECK(!g_toggleReq && g_pad[0].nTaps==0); // repeated Start never toggles closed
 g_pressed=0;g_rematchPage=true;g_rematchAwait=true;g_rematchView.flags=rmb::Editing;
 CHECK(TryPadHome(N_HOME));CHECK(g_open && !g_rematchPage && !g_rematchAwait && g_pages.page==pRoot);
 feed(0);CHECK(feed(XINPUT_GAMEPAD_Y)==0);CHECK(g_pressed & N_CLOSE);
 CHECK(TryPadClose((uint32_t)InterlockedExchange(&g_pressed,0),Now()));CHECK(!g_open);
 CHECK(!TryPadHome(N_HOME));
 g_cfg.gesture=0;Open("hold-mode home");EnterPage(pDisplay);
 feed(0);feed(XINPUT_GAMEPAD_BACK);CHECK(TryPadHome((uint32_t)InterlockedExchange(&g_pressed,0)));
 CHECK(g_open && g_pages.page==pRoot && !g_toggleReq && !g_pad[0].holdStart);
 feed(0);Close("hold-mode home");
 // #44232: bumpers are tabs (pad); keyboard PgUp / PgDn keep jumping rows
 CHECK(PadNav(XINPUT_GAMEPAD_LEFT_SHOULDER)==N_TAB_PREV && PadNav(XINPUT_GAMEPAD_RIGHT_SHOULDER)==N_TAB_NEXT);
 CHECK(PadNav(XINPUT_GAMEPAD_START)==0 && PadNav(XINPUT_GAMEPAD_BACK)==N_HOME);
 CHECK(KeyNav(K_PAGEUP|K_PAGEDOWN)==(N_PAGEUP|N_PAGEDOWN));
 g_cfg.layout=0;g_rematchSession=0;g_rematchPage=false;g_rematchAwait=false;g_rematchView.flags=0;
 Open("tabs");CHECK(g_pages.page==pRoot);
 int tabs[kMenuRowCap];const int nt=TabRows(tabs);
 CHECK(nt==4 && tabs[0]==kPageRow+pDisplay && tabs[1]==kPageRow+pCamera && tabs[2]==kPageRow+pSettings && tabs[3]==kSaveFilesRow);
 CHECK(CurrentTab()==0);
 g_pressed=N_TAB_NEXT;OnPresentImpl();CHECK(g_pages.page==pDisplay && g_pages.depth==1); // first page: RB -> first tab
 EnterPage(pOvFrameMeter);CHECK(CurrentTab()==kPageRow+pDisplay);   // nested page keeps its tab
 g_pressed=N_TAB_NEXT;OnPresentImpl();CHECK(g_pages.page==pCamera && g_pages.depth==1);
 g_pressed=N_TAB_NEXT;OnPresentImpl();CHECK(g_pages.page==pSettings);
 EnterPage(pSCpu);CHECK(g_pages.depth==2 && CurrentTab()==kPageRow+pSettings);
 g_pressed=N_TAB_PREV;OnPresentImpl();CHECK(g_pages.page==pCamera && g_pages.depth==1);
 g_pressed=N_TAB_PREV|N_TAB_NEXT;OnPresentImpl();CHECK(g_pages.page==pCamera); // both bumpers at once: nothing
 g_pressed=N_TAB_PREV;OnPresentImpl();CHECK(g_pages.page==pDisplay);
 CHECK(TabRows(tabs)==4 && g_pages.page==pDisplay); // listing tabs leaves the current page alone
 g_pages.Reset();g_pages.Enter(pSaveFiles,0);CHECK(CurrentTab()==kSaveFilesRow);g_pages.Enter(pSaveActions,0);CHECK(CurrentTab()==kSaveFilesRow);
 g_pages.Reset();g_rematchPage=true;g_hostTab=kPracticeRow;CHECK(CurrentTab()==kPracticeRow);g_rematchPage=false;
 Close("tabs");
 puts("PASS: View home from nested/hosted pages, Start inert while open; LB/RB tabs; triple-tap opening and Y close retained; hold gesture isolated");
}
