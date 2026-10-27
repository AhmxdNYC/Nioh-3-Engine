#pragma once
#include <cstdint>
#include <string_view>
namespace updatecheck {
inline constexpr char kSite[] = "https://nioh3-mod-feedback.vercel.app";
inline constexpr uint64_t kPollMs = 6ull * 60 * 60 * 1000;
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
// Endpoint contract: one bounded JSON object containing only the numeric release version.
// Reject HTML, duplicate keys, arbitrary strings and additional fields; never display network text.
inline bool ParseManifest(std::string_view s, Version& out) {
    if(s.size()>kMaxBody) return false;
    size_t p=0;
    auto ws=[&](){while(p<s.size()&&(s[p]==' '||s[p]=='\t'||s[p]=='\r'||s[p]=='\n'))++p;};
    auto take=[&](char c){ws();return p<s.size()&&s[p++]==c;};
    if(!take('{'))return false;
    ws();constexpr std::string_view key="\"version\"";
    if(s.substr(p,key.size())!=key)return false;p+=key.size();
    if(!take(':')||!take('"'))return false;
    const size_t start=p;while(p<s.size()&&s[p]!='"')++p;
    if(p==s.size()||!ParseVersion(s.substr(start,p-start),out))return false;
    ++p;if(!take('}'))return false;ws();return p==s.size();
}
inline bool Available(std::string_view json,std::string_view installed,Version& remote) {
    Version local{};return ParseVersion(installed,local,true)&&ParseManifest(json,remote)&&Newer(remote,local);
}
}
