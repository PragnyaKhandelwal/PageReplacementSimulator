// Core data model of the simulator: references, frames, per-step records,
// configuration and results.
#pragma once

#include <string>
#include <vector>

namespace prs {

constexpr int EMPTY = -1;

enum class AccessType { Read, Write };

// A single entry of a page reference trace.
struct PageRef {
    int page = 0;
    AccessType type = AccessType::Read;
    bool isWrite() const { return type == AccessType::Write; }
};

// One physical frame plus the bookkeeping bits that the MMU / OS keeps for it.
struct Frame {
    int page = EMPTY;
    long long loadTime = -1;    // reference index at which the page was brought in
    long long lastAccess = -1;  // reference index of the most recent use
    long long frequency = 0;    // number of references since the page was loaded
    bool referenced = false;    // R (use) bit
    bool dirty = false;         // M (modify) bit
    bool empty() const { return page == EMPTY; }
};

// Classification of a miss: first touch of a page (cold) vs. a page that was
// resident before and got evicted (capacity / replacement induced).
enum class FaultKind { None, Compulsory, Capacity };

// Everything that happened while serving one reference.
struct StepRecord {
    long long index = 0;
    PageRef ref;
    bool hit = false;
    FaultKind faultKind = FaultKind::None;
    int frameUsed = EMPTY;     // frame holding the page after this step
    int victimPage = EMPTY;    // evicted page, EMPTY when nothing was evicted
    bool victimDirty = false;  // victim had to be written back to disk
    std::vector<int> frames;   // frame contents after the step
    std::string reason;        // human readable explanation of the decision
};

struct SimulationConfig {
    int frames = 3;
    bool recordSteps = true;   // keep a StepRecord per reference
    bool explain = true;       // ask algorithms to justify each eviction

    // Timing model used for the Effective Access Time (EAT).
    double memoryAccessNs = 100.0;       // main memory access time
    double faultServiceNs = 8000000.0;   // page-fault service time (8 ms)
    double writeBackNs = 8000000.0;      // extra cost of flushing a dirty victim

    // Algorithm specific knobs.
    int nruResetInterval = 8;  // NRU: clear R bits every N references (clock tick)
    int agingBits = 8;         // Aging: width of the software counter
    int agingInterval = 1;     // Aging: references per counter shift
    unsigned int seed = 42;    // Random replacement seed (reproducible runs)
};

struct SimulationResult {
    std::string key;        // short id, e.g. "LRU"
    std::string algorithm;  // display name
    int frames = 0;

    long long references = 0;
    long long hits = 0;
    long long faults = 0;
    long long compulsoryFaults = 0;
    long long capacityFaults = 0;
    long long evictions = 0;
    long long writeBacks = 0;
    long long pageTableLookups = 0;

    double effectiveAccessNs = 0.0;  // EAT under the configured timing model
    double runtimeMicros = 0.0;      // wall-clock time spent simulating

    std::vector<StepRecord> steps;

    double hitRatio() const { return references ? double(hits) / references : 0.0; }
    double faultRatio() const { return references ? double(faults) / references : 0.0; }
};

}  // namespace prs
