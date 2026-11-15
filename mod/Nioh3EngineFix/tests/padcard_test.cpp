#define main original_menu_harness_main
#include "menu_harness.cpp"
#undef main
// UO open #8: controller status card on the Controller overlay page (which pad, does the game get XInput, stick drift).
static padcard::PadId g_fp[8]; static int g_fpN = 0;
static int FakePads(padcard::PadId* out, int cap) { int k = 0; for (; k < g_fpN && k < cap; ++k) out[k] = g_fp[k]; return k; }
static bool HasPrefix(const char* p){for(const auto& t:osd::drawnTexts)if(t.rfind(p,0)==0)return true;return false;}
static std::string Line(const char* p){for(const auto& t:osd::drawnTexts)if(t.rfind(p,0)==0)return t;return std::string();}
static void ClearSlots(){for(auto& s:padcard::g_slots){s.t=0;s.buttons=0;s.lx=s.ly=s.rx=s.ry=0;}}
int main() {
 using namespace padcard;
 char a[128], b[128];
 // names
 CHECK(!strcmp(Name(0x054C,0x0CE6),"DualSense")&&!strcmp(Name(0x057E,0x2009),"Switch Pro Controller")&&!strcmp(Name(0x0B05,0x1ABE),"ROG Ally controls")&&Name(0x1234,0x5678)==nullptr);
 // no pad at all
 {ClearSlots();Card c;Format(c,10000,0,a,sizeof a,b,sizeof b);CHECK(strstr(a,"none found")&&strstr(b,"no input"));}
 // DualSense listed by Windows, game gets no XInput -> Steam Input hint
 {ClearSlots();Card c;c.hid[0]={0x054C,0x0CE6};c.nHid=1;Format(c,10000,1,a,sizeof a,b,sizeof b);CHECK(strstr(a,"DualSense")&&strstr(a,"no XInput"));}
 // XInput slot 2 alive, unknown pad ids shown as numbers; stale slot (> 5 s) counts as gone
 {ClearSlots();Feed(1,0,100,-200,0,0,9000);Card c;c.hid[0]={0x1234,0xABCD};c.nHid=1;Format(c,10000,0,a,sizeof a,b,sizeof b);
  CHECK(strstr(a,"pad 1234:ABCD")&&strstr(a,"XInput 2")&&strstr(a,"(Xbox)")&&strstr(b,"at rest")&&strstr(b,"no drift"));
  Format(c,14001,0,a,sizeof a,b,sizeof b);CHECK(strstr(a,"no XInput"));}
 // DualSense beats Steam's virtual pad for the name
 {ClearSlots();Feed(0,0,0,0,0,0,10000);Card c;c.hid[0]={0x28DE,0x11FF};c.hid[1]={0x054C,0x0CE6};c.nHid=2;Format(c,10000,1,a,sizeof a,b,sizeof b);CHECK(strstr(a,"DualSense")&&strstr(a,"(PlayStation)"));}
 // Steam's virtual pad next to a real Xbox pad: the real one is named
 {ClearSlots();Feed(0,0,0,0,0,0,10000);Card c;c.hid[0]={0x28DE,0x11FF};c.hid[1]={0x045E,0x0B13};c.nHid=2;Format(c,10000,0,a,sizeof a,b,sizeof b);CHECK(strstr(a,"Xbox Series Controller"));
  c.nHid=1;Format(c,10000,0,a,sizeof a,b,sizeof b);CHECK(strstr(a,"Steam virtual pad"));}
 // drift: left stick still at 30% with no button for 1.5 s -> flagged; < 1.5 s not; moving resets; a held button never flags; back inside the deadzone clears
 {ClearSlots();Card c;c.nHid=0;const int x=9800;uint64_t t=20000;
  for(;t<=21400;t+=16){Feed(0,0,x+(int)(t%3)*50,0,0,0,t);Step(c,t);}
  Format(c,t,0,a,sizeof a,b,sizeof b);CHECK(!strstr(b,"possible drift")&&strstr(b,"moving"));
  for(;t<=21700;t+=16){Feed(0,0,x,0,0,0,t);Step(c,t);}
  Format(c,t,0,a,sizeof a,b,sizeof b);CHECK(strstr(b,"possible drift L 30%"));
  Feed(0,0,x+5000,0,0,0,t);Step(c,t);Format(c,t,0,a,sizeof a,b,sizeof b);CHECK(!strstr(b,"possible drift"));  // moved: restart
  Feed(0,0,1000,0,0,0,t);Step(c,t);CHECK(!c.l[0].flagged&&c.l[0].since==0);
  Card h;for(uint64_t u=30000;u<=33000;u+=16){Feed(0,0x1000,x,0,0,0,u);Step(h,u);}CHECK(!h.l[0].flagged);  // A held: player is steering
  Card r;for(uint64_t u=40000;u<=42000;u+=16){Feed(0,0,0,0,0,-12000,u);Step(r,u);}Format(r,42000,0,a,sizeof a,b,sizeof b);CHECK(strstr(b,"possible drift R 37%")&&r.r[0].flagged&&!r.l[0].flagged);}
 // every line fits the 58-column panel, also with the longest table name
 {ClearSlots();Card c;c.hid[0]={0x054C,0x0BA0};c.nHid=1;Format(c,10000,1,a,sizeof a,b,sizeof b);CHECK(strlen(a)<=58);
  Feed(0,0,0,0,0,0,10000);Format(c,10000,1,a,sizeof a,b,sizeof b);CHECK(strlen(a)<=58&&strlen(b)<=58);
  Card r;for(uint64_t u=40000;u<=42000;u+=16){Feed(0,0,20000,20000,-20000,20000,u);Step(r,u);}Format(r,42000,0,a,sizeof a,b,sizeof b);CHECK(strlen(b)<=58&&strstr(b,"L 86% R 86%"));}
 // the menu page shows the card (Controller overlay page only)
 {using namespace menu;ClearSlots();g_fp[0]={0x057E,0x2009};g_fpN=1;g_enumPads=FakePads;
  savefiles::testAllowed=true;g_disabled=0;g_rematchSession=0;g_cfg.layout=0;g_rematchPage=false;g_modality=MOD_PAD;
  Open("card");EnterPage(pOvController);const uint64_t t=Now();Feed(0,0,0,0,0,0,t);
  osd::drawnTexts.clear();DrawImpl(1920,1080);
  CHECK(Line("Pad: ")=="Pad: Switch Pro Controller (Nintendo), XInput 1");CHECK(HasPrefix("Sticks: at rest"));
  EnterPage(pOvFrameMeter);osd::drawnTexts.clear();DrawImpl(1920,1080);CHECK(!HasPrefix("Pad: ")&&!HasPrefix("Sticks: "));
  Close("card");g_enumPads=nullptr;ClearSlots();menu_style::g_btnStyle=0;}
 puts("PASS: controller status card (names, none / no-XInput / stale slot, DualSense over Steam pad, drift 1.5 s still, button / move / deadzone resets, menu page)");
}
