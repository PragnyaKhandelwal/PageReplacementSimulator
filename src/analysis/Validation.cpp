#include "analysis/Validation.h"

#include "algorithms/AlgorithmFactory.h"
#include "core/Simulator.h"
#include "io/TraceIO.h"

namespace prs {

namespace {
const char* SILBERSCHATZ = "7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1";
const char* BELADY = "1 2 3 4 1 2 5 1 2 3 4 5";
}  // namespace

const std::vector<ValidationCase>& textbookCases() {
    static const std::vector<ValidationCase> cases = {
        {"Silberschatz FIFO example", "Silberschatz, OS Concepts", SILBERSCHATZ, 3, "FIFO", 15},
        {"Silberschatz LRU example", "Silberschatz, OS Concepts", SILBERSCHATZ, 3, "LRU", 12},
        {"Silberschatz OPT example", "Silberschatz, OS Concepts", SILBERSCHATZ, 3, "OPT", 9},
        {"Belady anomaly, 3 frames", "Belady (1969), Silberschatz", BELADY, 3, "FIFO", 9},
        {"Belady anomaly, 4 frames", "Belady (1969), Silberschatz", BELADY, 4, "FIFO", 10},
        {"Belady string, LRU 3 frames", "hand computed", BELADY, 3, "LRU", 10},
        {"Belady string, LRU 4 frames", "hand computed", BELADY, 4, "LRU", 8},
        {"Belady string, OPT 3 frames", "hand computed", BELADY, 3, "OPT", 7},
        {"Belady string, OPT 4 frames", "hand computed", BELADY, 4, "OPT", 6},
        {"GfG FIFO example", "GeeksforGeeks", "1 3 0 3 5 6 3", 3, "FIFO", 6},
        {"GfG LRU example", "GeeksforGeeks", "7 0 1 2 0 3 0 4 2 3 0 3 2", 4, "LRU", 6},
        {"GfG Optimal example", "GeeksforGeeks", "7 0 1 2 0 3 0 4 2 3 0 3 2 3", 4, "OPT", 6},
        {"Clock on Belady string", "hand computed (R=1 on load)", BELADY, 3, "CLOCK", 9},
        {"LFU tie-break by age", "hand computed", "1 2 3 1 4 1 2 5", 3, "LFU", 6},
        {"MFU tie-break by age", "hand computed", "1 2 3 1 4 1 2 5", 3, "MFU", 7},
        {"ESC keeps dirty page", "hand computed", "1W 2 3 4 1", 3, "ESC", 4, 0},
        {"FIFO writes back dirty victim", "hand computed", "1W 2 3 4 1", 3, "FIFO", 5, 1},
        {"Footprint fits in memory", "definition", "1 2 3 1 2 3 1 2 3", 3, "ARC", 3},
    };
    return cases;
}

std::vector<ValidationOutcome> runValidation() {
    std::vector<ValidationOutcome> out;
    for (const auto& c : textbookCases()) {
        SimulationConfig cfg;
        cfg.frames = c.frames;
        cfg.recordSteps = false;
        auto algo = createAlgorithm(c.algorithm);
        SimulationResult r = Simulator::run(*algo, parseTrace(c.trace, c.name), cfg);
        ValidationOutcome o;
        o.testCase = c;
        o.actualFaults = r.faults;
        o.actualWriteBacks = r.writeBacks;
        o.passed = r.faults == c.expectedFaults &&
                   (c.expectedWriteBacks < 0 || r.writeBacks == c.expectedWriteBacks);
        out.push_back(o);
    }
    return out;
}

}  // namespace prs
