#include "updatecheck.h"
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
namespace updatecheck {
namespace {
SRWLOCK g_lock = SRWLOCK_INIT;
char g_notice[128]{};
char g_folderNotice[2048]{};
ULONGLONG g_folderUntil=0;
std::atomic<bool> g_busy{false};
std::atomic<ULONGLONG> g_next{0};
bool g_enabled=true;
std::wstring g_host=L"nioh3-mod-feedback.vercel.app",g_path=L"/version.json";
INTERNET_PORT g_port=INTERNET_DEFAULT_HTTPS_PORT;
bool SetEndpoint(const wchar_t* url) {
    URL_COMPONENTS u{};u.dwStructSize=sizeof(u);
    u.dwHostNameLength=u.dwUrlPathLength=u.dwExtraInfoLength=u.dwUserNameLength=u.dwPasswordLength=(DWORD)-1;
    if(!url || wcslen(url)>1023 || !WinHttpCrackUrl(url,0,0,&u) ||
       u.nScheme!=INTERNET_SCHEME_HTTPS || !u.dwHostNameLength ||
       u.dwUserNameLength || u.dwPasswordLength || wcschr(url,L'#')) return false;
    g_host.assign(u.lpszHostName,u.dwHostNameLength);
    g_path=u.dwUrlPathLength?std::wstring(u.lpszUrlPath,u.dwUrlPathLength):L"/";
    if(u.dwExtraInfoLength) g_path.append(u.lpszExtraInfo,u.dwExtraInfoLength);
    g_port=u.nPort;return true;
}
struct HttpHandle {
    HINTERNET value;
    explicit HttpHandle(HINTERNET h) : value(h) {}
    ~HttpHandle() { if(value) WinHttpCloseHandle(value); }
    HttpHandle(const HttpHandle&)=delete;
    HttpHandle& operator=(const HttpHandle&)=delete;
};
// Configured HTTPS endpoint; no identifiers, cookies, automatic credentials or redirects.
bool Fetch(char (&body)[kMaxBody+1], size_t& length) {
    length=0;
    const ULONGLONG start=GetTickCount64();
    HttpHandle session(WinHttpOpen(L"Nioh3EngineFix/update-check",WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.value || !WinHttpSetTimeouts(session.value,1000,2000,2000,2000)) return false;
    DWORD retries=0;
    if(!WinHttpSetOption(session.value,WINHTTP_OPTION_CONNECT_RETRIES,&retries,sizeof(retries))) return false;
    HttpHandle connection(WinHttpConnect(session.value,g_host.c_str(),g_port,0));
    if(!connection.value) return false;
    const wchar_t* accept[]={L"application/json",nullptr};
    HttpHandle request(WinHttpOpenRequest(connection.value,L"GET",g_path.c_str(),nullptr,
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
    char notice[128]{};
    if(Newer(remote,local)) std::snprintf(notice,sizeof(notice),"Update %u.%u.%u available: Mod Menu > Update + Page",
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
void Configure(const wchar_t* iniPath) {
    wchar_t url[1024]{};
    g_enabled=GetPrivateProfileIntW(L"Update",L"Check",1,iniPath)!=0;
    const DWORD n=GetPrivateProfileStringW(L"Update",L"Url",
        L"https://nioh3-mod-feedback.vercel.app/version.json",url,1024,iniPath);
    if(n>=1023 || !SetEndpoint(url)) g_enabled=false;
}
void Tick(const char* installedVersion) {
    if(!g_enabled) return;
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
namespace {
std::atomic<ULONGLONG> g_openAfter{0};
// Wine's Explorer can stay open indefinitely without reaching a Linux file manager.
// Launch the native desktop opener in a separate process, never wait on desktop UI.
bool IsWine() {
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    return ntdll && GetProcAddress(ntdll, "wine_get_version");
}
void ShowFolderNotice(const std::wstring& folder) {
    char path[1900]{};
    if (!WideCharToMultiByte(CP_UTF8, 0, folder.c_str(), -1, path, sizeof(path), nullptr, nullptr))
        std::snprintf(path, sizeof(path), "Path too long; use Steam > Manage > Browse local files");
    AcquireSRWLockExclusive(&g_lock);
    std::snprintf(g_folderNotice, sizeof(g_folderNotice), "Update website: https://nioh3-mod-feedback.vercel.app | Game folder: %s", path);
    g_folderUntil = 0; // starts only when the foreground game can draw it
    ReleaseSRWLockExclusive(&g_lock);
}
void OpenWineFolder(const std::wstring& folder) {
    using UnixName = char* (__cdecl*)(LPCWSTR);
    const HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    const auto convert = kernel ? reinterpret_cast<UnixName>(GetProcAddress(kernel, "wine_get_unix_file_name")) : nullptr;
    if (!convert) { ShowFolderNotice(folder); return; }
    char* path = convert(folder.c_str());
    if (!path) { ShowFolderNotice(folder); return; }
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, nullptr, 0);
    std::wstring target(count > 0 ? count : 0, L'\0');
    if (count > 0) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, target.data(), count);
    HeapFree(GetProcessHeap(), 0, path);
    if (count <= 1) { ShowFolderNotice(folder); return; }
    target.resize(count - 1);
    ShowFolderNotice(target);
}
void LaunchFixedPage(HMODULE module) {
    // Never use manifest or INI values as browser targets.
    const bool wine = IsWine();
    if (!wine) ShellExecuteW(nullptr, L"open", L"https://nioh3-mod-feedback.vercel.app", nullptr, nullptr, SW_SHOWNORMAL);
    // Resolve only this ASI's directory; no configuration, manifest or working-directory input.
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) { if(wine) ShowFolderNotice(L"Use Steam > Manage > Browse local files"); return; }
    path.resize(length);
    const auto slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) { if(wine) ShowFolderNotice(L"Use Steam > Manage > Browse local files"); return; }
    const std::wstring folder = path.substr(0, slash + 1);
    if (wine) { OpenWineFolder(folder); return; }
    const std::wstring updater = folder + L"Nioh3Updater.exe";
    const DWORD attributes = GetFileAttributesW(updater.c_str());
    const bool exists = attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    // Fully qualify Explorer too, so a local executable cannot shadow it.
    wchar_t windows[MAX_PATH]{};
    const UINT count = GetWindowsDirectoryW(windows, MAX_PATH);
    if (!count || count >= MAX_PATH) return;
    const std::wstring explorer = std::wstring(windows, count) + L"\\explorer.exe";
    // Avoid a trailing backslash before the closing quote for the folder fallback.
    const std::wstring target = exists ? updater : folder + L".";
    const std::wstring args = (exists ? L"/select,\"" : L"\"") + target + L"\"";
    ShellExecuteW(nullptr, L"open", explorer.c_str(), args.c_str(), nullptr, SW_SHOWNORMAL);
}
DWORD WINAPI OpenPageWorker(LPVOID module) {
    try { LaunchFixedPage(static_cast<HMODULE>(module)); } catch (...) { /* Shell UI is best-effort; never unwind into the game. */ }
    FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
}
}
void OpenPage() {
    const auto now = GetTickCount64();
    auto after = g_openAfter.load();
    if (now < after || !g_openAfter.compare_exchange_strong(after, now + 2000)) return;
    HMODULE module{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(&OpenPage), &module)) return;
    HANDLE thread = CreateThread(nullptr, 0, OpenPageWorker, module, 0, nullptr);
    if (thread) CloseHandle(thread); else FreeLibrary(module);
}
bool RunningUnderWine() { return IsWine(); }
bool CopyFolderNotice(char* dst, size_t capacity, bool focused) {
    if (!dst || !capacity) return false;
    dst[0]=0;
    if (!TryAcquireSRWLockExclusive(&g_lock)) return false;
    if (g_folderNotice[0]) {
        if (!focused) g_folderUntil=0;
        else {
            const auto now=GetTickCount64();
            if (!g_folderUntil) g_folderUntil=now+20000;
            if (now < g_folderUntil) std::snprintf(dst,capacity,"%s",g_folderNotice);
            else g_folderNotice[0]=0;
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
    return dst[0]!=0;
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
