#pragma once
#include <cstdint>
#include <string_view>
namespace updatecheck {
inline constexpr char kSite[] = "https://nioh3-mod-feedback.vercel.app";
inline constexpr uint64_t kPollMs = 1ull * 60 * 60 * 1000;
inline constexpr size_t kMaxBody = 512;
struct Version { uint32_t part[3]{}; };
inline bool ParseVersion(std::string_view s, Version& out, bool buildLabel = false) {
    Version v{}; size_t p = 0;
    for (int i=0;i<3;++i) {
        size_t start=p;
        while(p<s.size() && s[p]>='0' && s[p]<='9') {
            if(p-start>=6) return false;
            v.part[i]=v.part[i]*10+(unsigned)(s[p++]-'0');
        }
        if(p==start || (p-start>1 && s[start]=='0')) return false;
        if(i<2 && (p==s.size() || s[p++]!='.')) return false;
    }
    if(p!=s.size()) {
        if(!buildLabel || (s[p]!='-' && s[p]!='+') || ++p==s.size()) return false;
        for(;p<s.size();++p) if(!((s[p]>='a'&&s[p]<='z')||(s[p]>='A'&&s[p]<='Z')||(s[p]>='0'&&s[p]<='9')||s[p]=='-'||s[p]=='.'||s[p]=='+')) return false;
    }
    out=v;return true;
}
inline bool Newer(const Version& remote,const Version& local) {
    for(int i=0;i<3;++i) if(remote.part[i]!=local.part[i]) return remote.part[i]>local.part[i];
    return false;
}
// Bounded JSON: required numeric version, optional url/notes strings (metadata only).
// Only the parsed numeric version reaches the HUD; metadata is never executed or rendered.
inline bool ParseManifest(std::string_view s, Version& out) {
    if(s.size()>kMaxBody) return false;
    size_t p=0;
    auto ws=[&](){while(p<s.size()&&(s[p]==' '||s[p]=='\t'||s[p]=='\r'||s[p]=='\n'))++p;};
    auto take=[&](char c){ws();return p<s.size()&&s[p++]==c;};
    if(!take('{'))return false;
    auto string=[&](std::string_view& value) {
        if(!take('"'))return false;
        const size_t begin=p;
        while(p<s.size()) {
            const unsigned char c=(unsigned char)s[p++];
            if(c=='"'){value=s.substr(begin,p-begin-1);return true;}
            if(c<32)return false;
            if(c=='\\') {
                if(p==s.size())return false;
                const char e=s[p++];
                if(e=='u') {
                    for(int i=0;i<4;++i) {
                        if(p==s.size())return false;
                        const char h=s[p++];
                        if(!((h>='0'&&h<='9')||(h>='a'&&h<='f')||(h>='A'&&h<='F')))return false;
                    }
                } else if(e!='"'&&e!='\\'&&e!='/'&&e!='b'&&e!='f'&&e!='n'&&e!='r'&&e!='t')return false;
            }
        }
        return false;
    };
    unsigned seen=0;Version parsed{};
    for(;;) {
        std::string_view key,value;
        if(!string(key)||!take(':')||!string(value))return false;
        const unsigned bit=key=="version"?1u:key=="url"?2u:key=="notes"?4u:0u;
        if(!bit || (seen&bit))return false;
        seen|=bit;
        if(bit==1&&!ParseVersion(value,parsed))return false;
        ws();if(p==s.size())return false;
        if(s[p]=='}'){++p;break;}
        if(s[p++]!=',')return false;
    }
    ws();if(p!=s.size()||!(seen&1))return false;
    out=parsed;return true;
}
inline bool Available(std::string_view json,std::string_view installed,Version& remote) {
    Version local{};return ParseVersion(installed,local,true)&&ParseManifest(json,remote)&&Newer(remote,local);
}
}
