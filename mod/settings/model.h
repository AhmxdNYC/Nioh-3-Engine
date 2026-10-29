// Controller value editor. No game calls, files, or automatic enablement.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace setting_ui {
enum class Kind { Number, Choice, Manual };
enum class Apply { NextLaunch, OverlayLive, MenuLive, TuningLive };
struct Def {
    const char *section, *key, *label, *initial;
    Kind kind;
    double lo, hi, step;
    const char* choices;
    Apply apply;
    const char* liveKey;
    int legacy, group;
    bool experimental;
    const char* help;
};
// menu display text of a stored value (#13296): 0|1 switches read ON / OFF (drawn as pills), an empty value reads "(empty)"
inline const char* Shown(const Def& d, const std::string& v) {
    if (d.kind == Kind::Choice && d.choices && !std::strcmp(d.choices, "0|1") && (v == "0" || v == "1")) return v == "1" ? "ON" : "OFF";
    return v.empty() ? "(empty)" : v.c_str();
}
inline bool Number(const std::string& s, double& v) {
    char* end = nullptr;
    v = std::strtod(s.c_str(), &end);
    return end != s.c_str() && end && !*end && std::isfinite(v);
}
inline std::vector<std::string> Choices(const char* text) {
    std::vector<std::string> out;
    std::string s = text;
    size_t first = 0;
    for (;;) {
        const size_t end = s.find('|', first);
        out.push_back(s.substr(first, end - first));
        if (end == std::string::npos) break;
        first = end + 1;
    }
    return out;
}
inline bool Next(const Def& d, const std::string& current, int dir, std::string& next) {
    if (d.kind == Kind::Manual) return false;
    if (d.kind == Kind::Choice) {
        const auto values = Choices(d.choices);
        int index = -1;
        for (size_t i = 0; i < values.size(); ++i)
            if (values[i] == current) index = (int)i;
        if (index < 0) index = dir < 0 ? (int)values.size() - 1 : 0;
        else if (!dir) index = (index + 1) % (int)values.size();
        else index = std::clamp(index + (dir < 0 ? -1 : 1), 0, (int)values.size() - 1);
        next = values[(size_t)index];
    } else {
        double value;
        if (!Number(current, value) && !Number(d.initial, value)) value = d.lo;
        value = std::clamp(value + (dir < 0 ? -d.step : d.step), d.lo, d.hi);
        // Round decimal steps so repeated controller presses do not drift.
        value = std::clamp(std::round(value * 1000000.0) / 1000000.0, d.lo, d.hi);
        char text[64];
        const bool integral = std::floor(d.lo) == d.lo && std::floor(d.hi) == d.hi && std::floor(d.step) == d.step;
        std::snprintf(text, sizeof(text), integral ? "%.0f" : "%.6f", value);
        next = text;
        if (!integral) {
            while (!next.empty() && next.back() == '0') next.pop_back();
            if (!next.empty() && next.back() == '.') next.pop_back();
        }
    }
    return next != current;
}
struct Value { std::string text; bool changed = false; };
}  // namespace setting_ui
