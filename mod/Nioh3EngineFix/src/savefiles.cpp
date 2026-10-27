#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "savefiles.h"
namespace savefiles {
namespace {
uint8_t* base=nullptr;bool known=false,enabled=false,probe=false,allowed=false;
void (*logfn)(const char*,...)=nullptr;Gate gate;uint64_t polled=0;
std::wstring sandbox,helper,python;char note[200]="Copy-only test: choose a slot, then queue and quit normally.";
HANDLE child=nullptr;std::wstring cancel;int lastTitle=-1,lastLoading=-1,lastSave=-2,lastTitleState=-1,lastQueue=-1;
// Engine objects can disappear between samples. Every individual load is SEH
// protected, including the last admission read before helper dispatch.
template<class T> bool Rd(uintptr_t p,T& out){
 if(!p){out=T{};return false;}
 __try { out=*reinterpret_cast<const volatile T*>(p);return true; }
 __except(EXCEPTION_EXECUTE_HANDLER){out=T{};return false;}
}
Sample Read() {
 Sample s;s.known=known;if(!known||!base)return s;
 uintptr_t mgr=0,begin=0,end=0,mode=0,type=0,gm=0,sm=0;unsigned char loading=1;
 const uintptr_t b=(uintptr_t)base;
 if(!Rd(b+0x4709B50,mgr)||!mgr||!Rd(mgr,begin)||!Rd(mgr+8,end)||end<begin||end-begin>4096||end==begin||((end-begin)&7))return s;
 if(!Rd(end-8,mode)||!mode||!Rd(mode+8,type)||!Rd(b+0x4759910,gm)||!gm||!Rd(gm+0x58A,loading))return s;
 uintptr_t qbegin=0,qend=0;int state=-1;
 if(!Rd(mode+0x10,state)||!Rd(mgr+0x18,qbegin)||!Rd(mgr+0x20,qend))return s;
 // Reject a torn traversal or a transition queued while collecting this sample.
 uintptr_t again=0;int stateAgain=-1;
 if(!Rd(b+0x4709B50,again)||again!=mgr||!Rd(mgr+8,again)||again!=end||
    !Rd(end-8,again)||again!=mode||!Rd(mode+8,again)||again!=type||
    !Rd(mode+0x10,stateAgain)||stateAgain!=state||!Rd(mgr+0x18,again)||again!=qbegin||
    !Rd(mgr+0x20,again)||again!=qend)return s;
 s.titleState=state;s.queueEmpty=qbegin==qend;
 s.readable=true;s.manager=mgr;s.mode=mode;s.title=type==b+0x44C91D0;s.loading=loading!=0;
 if(Rd(b+0x45C8480,sm)&&sm)Rd(sm+0x10,s.saveState);
 return s;
}
std::wstring Value(const wchar_t* key,const wchar_t* fallback,const wchar_t* ini){wchar_t b[2048]{};GetPrivateProfileStringW(L"SaveFiles",key,fallback,b,2048,ini);return b;}
std::wstring Quote(const std::wstring& s){std::wstring out=L"\"";unsigned slashes=0;for(wchar_t c:s){if(c==L'\\'){++slashes;continue;}if(c==L'\"')out.append(slashes*2+1,L'\\');else out.append(slashes,L'\\');out+=c;slashes=0;}out.append(slashes*2,L'\\');return out+L"\"";}
void Cancel(){if(cancel.empty())return;HANDLE f=CreateFileW(cancel.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);}
}
void Configure(uint8_t* b,bool k,const wchar_t* ini,void (*log)(const char*,...)){
 base=b;known=k;logfn=log;enabled=GetPrivateProfileIntW(L"SaveFiles",L"Enabled",0,ini)!=0;
 probe=GetPrivateProfileIntW(L"SaveFiles",L"Probe",0,ini)!=0;
 sandbox=Value(L"CopySandbox",L"",ini);helper=Value(L"Helper",L"",ini);python=Value(L"Python",L"",ini);
 // Byte check the game's exact top-mode predicate; never invoke it or patch it.
 static const unsigned char bytes[]={0x48,0x8b,0x15,0xe9,0x41,0x68,0x04,0x33,0xc0,0x48,0x85,0xd2,0x74,0x1c,0x4c,0x8b,0x42,0x08,0x4c,0x39,0x02,0x74,0x13,0x49,0x8b,0x50,0xf8,0x48,0x85,0xd2,0x74,0x0a,0x48,0x8b,0x01,0x48,0x39,0x42,0x08,0x0f,0x94,0xc0,0xc3};
 unsigned char found[sizeof bytes]{};SIZE_T n=0;
 if(!known||!base||!ReadProcessMemory(GetCurrentProcess(),base+0x85960,found,sizeof found,&n)||n!=sizeof found||memcmp(found,bytes,sizeof bytes))known=false;
 allowed=false;gate={};polled=0;
 if(logfn&&(enabled||probe))logfn("SaveFiles: copy-only enabled=%d probe=%d known top-mode bytes=%d",enabled,probe,known);
}
void Poll(){
 if(!enabled&&!probe)return;const uint64_t now=GetTickCount64();if(now-polled<100)return;polled=now;
 const Sample s=Read();allowed=gate.Update(s,now)&&enabled;
 if(logfn&&(lastTitle!=(int)s.title||lastLoading!=(int)s.loading||lastSave!=s.saveState||lastTitleState!=s.titleState||lastQueue!=(int)s.queueEmpty)){
 logfn("SaveFilesProbe: readable=%d title=%d loading=%d saveState=%d titleState=%d queueEmpty=%d mode=%p manager=%p",s.readable,s.title,s.loading,s.saveState,s.titleState,s.queueEmpty,(void*)s.mode,(void*)s.manager);
 lastTitle=s.title;lastLoading=s.loading;lastSave=s.saveState;lastTitleState=s.titleState;lastQueue=s.queueEmpty;}
 if(child){if(WaitForSingleObject(child,0)==WAIT_OBJECT_0){DWORD code=1;GetExitCodeProcess(child,&code);CloseHandle(child);child=nullptr;snprintf(note,sizeof note,"Helper ended (%lu). Check helper receipt.",code);cancel.clear();}
 else if(!Ready(s)){Cancel();snprintf(note,sizeof note,"Cancelled: left the verified main menu.");}}
}
bool Allowed(){const auto now=GetTickCount64();const auto s=Read();return allowed&&now>=polled&&now-polled<500&&Ready(s)&&s.mode==gate.mode;}
const char* Note(){return note;}
bool Queue(int slot,bool backup){
 if(!Allowed()||slot<1||slot>8||child){snprintf(note,sizeof note,"Unavailable: title state changed or a request is active.");return false;}
 if(sandbox.empty()||helper.empty()||python.empty()){snprintf(note,sizeof note,"CopySandbox, Helper and Python paths must be configured.");return false;}
 FILETIME created,exit,kernel,user;if(!GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user))return false;
 const unsigned long long stamp=((unsigned long long)created.dwHighDateTime<<32)|created.dwLowDateTime;
 wchar_t token[96];swprintf(token,96,L"%lu-%llu-%llu",GetCurrentProcessId(),stamp,GetTickCount64());
 cancel=sandbox+L"\\cancel-"+token;
 std::wstring cmd=Quote(python)+L" "+Quote(helper)+L" --sandbox "+Quote(sandbox)+L" --pid "+std::to_wstring(GetCurrentProcessId())+L" --created "+std::to_wstring(stamp)+L" --slot slot"+std::to_wstring(slot)+L" --token "+Quote(token)+(backup?L" --action backup":L" --action switch");
 STARTUPINFOW si{};si.cb=sizeof si;PROCESS_INFORMATION pi{};
 // Final fresh SEH-backed admission after argument construction, immediately
 // before the only operation dispatch. The helper still waits for clean exit.
 if(!Allowed()){snprintf(note,sizeof note,"Cancelled: title state or request queue changed.");cancel.clear();return false;}
 if(!CreateProcessW(python.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){snprintf(note,sizeof note,"Helper launch failed (%lu).",GetLastError());cancel.clear();return false;}
 CloseHandle(pi.hThread);child=pi.hProcess;snprintf(note,sizeof note,"Queued COPY slot%d. Close mod menu; use the game's Quit within 2 minutes.",slot);
 return true;
}
}
