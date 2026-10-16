#include "analysis/Analyzer.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_map>

#include "algorithms/AlgorithmFactory.h"
#include "core/Simulator.h"
#include "core/Util.h"
#include "io/TraceGenerator.h"

namespace prs {

namespace {
SimulationConfig quiet(const SimulationConfig& base, int frames) {
    SimulationConfig cfg = base;
    cfg.frames = frames;
    cfg.recordSteps = false;
    cfg.explain = false;
    return cfg;
}

long long faultsFor(const std::string& key, const Trace& trace, const SimulationConfig& cfg) {
    auto algo = createAlgorithm(key);
    if (!algo) throw std::invalid_argument("unknown algorithm: " + key);
    return Simulator::run(*algo, trace, cfg).faults;
}
}  // namespace

// ------------------------------------------------------------ comparison --

long long optimalFaults(const Trace& trace, const SimulationConfig& config) {
    return faultsFor("OPT", trace, quiet(config, config.frames));
}

std::vector<RankedResult> rankResults(const std::vector<SimulationResult>& results,
                                      long long optFaults) {
    std::vector<RankedResult> ranked;
    for (const auto& r : results) {
        RankedResult rr;
        rr.result = &r;
        rr.extraFaultsVsOpt = r.faults - optFaults;
        rr.optimalityPercent = r.faults ? 100.0 * optFaults / r.faults : 100.0;
        ranked.push_back(rr);
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const RankedResult& a, const RankedResult& b) {
        if (a.result->faults != b.result->faults) return a.result->faults < b.result->faults;
        return a.result->writeBacks < b.result->writeBacks;
    });
    // Dense ranking: equal fault counts share a rank.
    for (size_t i = 0; i < ranked.size(); ++i) {
        if (i > 0 && ranked[i].result->faults == ranked[i - 1].result->faults &&
            ranked[i].result->writeBacks == ranked[i - 1].result->writeBacks)
            ranked[i].rank = ranked[i - 1].rank;
        else
            ranked[i].rank = static_cast<int>(i) + 1;
    }
    return ranked;
}

// --------------------------------------------------------- frame sweep -----

SweepResult frameSweep(const Trace& trace, const std::vector<std::string>& keys, int minFrames,
                       int maxFrames, const SimulationConfig& base) {
    if (minFrames < 1 || maxFrames < minFrames)
        throw std::invalid_argument("invalid frame range for sweep");
    SweepResult sweep;
    sweep.references = static_cast<long long>(trace.size());
    for (int f = minFrames; f <= maxFrames; ++f) sweep.frameCounts.push_back(f);
    for (const auto& key : keys) {
        sweep.algorithms.push_back(key);
        std::vector<long long> row;
        for (int f : sweep.frameCounts) row.push_back(faultsFor(key, trace, quiet(base, f)));
        sweep.faults.push_back(row);
    }
    return sweep;
}

// ------------------------------------------------------ Belady's anomaly ---

std::vector<BeladyAnomaly> detectBeladyAnomalies(const SweepResult& sweep) {
    std::vector<BeladyAnomaly> out;
    for (size_t a = 0; a < sweep.algorithms.size(); ++a) {
        for (size_t i = 1; i < sweep.frameCounts.size(); ++i) {
            if (sweep.faults[a][i] > sweep.faults[a][i - 1]) {
                out.push_back(BeladyAnomaly{sweep.algorithms[a], sweep.frameCounts[i - 1],
                                            sweep.faults[a][i - 1], sweep.frameCounts[i],
                                            sweep.faults[a][i]});
            }
        }
    }
    return out;
}

