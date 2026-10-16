// Unit + property tests. No external framework: build and run pagesim_tests.
//
//  * textbook cases with hand-calculated answers (validation suite)
//  * invariants that must hold for EVERY policy on EVERY trace
//  * cross-checks between independent implementations (Mattson vs LRU)
//  * theory checks (inclusion property, Belady's anomaly)
//  * parser / generator behaviour
#include <algorithm>
#include <functional>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "algorithms/AlgorithmFactory.h"
#include "analysis/Analyzer.h"
#include "analysis/Validation.h"
#include "core/Simulator.h"
#include "core/Util.h"
#include "io/TraceGenerator.h"
#include "io/TraceIO.h"

using namespace prs;

namespace {

int g_checks = 0, g_failures = 0;

void check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::cout << "    FAIL: " << what << "\n";
    }
}

template <typename A, typename B>
void checkEq(const A& actual, const B& expected, const std::string& what) {
    ++g_checks;
    if (!(actual == expected)) {
        ++g_failures;
        std::cout << "    FAIL: " << what << " (expected " << expected << ", got " << actual << ")\n";
    }
}

void test(const std::string& name, const std::function<void()>& body) {
    int before = g_failures;
    try {
        body();
    } catch (const std::exception& e) {
        ++g_failures;
        std::cout << "    EXCEPTION: " << e.what() << "\n";
    }
    std::cout << (g_failures == before ? "  [PASS] " : "  [FAIL] ") << name << "\n";
}

SimulationResult simulate(const std::string& key, const Trace& t, int frames, bool steps = false) {
    SimulationConfig cfg;
    cfg.frames = frames;
    cfg.recordSteps = steps;
    auto algo = createAlgorithm(key);
    return Simulator::run(*algo, t, cfg);
}

std::vector<Trace> randomTraces() {
    std::vector<Trace> traces;
    unsigned seed = 1;
    for (Pattern p : allPatterns()) {
        for (int i = 0; i < 6; ++i) {
            GeneratorConfig g;
            g.pattern = p;
            g.length = 60 + 25 * i;
            g.pages = 5 + 2 * i;
            g.writeProbability = 0.3;
            g.seed = seed++;
            traces.push_back(generateTrace(g));
        }
    }
    return traces;
}

}  // namespace

