// Software model of a process page table: maps virtual pages to physical
// frames and keeps per-page statistics (page-ins, evictions, accesses).
#pragma once

#include <unordered_map>

#include "core/Types.h"

namespace prs {

struct PageTableEntry {
    bool valid = false;      // present bit
    int frame = EMPTY;       // physical frame number when valid
    long long accesses = 0;  // total references to this page
    long long pageIns = 0;   // how many times it was faulted in
    long long evictions = 0; // how many times it was swapped out
};

class PageTable {
public:
    void clear();

    // Translate a page; returns the frame or EMPTY on a page fault.
    int lookup(int page);

    bool everLoaded(int page) const;  // used to tell cold misses from capacity misses
    void map(int page, int frame);
    void unmap(int page);
    void recordAccess(int page);

    long long lookups() const { return lookups_; }
    const std::unordered_map<int, PageTableEntry>& entries() const { return entries_; }

private:
    std::unordered_map<int, PageTableEntry> entries_;
    long long lookups_ = 0;
};

}  // namespace prs
