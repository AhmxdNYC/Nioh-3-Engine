#define main existing_ui_tests_main
#include "../../mod/Nioh3BossRematch/tests/ui_harness.cpp"
#undef main
int main() {
 using namespace addon::rematch;
 puts("OFFLINE MENU WORDING PREVIEW - sample values, no game");
 puts("PLAYER SETTINGS - Training / spawned Boss Matches");
 for(int id: {PS_PLAYER_INF_KI,PS_PLAYER_INF_NIN,PS_DMG_DEALT,PS_KI_DEALT,PS_PLAYER_SPEED}) {
  char v[24]; PstValueText(id,PstDefault(id),v,sizeof v); printf("%s %s\n",PstLabel(id),v);
 }
 puts("\nBOSS SETTINGS - see selected help for Training-only effects");
 for(int id: {PS_DMG_TAKEN,PS_BOSS_KI_DRAIN,PS_ONE_HIT_KI,PS_HOLD_KI_BREAK,PS_HOLD_KI_V2,PS_KI_FLOOR,PS_KI_LOG,PS_NEG_KI_METER,PS_BOSS_SPEED}) {
  char v[24]; PstValueText(id,PstDefault(id),v,sizeof v); printf("%s %s\n",PstLabel(id),v);
 }
 for(int id: {PS_BOSS_KI_DRAIN,PS_ONE_HIT_KI,PS_HOLD_KI_BREAK,PS_HOLD_KI_V2,PS_KI_FLOOR,PS_BOSS_SPEED}) if(!strstr(PstFooter(kItPstBase+id),"not applied during Boss Matches")) return 4;
 puts("\nSELECTED HELP");
 puts(PstFooter(kItPstBase+PS_DMG_DEALT));
 puts(PstFooter(kItPstBase+PS_HOLD_KI_BREAK));
 puts(PstFooter(kItPstBase+PS_FRAME_STYLE));
 char v[24]; PstValueText(PS_DMG_DEALT,150,v,sizeof v); if(strcmp(v,"150 %")) return 2;
 for(int c: kMcOrder) if(!N3MoveCatName(c)||!N3MoveCatHelp(c)) return 3;
 puts("\nPASS: production label/value/footer output; 150-percent formatting; all move group text available.");
}
