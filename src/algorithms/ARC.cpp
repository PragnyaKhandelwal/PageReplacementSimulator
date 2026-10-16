// Adaptive Replacement Cache, following the pseudo-code of Megiddo & Modha,
// "ARC: A Self-Tuning, Low Overhead Replacement Cache" (FAST 2003).
#include <algorithm>
#include <stdexcept>

#include "algorithms/Algorithms.h"
#include "core/Util.h"

namespace prs {

namespace {
bool contains(const std::list<int>& l, int page) {
    return std::find(l.begin(), l.end(), page) != l.end();
}
void erase(std::list<int>& l, int page) { l.remove(page); }
}  // namespace

std::string ARC::description() const {
    return "Keeps two LRU lists of resident pages - T1 (seen once recently) and T2 (seen at "
           "least twice) - plus two 'ghost' lists B1/B2 remembering pages recently evicted from "
           "each. A hit in a ghost list means that side was too small, so the target size p of "
           "T1 adapts on-line. Scan resistant (one-off scans only churn T1) and self-tuning; "
           "used in ZFS and IBM storage controllers.";
}

void ARC::reset(int frameCount, const Trace&, const SimulationConfig&) {
    c_ = frameCount;
    p_ = 0;
    t1_.clear(); t2_.clear(); b1_.clear(); b2_.clear();
    pendingVictim_ = EMPTY;
    insertIntoT2_ = false;
    pendingReason_.clear();
}

void ARC::onHit(std::vector<Frame>&, int, const AccessContext& ctx) {
    // Case I: hit in T1 or T2 -> move to MRU of T2.
    const int x = ctx.ref.page;
    erase(t1_, x);
    erase(t2_, x);
    t2_.push_front(x);
}

void ARC::replace(bool requestInB2) {
    const int t1 = static_cast<int>(t1_.size());
    bool fromT1 = !t1_.empty() && ((requestInB2 && t1 == p_) || t1 > p_);
    if (!fromT1 && t2_.empty()) fromT1 = true;  // defensive; ARC invariants prevent this
    if (fromT1) {
        pendingVictim_ = t1_.back();
        t1_.pop_back();
        b1_.push_front(pendingVictim_);
        pendingReason_ += cat("LRU of T1 (recency list, |T1|=", t1, " vs target p=", p_, ")");
    } else {
        pendingVictim_ = t2_.back();
        t2_.pop_back();
        b2_.push_front(pendingVictim_);
        pendingReason_ += cat("LRU of T2 (frequency list, |T1|=", t1, " within target p=", p_, ")");
    }
}

void ARC::onMiss(const std::vector<Frame>&, const AccessContext& ctx) {
    const int x = ctx.ref.page;
    pendingVictim_ = EMPTY;
    pendingReason_.clear();

    if (contains(b1_, x)) {
        // Case II: ghost hit in B1 - recency side deserved more room.
        int delta = std::max(static_cast<int>(b2_.size() / b1_.size()), 1);
        p_ = std::min(c_, p_ + delta);
        pendingReason_ = cat("ghost hit in B1, p grows to ", p_, "; ");
        replace(false);
        erase(b1_, x);
        insertIntoT2_ = true;
    } else if (contains(b2_, x)) {
        // Case III: ghost hit in B2 - frequency side deserved more room.
        int delta = std::max(static_cast<int>(b1_.size() / b2_.size()), 1);
        p_ = std::max(0, p_ - delta);
        pendingReason_ = cat("ghost hit in B2, p shrinks to ", p_, "; ");
        replace(true);
        erase(b2_, x);
        insertIntoT2_ = true;
    } else {
        // Case IV: brand-new page.
        insertIntoT2_ = false;
        const size_t l1 = t1_.size() + b1_.size();
        const size_t total = l1 + t2_.size() + b2_.size();
        if (l1 == static_cast<size_t>(c_)) {
            if (t1_.size() < static_cast<size_t>(c_)) {
                b1_.pop_back();
                replace(false);
            } else {
                pendingVictim_ = t1_.back();
                t1_.pop_back();
                pendingReason_ = "T1 holds the whole cache: drop its LRU page without a ghost";
            }
        } else if (total >= static_cast<size_t>(c_)) {
            if (total >= static_cast<size_t>(2 * c_)) b2_.pop_back();
            replace(false);
        }
    }
}

Victim ARC::selectVictim(std::vector<Frame>& frames, const AccessContext& ctx) {
    for (int i = 0; i < static_cast<int>(frames.size()); ++i) {
        if (frames[i].page == pendingVictim_ && pendingVictim_ != EMPTY) {
            Victim v{i, ""};
            if (ctx.explain) v.reason = pendingReason_;
            return v;
        }
    }
    throw std::logic_error("ARC directory out of sync with physical frames");
}

void ARC::onLoad(std::vector<Frame>&, int, const AccessContext& ctx) {
    if (insertIntoT2_) t2_.push_front(ctx.ref.page);
    else t1_.push_front(ctx.ref.page);
    pendingVictim_ = EMPTY;
}

}  // namespace prs
