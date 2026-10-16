// Small string / formatting helpers shared by every module.
#pragma once

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace prs {

// Concatenate any streamable values into a string: cat("t=", 4, " page ", 7)
template <typename... Args>
std::string cat(const Args&... args) {
    std::ostringstream os;
    using expand = int[];
    (void)expand{0, ((void)(os << args), 0)...};
    return os.str();
}

inline std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

inline std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

inline std::vector<std::string> split(const std::string& s, const std::string& delims) {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : s) {
        if (delims.find(ch) != std::string::npos) {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += ch;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

inline std::string join(const std::vector<std::string>& parts, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

// Append a decimal integer without std::to_string (very slow on MinGW, where it
// goes through vsnprintf); used on hot paths such as the per-step CSV export.
inline void appendInt(std::string& out, long long v) {
    char buf[24];
    int n = 0;
    unsigned long long u = v < 0 ? 0ULL - static_cast<unsigned long long>(v) : static_cast<unsigned long long>(v);
    do { buf[n++] = static_cast<char>('0' + u % 10); u /= 10; } while (u);
    if (v < 0) out += '-';
    while (n) out += buf[--n];
}

inline std::string fixed(double v, int precision = 2) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(precision) << v;
    return os.str();
}

inline std::string percent(double ratio, int precision = 2) {
    return fixed(ratio * 100.0, precision) + "%";
}

inline std::string padRight(const std::string& s, size_t width) {
    return s.size() >= width ? s : s + std::string(width - s.size(), ' ');
}

inline std::string padLeft(const std::string& s, size_t width) {
    return s.size() >= width ? s : std::string(width - s.size(), ' ') + s;
}

inline std::string center(const std::string& s, size_t width) {
    if (s.size() >= width) return s;
    size_t left = (width - s.size()) / 2;
    return std::string(left, ' ') + s + std::string(width - s.size() - left, ' ');
}

inline std::string repeat(const std::string& s, size_t n) {
    std::string out;
    out.reserve(s.size() * n);
    for (size_t i = 0; i < n; ++i) out += s;
    return out;
}

// Human friendly nanosecond duration: 100 ns, 12.50 us, 8.00 ms ...
inline std::string formatNs(double ns) {
    if (ns < 1e3) return fixed(ns, 2) + " ns";
    if (ns < 1e6) return fixed(ns / 1e3, 2) + " us";
    if (ns < 1e9) return fixed(ns / 1e6, 2) + " ms";
    return fixed(ns / 1e9, 2) + " s";
}

inline bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// Escape text for safe embedding in HTML.
inline std::string htmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }
    return out;
}

// Quote a CSV field when it contains separators or quotes.
inline std::string csvField(const std::string& s) {
    if (s.find_first_of(",\"\n") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    return out + "\"";
}

}  // namespace prs
