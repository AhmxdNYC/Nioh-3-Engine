#include "../src/savefiles.cpp"
#include <cstdlib>
#include <cstdarg>
#include <initializer_list>
int checks=0,logs=0;
char lastLog[2048]{};
uintptr_t* mutateState=nullptr;
void Capture(const char* fmt,...){++logs;va_list ap;va_start(ap,fmt);vsnprintf(lastLog,sizeof lastLog,fmt,ap);va_end(ap);
 if(mutateState&&strstr(lastLog,"action=switch-request")){*mutateState=10;mutateState=nullptr;}}
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 using namespace savefiles;
 // Configure defaults are enabled; explicit user opt-out must still win.
 wchar_t configDir[MAX_PATH]{},configPath[MAX_PATH]{};
 CHECK(GetTempPathW(MAX_PATH,configDir));CHECK(GetTempFileNameW(configDir,L"n3d",0,configPath));
 Configure(nullptr,false,configPath,nullptr);CHECK(enabled);CHECK(liveEnabled);CHECK(realRoot);
 CHECK(!saveRoot.empty());CHECK(!sandbox.empty());CHECK(!helper.empty());CHECK(!liveHelper.empty());
 CHECK(WritePrivateProfileStringW(L"SaveFiles",L"Enabled",L"0",configPath));
 CHECK(WritePrivateProfileStringW(L"SaveFiles",L"LiveTitle",L"0",configPath));
 Configure(nullptr,false,configPath,nullptr);CHECK(!enabled);CHECK(!liveEnabled);
 CHECK(DeleteFileW(configPath));
 base=(uint8_t*)VirtualAlloc(nullptr,0x4800000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(base);
 uintptr_t manager[8]{},mode[12]{},stack[1]{},messages[25]{};unsigned char gm[0x600]{};
 *(uintptr_t*)(base+0x4709B50)=(uintptr_t)manager;
 *(uintptr_t*)(base+0x45BBF80)=(uintptr_t)messages;
 *(uintptr_t*)(base+0x4759910)=(uintptr_t)gm;
 manager[0]=(uintptr_t)stack;manager[1]=(uintptr_t)(stack+1);stack[0]=(uintptr_t)mode;
 mode[1]=(uintptr_t)base+0x44C91D0;mode[2]=3;mode[6]=100;mode[7]=200;mode[8]=300;mode[11]=1;
 known=true;enabled=true;logfn=&Capture;
 CHECK(ValidSlotName("Yoritomo Practice"));CHECK(ValidSlotName("New Game"));
 for(const char* name:{"../escape","A/B","CON","com1","bad "," bad","","A\"B"})CHECK(!ValidSlotName(name));
 auto arm=[&](){const auto now=GetTickCount64();gate={};gate.tracking=true;gate.since=now-1000;gate.last=now;gate.manager=(uintptr_t)manager;gate.top=(uintptr_t)mode;polled=now;allowed=true;};
 // Exercise real Win32 directory enumeration against disposable metadata only.
 wchar_t temp[MAX_PATH]{};CHECK(GetTempPathW(MAX_PATH,temp));
 sandbox=std::wstring(temp)+L"nioh3-slot-catalog-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
 CHECK(CreateDirectoryW(sandbox.c_str(),nullptr));CHECK(CreateDirectoryW((sandbox+L"\\slots").c_str(),nullptr));
 auto addSlot=[&](const wchar_t* name){
  const auto path=sandbox+L"\\slots\\"+name;if(!CreateDirectoryW(path.c_str(),nullptr))return false;
  HANDLE f=CreateFileW((path+L"\\manifest.json").c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(f==INVALID_HANDLE_VALUE)return false;CloseHandle(f);return true;
 };
 CHECK(addSlot(L"Yoritomo Practice"));CHECK(addSlot(L"New Game"));CHECK(addSlot(L"slot1"));
 CHECK(CreateDirectoryW((sandbox+L"\\slots\\Incomplete").c_str(),nullptr));
 arm();RefreshSlots();CHECK(SlotCount()==3);CHECK(!strcmp(SlotName(0),"New Game"));CHECK(!strcmp(SlotName(1),"Yoritomo Practice"));
 CHECK(!strcmp(SlotName(2),"slot1"));CHECK(!*SlotName(-1)&&!*SlotName(3));
 for(int i=3;i<=64;++i)CHECK(addSlot((L"extra"+std::to_wstring(i)).c_str()));
 arm();RefreshSlots();CHECK(SlotCount()==0);CHECK(strstr(Note(),"more than 64"));sandbox.clear();
 CHECK(Ready(Read()));arm();CHECK(Allowed());CHECK(ActionAllowed("test-main"));CHECK(strstr(lastLog,"allow=1"));
 CHECK(!Queue(0,false));CHECK(!Queue(9,false));CHECK(!Queue(1,false)); // no helper config
 native=false;  // the Python helper path ([SaveFiles] Native = 0)
 sandbox=L"D:\\fake-sandbox";helper=L"D:\\fake-helper.pyz";python=L"D:\\fake-python.exe";
 arm();mutateState=&mode[2];CHECK(!Queue(1,false));CHECK(strstr(lastLog,"switch-dispatch")&&strstr(lastLog,"allow=0"));
 CHECK(!child);mode[2]=3;sandbox.clear();helper.clear();python.clear();
 native=true;  // native: a switch is a live import; refused without SaveRoot/CopySandbox, nothing dispatched
 saveRoot.clear();arm();CHECK(!Queue(1,false));CHECK(!child);CHECK(strstr(Note(),"Unavailable"));
 arm();CHECK(!QueueNewBackup());CHECK(!child);
 enabled=false;arm();CHECK(!ActionAllowed("disabled"));enabled=true;
 auto deny=[&](){arm();return !ActionAllowed("deny")&&strstr(lastLog,"allow=0");};
 // Live-reset PRESS ANY BUTTON (self-pointer and all-zero phases) is an idle title for save actions...
 mode[6]=mode[7]=mode[8]=(uintptr_t)mode+0x30;mode[11]=0;arm();CHECK(ActionAllowed("press-any"));CHECK(strstr(lastLog,"allow=1"));
 mode[6]=mode[7]=mode[8]=0;arm();CHECK(ActionAllowed("press-any-zero"));
 // ...but loading, other states, queued modes and system messages there still deny.
 mode[6]=mode[7]=mode[8]=(uintptr_t)mode+0x30;
 gm[0x58A]=1;CHECK(deny());gm[0x58A]=0;
 for(uint32_t state:{0u,2u,9u,10u}){mode[2]=state;CHECK(deny());}mode[2]=3;
 messages[24]=1;CHECK(deny());messages[24]=0;
 manager[3]=0x100;manager[4]=0x108;CHECK(deny());manager[3]=manager[4]=0;
 mode[1]=(uintptr_t)base+0x44C91E0;CHECK(deny());mode[1]=(uintptr_t)base+0x44C91D0;
 arm();gate.since=GetTickCount64();CHECK(!ActionAllowed("press-any-not-stable"));
 mode[6]=100;mode[7]=200;mode[8]=300;mode[11]=1;
 for(uint32_t state:{0u,1u,2u,6u,9u,10u,18u,99u}){mode[2]=state;CHECK(deny());}
 mode[2]=3;messages[24]=1;CHECK(deny());messages[24]=0;
 *(uintptr_t*)(base+0x45BBF80)=1;CHECK(deny());*(uintptr_t*)(base+0x45BBF80)=(uintptr_t)messages;
 manager[3]=0x100;manager[4]=0x108;CHECK(deny());manager[4]=0x100;arm();CHECK(Allowed());
 gm[0x58A]=1;CHECK(deny());gm[0x58A]=0;
 known=false;CHECK(deny());known=true;
 mode[1]=(uintptr_t)base+0x44C91E0;CHECK(deny());mode[1]=(uintptr_t)base+0x44C91D0;
 arm();polled-=600;CHECK(!ActionAllowed("stale"));
 arm();gate.top++;CHECK(!ActionAllowed("new-object"));
 arm();gate.since=GetTickCount64();CHECK(!ActionAllowed("not-stable"));
 uintptr_t invalid=123;CHECK(!Rd((uintptr_t)1,invalid)&&invalid==0);
 void* unreadable=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS);CHECK(unreadable);
 stack[0]=(uintptr_t)unreadable;CHECK(deny());stack[0]=(uintptr_t)mode;
 VirtualFree(unreadable,0,MEM_RELEASE);
 arm();CHECK(ActionAllowed("good-again"));CHECK(logs>=20);
 // An action fault resets accumulated stability even if the next read recovers.
 stack[0]=1;CHECK(!ActionAllowed("fault"));stack[0]=(uintptr_t)mode;CHECK(!ActionAllowed("recovered-too-soon"));
 manager[1]=manager[0];CHECK(!Read().readable);manager[1]=manager[0]+5000;CHECK(!Read().readable);
 VirtualFree(base,0,MEM_RELEASE);base=nullptr;
 printf("ALL PASSED: %d save-action checks, %d action recheck logs\n",checks,logs);return 0;
}
