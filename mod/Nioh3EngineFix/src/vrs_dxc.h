// Minimal optional DXC COM ABI. Reference: Microsoft's include/dxc/dxcapi.h.
// No new shipping DLL: use dxcompiler.dll beside the game EXE; failure stays 1x1.
#pragma once
#include <windows.h>
#include <d3dcompiler.h>
#include <string>
namespace vrsdxc {
struct Buffer { const void* data; SIZE_T size; UINT encoding; };
struct Result : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Status(HRESULT*)=0;
    virtual HRESULT STDMETHODCALLTYPE Output(ID3DBlob**)=0;
    virtual HRESULT STDMETHODCALLTYPE Errors(ID3DBlob**)=0;
};
struct Compiler : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Compile(const Buffer*,const wchar_t**,UINT,IUnknown*,REFIID,void**)=0;
    virtual HRESULT STDMETHODCALLTYPE Disassemble(const Buffer*,REFIID,void**)=0;
};
inline const GUID cls={0x73e22d93,0xe6ce,0x47f3,{0xb5,0xbf,0xf0,0x66,0x4f,0x39,0xc1,0xb0}};
inline const GUID iid={0x228b4687,0x5a6a,0x4730,{0x90,0x0c,0x97,0x02,0xb2,0x20,0x3f,0x54}};
inline const GUID resultIid={0xcedb484a,0xd4e9,0x445a,{0xb9,0x91,0xca,0x21,0xca,0x15,0x7d,0xc2}};
using Create=HRESULT(WINAPI*)(REFCLSID,REFIID,void**);
inline HMODULE Load(const wchar_t* explicitPath=nullptr) {
    if (explicitPath) return LoadLibraryExW(explicitPath,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    wchar_t path[32768]; DWORD n=GetModuleFileNameW(nullptr,path,32768);
    if (!n || n>=32768) return nullptr;
    auto* slash=wcsrchr(path,L'\\'); if (!slash) return nullptr;
    std::wstring dll(path,slash+1); dll+=L"dxcompiler.dll";
    return LoadLibraryExW(dll.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
}
inline Compiler* New(HMODULE module) {
    auto create=module ? reinterpret_cast<Create>(GetProcAddress(module,"DxcCreateInstance")) : nullptr;
    Compiler* c=nullptr;
    if (create && FAILED(create(cls,iid,reinterpret_cast<void**>(&c)))) c=nullptr;
    return c;
}
inline bool Text(Compiler* c,const void* data,SIZE_T size,std::string& text) {
    if (!c || !data || !size) return false;
    Buffer b{data,size,0}; Result* r=nullptr;
    HRESULT hr=c->Disassemble(&b,resultIid,reinterpret_cast<void**>(&r));
    if (FAILED(hr) || !r) { if(r) r->Release(); return false; }
    HRESULT status=E_FAIL; ID3DBlob* blob=nullptr;
    bool ok=SUCCEEDED(r->Status(&status)) && SUCCEEDED(status) && SUCCEEDED(r->Output(&blob)) && blob;
    if (ok) text.assign(static_cast<const char*>(blob->GetBufferPointer()),blob->GetBufferSize());
    if (blob) blob->Release(); r->Release(); return ok;
}
inline bool Text(const void* data,SIZE_T size,std::string& text) {
    static HMODULE module=Load();
    Compiler* c=New(module); if (!c) return false;
    bool ok=Text(c,data,size,text); c->Release(); return ok;
}
}
