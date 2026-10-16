// Analysis modules built on top of the simulator.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/Trace.h"
#include "core/Types.h"

namespace prs {

// ------------------------------------------------------------ comparison --

struct RankedResult {
    int rank = 0;
    const SimulationResult* result = nullptr;
    long long extraFaultsVsOpt = 0;  // faults above the optimal benchmark
    double optimalityPercent = 0;    // OPT faults / faults * 100
};

// Ranks by faults, then write-backs. optFaults is the benchmark (run OPT if
// it is not among the results).
std::vector<RankedResult> rankResults(const std::vector<SimulationResult>& results,
                                      long long optFaults);

long long optimalFaults(const Trace& trace, const SimulationConfig& config);

// --------------------------------------------------------- frame sweep -----

struct SweepResult {
    std::vector<int> frameCounts;
    std::vector<std::string> algorithms;
    std::vector<std::vector<long long>> faults;  // [algorithm][frameIndex]
    long long references = 0;
};

SweepResult frameSweep(const Trace& trace, const std::vector<std::string>& keys, int minFrames,
                       int maxFrames, const SimulationConfig& base);

// ------------------------------------------------------ Belady's anomaly ---

struct BeladyAnomaly {
    std::string algorithm;
    int framesFrom = 0;
    long long faultsFrom = 0;
    int framesTo = 0;
    long long faultsTo = 0;
};

std::vector<BeladyAnomaly> detectBeladyAnomalies(const SweepResult& sweep);

struct BeladyHunt {
    bool found = false;
    int attempts = 0;
    int originalLength = 0;  // length of the random trace before it was shrunk
    Trace trace;
    BeladyAnomaly anomaly;
};

// Random search for a trace on which `key` exhibits Belady's anomaly. The trace
// found is shrunk to a minimal anomalous string and its pages are renumbered 1..k.
BeladyHunt huntBeladyAnomaly(const std::string& key, int maxAttempts, int length, int pages,
                             int maxFrames, unsigned int seed);

// ------------------------------------------------------- working set -------

struct WorkingSetReport {
    int window = 0;              // delta
    int frames = 0;              // frames available for comparison
    std::vector<int> sizes;      // |WS(t, delta)| for every t
    double average = 0;
    int peak = 0;
    int percentile90 = 0;
    double fractionAboveFrames = 0;  // share of time the WS does not fit
    bool thrashingRisk = false;
};

WorkingSetReport analyzeWorkingSet(const Trace& trace, int window, int frames);

// --------------------------------------------- stack distance (Mattson) ----

struct StackDistanceReport {
    std::vector<long long> histogram;      // histogram[d] = re-references at LRU depth d (1-based)
    long long coldMisses = 0;
    std::vector<long long> lruFaultsByFrames;  // index f (0 unused) = LRU faults with f frames
    double meanDistance = 0;
    double medianDistance = 0;
};

// One pass computes LRU faults for every memory size up to maxFrames.
StackDistanceReport analyzeStackDistances(const Trace& trace, int maxFrames);

// ------------------------------------------------- inclusion property ------

struct InclusionReport {
    std::string algorithm;
    int frames = 0;  // compares M(frames) with M(frames + 1)
    bool holds = true;
    long long violationStep = -1;
    std::vector<int> smallerSet, largerSet;
};

InclusionReport checkInclusion(const std::string& key, const Trace& trace, int frames,
                               const SimulationConfig& base);

// ------------------------------------------------------- trace profile -----

struct TraceProfile {
    size_t length = 0;
    int distinct = 0;
    int minPage = 0, maxPage = 0;
    double writeFraction = 0;
    double reuseRatio = 0;          // 1 - distinct / length
    double sequentialFraction = 0;  // refs where page == previous + 1
    double hotSetShare = 0;         // share of refs going to the top 20% pages
    double meanStackDistance = 0;
    double medianStackDistance = 0;
    std::string localityClass;
    std::vector<std::pair<int, long long>> hottestPages;  // (page, count), top 5
    std::vector<std::string> insights;
};

TraceProfile profileTrace(const Trace& trace, int frames);

// ----------------------------------------------------- locality heatmap ----

struct Heatmap {
    std::vector<int> pages;                 // row labels
    std::vector<int> windowStart;           // first reference index of each column
    int windowSize = 1;
    std::vector<std::vector<int>> counts;   // [row][column]
    int maxCount = 0;
};

Heatmap buildHeatmap(const Trace& trace, int maxColumns, int maxRows);

}  // namespace prs
