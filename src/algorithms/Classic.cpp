// FIFO, LRU and Optimal - the three policies required by the synopsis.
#include <limits>
#include <unordered_map>

#include "algorithms/Algorithms.h"
#include "core/Util.h"

namespace prs {

namespace {
constexpr long long NEVER = std::numeric_limits<long long>::max();
}

// ------------------------------------------------------------------- FIFO --

std::string FIFO::description() const {
    return "Evicts the page that has been in memory the longest, regardless of how often "
           "or how recently it is used. Implemented as a queue ordered by load time.\n"
           "Cost: O(1) per fault with a queue. Simple and fair, but it ignores locality and "
           "is the textbook example of Belady's anomaly - adding frames can INCREASE faults.";
}

Victim FIFO::selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) {
    int best = 0;
    for (int i = 1; i < static_cast<int>(frames.size()); ++i)
        if (frames[i].loadTime < frames[best].loadTime) best = i;
    Victim v{best, ""};
    if (ctx.explain)
        v.reason = cat("oldest resident page (loaded at t=", frames[best].loadTime, ")");
    return v;
}

// -------------------------------------------------------------------- LRU --

std::string LRU::description() const {
    return "Evicts the page whose last use lies furthest in the past, betting that the "
           "recent past predicts the near future (temporal locality).\n"
           "Cost: O(1) with a hash map + doubly linked list, but true LRU needs hardware "
           "support on every access. It is a stack algorithm, so it never suffers Belady's "
           "anomaly. Weak spot: cyclic loops slightly larger than memory.";
}

Victim LRU::selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) {
    int best = 0;
    for (int i = 1; i < static_cast<int>(frames.size()); ++i)
        if (frames[i].lastAccess < frames[best].lastAccess) best = i;
    Victim v{best, ""};
    if (ctx.explain)
        v.reason = cat("least recently used (last referenced at t=", frames[best].lastAccess, ")");
    return v;
}

// ---------------------------------------------------------------- Optimal --

std::string Optimal::description() const {
    return "Evicts the page that will not be used for the longest time in the future "
           "(Belady, 1966). It needs knowledge of future references, so it cannot be built "
           "in a real OS - but it gives the minimum possible number of faults and is the "
           "benchmark every other policy is measured against.\n"
           "Implementation: next-use indices are precomputed in one backward pass, so each "
           "eviction costs O(frames).";
}

void Optimal::reset(int frameCount, const Trace& trace, const SimulationConfig&) {
    nextUse_.assign(trace.size(), NEVER);
    std::unordered_map<int, long long> seen;
    for (long long i = static_cast<long long>(trace.size()) - 1; i >= 0; --i) {
        int page = trace[static_cast<size_t>(i)].page;
        auto it = seen.find(page);
        nextUse_[static_cast<size_t>(i)] = (it == seen.end()) ? NEVER : it->second;
        seen[page] = i;
    }
    frameNext_.assign(frameCount, NEVER);
}

void Optimal::onHit(std::vector<Frame>&, int frame, const AccessContext& ctx) {
    frameNext_[frame] = nextUse_[static_cast<size_t>(ctx.time)];
}

void Optimal::onLoad(std::vector<Frame>&, int frame, const AccessContext& ctx) {
    frameNext_[frame] = nextUse_[static_cast<size_t>(ctx.time)];
}

Victim Optimal::selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) {
    int best = 0;
    for (int i = 1; i < static_cast<int>(frames.size()); ++i) {
        // Farthest next use wins. Ties only happen between pages that are never
        // used again; break them by least recent use, which depends on the trace
        // alone (not on memory size), so OPT keeps the inclusion property.
        if (frameNext_[i] > frameNext_[best] ||
            (frameNext_[i] == frameNext_[best] && frames[i].lastAccess < frames[best].lastAccess))
            best = i;
    }
    Victim v{best, ""};
    if (ctx.explain) {
        v.reason = frameNext_[best] == NEVER
                       ? std::string("never referenced again")
                       : cat("next use farthest in the future (t=", frameNext_[best], ")");
    }
    return v;
}

}  // namespace prs
