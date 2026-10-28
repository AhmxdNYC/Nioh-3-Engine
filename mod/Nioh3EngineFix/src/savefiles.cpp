#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include "savefiles.h"
namespace savefiles {
namespace {
uint8_t* base=nullptr;bool known=false,enabled=false,probe=false,allowed=false,realRoot=false;
void (*logfn)(const char*,...)=nullptr;Gate gate;uint64_t polled=0;
std::wstring sandbox,helper,python,saveRoot;char note[200]="Copy-only test: choose a slot, then queue and quit normally.";
HANDLE child=nullptr;std::wstring cancel;int lastVerdict=-1;uint32_t lastState=~0u;
bool requestCancelled=false;
bool liveEnabled=false,liveResetArmed=false,liveChild=false,liveCloseMenu=false;std::wstring liveHelper;uint64_t liveArmedAt=0;
std::vector<std::string> slotNames;
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
void Cancel(){if(cancel.empty())return;HANDLE f=CreateFileW(cancel.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
 else {if(logfn)logfn("SaveFiles: cancel marker failed (%lu); terminating waiting helper",GetLastError());if(child)TerminateProcess(child,1);}}
}
void Configure(uint8_t* b,bool k,const wchar_t* ini,void (*log)(const char*,...)){
 base=b;known=k;logfn=log;enabled=GetPrivateProfileIntW(L"SaveFiles",L"Enabled",1,ini)!=0;
 realRoot=GetPrivateProfileIntW(L"SaveFiles",L"RealRoot",0,ini)!=0;
 saveRoot=Value(L"SaveRoot",L"",ini);
 probe=GetPrivateProfileIntW(L"SaveFiles",L"Probe",0,ini)!=0;
 liveEnabled=GetPrivateProfileIntW(L"SaveFiles",L"LiveTitle",1,ini)!=0;liveHelper=Value(L"LiveHelper",L"",ini);liveResetArmed=false;liveChild=false;
 sandbox=Value(L"CopySandbox",L"",ini);helper=Value(L"Helper",L"",ini);python=Value(L"Python",L"",ini);
 // Byte check the game's exact top-mode predicate; never invoke it or patch it.
 static const unsigned char bytes[]={0x48,0x8b,0x15,0xe9,0x41,0x68,0x04,0x33,0xc0,0x48,0x85,0xd2,0x74,0x1c,0x4c,0x8b,0x42,0x08,0x4c,0x39,0x02,0x74,0x13,0x49,0x8b,0x50,0xf8,0x48,0x85,0xd2,0x74,0x0a,0x48,0x8b,0x01,0x48,0x39,0x42,0x08,0x0f,0x94,0xc0,0xc3};
 unsigned char found[sizeof bytes]{};SIZE_T n=0;
 if(!known||!base||!ReadProcessMemory(GetCurrentProcess(),base+0x85960,found,sizeof found,&n)||n!=sizeof found||memcmp(found,bytes,sizeof bytes))known=false;
 allowed=false;gate={};polled=0;
 snprintf(note,sizeof note,"%s: choose a slot, then queue and quit normally.",realRoot?"Full-root backup required":"Copy-only test");
 if(logfn)logfn("SaveFiles: title candidate promoted; Enabled=%d RealRoot=%d; 1000ms stable + fresh action reads",enabled,realRoot);
 if(logfn&&(enabled||probe))logfn("SaveFiles: enabled=%d probe=%d known top-mode bytes=%d",enabled,probe,known);
}
void Poll(){
 if(!enabled&&!probe)return;const uint64_t now=GetTickCount64();if(now-polled<100)return;polled=now;
 const Sample s=Read();const auto verdict=gate.Update(s,now);allowed=enabled&&verdict==titlegate::Candidate;
 if(logfn&&(lastVerdict!=(int)verdict||lastState!=s.state)){
  logfn("SaveFilesGate: candidate=%d verdict=%d Enabled=%d readable=%d title=%d state=%u queueEmpty=%d loading=%d top=%p",allowed,(int)verdict,enabled,s.readable,s.title,s.state,s.queueEmpty,s.loading,(void*)s.top);
  lastVerdict=verdict;lastState=s.state;
 }
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
 if(child){if(WaitForSingleObject(child,0)==WAIT_OBJECT_0){DWORD code=1;GetExitCodeProcess(child,&code);CloseHandle(child);child=nullptr;if(logfn)logfn("SaveFiles: helper exited code=%lu live=%d cancelled=%d",code,(int)liveChild,(int)requestCancelled);if(liveChild&&code==0&&!requestCancelled){liveResetArmed=true;liveCloseMenu=true;liveArmedAt=now;snprintf(note,sizeof note,"Imported. Refreshing title in this session...");}
  else snprintf(note,sizeof note,"Helper ended (%lu). Check helper receipt.",code);liveChild=false;cancel.clear();}
 }
 if(liveResetArmed){
  // Native soft reset (RVA 1C205A4): one call, game's own fade/teardown; requires an idle title gate and no pending reset.
  if(now-liveArmedAt>15000){liveResetArmed=false;snprintf(note,sizeof note,"Imported, but title not idle for reset. Choose Load Game.");return;}
  if(!allowed||!ActionAllowed("live-soft-reset"))return;
  liveResetArmed=false;
  uintptr_t holder=0,app=0;uint8_t pend=1;
  if(!Rd((uintptr_t)base+0x45B9E40,holder)||!Rd(holder,app)||!app||!Rd(app+0x279A,pend)||pend!=0){snprintf(note,sizeof note,"Imported, reset refused (game busy). Choose Load Game.");return;}
  __try{reinterpret_cast<void(*)(void*)>((uintptr_t)base+0x1C205A4)(reinterpret_cast<void*>(holder));snprintf(note,sizeof note,"Imported; title reloading with the newest save.");if(logfn)logfn("SaveFiles: live soft reset requested (holder=%p app=%p)",(void*)holder,(void*)app);}
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
  snprintf(note,sizeof note,"No slot library. Configure CopySandbox and create a backup first.");return;
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
int SlotCount(){return (int)slotNames.size();}
const char* SlotName(int index){return index>=0&&index<SlotCount()?slotNames[index].c_str():"";}
bool QueueNamed(const char* name,bool backup){
 if(!ActionAllowed(backup?"backup-request":"switch-request")||!ValidSlotName(name)||child){snprintf(note,sizeof note,"Unavailable: title state changed or a request is active.");return false;}
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
bool LiveAvailable(){return liveEnabled&&!liveHelper.empty()&&!sandbox.empty()&&!python.empty()&&!saveRoot.empty();}
bool QueueLive(const char* name){
 if(!LiveAvailable()||!ValidSlotName(name)||child||!ActionAllowed("live-import-request")){snprintf(note,sizeof note,"Unavailable: set LiveTitle=1, LiveHelper, SaveRoot, CopySandbox, Python; title must be idle.");return false;}
 FILETIME created,exit,kernel,user;if(!GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user))return false;
 const unsigned long long stamp=((unsigned long long)created.dwHighDateTime<<32)|created.dwLowDateTime;
 wchar_t token[96];swprintf(token,96,L"live-%lu-%llu-%llu",GetCurrentProcessId(),stamp,GetTickCount64());
 cancel=sandbox+L"\\cancel-"+token;
 const std::string slot(name);const std::wstring wideSlot(slot.begin(),slot.end());
 std::wstring cmd=Quote(python)+L" "+Quote(liveHelper)+L" --sandbox "+Quote(sandbox)+L" --save-root "+Quote(saveRoot)+L" --slot "+Quote(wideSlot)+L" --token "+Quote(token)+L" --pid "+std::to_wstring(GetCurrentProcessId())+L" --created "+std::to_wstring(stamp)+L" --make-newest";
 STARTUPINFOW si{};si.cb=sizeof si;PROCESS_INFORMATION pi{};
 if(!ActionAllowed("live-import-dispatch")){snprintf(note,sizeof note,"Cancelled: title state changed.");cancel.clear();return false;}
 requestCancelled=false;
 if(!CreateProcessW(python.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){snprintf(note,sizeof note,"Helper launch failed (%lu).",GetLastError());cancel.clear();return false;}
 CloseHandle(pi.hThread);child=pi.hProcess;liveChild=true;snprintf(note,sizeof note,"Importing %s at the title...",name);
 if(logfn)logfn("SaveFiles: live import dispatched slot=%s helper pid=%lu",name,pi.dwProcessId);
 return true;
}
}
