// LRU approximations that rely on the hardware R (reference) and M (modify)
// bits: Second Chance (Clock), Enhanced Second Chance, NRU and Aging.
#include <algorithm>

#include "algorithms/Algorithms.h"
#include "core/Util.h"

namespace prs {

namespace {
int oldestAmong(const std::vector<Frame>& frames, const std::vector<int>& candidates) {
    int best = candidates.front();
    for (int idx : candidates)
        if (frames[idx].loadTime < frames[best].loadTime) best = idx;
    return best;
}

std::string bitsOf(const Frame& f) {
    return cat("(R=", f.referenced ? 1 : 0, ",M=", f.dirty ? 1 : 0, ")");
}
}  // namespace

// ------------------------------------------------------------------ Clock --

std::string Clock::description() const {
    return "FIFO with a second chance. Frames form a circular list swept by a 'clock hand'. "
           "If the page under the hand has its reference bit set, the bit is cleared and the "
           "hand moves on; the first page found with R=0 is evicted.\n"
           "Cost: amortised O(1). Used (in variants) by most real kernels because it "
           "approximates LRU using only one hardware bit per frame.";
}

void Clock::reset(int, const Trace&, const SimulationConfig&) { hand_ = 0; }

Victim Clock::selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) {
    const int n = static_cast<int>(frames.size());
    std::vector<int> spared;
    while (frames[hand_].referenced) {
        frames[hand_].referenced = false;  // second chance
        if (ctx.explain) spared.push_back(frames[hand_].page);
        hand_ = (hand_ + 1) % n;
    }
    Victim v{hand_, ""};
    hand_ = (hand_ + 1) % n;
    if (ctx.explain) {
        v.reason = "clock hand found R=0";
        if (!spared.empty()) {
            std::vector<std::string> s;
            for (int p : spared) s.push_back(std::to_string(p));
            v.reason += cat(" after giving a second chance to {", join(s, ","), "}");
        }
    }
    return v;
}

// --------------------------------------------------------- Enhanced Clock --

std::string EnhancedClock::description() const {
    return "Second chance that also looks at the modify bit, ranking pages into four classes "
           "(R,M): (0,0) best victim, (0,1), (1,0), (1,1) worst. Pass 1 searches for (0,0) "
           "without touching bits; pass 2 searches for (0,1) while clearing R bits; repeat.\n"
           "Prefers clean pages, which avoids expensive disk write-backs (used in classic Mac OS "
           "and described in Silberschatz 10.4.5.3).";
}

void EnhancedClock::reset(int, const Trace&, const SimulationConfig&) { hand_ = 0; }

Victim EnhancedClock::selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) {
    const int n = static_cast<int>(frames.size());
    for (int round = 0; round < 3; ++round) {
        // Pass 1: look for (0,0) without modifying anything.
        for (int i = 0; i < n; ++i) {
            int idx = (hand_ + i) % n;
            if (!frames[idx].referenced && !frames[idx].dirty) {
                hand_ = (idx + 1) % n;
                Victim v{idx, ""};
                if (ctx.explain) v.reason = "class (R=0,M=0): not recently used and clean";
                return v;
            }
        }
        // Pass 2: look for (0,1), clearing reference bits as the hand sweeps.
        for (int i = 0; i < n; ++i) {
            int idx = (hand_ + i) % n;
            if (!frames[idx].referenced && frames[idx].dirty) {
                hand_ = (idx + 1) % n;
                Victim v{idx, ""};
                if (ctx.explain) v.reason = "class (R=0,M=1): not recently used but dirty";
                return v;
            }
            frames[idx].referenced = false;
        }
    }
    // Unreachable: after one full pass 2 every R bit is clear.
    Victim v{hand_, "fallback"};
    hand_ = (hand_ + 1) % n;
    return v;
}

// -------------------------------------------------------------------- NRU --

std::string NRU::description() const {
    return "Not Recently Used (Tanenbaum). A periodic clock interrupt clears every R bit; on "
           "a fault, pages are ranked by class = 2*R + M and a page from the lowest non-empty "
           "class is evicted (oldest first here, for reproducibility).\n"
           "Crude but cheap; the reset interval is configurable to show how the 'recently' "
           "window changes behaviour.";
}

void NRU::reset(int, const Trace&, const SimulationConfig& cfg) {
    interval_ = std::max(1, cfg.nruResetInterval);
}

void NRU::afterReference(std::vector<Frame>& frames, const AccessContext& ctx) {
    if ((ctx.time + 1) % interval_ == 0)
        for (Frame& f : frames) f.referenced = false;  // simulated clock tick
}

Victim NRU::selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) {
    for (int cls = 0; cls < 4; ++cls) {
        std::vector<int> candidates;
        for (int i = 0; i < static_cast<int>(frames.size()); ++i) {
            int c = (frames[i].referenced ? 2 : 0) + (frames[i].dirty ? 1 : 0);
            if (c == cls) candidates.push_back(i);
        }
        if (!candidates.empty()) {
            int idx = oldestAmong(frames, candidates);
            Victim v{idx, ""};
            if (ctx.explain)
                v.reason = cat("lowest NRU class ", cls, " ", bitsOf(frames[idx]),
                               ", oldest among ", candidates.size(), " candidate(s)");
            return v;
        }
    }
    return Victim{0, "fallback"};
}

// ------------------------------------------------------------------ Aging --

std::string Aging::description() const {
    return "Software approximation of LRU. Each frame has an n-bit counter; on every tick the "
           "counter is shifted right and the R bit is inserted at the most significant bit, then "
           "R is cleared. The page with the smallest counter (least recent activity) is evicted.\n"
           "Unlike LRU it only remembers the last n ticks, and it cannot order two pages "
           "referenced within the same tick - showing the price of approximation.";
}

void Aging::reset(int frameCount, const Trace&, const SimulationConfig& cfg) {
    bits_ = std::min(32, std::max(1, cfg.agingBits));
    interval_ = std::max(1, cfg.agingInterval);
    counters_.assign(frameCount, 0);
}

void Aging::onLoad(std::vector<Frame>&, int frame, const AccessContext&) { counters_[frame] = 0; }

void Aging::afterReference(std::vector<Frame>& frames, const AccessContext& ctx) {
    if ((ctx.time + 1) % interval_ != 0) return;
    const std::uint32_t msb = 1u << (bits_ - 1);
    for (size_t i = 0; i < frames.size(); ++i) {
        if (frames[i].empty()) continue;
        counters_[i] = (counters_[i] >> 1) | (frames[i].referenced ? msb : 0u);
        frames[i].referenced = false;
    }
}

Victim Aging::selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) {
    // A set R bit means activity newer than any shifted history.
    auto score = [&](int i) {
        return (static_cast<unsigned long long>(frames[i].referenced) << bits_) | counters_[i];
    };
    int best = 0;
    for (int i = 1; i < static_cast<int>(frames.size()); ++i) {
        if (score(i) < score(best) ||
            (score(i) == score(best) && frames[i].loadTime < frames[best].loadTime))
            best = i;
    }
    Victim v{best, ""};
    if (ctx.explain) {
        std::string binary;
        for (int b = bits_ - 1; b >= 0; --b) binary += ((counters_[best] >> b) & 1u) ? '1' : '0';
        v.reason = cat("smallest aging counter ", binary);
    }
    return v;
}

}  // namespace prs