namespace {

std::vector<BeladyAnomaly> anomaliesOf(const std::string& key, const std::vector<PageRef>& refs,
                                       int maxFrames, const SimulationConfig& base) {
    return detectBeladyAnomalies(frameSweep(Trace(refs, "candidate"), {key}, 1, maxFrames, base));
}

// Greedy delta-debugging: drop references one at a time while the anomaly
// survives, so the result is a short string a human can follow by hand.
std::vector<PageRef> shrinkAnomalousTrace(const std::string& key, std::vector<PageRef> refs,
                                          int maxFrames, const SimulationConfig& base) {
    for (bool changed = true; changed;) {
        changed = false;
        for (size_t i = 0; i < refs.size();) {
            std::vector<PageRef> shorter = refs;
            shorter.erase(shorter.begin() + static_cast<std::ptrdiff_t>(i));
            if (!shorter.empty() && !anomaliesOf(key, shorter, maxFrames, base).empty()) {
                refs.swap(shorter);
                changed = true;
            } else {
                ++i;
            }
        }
    }
    return refs;
}

// Renumber pages 1..k by first appearance; kept only if the anomaly survives.
std::vector<PageRef> relabelPages(const std::string& key, const std::vector<PageRef>& refs,
                                  int maxFrames, const SimulationConfig& base) {
    std::map<int, int> label;
    std::vector<PageRef> out = refs;
    for (auto& r : out) {
        auto it = label.find(r.page);
        if (it == label.end()) it = label.insert({r.page, static_cast<int>(label.size()) + 1}).first;
        r.page = it->second;
    }
    return anomaliesOf(key, out, maxFrames, base).empty() ? refs : out;
}

}  // namespace

BeladyHunt huntBeladyAnomaly(const std::string& key, int maxAttempts, int length, int pages,
                             int maxFrames, unsigned int seed) {
    BeladyHunt hunt;
    SimulationConfig base;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        GeneratorConfig g;
        g.pattern = Pattern::Uniform;
        g.length = length;
        g.pages = pages;
        g.seed = seed + static_cast<unsigned int>(attempt);
        Trace t = generateTrace(g);
        SweepResult s = frameSweep(t, {key}, 1, maxFrames, base);
        auto anomalies = detectBeladyAnomalies(s);
        hunt.attempts = attempt;
        if (!anomalies.empty()) {
            std::vector<PageRef> refs = shrinkAnomalousTrace(key, t.refs(), maxFrames, base);
            refs = relabelPages(key, refs, maxFrames, base);
            hunt.found = true;
            hunt.originalLength = static_cast<int>(t.size());
            hunt.trace = Trace(refs, cat("belady-hunt-", key, "-", attempt));
            hunt.anomaly = anomaliesOf(key, refs, maxFrames, base).front();
            return hunt;
        }
    }
    return hunt;
}

// ------------------------------------------------------- working set -------

WorkingSetReport analyzeWorkingSet(const Trace& trace, int window, int frames) {
    if (window < 1) throw std::invalid_argument("working-set window must be positive");
    WorkingSetReport r;
    r.window = window;
    r.frames = frames;
    std::unordered_map<int, int> inWindow;
    long long sum = 0;
    int above = 0;
    for (size_t t = 0; t < trace.size(); ++t) {
        ++inWindow[trace[t].page];
        if (t >= static_cast<size_t>(window)) {
            int old = trace[t - window].page;
            if (--inWindow[old] == 0) inWindow.erase(old);
        }
        int size = static_cast<int>(inWindow.size());
        r.sizes.push_back(size);
        sum += size;
        r.peak = std::max(r.peak, size);
        if (size > frames) ++above;
    }
    if (!r.sizes.empty()) {
        r.average = double(sum) / r.sizes.size();
        r.fractionAboveFrames = double(above) / r.sizes.size();
        std::vector<int> sorted = r.sizes;
        std::sort(sorted.begin(), sorted.end());
        r.percentile90 = sorted[std::min(sorted.size() - 1, (sorted.size() * 9) / 10)];
    }
    // Denning: if the demand (sum of working sets) exceeds memory, the process thrashes.
    r.thrashingRisk = r.average > frames || r.fractionAboveFrames > 0.5;
    return r;
}

// --------------------------------------------- stack distance (Mattson) ----