int main() {
    std::cout << "Page Replacement Simulator - test suite\n\n";

    test("textbook / hand-calculated validation cases", [] {
        for (const auto& o : runValidation())
            check(o.passed, o.testCase.name + " [" + o.testCase.algorithm + "] expected " +
                                std::to_string(o.testCase.expectedFaults) + " got " +
                                std::to_string(o.actualFaults));
    });

    const auto traces = randomTraces();

    test("invariants for every policy on random traces", [&] {
        for (const auto& t : traces) {
            int distinct = t.distinctPages();
            for (int frames : {1, 2, 3, 5, 8}) {
                long long opt = simulate("OPT", t, frames).faults;
                for (const auto& key : allAlgorithmKeys()) {
                    SimulationResult r = simulate(key, t, frames, true);
                    std::string tag = key + " on " + t.name() + " f=" + std::to_string(frames);
                    checkEq(r.hits + r.faults, r.references, "hits + faults = refs: " + tag);
                    check(r.faults >= opt, "never better than OPT: " + tag);
                    checkEq(r.compulsoryFaults, static_cast<long long>(distinct), "cold misses = distinct pages: " + tag);
                    check(r.writeBacks <= r.evictions, "write-backs <= evictions: " + tag);
                    if (frames >= distinct) checkEq(r.faults, static_cast<long long>(distinct), "fits in memory: " + tag);
                    // Frames never hold duplicates and the referenced page is always resident.
                    for (const auto& s : r.steps) {
                        std::set<int> seen;
                        for (int p : s.frames) {
                            if (p == EMPTY) continue;
                            check(seen.insert(p).second, "duplicate page in frames: " + tag);
                        }
                        check(seen.count(s.ref.page) == 1, "referenced page resident after step: " + tag);
                    }
                }
            }
        }
    });

    test("Mattson stack distances reproduce LRU for every frame count", [&] {
        for (const auto& t : traces) {
            StackDistanceReport sd = analyzeStackDistances(t, 12);
            for (int f = 1; f <= 12; ++f)
                checkEq(sd.lruFaultsByFrames[f], simulate("LRU", t, f).faults, "LRU f=" + std::to_string(f) + " " + t.name());
        }
    });

    test("LRU and OPT satisfy the inclusion property (stack algorithms)", [&] {
        SimulationConfig cfg;
        for (const auto& t : traces) {
            for (int f = 1; f <= 6; ++f) {
                check(checkInclusion("LRU", t, f, cfg).holds, "LRU inclusion " + t.name());
                check(checkInclusion("OPT", t, f, cfg).holds, "OPT inclusion " + t.name());
            }
        }
    });

    test("stack algorithms never show Belady's anomaly", [&] {
        SimulationConfig cfg;
        for (const auto& t : traces) {
            auto sweep = frameSweep(t, {"LRU", "OPT"}, 1, 10, cfg);
            check(detectBeladyAnomalies(sweep).empty(), "no anomaly for LRU/OPT on " + t.name());
        }
    });

    test("FIFO Belady anomaly is detected on the classic string", [] {
        Trace t = parseTrace("1 2 3 4 1 2 5 1 2 3 4 5");
        SimulationConfig cfg;
        auto anomalies = detectBeladyAnomalies(frameSweep(t, {"FIFO"}, 1, 6, cfg));
        bool found = std::any_of(anomalies.begin(), anomalies.end(), [](const BeladyAnomaly& a) {
            return a.framesFrom == 3 && a.faultsFrom == 9 && a.framesTo == 4 && a.faultsTo == 10;
        });
        check(found, "3 frames = 9 faults, 4 frames = 10 faults");
        check(!checkInclusion("FIFO", t, 3, cfg).holds, "FIFO violates inclusion on this string");
    });

    test("Belady hunter finds a FIFO anomaly", [] {
        BeladyHunt h = huntBeladyAnomaly("FIFO", 20000, 14, 6, 6, 7);
        check(h.found, "hunt found an anomaly");
        if (h.found) {
            SimulationConfig cfg;
            auto s = frameSweep(h.trace, {"FIFO"}, 1, 6, cfg);
            check(!detectBeladyAnomalies(s).empty(), "reported trace really is anomalous");
        }
    });

    test("Belady hunt (default parameters) shrinks to a minimal anomalous string", [] {
        BeladyHunt h = huntBeladyAnomaly("FIFO", 20000, 50, 10, 9, 42);
        check(h.found, "default-parameter hunt succeeds");
        if (!h.found) return;
        SimulationConfig cfg;
        check(h.trace.size() < static_cast<size_t>(h.originalLength), "trace was shrunk");
        check(h.trace.size() <= 20, "shrunk trace is short enough to follow by hand");
        check(!detectBeladyAnomalies(frameSweep(h.trace, {"FIFO"}, 1, 9, cfg)).empty(), "still anomalous");
        // Minimal: deleting any single reference destroys the anomaly.
        std::vector<PageRef> refs = h.trace.refs();
        for (size_t i = 0; i < refs.size(); ++i) {
            std::vector<PageRef> fewer = refs;
            fewer.erase(fewer.begin() + static_cast<std::ptrdiff_t>(i));
            auto s = frameSweep(Trace(fewer, "x"), {"FIFO"}, 1, 9, cfg);
            check(detectBeladyAnomalies(s).empty(), "removing reference " + std::to_string(i) + " loses the anomaly");
        }
        // Pages are numbered 1..k in order of first appearance.
        int next = 1;
        bool tidy = true;
        std::set<int> seen;
        for (const auto& r : refs) {
            if (seen.insert(r.page).second) tidy = tidy && r.page == next++;
        }
        check(tidy, "pages renumbered 1..k");
    });

    test("appendInt matches std::to_string", [] {
        for (long long v : {0LL, 7LL, 10LL, 12345LL, -1LL, -987654321LL, 2147483647LL, -2147483647LL - 1}) {
            std::string s;
            appendInt(s, v);
            checkEq(s, std::to_string(v), "appendInt " + std::to_string(v));
        }
    });

    test("LRU worst case: loop one page larger than memory", [] {
        Trace t = parseTrace("0 1 2 3 4 0 1 2 3 4 0 1 2 3 4");
        checkEq(simulate("LRU", t, 4).faults, 15LL, "LRU faults on every reference");
        checkEq(simulate("FIFO", t, 4).faults, 15LL, "FIFO faults on every reference");
        check(simulate("OPT", t, 4).faults < 15, "OPT does better");
    });

    test("dirty pages and write-backs", [] {
        Trace t = parseTrace("1W 2W 3W 4 5 6");
        SimulationResult r = simulate("FIFO", t, 3, true);
        checkEq(r.writeBacks, 3LL, "three dirty victims written back");
        check(r.steps[3].victimDirty && r.steps[3].victimPage == 1, "page 1 evicted dirty at t=3");
        // A write hit dirties a clean page.
        SimulationResult r2 = simulate("FIFO", parseTrace("1 2 1W 3"), 2, true);
        checkEq(r2.writeBacks, 1LL, "write hit marks page dirty");
    });

    test("Clock gives second chances (hand-traced)", [] {
        SimulationResult r = simulate("CLOCK", parseTrace("1 2 3 4 1 2 5 1 2 3 4 5"), 3, true);
        checkEq(r.steps[3].victimPage, 1, "t=3 evicts 1 after a full sweep");
        checkEq(r.steps[9].victimPage, 1, "t=9 evicts 1");
        check(r.steps[7].hit && r.steps[8].hit && r.steps[11].hit, "hits at t=7,8,11");
    });

    test("ARC adapts and keeps the hot set through a scan", [] {
        Trace t = parseTrace("1 2 1 2 1 2 10 11 12 13 14 1 2 1 2 15 16 17 18 1 2 1 2");
        long long arc = simulate("ARC", t, 3).faults;
        long long lru = simulate("LRU", t, 3).faults;
        check(arc <= lru, "ARC no worse than LRU on scan pollution");
    });

    test("trace parser: separators, writes, comments, addresses", [] {
        ParseOptions opt;
        opt.pageSize = 4096;
        Trace t = parseTrace("7, 0 ;1 w2 3W  # comment 99\n0x1000 0x2FFF R:4 5:w", "p", opt);
        std::vector<int> expect = {7, 0, 1, 2, 3, 1, 2, 4, 5};
        check(t.pages() == expect, "pages parsed: " + t.toString());
        check(t[3].isWrite() && t[4].isWrite() && t[8].isWrite(), "write markers");
        check(!t[0].isWrite() && !t[7].isWrite(), "reads");
        bool threw = false;
        try { parseTrace("1 2 x3"); } catch (const std::invalid_argument&) { threw = true; }
        check(threw, "invalid token rejected");
        opt.addressMode = true;
        check(parseTrace("8191 8192", "a", opt).pages() == std::vector<int>({1, 2}), "decimal address mode");
    });

    test("generator is deterministic and respects bounds", [] {
        for (Pattern p : allPatterns()) {
            GeneratorConfig g;
            g.pattern = p;
            g.length = 200;
            g.pages = 12;
            g.seed = 99;
            Trace a = generateTrace(g), b = generateTrace(g);
            check(a.pages() == b.pages(), "same seed, same trace: " + patternName(p));
            checkEq(a.size(), static_cast<size_t>(200), "length " + patternName(p));
            for (int page : a.pages()) check(page >= 0 && page < 12, "page in range: " + patternName(p));
        }
    });

    test("working set analysis", [] {
        Trace t = parseTrace("1 2 3 1 2 3 4 5 6 4 5 6");
        WorkingSetReport ws = analyzeWorkingSet(t, 3, 2);
        checkEq(ws.peak, 3, "peak working set");
        check(ws.thrashingRisk, "3-page working set with 2 frames thrashes");
        check(!analyzeWorkingSet(t, 3, 4).thrashingRisk, "fits with 4 frames");
    });

    test("factory aliases and list parsing", [] {
        checkEq(canonicalKey("optimal"), std::string("OPT"), "optimal alias");
        checkEq(canonicalKey("second-chance"), std::string("CLOCK"), "second chance alias");
        checkEq(parseAlgorithmList("all").size(), allAlgorithmKeys().size(), "all");
        check(parseAlgorithmList("fifo, lru,fifo") == std::vector<std::string>({"FIFO", "LRU"}), "dedupe");
        bool threw = false;
        try { parseAlgorithmList("fifo,nope"); } catch (const std::invalid_argument&) { threw = true; }
        check(threw, "unknown name rejected");
    });

    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " checks passed";
    std::cout << (g_failures ? " - FAILURES!\n" : " - all good.\n");
    return g_failures ? 1 : 0;
}
