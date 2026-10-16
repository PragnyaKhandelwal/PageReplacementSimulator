// Strategy interface for page replacement policies.
//
// The Simulator owns the frames and the page table and drives the policy
// through a set of hooks. A policy only has to answer one question -
// "which frame do I evict?" - but may observe every event to keep its own
// state (queues, counters, clock hands, ghost lists ...).
//
// Call order for each reference:
//   onReference -> (hit)  onHit
//               -> (miss) onMiss -> [selectVictim if memory is full] -> onLoad
//   -> afterReference
#pragma once

#include <string>
#include <vector>

#include "core/Trace.h"
#include "core/Types.h"

namespace prs {

struct AccessContext {
    long long time = 0;           // index of the current reference
    PageRef ref;                  // the reference being served
    const Trace* trace = nullptr; // full trace (look-ahead for OPT)
    bool explain = false;         // produce a human readable reason
};

struct Victim {
    int frame = EMPTY;
    std::string reason;
};

class ReplacementAlgorithm {
public:
    virtual ~ReplacementAlgorithm() = default;

    virtual std::string key() const = 0;          // "LRU"
    virtual std::string name() const = 0;         // "Least Recently Used"
    virtual std::string description() const = 0;  // encyclopedia text
    virtual std::string family() const = 0;       // "Recency", "Frequency" ...

    // Stack algorithms obey the inclusion property and can never show
    // Belady's anomaly.
    virtual bool isStackAlgorithm() const { return false; }

    virtual void reset(int frameCount, const Trace& trace, const SimulationConfig& cfg) {
        (void)frameCount; (void)trace; (void)cfg;
    }
    virtual void onReference(std::vector<Frame>& frames, const AccessContext& ctx) {
        (void)frames; (void)ctx;
    }
    virtual void onHit(std::vector<Frame>& frames, int frame, const AccessContext& ctx) {
        (void)frames; (void)frame; (void)ctx;
    }
    virtual void onMiss(const std::vector<Frame>& frames, const AccessContext& ctx) {
        (void)frames; (void)ctx;
    }
    virtual Victim selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) = 0;
    virtual void onLoad(std::vector<Frame>& frames, int frame, const AccessContext& ctx) {
        (void)frames; (void)frame; (void)ctx;
    }
    virtual void afterReference(std::vector<Frame>& frames, const AccessContext& ctx) {
        (void)frames; (void)ctx;
    }
};

}  // namespace prs