StackDistanceReport analyzeStackDistances(const Trace& trace, int maxFrames) {
    StackDistanceReport r;
    std::vector<int> stack;  // front = most recently used
    std::vector<long long> distances;
    for (const auto& ref : trace.refs()) {
        auto it = std::find(stack.begin(), stack.end(), ref.page);
        if (it == stack.end()) {
            ++r.coldMisses;
        } else {
            long long d = (it - stack.begin()) + 1;
            if (static_cast<long long>(r.histogram.size()) <= d) r.histogram.resize(d + 1, 0);
            ++r.histogram[d];
            distances.push_back(d);
            stack.erase(it);
        }
        stack.insert(stack.begin(), ref.page);
    }
    if (r.histogram.empty()) r.histogram.resize(1, 0);

    r.lruFaultsByFrames.assign(maxFrames + 1, 0);
    for (int f = 1; f <= maxFrames; ++f) {
        long long faults = r.coldMisses;
        for (size_t d = f + 1; d < r.histogram.size(); ++d) faults += r.histogram[d];
        r.lruFaultsByFrames[f] = faults;
    }
    if (!distances.empty()) {
        long long sum = 0;
        for (long long d : distances) sum += d;
        r.meanDistance = double(sum) / distances.size();
        std::sort(distances.begin(), distances.end());
        size_t n = distances.size();
        r.medianDistance = (n % 2) ? distances[n / 2]
                                   : (distances[n / 2 - 1] + distances[n / 2]) / 2.0;
    }
    return r;
}

// ------------------------------------------------- inclusion property ------

InclusionReport checkInclusion(const std::string& key, const Trace& trace, int frames,
                               const SimulationConfig& base) {
    InclusionReport rep;
    rep.algorithm = key;
    rep.frames = frames;
    SimulationConfig a = base, b = base;
    a.frames = frames;
    b.frames = frames + 1;
    a.recordSteps = b.recordSteps = true;
    a.explain = b.explain = false;
    auto algA = createAlgorithm(key);
    auto algB = createAlgorithm(key);
    if (!algA || !algB) throw std::invalid_argument("unknown algorithm: " + key);
    SimulationResult ra = Simulator::run(*algA, trace, a);
    SimulationResult rb = Simulator::run(*algB, trace, b);

    for (size_t i = 0; i < trace.size(); ++i) {
        std::set<int> small, large;
        for (int p : ra.steps[i].frames) if (p != EMPTY) small.insert(p);
        for (int p : rb.steps[i].frames) if (p != EMPTY) large.insert(p);
        if (!std::includes(large.begin(), large.end(), small.begin(), small.end())) {
            rep.holds = false;
            rep.violationStep = static_cast<long long>(i);
            rep.smallerSet.assign(small.begin(), small.end());
            rep.largerSet.assign(large.begin(), large.end());
            break;
        }
    }
    return rep;
}

// ------------------------------------------------------- trace profile -----

