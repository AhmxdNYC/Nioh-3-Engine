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
 CHECK(feed(XINPUT_GAMEPAD_START)==0);CHECK(g_pressed & N_HOME);
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
 feed(0);feed(XINPUT_GAMEPAD_START);CHECK(TryPadHome((uint32_t)InterlockedExchange(&g_pressed,0)));
 CHECK(g_open && g_pages.page==pRoot && !g_toggleReq && !g_pad[0].holdStart);
 puts("PASS: Start home from nested/hosted pages; triple-tap opening and Y close retained; hold gesture isolated");
}
