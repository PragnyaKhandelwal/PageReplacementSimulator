// Synthetic workload generator. Each pattern stresses the policies in a
// different way, so comparisons can be made "under different access patterns"
// as required by the synopsis.
#pragma once

#include <string>
#include <vector>

#include "core/Trace.h"

namespace prs {

enum class Pattern {
    Uniform,     // every page equally likely - no locality at all
    Locality,    // phases with a small working set that shifts over time
    Loop,        // cyclic sweep 0..L-1 repeated (LRU/FIFO worst case when L > frames)
    Sequential,  // runs of consecutive pages from random offsets (streaming)
    Zipf,        // skewed popularity: few hot pages, long cold tail (80/20 rule)
    Mixed        // hot set interrupted by one-off scans (database-like)
};

struct GeneratorConfig {
    Pattern pattern = Pattern::Locality;
    int length = 40;              // number of references
    int pages = 10;               // size of the virtual address space in pages
    double writeProbability = 0;  // chance that a reference is a write
    unsigned int seed = 42;
    int workingSetSize = 4;       // Locality: pages in each phase
    int phaseLength = 15;         // Locality: references per phase
    double zipfExponent = 1.0;    // Zipf: skew (higher = hotter hot set)
    int loopSize = 0;             // Loop: cycle length (0 = all pages)
};

Trace generateTrace(const GeneratorConfig& config);

std::string patternName(Pattern p);
std::string patternDescription(Pattern p);
bool parsePattern(const std::string& name, Pattern& out);
const std::vector<Pattern>& allPatterns();

}  // namespace prs
