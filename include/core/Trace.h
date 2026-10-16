// A page reference trace: the ordered list of pages a process touches.
#pragma once

#include <string>
#include <vector>

#include "core/Types.h"

namespace prs {

class Trace {
public:
    Trace() = default;
    explicit Trace(std::vector<PageRef> refs, std::string name = "custom");

    static Trace fromPages(const std::vector<int>& pages, const std::string& name = "custom");

    const std::vector<PageRef>& refs() const { return refs_; }
    const PageRef& operator[](size_t i) const { return refs_[i]; }
    size_t size() const { return refs_.size(); }
    bool empty() const { return refs_.empty(); }

    const std::string& name() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    std::vector<int> pages() const;
    std::vector<int> distinctPageList() const;  // sorted
    int distinctPages() const;
    double writeFraction() const;

    // "7 0 1 2W 0 ..." (truncated with an ellipsis after maxItems when > 0)
    std::string toString(size_t maxItems = 0) const;

private:
    std::vector<PageRef> refs_;
    std::string name_ = "custom";
};

}  // namespace prs
