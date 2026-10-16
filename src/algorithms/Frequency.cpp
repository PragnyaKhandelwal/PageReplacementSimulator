// Frequency based policies (LFU, MFU) and the Random baseline.
#include "algorithms/Algorithms.h"
#include "core/Util.h"

namespace prs {

// -------------------------------------------------------------------- LFU --

std::string LFU::description() const {
    return "Evicts the page with the smallest reference count since it was loaded; ties go to "
           "the oldest page. Good when a stable hot set exists, but a page that was heavily "
           "used long ago can linger ('cache pollution'), and new pages are easily evicted.";
}

Victim LFU::selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) {
    int best = 0;
    for (int i = 1; i < static_cast<int>(frames.size()); ++i) {
        if (frames[i].frequency < frames[best].frequency ||
            (frames[i].frequency == frames[best].frequency &&
             frames[i].loadTime < frames[best].loadTime))
            best = i;
    }
    Victim v{best, ""};
    if (ctx.explain) v.reason = cat("lowest use count (", frames[best].frequency, ")");
    return v;
}

// -------------------------------------------------------------------- MFU --

std::string MFU::description() const {
    return "Evicts the page with the LARGEST reference count, on the argument that a page with "
           "a small count was probably just brought in and is yet to be used. Rarely good in "
           "practice; included to contrast with LFU.";
}

Victim MFU::selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) {
    int best = 0;
    for (int i = 1; i < static_cast<int>(frames.size()); ++i) {
        if (frames[i].frequency > frames[best].frequency ||
            (frames[i].frequency == frames[best].frequency &&
             frames[i].loadTime < frames[best].loadTime))
            best = i;
    }
    Victim v{best, ""};
    if (ctx.explain) v.reason = cat("highest use count (", frames[best].frequency, ")");
    return v;
}

// ----------------------------------------------------------------- Random --

std::string RandomReplacement::description() const {
    return "Evicts a uniformly random frame (seeded, so runs are reproducible). Zero "
           "bookkeeping; serves as the 'no intelligence' baseline. Surprisingly robust on "
           "loops where deterministic policies fall into pathological patterns.";
}

void RandomReplacement::reset(int, const Trace&, const SimulationConfig& cfg) { rng_.seed(cfg.seed); }

Victim RandomReplacement::selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) {
    std::uniform_int_distribution<int> dist(0, static_cast<int>(frames.size()) - 1);
    Victim v{dist(rng_), ""};
    if (ctx.explain) v.reason = "chosen uniformly at random";
    return v;
}

}  // namespace prs
