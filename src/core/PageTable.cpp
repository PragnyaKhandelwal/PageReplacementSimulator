#include "core/PageTable.h"

namespace prs {

void PageTable::clear() {
    entries_.clear();
    lookups_ = 0;
}

int PageTable::lookup(int page) {
    ++lookups_;
    auto it = entries_.find(page);
    if (it == entries_.end() || !it->second.valid) return EMPTY;
    return it->second.frame;
}

bool PageTable::everLoaded(int page) const {
    auto it = entries_.find(page);
    return it != entries_.end() && it->second.pageIns > 0;
}

void PageTable::map(int page, int frame) {
    PageTableEntry& e = entries_[page];
    e.valid = true;
    e.frame = frame;
    ++e.pageIns;
}

void PageTable::unmap(int page) {
    PageTableEntry& e = entries_[page];
    e.valid = false;
    e.frame = EMPTY;
    ++e.evictions;
}

void PageTable::recordAccess(int page) { ++entries_[page].accesses; }

}  // namespace prs
