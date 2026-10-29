#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include "savefiles.h"
#include "savelib.h"
#include <atomic>
namespace savefiles {
namespace {
uint8_t* base=nullptr;bool known=false,enabled=false,probe=false,allowed=false,realRoot=false;
void (*logfn)(const char*,...)=nullptr;Gate gate;uint64_t polled=0;
std::wstring sandbox,helper,python,saveRoot;char note[200]="Copy-only test: choose a slot, then queue and quit normally.";
HANDLE child=nullptr;std::wstring cancel;int lastVerdict=-1;uint32_t lastState=~0u;
bool requestCancelled=false;
bool liveEnabled=false,liveResetArmed=false,liveChild=false,liveCloseMenu=false;std::wstring liveHelper;uint64_t liveArmedAt=0;
std::vector<std::string> slotNames;std::string pendingLive,activeSlot;uint64_t importStart=0,doneAt=0;
HANDLE setupChild=nullptr;bool setupAttempted=false;
// [SaveFiles] Native = 1 (default): save operations run in-process (savelib.cpp, plain Win32 file APIs that Wine/Proton implement),
// no Python. Native = 0 keeps the old Python helper processes. The worker thread asks NativeGate() before every mutation:
// the idle-title verdict Poll() publishes from the present thread, valid for 500 ms.
bool native=true;std::atomic<bool> gateOk{false};std::atomic<uint64_t> gateAt{0};
bool NativeGate(){return gateOk.load()&&GetTickCount64()-gateAt.load()<500;}
DWORD ExitCode(HANDLE h){DWORD c=1;if(!GetExitCodeThread(h,&c)&&!GetExitCodeProcess(h,&c))c=1;return c;}
void Result(const char* what){const char* r=savelib::LastResult();snprintf(note,sizeof note,"%s%s%s",what,*r?": ":"",r);}
void SetActive(){activeSlot=pendingLive;const std::wstring f=sandbox+L"\\active.txt";HANDLE h=CreateFileW(f.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h!=INVALID_HANDLE_VALUE){DWORD w;WriteFile(h,activeSlot.data(),(DWORD)activeSlot.size(),&w,nullptr);CloseHandle(h);}}
// Engine objects can disappear between samples. Every individual load is SEH
// protected, including the last admission read before helper dispatch.
template<class T> bool Rd(uintptr_t p,T& out){
 if(!p){out=T{};return false;}
 __try { out=*reinterpret_cast<const volatile T*>(p);return true; }
 __except(EXCEPTION_EXECUTE_HANDLER){out=T{};return false;}
}
struct Reader { template<class T> bool operator()(uintptr_t p,T& out) const {return Rd(p,out);} };
Sample Read() {
 Sample s;if(!known||!base)return s;
 Reader rd;s=titlegate::Read((uintptr_t)base,rd);
 // Retain the independent system-message exclusion. Faults remain a hard denial.
 uintptr_t manager=0,messages=0,again=0;
 if(!s.readable || !Rd((uintptr_t)base+0x45BBF80,manager)||!manager||
    !Rd(manager+0xC0,messages)||!Rd((uintptr_t)base+0x45BBF80,again)||again!=manager||
    !Rd(manager+0xC0,again)||again!=messages||messages){s.readable=false;}
 return s;
}
std::wstring Value(const wchar_t* key,const wchar_t* fallback,const wchar_t* ini){wchar_t b[2048]{};GetPrivateProfileStringW(L"SaveFiles",key,fallback,b,2048,ini);return b;}
std::wstring Quote(const std::wstring& s){std::wstring out=L"\"";unsigned slashes=0;for(wchar_t c:s){if(c==L'\\'){++slashes;continue;}if(c==L'\"')out.append(slashes*2+1,L'\\');else out.append(slashes,L'\\');out+=c;slashes=0;}out.append(slashes*2,L'\\');return out+L"\"";}
bool File(const std::wstring& path){const DWORD a=GetFileAttributesW(path.c_str());return a!=INVALID_FILE_ATTRIBUTES&&!(a&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));}
std::wstring Env(const wchar_t* name){wchar_t value[32768]{};const DWORD n=GetEnvironmentVariableW(name,value,32768);return n&&n<32768?value:L"";}
std::wstring FindPython(const std::wstring& local){
 // Prefer actual per-user installations; WindowsApps execution aliases can open the Store.
 for(const auto& folder:{local+L"\\Python",local+L"\\Programs\\Python"}){
  WIN32_FIND_DATAW d{};HANDLE h=FindFirstFileW((folder+L"\\*").c_str(),&d);
  if(h==INVALID_HANDLE_VALUE)continue;std::wstring found;
  do{if((d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&d.cFileName[0]!=L'.'){
   const auto p=folder+L"\\"+d.cFileName+L"\\python.exe";if(File(p)){found=p;break;}
  }}while(FindNextFileW(h,&d));FindClose(h);if(!found.empty())return found;
 }
 wchar_t path[32768]{};
 for(const auto name:{L"py.exe",L"python.exe"}){const DWORD n=SearchPathW(nullptr,name,nullptr,32768,path,nullptr);
  if(n&&n<32768&&File(path)&&std::wstring(path).find(L"WindowsApps")==std::wstring::npos)return path;}
 return L"";
}
void StartSetup(){
 if(setupAttempted||setupChild||!allowed||sandbox.empty()||saveRoot.empty())return;
 if(File(sandbox+L"\\sandbox.json")){setupAttempted=true;return;}
 setupAttempted=true;
 if(native){setupChild=savelib::StartBootstrap(sandbox,saveRoot);
  if(!setupChild){Result("Save library setup could not start");return;}
  snprintf(note,sizeof note,"Creating a verified copy of your saves...");if(logfn)logfn("SaveFiles: automatic library setup started (native)");return;}
 if(!File(python)||!File(liveHelper)){snprintf(note,sizeof note,"Save manager needs Python and the shipped save_helpers folder.");return;}
 std::wstring cmd=Quote(python)+L" "+Quote(liveHelper)+L" --bootstrap --sandbox "+Quote(sandbox)+L" --save-root "+Quote(saveRoot);
 STARTUPINFOW si{};si.cb=sizeof si;PROCESS_INFORMATION pi{};
 if(!CreateProcessW(python.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){
  snprintf(note,sizeof note,"Save library setup could not start (%lu).",GetLastError());return;}
 CloseHandle(pi.hThread);setupChild=pi.hProcess;
 snprintf(note,sizeof note,"Creating a verified copy of your saves...");
 if(logfn)logfn("SaveFiles: automatic library setup started pid=%lu",pi.dwProcessId);
}
void Cancel(){if(native)savelib::RequestCancel();if(cancel.empty())return;HANDLE f=CreateFileW(cancel.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
 else {if(logfn)logfn("SaveFiles: cancel marker failed (%lu); terminating waiting helper",GetLastError());if(child&&!native)TerminateProcess(child,1);}}
}
void Configure(uint8_t* b,bool k,const wchar_t* ini,void (*log)(const char*,...)){
 base=b;known=k;logfn=log;enabled=GetPrivateProfileIntW(L"SaveFiles",L"Enabled",1,ini)!=0;
 realRoot=GetPrivateProfileIntW(L"SaveFiles",L"RealRoot",1,ini)!=0;
 native=GetPrivateProfileIntW(L"SaveFiles",L"Native",1,ini)!=0;
 saveRoot=Value(L"SaveRoot",L"",ini);
 probe=GetPrivateProfileIntW(L"SaveFiles",L"Probe",0,ini)!=0;
 liveEnabled=GetPrivateProfileIntW(L"SaveFiles",L"LiveTitle",1,ini)!=0;liveHelper=Value(L"LiveHelper",L"",ini);liveResetArmed=false;liveChild=false;
 sandbox=Value(L"CopySandbox",L"",ini);helper=Value(L"Helper",L"",ini);python=Value(L"Python",L"",ini);
 const std::wstring local=Env(L"LOCALAPPDATA");std::wstring folder=ini?ini:L"";
 const auto slash=folder.find_last_of(L"\\/");folder=slash==std::wstring::npos?L".":folder.substr(0,slash);
 const std::wstring packaged=folder+L"\\save_helpers";
 if(!local.empty()){
  if(saveRoot.empty())saveRoot=local+L"\\KoeiTecmo\\NIOH3";
  if(sandbox.empty())sandbox=local+L"\\Nioh3SaveManager\\library";
 }
 if(helper.empty())helper=packaged+L"\\SaveSlots-after-exit.pyz";
 if(liveHelper.empty())liveHelper=packaged+L"\\SaveSlots-live-title.pyz";
 if(python.empty()&&!native)python=FindPython(local);
 setupAttempted=false;

 // Byte check the game's exact top-mode predicate; never invoke it or patch it.
 static const unsigned char bytes[]={0x48,0x8b,0x15,0xe9,0x41,0x68,0x04,0x33,0xc0,0x48,0x85,0xd2,0x74,0x1c,0x4c,0x8b,0x42,0x08,0x4c,0x39,0x02,0x74,0x13,0x49,0x8b,0x50,0xf8,0x48,0x85,0xd2,0x74,0x0a,0x48,0x8b,0x01,0x48,0x39,0x42,0x08,0x0f,0x94,0xc0,0xc3};
 unsigned char found[sizeof bytes]{};SIZE_T n=0;
 if(!known||!base||!ReadProcessMemory(GetCurrentProcess(),base+0x85960,found,sizeof found,&n)||n!=sizeof found||memcmp(found,bytes,sizeof bytes))known=false;
 allowed=false;gate={};polled=0;
 snprintf(note,sizeof note,"%s: choose a slot, then queue and quit normally.",realRoot?"Full-root backup required":"Copy-only test");
 if(logfn)logfn("SaveFiles: title candidate promoted; Enabled=%d RealRoot=%d; 1000ms stable + fresh action reads",enabled,realRoot);
 if(logfn&&(enabled||probe))logfn("SaveFiles: enabled=%d probe=%d native=%d known top-mode bytes=%d",enabled,probe,native,known);
}
void Poll(){
 if(!enabled&&!probe)return;const uint64_t now=GetTickCount64();if(now-polled<100)return;polled=now;
 const Sample s=Read();const auto verdict=gate.Update(s,now);allowed=enabled&&verdict==titlegate::Candidate;gateOk.store(allowed);gateAt.store(now);
 if(logfn&&(lastVerdict!=(int)verdict||lastState!=s.state)){
  logfn("SaveFilesGate: candidate=%d verdict=%d Enabled=%d readable=%d title=%d state=%u queueEmpty=%d loading=%d top=%p",allowed,(int)verdict,enabled,s.readable,s.title,s.state,s.queueEmpty,s.loading,(void*)s.top);
  lastVerdict=verdict;lastState=s.state;
 }
 if(setupChild&&WaitForSingleObject(setupChild,0)==WAIT_OBJECT_0){
  const DWORD code=ExitCode(setupChild);CloseHandle(setupChild);setupChild=nullptr;
  if(logfn)logfn("SaveFiles: automatic library setup exited code=%lu %s",code,native?savelib::LastResult():"");
  if(code==0){RefreshSlots();}else if(native)Result("Save copy setup failed; originals unchanged");else snprintf(note,sizeof note,"Save copy setup failed; originals unchanged. Check library setup receipt.");
 }
 if(enabled)StartSetup();
 if(liveEnabled&&logfn){
  // Read-only live-title watch, logged on change: soft-reset pending/countdown, save completion,
  // loaded slot index S+0x28, checksum-fail S+0x41, busy S+0x38, live save op, top mode.
  static uint32_t last[8]={~0u,~0u,~0u,~0u,~0u,~0u,~0u,~0u};uint32_t v[8]={~1u,~1u,~1u,~1u,~1u,~1u,~1u,~1u};
  uintptr_t holder=0,app=0,m=0,S=0,op=0;uint8_t b8=0;uint32_t u32=0;
  if(Rd((uintptr_t)base+0x45B9E40,holder)&&Rd(holder,app)&&app){if(Rd(app+0x279A,b8))v[0]=b8;float f=0;if(Rd(app+0x279C,f))v[1]=(uint32_t)(f>0?f:0);}
  if(Rd((uintptr_t)base+0x45C8480,m)&&m){if(Rd(m+0x10,u32))v[2]=u32;if(Rd(m,S)&&S){if(Rd(S+0x28,u32))v[3]=u32;if(Rd(S+0x41,b8))v[4]=b8;if(Rd(S+0x38,b8))v[5]=b8;}}
  if(Rd((uintptr_t)base+0x45C6EC0,op))v[6]=op?1u:0u;
  v[7]=(uint32_t)(s.top&0xFFFFFFFFu);
  if(memcmp(v,last,sizeof v)){logfn("LiveTitleWatch: reset279A=%u countdown=%u m+10=%u S+28(slot)=%d S+41(ckfail)=%u S+38(busy)=%u op=%u top=%p state=%u title=%d",v[0],v[1],v[2],(int)v[3],v[4],v[5],v[6],(void*)s.top,s.state,s.title);memcpy(last,v,sizeof v);}
 }
 if(child){if(WaitForSingleObject(child,0)==WAIT_OBJECT_0){const DWORD code=ExitCode(child);CloseHandle(child);child=nullptr;if(logfn)logfn("SaveFiles: helper exited code=%lu live=%d cancelled=%d native=%d %s",code,(int)liveChild,(int)requestCancelled,(int)native,native?savelib::LastResult():"");if(liveChild&&code==0&&!requestCancelled){SetActive();liveResetArmed=true;liveCloseMenu=true;liveArmedAt=now;snprintf(note,sizeof note,"Imported. Refreshing title in this session...");}
  else if(native&&code==0&&!requestCancelled&&!liveChild){snprintf(note,sizeof note,"Backup saved.");RefreshSlots();}
  else if(native)Result(requestCancelled?"Cancelled":"Refused; nothing overwritten");
  else snprintf(note,sizeof note,"Helper ended (%lu). Check helper receipt.",code);liveChild=false;cancel.clear();}
 }
 if(liveResetArmed){
  // Native soft reset (RVA 1C205A4): one call, game's own fade/teardown; requires an idle title gate and no pending reset.
  if(now-liveArmedAt>15000){liveResetArmed=false;snprintf(note,sizeof note,"Imported, but title not idle for reset. Choose Load Game.");return;}
  if(!allowed||!ActionAllowed("live-soft-reset"))return;
  liveResetArmed=false;
  uintptr_t holder=0,app=0;uint8_t pend=1;
  if(!Rd((uintptr_t)base+0x45B9E40,holder)||!Rd(holder,app)||!app||!Rd(app+0x279A,pend)||pend!=0){snprintf(note,sizeof note,"Imported, reset refused (game busy). Choose Load Game.");return;}
  __try{reinterpret_cast<void(*)(void*)>((uintptr_t)base+0x1C205A4)(reinterpret_cast<void*>(holder));snprintf(note,sizeof note,"Imported; title reloading with the newest save.");doneAt=GetTickCount64();if(logfn)logfn("SaveFiles: live soft reset requested (holder=%p app=%p)",(void*)holder,(void*)app);}
  __except(EXCEPTION_EXECUTE_HANDLER){snprintf(note,sizeof note,"Soft reset faulted.");if(logfn)logfn("SaveFiles: soft reset FAULT");}
 }
}
bool Check(const char* action){
 const auto now=GetTickCount64();const auto s=Read();
 const bool fresh=now>=polled&&now-polled<500;
 const auto verdict=gate.Update(s,now);
 const bool result=enabled&&allowed&&fresh&&verdict==titlegate::Candidate;
 if(!result)allowed=false;
 if(action&&logfn)logfn("SaveFilesAction: action=%s allow=%d Enabled=%d fresh=%d verdict=%d readable=%d title=%d state=%u queueEmpty=%d loading=%d top=%p +30=%p +38=%p +40=%p +58=%p",
  action,result,enabled,fresh,(int)verdict,s.readable,s.title,s.state,s.queueEmpty,s.loading,(void*)s.top,(void*)s.p30,(void*)s.p38,(void*)s.p40,(void*)s.p58);
 return result;
}
bool Allowed(){return Check(nullptr);}
bool ActionAllowed(const char* action){return Check(action);}
int Progress(char* stage,size_t n){
 const uint64_t t=GetTickCount64();
 if(liveChild&&child){const uint64_t e=t-importStart;int p=5+(int)(e/100);if(p>60)p=60;snprintf(stage,n,"Step 1/3: copying the save in (%s)",pendingLive.c_str());return p;}
 if(liveResetArmed){snprintf(stage,n,"Step 2/3: waiting for the title to be idle");return 70;}
 if(doneAt&&t-doneAt<8000){const uint64_t e=t-doneAt;if(e<3000){snprintf(stage,n,"Step 3/3: reloading the title with your save");return 85;}snprintf(stage,n,"Done: now using %s",activeSlot.c_str());return 100;}
 return -1;
}
bool Pending(){return child&&!requestCancelled;}
bool LivePending(){return Pending()&&liveChild;}
bool TakeMenuCloseRequest(){const bool r=liveCloseMenu;liveCloseMenu=false;return r;}
void CancelPending(){
 if(logfn)logfn("SaveFilesAction: action=cancel pending=%d (no save writes)",Pending());
 if(Pending()){Cancel();requestCancelled=true;snprintf(note,sizeof note,"Queued save operation cancelled.");}
}
const char* Note(){return note;}
bool ValidSlotName(const char* name){
 if(!name)return false;const size_t n=strlen(name);if(!n||n>48||name[n-1]==' ')return false;
 auto alpha=[](char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9');};
 if(!alpha(name[0]))return false;
 for(size_t i=1;i<n;++i)if(!alpha(name[i])&&name[i]!=' '&&name[i]!='_'&&name[i]!='-')return false;
 if(!_stricmp(name,"CON")||!_stricmp(name,"PRN")||!_stricmp(name,"AUX")||!_stricmp(name,"NUL"))return false;
 if(n==4&&(!_strnicmp(name,"COM",3)||!_strnicmp(name,"LPT",3))&&name[3]>='1'&&name[3]<='9')return false;
 return true;
}
void RefreshSlots(){
 if(!ActionAllowed("refresh-slots"))return;
 slotNames.clear();const std::wstring dir=sandbox+L"\\slots";
 const DWORD attrs=GetFileAttributesW(dir.c_str());
 if(sandbox.empty()||attrs==INVALID_FILE_ATTRIBUTES||!(attrs&FILE_ATTRIBUTE_DIRECTORY)||(attrs&FILE_ATTRIBUTE_REPARSE_POINT)){
  if(setupChild)snprintf(note,sizeof note,"Creating a verified copy of your saves...");else snprintf(note,sizeof note,native?"Save library unavailable; see Nioh3EngineFix.log (SaveFiles).":"Save library unavailable; check Python and shipped save_helpers.");return;
 }
 WIN32_FIND_DATAW data{};HANDLE find=FindFirstFileW((dir+L"\\*").c_str(),&data);
 if(find==INVALID_HANDLE_VALUE){snprintf(note,sizeof note,"Could not read the slot library.");return;}
 bool overflow=false;
 do{
  if(!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||(data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))continue;
  std::string name;for(const wchar_t* c=data.cFileName;*c;++c){if(*c>127){name.clear();break;}name+=(char)*c;}
  if(!ValidSlotName(name.c_str()))continue;
  const DWORD manifest=GetFileAttributesW((dir+L"\\"+data.cFileName+L"\\manifest.json").c_str());
  if(manifest==INVALID_FILE_ATTRIBUTES||(manifest&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))continue;
  slotNames.push_back(name);if(slotNames.size()>kMaxSlots){overflow=true;break;}
 }while(FindNextFileW(find,&data));
 const DWORD err=GetLastError();FindClose(find);
 if(overflow||err!=ERROR_NO_MORE_FILES){slotNames.clear();snprintf(note,sizeof note,"Slot scan refused: %s.",overflow?"more than 64 slots":"read error");return;}
 std::sort(slotNames.begin(),slotNames.end());
 snprintf(note,sizeof note,"%u named saves. Pick a save to view switch options.",(unsigned)slotNames.size());
}
bool IsActive(int index){
 if(index<0||index>=(int)slotNames.size())return false;
 if(activeSlot.empty()){char b[96]{};DWORD r=0;const std::wstring f=sandbox+L"\\active.txt";HANDLE h=CreateFileW(f.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(h!=INVALID_HANDLE_VALUE){ReadFile(h,b,sizeof b-1,&r,nullptr);CloseHandle(h);}activeSlot=r?std::string(b,r):"My current save";}
 return slotNames[index]==activeSlot;
}
int SlotCount(){return (int)slotNames.size();}
const char* SlotName(int index){return index>=0&&index<SlotCount()?slotNames[index].c_str():"";}
bool QueueNamed(const char* name,bool backup){
 if(!ActionAllowed(backup?"backup-request":"switch-request")||!ValidSlotName(name)||child){snprintf(note,sizeof note,"Unavailable: title state changed or a request is active.");return false;}
 if(native){
  if(!backup)return QueueLive(name);  // native switch = live import at the title (no quit, no full-root replacement)
  if(!realRoot||sandbox.empty()||saveRoot.empty()){snprintf(note,sizeof note,"Native backup needs RealRoot=1, SaveRoot and CopySandbox.");return false;}
  wchar_t tok[96];swprintf(tok,96,L"bk-%lu-%llu",GetCurrentProcessId(),GetTickCount64());cancel=sandbox+L"\\cancel-"+tok;
  if(!ActionAllowed("backup-dispatch")){snprintf(note,sizeof note,"Cancelled: title state or request queue changed.");cancel.clear();return false;}
  requestCancelled=false;child=savelib::StartBackup(sandbox,saveRoot,name,tok,&NativeGate);
  if(!child){Result("Backup could not start");cancel.clear();return false;}
  liveChild=false;snprintf(note,sizeof note,"Backing up your saves as %s...",name);if(logfn)logfn("SaveFiles: native backup dispatched slot=%s",name);return true;
 }
 if(sandbox.empty()||helper.empty()||python.empty()||(realRoot&&saveRoot.empty())){snprintf(note,sizeof note,"Configure CopySandbox, Helper, Python and (for RealRoot) SaveRoot.");return false;}
 FILETIME created,exit,kernel,user;if(!GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user))return false;
 const unsigned long long stamp=((unsigned long long)created.dwHighDateTime<<32)|created.dwLowDateTime;
 wchar_t token[96];swprintf(token,96,L"%lu-%llu-%llu",GetCurrentProcessId(),stamp,GetTickCount64());
 cancel=sandbox+L"\\cancel-"+token;
 const std::string slot(name);const std::wstring wideSlot(slot.begin(),slot.end());
 std::wstring cmd=Quote(python)+L" "+Quote(helper)+L" --sandbox "+Quote(sandbox)+L" --pid "+std::to_wstring(GetCurrentProcessId())+L" --created "+std::to_wstring(stamp)+L" --slot "+Quote(wideSlot)+L" --token "+Quote(token)+(backup?L" --action backup":L" --action switch");
 if(realRoot)cmd+=L" --real-root --save-root "+Quote(saveRoot);
 STARTUPINFOW si{};si.cb=sizeof si;PROCESS_INFORMATION pi{};
 // Final fresh SEH-backed admission after argument construction, immediately
 // before the only operation dispatch. The helper still waits for clean exit.
 if(!ActionAllowed(backup?"backup-dispatch":"switch-dispatch")){snprintf(note,sizeof note,"Cancelled: title state or request queue changed.");cancel.clear();return false;}
 requestCancelled=false;
 if(!CreateProcessW(python.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){snprintf(note,sizeof note,"Helper launch failed (%lu).",GetLastError());cancel.clear();return false;}
 CloseHandle(pi.hThread);child=pi.hProcess;snprintf(note,sizeof note,"%s queued: %s%s.",backup?"Backup":"Switch",realRoot?"":"copy ",name);
 return true;
}
bool Queue(int slot,bool backup){if(slot<1||slot>8)return false;return QueueNamed(("slot"+std::to_string(slot)).c_str(),backup);}
bool QueueNewBackup(){
 SYSTEMTIME t{};GetLocalTime(&t);char name[49];
 snprintf(name,sizeof name,"Backup-%04u%02u%02u-%02u%02u%02u-%03u",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);
 return QueueNamed(name,true);
}
void OpenFolder(){const std::wstring dir=saveRoot.empty()?sandbox:saveRoot;if(dir.empty()){snprintf(note,sizeof note,"No saves folder configured.");return;}ShellExecuteW(nullptr,L"open",dir.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
bool LiveAvailable(){return liveEnabled&&!sandbox.empty()&&!saveRoot.empty()&&(native||(!liveHelper.empty()&&!python.empty()));}
bool Native(){return native;}
bool QueueLive(const char* name){
 if(!LiveAvailable()||!ValidSlotName(name)||child||!ActionAllowed("live-import-request")){snprintf(note,sizeof note,native?"Unavailable: set LiveTitle=1, SaveRoot, CopySandbox; title must be idle.":"Unavailable: set LiveTitle=1, LiveHelper, SaveRoot, CopySandbox, Python; title must be idle.");return false;}
 FILETIME created,exit,kernel,user;if(!GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user))return false;
 const unsigned long long stamp=((unsigned long long)created.dwHighDateTime<<32)|created.dwLowDateTime;
 wchar_t token[96];swprintf(token,96,L"live-%lu-%llu-%llu",GetCurrentProcessId(),stamp,GetTickCount64());
 cancel=sandbox+L"\\cancel-"+token;
 const std::string slot(name);const std::wstring wideSlot(slot.begin(),slot.end());
 if(native){
  if(!ActionAllowed("live-import-dispatch")){snprintf(note,sizeof note,"Cancelled: title state changed.");cancel.clear();return false;}
  requestCancelled=false;child=savelib::StartLiveImport(sandbox,saveRoot,slot,token,true,&NativeGate);
  if(!child){Result("Import could not start");cancel.clear();return false;}
  liveChild=true;pendingLive=name;importStart=GetTickCount64();doneAt=0;snprintf(note,sizeof note,"Importing %s at the title...",name);
  if(logfn)logfn("SaveFiles: native live import dispatched slot=%s",name);return true;
 }
 std::wstring cmd=Quote(python)+L" "+Quote(liveHelper)+L" --sandbox "+Quote(sandbox)+L" --save-root "+Quote(saveRoot)+L" --slot "+Quote(wideSlot)+L" --token "+Quote(token)+L" --pid "+std::to_wstring(GetCurrentProcessId())+L" --created "+std::to_wstring(stamp)+L" --make-newest";
 STARTUPINFOW si{};si.cb=sizeof si;PROCESS_INFORMATION pi{};
 if(!ActionAllowed("live-import-dispatch")){snprintf(note,sizeof note,"Cancelled: title state changed.");cancel.clear();return false;}
 requestCancelled=false;
 if(!CreateProcessW(python.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){snprintf(note,sizeof note,"Helper launch failed (%lu).",GetLastError());cancel.clear();return false;}
 CloseHandle(pi.hThread);child=pi.hProcess;liveChild=true;pendingLive=name;importStart=GetTickCount64();doneAt=0;snprintf(note,sizeof note,"Importing %s at the title...",name);
 if(logfn)logfn("SaveFiles: live import dispatched slot=%s helper pid=%lu",name,pi.dwProcessId);
 return true;
}
}
