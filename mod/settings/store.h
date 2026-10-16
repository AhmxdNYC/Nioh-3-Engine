// Profile I/O is separate from the platform-independent value editor.
#pragma once
#include "model.h"
#include <array>
#include <windows.h>

namespace setting_ui {
inline std::wstring Wide(const char* s) {
    std::wstring out;
    while (*s) out.push_back((wchar_t)(unsigned char)*s++);
    return out;
}
template <size_t N>
struct Store {
    std::array<Value, N> values;
    void Load(const Def (&defs)[N], const wchar_t* ini) {
        for (size_t i = 0; i < N; ++i) {
            wchar_t b[1024];
            GetPrivateProfileStringW(Wide(defs[i].section).c_str(), Wide(defs[i].key).c_str(),
                Wide(defs[i].initial).c_str(), b, (DWORD)(sizeof(b) / sizeof(b[0])), ini);
            std::string raw;
            // Preserve manual unicode paths verbatim: they are never submitted to Save.
            for (const wchar_t* p = b; *p; ++p) raw.push_back(*p < 128 ? (char)*p : '?');
            values[i] = {raw, false};
        }
    }
    bool Change(const Def (&defs)[N], size_t index, int dir) {
        if (index >= N) return false;
        std::string next;
        if (!Next(defs[index], values[index].text, dir, next)) return false;
        values[index] = {next, true};
        return true;
    }
    template <class Sink>
    void Save(const Def (&defs)[N], Sink sink) const {
        for (size_t i = 0; i < N; ++i)
            if (values[i].changed && defs[i].kind != Kind::Manual)
                sink(Wide(defs[i].section).c_str(), Wide(defs[i].key).c_str(), Wide(values[i].text.c_str()));
    }
};
}  // namespace setting_ui
