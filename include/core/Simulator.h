// The simulation engine: replays a trace against a replacement policy while
// modelling page-table lookups, page faults, frame allocation, dirty-page
// write-backs and timing.
#pragma once

#include <string>
#include <vector>

#include "algorithms/ReplacementAlgorithm.h"
#include "core/PageTable.h"
#include "core/Trace.h"
#include "core/Types.h"

namespace prs {

class Simulator {
public:
    // Run one policy on one trace. Throws std::invalid_argument on bad input.
    static SimulationResult run(ReplacementAlgorithm& algorithm, const Trace& trace,
                                const SimulationConfig& config);

    // Convenience: build each policy by key and run it on the same trace.
    static std::vector<SimulationResult> runAll(const std::vector<std::string>& keys,
                                                const Trace& trace,
                                                const SimulationConfig& config);

    // Same as run() but also hands back the final page table (per-page stats).
    static SimulationResult run(ReplacementAlgorithm& algorithm, const Trace& trace,
                                const SimulationConfig& config, PageTable& pageTableOut);
};

}  // namespace prs
