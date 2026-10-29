#define main original_menu_harness_main
#include "menu_harness.cpp"
#undef main
static int bossAllowed=0;
static int PauseAllowed(){return bossAllowed;}
int main(){
 using namespace menu;
 auto* base=(uint8_t*)VirtualAlloc(nullptr,kRvaFrameMgrPtr+8,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);CHECK(base);
 alignas(8) unsigned char frame[0x700]{};
 *(uint64_t*)(base+kRvaFrameMgrPtr)=(uint64_t)frame;
 volatile LONG* counter=(volatile LONG*)(frame+kOffFreezeCounter);*counter=2; // native and other owner requests
 g_host.base=base;g_host.knownBuild=true;g_cfg.freezeGame=1;g_open=1;g_disabled=0;g_menuPauseAllowed=PauseAllowed;
 SyncFreeze();CHECK(*counter==2&&!g_freezeCounter); // empty scene
 bossAllowed=1;SyncFreeze();CHECK(*counter==3&&g_freezeCounter);SyncFreeze();CHECK(*counter==3);
 bossAllowed=0;SyncFreeze();CHECK(*counter==2&&!g_freezeCounter); // boss gone while menu stays open
 bossAllowed=1;SyncFreeze();CHECK(*counter==3);g_open=0;SyncFreeze();CHECK(*counter==2);
 g_open=1;g_menuPauseAllowed=nullptr;SyncFreeze();CHECK(*counter==2&&!g_freezeCounter);
 g_host.base=nullptr;VirtualFree(base,0,MEM_RELEASE);
 puts("PASS: empty scene never freezes; live boss freezes once; disappearance/close release only the menu request; missing provider stays running");
}