TraceProfile profileTrace(const Trace& trace, int frames) {
    TraceProfile p;
    p.length = trace.size();
    if (trace.empty()) {
        p.localityClass = "empty";
        return p;
    }
    std::map<int, long long> counts;
    long long sequential = 0;
    for (size_t i = 0; i < trace.size(); ++i) {
        ++counts[trace[i].page];
        if (i > 0 && trace[i].page == trace[i - 1].page + 1) ++sequential;
    }
    p.distinct = static_cast<int>(counts.size());
    p.minPage = counts.begin()->first;
    p.maxPage = counts.rbegin()->first;
    p.writeFraction = trace.writeFraction();
    p.reuseRatio = 1.0 - double(p.distinct) / p.length;
    p.sequentialFraction = p.length > 1 ? double(sequential) / (p.length - 1) : 0.0;

    std::vector<std::pair<int, long long>> byCount(counts.begin(), counts.end());
    std::stable_sort(byCount.begin(), byCount.end(),
                     [](const std::pair<int, long long>& a, const std::pair<int, long long>& b) {
                         return a.second > b.second;
                     });
    for (size_t i = 0; i < byCount.size() && i < 5; ++i) p.hottestPages.push_back(byCount[i]);
    size_t hotN = std::max<size_t>(1, (byCount.size() + 4) / 5);
    long long hotRefs = 0;
    for (size_t i = 0; i < hotN; ++i) hotRefs += byCount[i].second;
    p.hotSetShare = double(hotRefs) / p.length;

    StackDistanceReport sd = analyzeStackDistances(trace, std::max(1, frames));
    p.meanStackDistance = sd.meanDistance;
    p.medianStackDistance = sd.medianDistance;

    // Classification + actionable insights.
    if (p.distinct <= frames) {
        p.localityClass = "fits in memory";
        p.insights.push_back(cat("The whole footprint (", p.distinct, " pages) fits in ", frames,
                                 " frames: every policy incurs only the ", p.distinct,
                                 " compulsory faults."));
    } else if (p.reuseRatio < 0.15) {
        p.localityClass = "streaming (almost no reuse)";
        p.insights.push_back("Almost every reference touches a new page - faults are dominated by "
                             "cold misses and no policy can help much.");
    } else if (sd.medianDistance > frames && p.sequentialFraction > 0.6) {
        p.localityClass = "cyclic / looping";
        p.insights.push_back(cat("Looping pattern whose reuse distance (median ",
                                 fixed(sd.medianDistance, 1), ") exceeds the ", frames,
                                 " frames: LRU and FIFO evict exactly the page needed next "
                                 "(worst case). RANDOM and OPT keep part of the loop resident."));
    } else if (sd.medianDistance <= frames) {
        p.localityClass = "strong temporal locality";
        p.insights.push_back(cat("Median LRU stack distance is ", fixed(sd.medianDistance, 1),
                                 " (<= ", frames, " frames): most re-references are recent, so "
                                 "recency-based policies (LRU, CLOCK, AGING, ARC) should approach OPT."));
    } else {
        p.localityClass = "weak locality";
        p.insights.push_back(cat("Median reuse distance ", fixed(sd.medianDistance, 1),
                                 " is larger than memory - expect high fault ratios; adding frames "
                                 "pays off until the reuse distance is covered."));
    }
    if (p.sequentialFraction > 0.4 && p.localityClass != "cyclic / looping")
        p.insights.push_back(cat(percent(p.sequentialFraction, 0), " of references continue a "
                                 "sequential run: scan-resistant ARC protects the hot set from "
                                 "being flushed by scans."));
    if (p.hotSetShare > 0.5 && p.distinct >= 5)
        p.insights.push_back(cat("Skewed popularity: the hottest 20% of pages receive ",
                                 percent(p.hotSetShare, 0), " of references - frequency-aware "
                                 "policies (LFU, ARC) are competitive."));
    if (p.writeFraction > 0.25)
        p.insights.push_back(cat(percent(p.writeFraction, 0), " of references are writes: "
                                 "dirty-aware policies (ESC, NRU) can cut costly write-backs."));
    return p;
}

// ----------------------------------------------------- locality heatmap ----

Heatmap buildHeatmap(const Trace& trace, int maxColumns, int maxRows) {
    Heatmap h;
    if (trace.empty()) return h;
    maxColumns = std::max(1, maxColumns);
    h.windowSize = static_cast<int>((trace.size() + maxColumns - 1) / maxColumns);
    int columns = static_cast<int>((trace.size() + h.windowSize - 1) / h.windowSize);

    // Keep the most referenced pages when the address space is large.
    std::map<int, long long> counts;
    for (const auto& r : trace.refs()) ++counts[r.page];
    std::vector<std::pair<int, long long>> byCount(counts.begin(), counts.end());
    std::stable_sort(byCount.begin(), byCount.end(),
                     [](const std::pair<int, long long>& a, const std::pair<int, long long>& b) {
                         return a.second > b.second;
                     });
    if (static_cast<int>(byCount.size()) > maxRows) byCount.resize(maxRows);
    for (const auto& pc : byCount) h.pages.push_back(pc.first);
    std::sort(h.pages.begin(), h.pages.end());

    std::unordered_map<int, int> row;
    for (size_t i = 0; i < h.pages.size(); ++i) row[h.pages[i]] = static_cast<int>(i);
    h.counts.assign(h.pages.size(), std::vector<int>(columns, 0));
    for (int c = 0; c < columns; ++c) h.windowStart.push_back(c * h.windowSize);

    for (size_t i = 0; i < trace.size(); ++i) {
        auto it = row.find(trace[i].page);
        if (it == row.end()) continue;
        int& cell = h.counts[it->second][i / h.windowSize];
        ++cell;
        h.maxCount = std::max(h.maxCount, cell);
    }
    return h;
}

}  // namespace prs
