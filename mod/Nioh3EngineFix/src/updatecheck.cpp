#include "updatecheck.h"
#include <windows.h>
#include <winhttp.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>
namespace updatecheck {
namespace {
SRWLOCK g_lock = SRWLOCK_INIT;
char g_notice[64]{};
std::atomic<bool> g_busy{false};
std::atomic<ULONGLONG> g_next{0};
struct HttpHandle {
    HINTERNET value;
    explicit HttpHandle(HINTERNET h) : value(h) {}
    ~HttpHandle() { if(value) WinHttpCloseHandle(value); }
    HttpHandle(const HttpHandle&)=delete;
    HttpHandle& operator=(const HttpHandle&)=delete;
};
// Fixed HTTPS origin/path, no identifiers, cookies, automatic credentials or redirects.
bool Fetch(char (&body)[kMaxBody+1], size_t& length) {
    length=0;
    const ULONGLONG start=GetTickCount64();
    HttpHandle session(WinHttpOpen(L"Nioh3EngineFix/update-check",WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.value || !WinHttpSetTimeouts(session.value,1000,2000,2000,2000)) return false;
    DWORD retries=0;
    if(!WinHttpSetOption(session.value,WINHTTP_OPTION_CONNECT_RETRIES,&retries,sizeof(retries))) return false;
    HttpHandle connection(WinHttpConnect(session.value,L"nioh3-mod-feedback.vercel.app",INTERNET_DEFAULT_HTTPS_PORT,0));
    if(!connection.value) return false;
    const wchar_t* accept[]={L"application/json",nullptr};
    HttpHandle request(WinHttpOpenRequest(connection.value,L"GET",L"/version.json",nullptr,
        WINHTTP_NO_REFERER,accept,WINHTTP_FLAG_SECURE));
    if(!request.value) return false;
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    DWORD disabled=WINHTTP_DISABLE_COOKIES|WINHTTP_DISABLE_AUTHENTICATION;
    if(!WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect)) ||
       !WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled))) return false;
    if(!WinHttpSendRequest(request.value,L"Cache-Control: no-cache\r\n",(DWORD)-1L,
           WINHTTP_NO_REQUEST_DATA,0,0,0) ||
       !WinHttpReceiveResponse(request.value,nullptr)) return false;
    DWORD status=0,bytes=sizeof(status);
    if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,&status,&bytes,WINHTTP_NO_HEADER_INDEX) || status!=200) return false;
    // One extra byte detects oversized bodies, including responses without Content-Length.
    while(length<sizeof(body)) {
        if(GetTickCount64()-start>=8000) return false;
        DWORD got=0;
        if(!WinHttpReadData(request.value,body+length,(DWORD)(sizeof(body)-length),&got)) return false;
        if(!got) { body[length]='\0'; return true; }
        length+=got;
        if(length>kMaxBody) return false;
    }
    return false;
}
void Publish(std::string_view body,const char* installed) {
    Version remote{},local{};
    if(!ParseManifest(body,remote)||!ParseVersion(installed,local,true)) return;
    char notice[64]{};
    if(Newer(remote,local)) std::snprintf(notice,sizeof(notice),"Update available v%u.%u.%u",
        remote.part[0],remote.part[1],remote.part[2]);
    AcquireSRWLockExclusive(&g_lock);
    std::memcpy(g_notice,notice,sizeof(g_notice));
    ReleaseSRWLockExclusive(&g_lock);
}
struct Context { HMODULE module{}; char installed[64]{}; };
DWORD WINAPI Worker(LPVOID p) {
    auto* ctx=static_cast<Context*>(p);
    char body[kMaxBody+1]{};size_t length=0;
    if(Fetch(body,length)) Publish(std::string_view(body,length),ctx->installed);
    const HMODULE module=ctx->module;
    delete ctx;
    g_next.store(GetTickCount64()+kPollMs);
    g_busy.store(false);
    FreeLibraryAndExitThread(module,0);
}
}
void Tick(const char* installedVersion) {
    const ULONGLONG now=GetTickCount64();
    if(now<g_next.load()) return;
    bool expected=false;
    if(!g_busy.compare_exchange_strong(expected,true)) return;
    auto* ctx=new(std::nothrow) Context;
    if(ctx) {
        std::snprintf(ctx->installed,sizeof(ctx->installed),"%s",installedVersion);
        // Keep our code mapped only while the request thread runs, including during shutdown/unload.
        if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(&Tick),&ctx->module)) {
            if(HANDLE thread=CreateThread(nullptr,0,Worker,ctx,0,nullptr)) { CloseHandle(thread);return; }
            FreeLibrary(ctx->module);
        }
        delete ctx;
    }
    g_next.store(now+30ull*60*1000); // allocation/thread failure: quiet backoff
    g_busy.store(false);
}
bool CopyNotice(char* dst,size_t capacity) {
    if(!dst||!capacity) return false;
    dst[0]='\0';
    if(!TryAcquireSRWLockShared(&g_lock)) return false;
    std::snprintf(dst,capacity,"%s",g_notice);
    ReleaseSRWLockShared(&g_lock);
    return dst[0]!=0;
}
}
