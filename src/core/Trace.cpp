#include "core/Trace.h"

#include <algorithm>
#include <set>
#include <sstream>

namespace prs {

Trace::Trace(std::vector<PageRef> refs, std::string name)
    : refs_(std::move(refs)), name_(std::move(name)) {}

Trace Trace::fromPages(const std::vector<int>& pages, const std::string& name) {
    std::vector<PageRef> refs;
    refs.reserve(pages.size());
    for (int p : pages) refs.push_back(PageRef{p, AccessType::Read});
    return Trace(std::move(refs), name);
}

std::vector<int> Trace::pages() const {
    std::vector<int> out;
    out.reserve(refs_.size());
    for (const auto& r : refs_) out.push_back(r.page);
    return out;
}

std::vector<int> Trace::distinctPageList() const {
    std::set<int> s;
    for (const auto& r : refs_) s.insert(r.page);
    return std::vector<int>(s.begin(), s.end());
}

int Trace::distinctPages() const { return static_cast<int>(distinctPageList().size()); }

double Trace::writeFraction() const {
    if (refs_.empty()) return 0.0;
    long long writes = std::count_if(refs_.begin(), refs_.end(),
                                     [](const PageRef& r) { return r.isWrite(); });
    return double(writes) / refs_.size();
}

std::string Trace::toString(size_t maxItems) const {
    std::ostringstream os;
    size_t n = (maxItems == 0) ? refs_.size() : std::min(maxItems, refs_.size());
    for (size_t i = 0; i < n; ++i) {
        if (i) os << ' ';
        os << refs_[i].page;
        if (refs_[i].isWrite()) os << 'W';
    }
    if (n < refs_.size()) os << " ... (+" << (refs_.size() - n) << " more)";
    return os.str();
}

}  // namespace prs
