#include "io/TraceIO.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "core/Util.h"

namespace prs {

namespace {

bool parseNumber(const std::string& s, long long& value, bool& isHex) {
    if (s.empty()) return false;
    isHex = s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X');
    try {
        size_t used = 0;
        value = isHex ? std::stoll(s.substr(2), &used, 16) : std::stoll(s, &used, 10);
        return used == (isHex ? s.size() - 2 : s.size()) && value >= 0;
    } catch (...) {
        return false;
    }
}

PageRef parseToken(const std::string& raw, const ParseOptions& opt) {
    std::string t = raw;
    AccessType type = AccessType::Read;

    auto stripMarker = [&](bool prefix) {
        if (t.size() < 2) return;
        char c = prefix ? t.front() : t.back();
        char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (up != 'R' && up != 'W') return;
        // Don't eat the hex prefix "0x..." or a hex digit; R/W are not hex digits.
        type = (up == 'W') ? AccessType::Write : AccessType::Read;
        if (prefix) t.erase(0, 1);
        else t.pop_back();
        if (!t.empty() && (prefix ? t.front() : t.back()) == ':') {
            if (prefix) t.erase(0, 1);
            else t.pop_back();
        }
    };
    stripMarker(true);
    stripMarker(false);

    long long value = 0;
    bool isHex = false;
    if (!parseNumber(t, value, isHex))
        throw std::invalid_argument("invalid trace token '" + raw + "'");

    long long page = value;
    if (isHex || opt.addressMode) {
        if (opt.pageSize <= 0) throw std::invalid_argument("page size must be positive");
        page = value / opt.pageSize;
    }
    if (page > 2147483647LL) throw std::invalid_argument("page number too large in '" + raw + "'");
    return PageRef{static_cast<int>(page), type};
}

}  // namespace

Trace parseTrace(const std::string& text, const std::string& name, const ParseOptions& options) {
    std::vector<PageRef> refs;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        for (const auto& token : split(line, " \t\r,;|"))
            refs.push_back(parseToken(token, options));
    }
    return Trace(std::move(refs), name);
}

Trace loadTraceFile(const std::string& path, const ParseOptions& options) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("cannot open trace file '" + path + "'");
    std::stringstream buffer;
    buffer << file.rdbuf();

    std::string name = path;
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) name = name.substr(0, dot);
    return parseTrace(buffer.str(), name, options);
}

void saveTraceFile(const Trace& trace, const std::string& path) {
    std::ofstream file(path);
    if (!file) throw std::runtime_error("cannot write trace file '" + path + "'");
    file << "# Page reference trace: " << trace.name() << "\n";
    file << "# " << trace.size() << " references, " << trace.distinctPages()
         << " distinct pages. Suffix W marks a write.\n";
    for (size_t i = 0; i < trace.size(); ++i) {
        file << trace[i].page << (trace[i].isWrite() ? "W" : "");
        file << (((i + 1) % 20 == 0) ? '\n' : ' ');
    }
    file << '\n';
}

}  // namespace prs
