// No network or game access: production Fetch and Publish use a deterministic WinHTTP fake.
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdlib>
static std::string response;
static size_t cursor=0,chunk=11;
static DWORD statusCode=200;
static int calls=0,failAt=0,opened=0,closed=0,readFail=0;
static ULONGLONG clockMs=100;
static bool deadline=false;
static bool ok(){return ++calls!=failAt;}
static HINTERNET openFake(){if(!ok())return nullptr;++opened;return reinterpret_cast<HINTERNET>((uintptr_t)opened);}
static HINTERNET Open(...){return openFake();}
static HINTERNET Connect(...){return openFake();}
static HINTERNET Request(...){return openFake();}
static BOOL Timeouts(HINTERNET,int resolve,int connect,int send,int receive){if(resolve!=1000||connect!=2000||send!=2000||receive!=2000)std::abort();return ok();}
static BOOL Option(HINTERNET,DWORD option,LPVOID value,DWORD size){
 if(size!=sizeof(DWORD))std::abort();
 DWORD v=*static_cast<DWORD*>(value);
 if(option==WINHTTP_OPTION_REDIRECT_POLICY && v!=WINHTTP_OPTION_REDIRECT_POLICY_NEVER)std::abort();
 if(option==WINHTTP_OPTION_DISABLE_FEATURE && v!=(WINHTTP_DISABLE_COOKIES|WINHTTP_DISABLE_AUTHENTICATION))std::abort();
 if(option==WINHTTP_OPTION_CONNECT_RETRIES && v!=0)std::abort();
 return ok();
}
static BOOL Send(...){return ok();}
static BOOL Receive(...){return ok();}
static BOOL Headers(HINTERNET,DWORD,LPCWSTR,LPVOID out,LPDWORD,LPDWORD){*static_cast<DWORD*>(out)=statusCode;return ok();}
static BOOL Read(HINTERNET,LPVOID dst,DWORD count,LPDWORD got){
 if(readFail || !ok())return FALSE;
 size_t n=std::min((size_t)count,std::min(chunk,response.size()-cursor));
 std::memcpy(dst,response.data()+cursor,n);cursor+=n;*got=(DWORD)n;
 if(deadline)clockMs+=8000;
 return TRUE;
}
static BOOL Close(HINTERNET){++closed;return TRUE;}
static ULONGLONG Clock(){return clockMs;}
#define WinHttpOpen Open
#define WinHttpConnect Connect
#define WinHttpOpenRequest Request
#define WinHttpSetTimeouts Timeouts
#define WinHttpSetOption Option
#define WinHttpSendRequest Send
#define WinHttpReceiveResponse Receive
#define WinHttpQueryHeaders Headers
#define WinHttpReadData Read
#define WinHttpCloseHandle Close
#define GetTickCount64 Clock
#include "../src/updatecheck.cpp"
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int checks=0;
static void reset(){response="{\"version\":\"1.8.0\"}";cursor=0;chunk=11;statusCode=200;calls=failAt=opened=closed=readFail=0;clockMs=100;deadline=false;}
int main(){
 using namespace updatecheck;
 Version v{};
 CHECK(Available("{\"version\":\"1.8.0\"}","1.7.80-settings37433",v));
 CHECK(Available(" { \"version\" : \"1.7.81\" }\r\n","1.7.80",v));
 CHECK(Available("{\"version\":\"2.0.0\"}","1.999999.999999",v));
 CHECK(!Available("{\"version\":\"1.7.80\"}","1.7.80-settings37433",v));
 CHECK(!Available("{\"version\":\"1.7.9\"}","1.7.80",v));
 CHECK(!Available("{\"version\":\"1.8.0\"}","broken",v));
 for(const char* s:{"", "{}","[]","<html>offline</html>","{\"version\":3}","{\"version\":\"1.8\"}","{\"version\":\"1.8.0\",\"version\":\"9.0.0\"}","{\"version\":\"1.8.0\",\"url\":\"bad\"}","{\"version\":\"01.8.0\"}","{\"version\":\"1.8.-1\"}","{\"version\":\"1.8.1000000\"}","{\"version\":\"1.8.0<script>\"}","{\"version\":\"1.8.0-beta\"}","{\"version\":\"1.8.0\"}junk"}) CHECK(!ParseManifest(s,v));
 CHECK(!ParseManifest(std::string(513,' '),v));
 char body[kMaxBody+1]{};size_t length=0;
 reset();CHECK(Fetch(body,length));CHECK(std::string(body,length)==response);CHECK(opened==closed);
 for(int failure=1;failure<=10;++failure){reset();failAt=failure;CHECK(!Fetch(body,length));CHECK(opened==closed);}
 for(DWORD code:{301u,404u,500u}){reset();statusCode=code;CHECK(!Fetch(body,length));CHECK(opened==closed);}
 reset();readFail=1;CHECK(!Fetch(body,length));CHECK(opened==closed);
 reset();response=std::string(513,'x');CHECK(!Fetch(body,length));CHECK(opened==closed);
 reset();response=std::string(512,' ');CHECK(Fetch(body,length));CHECK(length==512);CHECK(opened==closed);
 reset();deadline=true;chunk=1;CHECK(!Fetch(body,length));CHECK(opened==closed);
 char notice[64]{};
 Publish("{\"version\":\"1.8.0\"}","1.7.80");CHECK(CopyNotice(notice,sizeof(notice)));CHECK(std::string(notice)=="Update available v1.8.0");
 Publish("<html>error</html>","1.7.80");CHECK(CopyNotice(notice,sizeof(notice))); // silent failure retains last validated result
 Publish("{\"version\":\"1.7.80\"}","1.7.80");CHECK(!CopyNotice(notice,sizeof(notice)));
 CHECK(!CopyNotice(nullptr,0));
 std::printf("PASS %d update-check assertions; no network, game or installed files accessed\n",checks);
}
