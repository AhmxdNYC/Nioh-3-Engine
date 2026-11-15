#define main original_menu_harness_main
#include "menu_harness.cpp"
#undef main
// UO open #10: glyph set picked from the connected pad's vendor id; [Menu] ButtonStyle Auto (default) or an override.
static uint16_t g_fake[8]; static int g_fakeN = 0;
static int FakeVids(uint16_t* out, int cap) { int k = 0; for (; k < g_fakeN && k < cap; ++k) out[k] = g_fake[k]; return k; }
static void Pads(std::initializer_list<uint16_t> v) { g_fakeN = 0; for (uint16_t x : v) g_fake[g_fakeN++] = x; }
static int Find(const char* t){for(size_t i=0;i<osd::drawnTexts.size();++i)if(osd::drawnTexts[i]==t)return (int)i;return -1;}
// the badge letter drawn just before a footer label ("A" then "Select")
static std::string Before(const char* label){for(size_t i=osd::drawnTexts.size();i-- > 1;)if(osd::drawnTexts[i]==label)return osd::drawnTexts[i-1];return std::string();}  // last = the footer
int main() {
 using namespace menu; namespace pf = pad_family;
 // pure mapping: each family by vendor id, unknown = Xbox
 CHECK(pf::FamilyOfVid(0x054C) == pf::kPlayStation && pf::FamilyOfVid(0x057E) == pf::kNintendo);
 for (uint16_t v : {0x045E, 0x0B05, 0x17EF, 0x28DE, 0x0F0D, 0x2DC8, 0x0000, 0xFFFF}) CHECK(pf::FamilyOfVid(v) == pf::kXbox);
 {const uint16_t none[1] = {0}; CHECK(pf::Pick(none, 0) == pf::kXbox);
  const uint16_t ds[] = {0x045E, 0x054C}; CHECK(pf::Pick(ds, 2) == pf::kPlayStation);   // DS4Windows virtual Xbox + real DualSense
  const uint16_t sw[] = {0x28DE, 0x057E}; CHECK(pf::Pick(sw, 2) == pf::kNintendo);      // Steam virtual pad + Switch Pro
  const uint16_t both[] = {0x057E, 0x054C}; CHECK(pf::Pick(both, 2) == pf::kPlayStation);
  const uint16_t xb[] = {0x045E, 0x0B05}; CHECK(pf::Pick(xb, 2) == pf::kXbox);
  // override wins over detection
  CHECK(pf::Effective(pf::kXbox, ds, 2) == pf::kXbox && pf::Effective(pf::kNintendo, xb, 2) == pf::kNintendo && pf::Effective(pf::kAuto, ds, 2) == pf::kPlayStation);}
 CHECK(pf::ParseStyle("Auto") == pf::kAuto && pf::ParseStyle("") == pf::kAuto && pf::ParseStyle(nullptr) == pf::kAuto && pf::ParseStyle("junk") == pf::kAuto);
 CHECK(pf::ParseStyle("xbox") == pf::kXbox && pf::ParseStyle("PlayStation") == pf::kPlayStation && pf::ParseStyle("NINTENDO") == pf::kNintendo);
 // menu: Auto follows the fake pads at every open; overrides ignore them
 g_enumPadVids = FakeVids; g_btnStyleIni = pf::kAuto;
 savefiles::testAllowed=true;g_disabled=0;g_rematchSession=0;g_cfg.layout=0;g_rematchPage=false;g_modality=MOD_PAD;
 struct Case { std::initializer_list<uint16_t> v; int want; const char* faceA; const char* view; };
 const Case cases[] = {{{0x054C}, 1, nullptr, "Create"}, {{0x057E}, 2, "B", "Minus"}, {{0x045E}, 0, "A", "View"}, {{}, 0, "A", "View"}, {{0x1234}, 0, "A", "View"}};
 for (const Case& c : cases) {
  Pads(c.v); Open("padfamily"); CHECK(menu_style::g_btnStyle == c.want); CHECK(padviz::Style() == (c.want == 1 ? 1 : 0));
  osd::drawnTexts.clear(); DrawImpl(1920, 1080);
  if (c.faceA) CHECK(Before("Select") == c.faceA);  // bottom face button letter (Nintendo prints B there)
  CHECK(Find(c.view) >= 0);                          // View / Create / Minus word in the tab row
  Close("padfamily"); }
 // Nintendo: the bottom button reads B, not A
 Pads({0x057E}); Open("nin"); osd::drawnTexts.clear(); DrawImpl(1920, 1080);
 {const int i=Find("Select");CHECK(i>0);const char* want[]={"B","Select","A","Close","X","Close"};  // A B Y positions read B A X
  for(int j=0;j<6;++j)CHECK(osd::drawnTexts[i-1+j]==want[j]);}
 Close("nin");
 // override via the settings path: Xbox forced while a DualSense is connected; back to Auto picks it up again
 Pads({0x054C}); g_btnStyleIni = pf::ParseStyle("Xbox"); Open("override"); CHECK(menu_style::g_btnStyle == 0); Close("override");
 g_btnStyleIni = pf::ParseStyle("Nintendo"); Pads({0x045E}); Open("override2"); CHECK(menu_style::g_btnStyle == 2); Close("override2");
 g_btnStyleIni = pf::kAuto; Open("auto"); CHECK(menu_style::g_btnStyle == 0); Pads({0x054C}); Close("auto"); Open("auto2"); CHECK(menu_style::g_btnStyle == 1); Close("auto2");
 // first frame detects before any menu open
 g_btnStyleChecked = 0; Pads({0x057E}); OnPresentImpl(); CHECK(g_btnStyleChecked && menu_style::g_btnStyle == 2);
 // Nintendo shoulder caps: L / R / ZL / ZR glyph strings (stroke text is drawn as rects; check it draws something)
 {osd::drawnRects.clear(); hint_shapes::ShoulderCap(2, 3, 0, 20, 20, 0xFF000000u, 0xFFFFFFFFu); CHECK(!osd::drawnRects.empty());}
 // shipped default is Auto
 {bool found=false; for (const auto& e : shipped_defaults::kEntries) if (!strcmp(e.section,"Menu") && !strcmp(e.key,"ButtonStyle")) { found=true; CHECK(!strcmp(e.value,"Auto")); } CHECK(found);}
 g_enumPadVids = nullptr; menu_style::g_btnStyle = 0; g_btnStyleIni = pf::kAuto;
 puts("PASS: glyph set from pad vendor id (Sony PlayStation, Nintendo, else Xbox; unknown/none Xbox), ini override, open + first frame, shipped Auto");
}
