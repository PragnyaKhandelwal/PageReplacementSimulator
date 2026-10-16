#include "io/TraceGenerator.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <stdexcept>

#include "core/Util.h"

namespace prs {

const std::vector<Pattern>& allPatterns() {
    static const std::vector<Pattern> p = {Pattern::Uniform, Pattern::Locality, Pattern::Loop,
                                           Pattern::Sequential, Pattern::Zipf, Pattern::Mixed};
    return p;
}

std::string patternName(Pattern p) {
    switch (p) {
        case Pattern::Uniform: return "uniform";
        case Pattern::Locality: return "locality";
        case Pattern::Loop: return "loop";
        case Pattern::Sequential: return "sequential";
        case Pattern::Zipf: return "zipf";
        case Pattern::Mixed: return "mixed";
    }
    return "?";
}

std::string patternDescription(Pattern p) {
    switch (p) {
        case Pattern::Uniform: return "uniform random pages, no locality";
        case Pattern::Locality: return "shifting working sets (program phases)";
        case Pattern::Loop: return "cyclic sweep over a fixed set of pages";
        case Pattern::Sequential: return "sequential runs from random offsets (streaming)";
        case Pattern::Zipf: return "skewed popularity, hot set + cold tail";
        case Pattern::Mixed: return "hot set polluted by one-off scans";
    }
    return "";
}

bool parsePattern(const std::string& name, Pattern& out) {
    std::string n = toLower(trim(name));
    for (Pattern p : allPatterns()) {
        if (patternName(p) == n) { out = p; return true; }
    }
    if (n == "random") { out = Pattern::Uniform; return true; }
    if (n == "scan") { out = Pattern::Sequential; return true; }
    if (n == "hotspot" || n == "skewed") { out = Pattern::Zipf; return true; }
    if (n == "cyclic") { out = Pattern::Loop; return true; }
    return false;
}

Trace generateTrace(const GeneratorConfig& cfg) {
    if (cfg.length < 1) throw std::invalid_argument("trace length must be positive");
    if (cfg.pages < 1) throw std::invalid_argument("page count must be positive");

    std::mt19937 rng(cfg.seed);
    std::uniform_int_distribution<int> anyPage(0, cfg.pages - 1);
    std::bernoulli_distribution isWrite(std::min(1.0, std::max(0.0, cfg.writeProbability)));
    std::vector<int> pages;
    pages.reserve(cfg.length);

    switch (cfg.pattern) {
        case Pattern::Uniform:
            for (int i = 0; i < cfg.length; ++i) pages.push_back(anyPage(rng));
            break;

        case Pattern::Locality: {
            const int ws = std::max(1, std::min(cfg.workingSetSize, cfg.pages));
            const int phase = std::max(1, cfg.phaseLength);
            std::vector<int> set;
            for (int i = 0; i < cfg.length; ++i) {
                if (i % phase == 0) {
                    // New phase: slide the window, keeping some overlap with the last one.
                    int base = anyPage(rng);
                    set.clear();
                    for (int k = 0; k < ws; ++k) set.push_back((base + k) % cfg.pages);
                }
                // 90% of references stay inside the working set.
                std::uniform_real_distribution<double> u(0.0, 1.0);
                if (u(rng) < 0.9) {
                    std::uniform_int_distribution<int> pick(0, ws - 1);
                    pages.push_back(set[pick(rng)]);
                } else {
                    pages.push_back(anyPage(rng));
                }
            }
            break;
        }

        case Pattern::Loop: {
            const int loop = (cfg.loopSize > 0) ? std::min(cfg.loopSize, cfg.pages) : cfg.pages;
            for (int i = 0; i < cfg.length; ++i) pages.push_back(i % loop);
            break;
        }

        case Pattern::Sequential: {
            // Runs of consecutive pages starting at random offsets (file streaming).
            std::uniform_int_distribution<int> runLength(4, std::max(4, cfg.pages / 2));
            int cursor = 0, remaining = 0;
            for (int i = 0; i < cfg.length; ++i) {
                if (remaining == 0) {
                    cursor = anyPage(rng);
                    remaining = runLength(rng);
                }
                pages.push_back(cursor);
                cursor = (cursor + 1) % cfg.pages;
                --remaining;
            }
            break;
        }

        case Pattern::Zipf: {
            std::vector<double> weights(cfg.pages);
            for (int k = 0; k < cfg.pages; ++k)
                weights[k] = 1.0 / std::pow(static_cast<double>(k + 1), cfg.zipfExponent);
            std::discrete_distribution<int> zipf(weights.begin(), weights.end());
            // Shuffle rank -> page so hot pages are not simply 0,1,2...
            std::vector<int> perm(cfg.pages);
            std::iota(perm.begin(), perm.end(), 0);
            std::shuffle(perm.begin(), perm.end(), rng);
            for (int i = 0; i < cfg.length; ++i) pages.push_back(perm[zipf(rng)]);
            break;
        }

        case Pattern::Mixed: {
            // Hot set in the low half of the address space, scans through the high half.
            const int hot = std::max(1, std::min(cfg.workingSetSize, cfg.pages / 2 > 0 ? cfg.pages / 2 : 1));
            std::uniform_int_distribution<int> hotPick(0, hot - 1);
            std::uniform_real_distribution<double> u(0.0, 1.0);
            int scanCursor = hot;
            int i = 0;
            while (i < cfg.length) {
                if (u(rng) < 0.15 && cfg.pages > hot) {
                    int scanLen = std::min(cfg.length - i, std::max(2, (cfg.pages - hot) / 2));
                    for (int k = 0; k < scanLen; ++k, ++i) {
                        pages.push_back(scanCursor);
                        scanCursor = (scanCursor + 1 < cfg.pages) ? scanCursor + 1 : hot;
                    }
                } else {
                    pages.push_back(hotPick(rng));
                    ++i;
                }
            }
            break;
        }
    }

    std::vector<PageRef> refs;
    refs.reserve(pages.size());
    for (int p : pages)
        refs.push_back(PageRef{p, isWrite(rng) ? AccessType::Write : AccessType::Read});

    return Trace(std::move(refs),
                 cat(patternName(cfg.pattern), "-", cfg.length, "x", cfg.pages, "-s", cfg.seed));
}

}  // namespace prs
