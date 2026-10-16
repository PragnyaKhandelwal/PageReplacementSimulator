// Concrete page replacement policies.
#pragma once

#include <cstdint>
#include <list>
#include <random>
#include <vector>

#include "algorithms/ReplacementAlgorithm.h"

namespace prs {

// ---------------------------------------------------------------- classic --

class FIFO : public ReplacementAlgorithm {
public:
    std::string key() const override { return "FIFO"; }
    std::string name() const override { return "First-In First-Out"; }
    std::string family() const override { return "Arrival order"; }
    std::string description() const override;
    Victim selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) override;
};

class LRU : public ReplacementAlgorithm {
public:
    std::string key() const override { return "LRU"; }
    std::string name() const override { return "Least Recently Used"; }
    std::string family() const override { return "Recency"; }
    std::string description() const override;
    bool isStackAlgorithm() const override { return true; }
    Victim selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) override;
};

class Optimal : public ReplacementAlgorithm {
public:
    std::string key() const override { return "OPT"; }
    std::string name() const override { return "Optimal (Belady's MIN)"; }
    std::string family() const override { return "Clairvoyant"; }
    std::string description() const override;
    bool isStackAlgorithm() const override { return true; }
    void reset(int frameCount, const Trace& trace, const SimulationConfig& cfg) override;
    void onHit(std::vector<Frame>& frames, int frame, const AccessContext& ctx) override;
    void onLoad(std::vector<Frame>& frames, int frame, const AccessContext& ctx) override;
    Victim selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) override;

private:
    std::vector<long long> nextUse_;    // nextUse_[i] = next index referencing trace[i].page
    std::vector<long long> frameNext_;  // next use of the page currently in each frame
};

// ------------------------------------------------------ LRU approximations --

class Clock : public ReplacementAlgorithm {
public:
    std::string key() const override { return "CLOCK"; }
    std::string name() const override { return "Second Chance (Clock)"; }
    std::string family() const override { return "LRU approximation"; }
    std::string description() const override;
    void reset(int frameCount, const Trace& trace, const SimulationConfig& cfg) override;
    Victim selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) override;

private:
    int hand_ = 0;
};

class EnhancedClock : public ReplacementAlgorithm {
public:
    std::string key() const override { return "ESC"; }
    std::string name() const override { return "Enhanced Second Chance (R,M)"; }
    std::string family() const override { return "LRU approximation (dirty aware)"; }
    std::string description() const override;
    void reset(int frameCount, const Trace& trace, const SimulationConfig& cfg) override;
    Victim selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) override;

private:
    int hand_ = 0;
};

class NRU : public ReplacementAlgorithm {
public:
    std::string key() const override { return "NRU"; }
    std::string name() const override { return "Not Recently Used"; }
    std::string family() const override { return "LRU approximation (dirty aware)"; }
    std::string description() const override;
    void reset(int frameCount, const Trace& trace, const SimulationConfig& cfg) override;
    void afterReference(std::vector<Frame>& frames, const AccessContext& ctx) override;
    Victim selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) override;

private:
    int interval_ = 8;
};

class Aging : public ReplacementAlgorithm {
public:
    std::string key() const override { return "AGING"; }
    std::string name() const override { return "Aging (software LRU counters)"; }
    std::string family() const override { return "LRU approximation"; }
    std::string description() const override;
    void reset(int frameCount, const Trace& trace, const SimulationConfig& cfg) override;
    void onLoad(std::vector<Frame>& frames, int frame, const AccessContext& ctx) override;
    void afterReference(std::vector<Frame>& frames, const AccessContext& ctx) override;
    Victim selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) override;

private:
    std::vector<std::uint32_t> counters_;
    int bits_ = 8;
    int interval_ = 1;
};

// -------------------------------------------------------------- frequency --

class LFU : public ReplacementAlgorithm {
public:
    std::string key() const override { return "LFU"; }
    std::string name() const override { return "Least Frequently Used"; }
    std::string family() const override { return "Frequency"; }
    std::string description() const override;
    Victim selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) override;
};

class MFU : public ReplacementAlgorithm {
public:
    std::string key() const override { return "MFU"; }
    std::string name() const override { return "Most Frequently Used"; }
    std::string family() const override { return "Frequency"; }
    std::string description() const override;
    Victim selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) override;
};

// ------------------------------------------------------------- baselines --

class RandomReplacement : public ReplacementAlgorithm {
public:
    std::string key() const override { return "RANDOM"; }
    std::string name() const override { return "Random Replacement"; }
    std::string family() const override { return "Baseline"; }
    std::string description() const override;
    void reset(int frameCount, const Trace& trace, const SimulationConfig& cfg) override;
    Victim selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) override;

private:
    std::mt19937 rng_;
};

// --------------------------------------------------------------- adaptive --

// Adaptive Replacement Cache (Megiddo & Modha, FAST'03). Balances a recency
// list (T1) and a frequency list (T2) using ghost lists (B1, B2) of recently
// evicted pages to learn the right split p on-line.
class ARC : public ReplacementAlgorithm {
public:
    std::string key() const override { return "ARC"; }
    std::string name() const override { return "Adaptive Replacement Cache"; }
    std::string family() const override { return "Adaptive (recency + frequency)"; }
    std::string description() const override;
    void reset(int frameCount, const Trace& trace, const SimulationConfig& cfg) override;
    void onHit(std::vector<Frame>& frames, int frame, const AccessContext& ctx) override;
    void onMiss(const std::vector<Frame>& frames, const AccessContext& ctx) override;
    Victim selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) override;
    void onLoad(std::vector<Frame>& frames, int frame, const AccessContext& ctx) override;

    int target() const { return p_; }

private:
    void replace(bool requestInB2);

    int c_ = 0;  // cache size (frames)
    int p_ = 0;  // adaptive target size of T1
    std::list<int> t1_, t2_, b1_, b2_;  // front = MRU, back = LRU
    int pendingVictim_ = EMPTY;
    bool insertIntoT2_ = false;
    std::string pendingReason_;
};

}  // namespace prs
