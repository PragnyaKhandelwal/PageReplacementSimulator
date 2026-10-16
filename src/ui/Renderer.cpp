#include "ui/Renderer.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <ostream>

#include "algorithms/AlgorithmFactory.h"
#include "core/Util.h"
#include "ui/Console.h"

namespace prs {
namespace render {

using console::paint;
using console::Tone;

namespace {

std::vector<std::string> wrap(const std::string& text, size_t width) {
    std::vector<std::string> lines;
    for (const auto& paragraph : split(text, "\n")) {
        std::string line;
        for (const auto& word : split(paragraph, " ")) {
            if (!line.empty() && line.size() + 1 + word.size() > width) {
                lines.push_back(line);
                line.clear();
            }
            line += (line.empty() ? "" : " ") + word;
        }
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

std::string pageLabel(const PageRef& r) {
    return std::to_string(r.page) + (r.isWrite() ? "w" : "");
}

}  // namespace

char symbolFor(const std::string& key) {
    static const std::map<std::string, char> symbols = {
        {"FIFO", 'F'}, {"LRU", 'L'}, {"OPT", 'O'},  {"CLOCK", 'C'},  {"ESC", 'E'}, {"NRU", 'N'},
        {"AGING", 'G'}, {"LFU", 'U'}, {"MFU", 'M'}, {"RANDOM", 'R'}, {"ARC", 'A'}};
    auto it = symbols.find(key);
    return it == symbols.end() ? '?' : it->second;
}

// ------------------------------------------------------------- general ----

void banner(std::ostream& os) {
    const char* art[] = {
        R"(  ____                   ____            _                                    _   )",
        R"( |  _ \ __ _  __ _  ___ |  _ \ ___ _ __ | | __ _  ___ ___ _ __ ___   ___ _ __ | |_ )",
        R"( | |_) / _` |/ _` |/ _ \| |_) / _ \ '_ \| |/ _` |/ __/ _ \ '_ ` _ \ / _ \ '_ \| __|)",
        R"( |  __/ (_| | (_| |  __/|  _ <  __/ |_) | | (_| | (_|  __/ | | | | |  __/ | | | |_ )",
        R"( |_|   \__,_|\__, |\___||_| \_\___| .__/|_|\__,_|\___\___|_| |_| |_|\___|_| |_|\__|)",
        R"(             |___/                |_|                    S I M U L A T O R  v2.0   )"};
    os << "\n";
    for (const char* line : art) os << paint(line, Tone::Title) << "\n";
    os << paint("  Virtual memory page replacement lab - 11 policies, analytics & reports", Tone::Muted)
       << "\n  " << paint("JIIT | Operating Systems PBL | B.Tech V Semester", Tone::Muted) << "\n";
}

void traceSummary(std::ostream& os, const Trace& trace) {
    os << "  Trace     : " << paint(trace.name(), Tone::Accent) << "\n";
    os << "  Length    : " << trace.size() << " references, " << trace.distinctPages()
       << " distinct pages";
    if (trace.writeFraction() > 0) os << ", " << percent(trace.writeFraction(), 1) << " writes";
    os << "\n  Sequence  : " << trace.toString(40) << "\n";
}

void config(std::ostream& os, const SimulationConfig& cfg, const std::vector<std::string>& keys) {
    os << "  Frames    : " << cfg.frames << "\n";
    os << "  Policies  : " << join(keys, ", ") << "\n";
    os << "  Timing    : memory " << formatNs(cfg.memoryAccessNs) << ", fault service "
       << formatNs(cfg.faultServiceNs) << ", write-back " << formatNs(cfg.writeBackNs) << "\n";
    os << "  Knobs     : NRU reset every " << cfg.nruResetInterval << " refs, aging "
       << cfg.agingBits << "-bit / tick every " << cfg.agingInterval << " ref(s), random seed "
       << cfg.seed << "\n";
}

// ------------------------------------------------------- step views -------

void stepGrid(std::ostream& os, const SimulationResult& r, int columns) {
    if (r.steps.empty()) {
        os << "  (step recording disabled)\n";
        return;
    }
    size_t cell = 4;
    for (const auto& s : r.steps) cell = std::max(cell, pageLabel(s.ref).size() + 2);
    const size_t label = 9;

    os << paint("  " + r.key + " - " + r.algorithm + " (" + std::to_string(r.frames) + " frames)",
                Tone::Bold) << "\n";
    for (size_t start = 0; start < r.steps.size(); start += columns) {
        size_t end = std::min(r.steps.size(), start + static_cast<size_t>(columns));
        std::string rule = "  " + std::string(label, '-') + "+" + std::string((end - start) * cell, '-');

        os << "  " << padRight("Ref", label) << "|";
        for (size_t i = start; i < end; ++i) os << paint(center(pageLabel(r.steps[i].ref), cell), Tone::Bold);
        os << "\n" << rule << "\n";

        for (int k = 0; k < r.frames; ++k) {
            os << "  " << padRight("Frame " + std::to_string(k), label) << "|";
            for (size_t i = start; i < end; ++i) {
                const StepRecord& s = r.steps[i];
                int page = s.frames[k];
                std::string text = center(page == EMPTY ? "" : std::to_string(page), cell);
                if (s.frameUsed == k) text = paint(text, s.hit ? Tone::Hit : Tone::Fault);
                os << text;
            }
            os << "\n";
        }
        os << rule << "\n  " << padRight("Evicted", label) << "|";
        for (size_t i = start; i < end; ++i) {
            const StepRecord& s = r.steps[i];
            std::string text = s.victimPage == EMPTY ? "" : std::to_string(s.victimPage) + (s.victimDirty ? "!" : "");
            os << paint(center(text, cell), Tone::Victim);
        }
        os << "\n  " << padRight("Result", label) << "|";
        for (size_t i = start; i < end; ++i) {
            const StepRecord& s = r.steps[i];
            os << (s.hit ? paint(center("H", cell), Tone::Hit) : paint(center("F", cell), Tone::Fault));
        }
        os << "\n\n";
    }
    os << paint("  H = hit, F = fault, w = write, ! = dirty victim written back", Tone::Muted) << "\n";
    resultSummary(os, r);
}

void resultSummary(std::ostream& os, const SimulationResult& r) {
    os << "  Faults: " << paint(std::to_string(r.faults), Tone::Fault) << " ("
       << r.compulsoryFaults << " compulsory + " << r.capacityFaults << " capacity)   Hits: "
       << paint(std::to_string(r.hits), Tone::Hit) << "   Fault ratio: " << percent(r.faultRatio())
       << "   Hit ratio: " << percent(r.hitRatio()) << "\n";
    os << "  Write-backs: " << r.writeBacks << "   Page-table lookups: " << r.pageTableLookups
       << "   EAT: " << formatNs(r.effectiveAccessNs) << "\n";
}

void stepLog(std::ostream& os, const SimulationResult& r, size_t limit) {
    size_t n = (limit == 0) ? r.steps.size() : std::min(limit, r.steps.size());
    for (size_t i = 0; i < n; ++i) singleStep(os, r, i);
    if (n < r.steps.size()) os << paint(cat("  ... ", r.steps.size() - n, " more steps"), Tone::Muted) << "\n";
}

void singleStep(std::ostream& os, const SimulationResult& r, size_t index) {
    const StepRecord& s = r.steps[index];
    std::string frames = "[";
    for (size_t k = 0; k < s.frames.size(); ++k) {
        std::string cell = s.frames[k] == EMPTY ? "-" : std::to_string(s.frames[k]);
        cell = padLeft(cell, 3);
        if (static_cast<int>(k) == s.frameUsed) cell = paint(cell, s.hit ? Tone::Hit : Tone::Fault);
        frames += cell;
    }
    frames += " ]";
    std::string status = s.hit ? paint("HIT  ", Tone::Hit)
                               : paint(s.faultKind == FaultKind::Compulsory ? "COLD " : "FAULT", Tone::Fault);
    os << "  t=" << padRight(std::to_string(s.index), 4) << " ref " << padRight(pageLabel(s.ref), 5)
       << status << " " << frames << "  " << paint(s.reason, Tone::Muted) << "\n";
}

void pageTableStats(std::ostream& os, const PageTable& table, size_t limit) {
    std::vector<std::pair<int, PageTableEntry>> rows(table.entries().begin(), table.entries().end());
    std::sort(rows.begin(), rows.end(), [](const std::pair<int, PageTableEntry>& a,
                                           const std::pair<int, PageTableEntry>& b) {
        if (a.second.pageIns != b.second.pageIns) return a.second.pageIns > b.second.pageIns;
        return a.first < b.first;
    });
    os << "  " << padRight("Page", 7) << padLeft("Accesses", 10) << padLeft("Page-ins", 10)
       << padLeft("Evicted", 9) << "   Final state\n";
    for (size_t i = 0; i < rows.size() && i < limit; ++i) {
        const auto& e = rows[i].second;
        os << "  " << padRight(std::to_string(rows[i].first), 7) << padLeft(std::to_string(e.accesses), 10)
           << padLeft(std::to_string(e.pageIns), 10) << padLeft(std::to_string(e.evictions), 9) << "   "
           << (e.valid ? paint("resident in frame " + std::to_string(e.frame), Tone::Hit)
                       : paint("on disk", Tone::Muted))
           << "\n";
    }
    if (rows.size() > limit) os << paint(cat("  ... ", rows.size() - limit, " more pages"), Tone::Muted) << "\n";
}

// ------------------------------------------------------ comparison --------

void comparison(std::ostream& os, const std::vector<SimulationResult>& results, long long optFaults,
                const SimulationConfig& cfg) {
    auto ranked = rankResults(results, optFaults);
    os << "  " << padRight("#", 3) << padRight("Policy", 8) << padLeft("Faults", 7) << padLeft("Hits", 6)
       << padLeft("Hit%", 9) << padLeft("Fault%", 9) << padLeft("Cold", 6) << padLeft("Cap", 6)
       << padLeft("WB", 5) << padLeft("EAT", 12) << padLeft("vs OPT", 9) << padLeft("Optimal", 9) << "\n";
    os << "  " << std::string(89, '-') << "\n";
    bool bestMarked = false;
    for (const auto& rr : ranked) {
        const SimulationResult& r = *rr.result;
        std::string name = padRight(r.key, 8);
        if (r.key == "OPT") name = paint(name, Tone::Muted);
        else if (!bestMarked) { name = paint(name, Tone::Good); bestMarked = true; }
        std::string vs = rr.extraFaultsVsOpt == 0 ? "=" : "+" + std::to_string(rr.extraFaultsVsOpt);
        os << "  " << padRight(std::to_string(rr.rank), 3) << name << padLeft(std::to_string(r.faults), 7)
           << padLeft(std::to_string(r.hits), 6) << padLeft(percent(r.hitRatio()), 9)
           << padLeft(percent(r.faultRatio()), 9) << padLeft(std::to_string(r.compulsoryFaults), 6)
           << padLeft(std::to_string(r.capacityFaults), 6) << padLeft(std::to_string(r.writeBacks), 5)
           << padLeft(formatNs(r.effectiveAccessNs), 12) << padLeft(vs, 9)
           << padLeft(fixed(rr.optimalityPercent, 1) + "%", 9) << "\n";
    }
    os << paint("  Cold = compulsory misses, Cap = capacity misses, WB = dirty write-backs,\n"
                "  EAT = effective access time (ma=" + formatNs(cfg.memoryAccessNs) +
                ", fault=" + formatNs(cfg.faultServiceNs) + "), Optimal = OPT faults / faults",
                Tone::Muted) << "\n";

    // Verdict.
    const RankedResult* best = nullptr;
    for (const auto& rr : ranked) if (rr.result->key != "OPT") { best = &rr; break; }
    if (best) {
        std::vector<std::string> ties;
        for (const auto& rr : ranked)
            if (rr.result->key != "OPT" && rr.result->faults == best->result->faults)
                ties.push_back(rr.result->key);
        size_t practical = 0;
        for (const auto& rr : ranked) if (rr.result->key != "OPT") ++practical;
        const std::string winners = (ties.size() > 3 && ties.size() == practical)
                                        ? cat("all ", practical, " policies tie")
                                        : join(ties, " / ");
        os << "\n  " << paint("Verdict:", Tone::Bold) << " best practical policy "
           << paint(winners, Tone::Good) << " with " << best->result->faults
           << (best->result->faults == 1 ? " fault, " : " faults, ")
           << (best->extraFaultsVsOpt == 0 ? std::string("matching the optimal benchmark")
                                           : cat(best->extraFaultsVsOpt, " above the optimal ", optFaults))
           << ".\n";
    }
}

void faultBars(std::ostream& os, const std::vector<SimulationResult>& results) {
    const int width = 46;
    for (const auto& r : results) {
        int faults = static_cast<int>(std::round(r.faultRatio() * width));
        os << "  " << padRight(r.key, 7) << "|" << paint(std::string(faults, '#'), Tone::Fault)
           << paint(std::string(width - faults, '.'), Tone::Hit) << "| " << padLeft(std::to_string(r.faults), 4)
           << " faults  " << percent(r.faultRatio(), 1) << "\n";
    }
    os << paint("  # = faults, . = hits (bar = 100% of references)", Tone::Muted) << "\n";
}

// ---------------------------------------------------------- analyses ------

void sweep(std::ostream& os, const SweepResult& s) {
    os << "  " << padRight("Frames", 8);
    for (int f : s.frameCounts) os << padLeft(std::to_string(f), 6);
    os << "\n  " << std::string(8 + 6 * s.frameCounts.size(), '-') << "\n";
    long long lo = -1, hi = 0;
    for (size_t a = 0; a < s.algorithms.size(); ++a) {
        os << "  " << padRight(s.algorithms[a], 8);
        for (size_t i = 0; i < s.frameCounts.size(); ++i) {
            long long v = s.faults[a][i];
            lo = (lo < 0) ? v : std::min(lo, v);
            hi = std::max(hi, v);
            std::string cell = padLeft(std::to_string(v), 6);
            if (i > 0 && v > s.faults[a][i - 1]) cell = paint(cell, Tone::Bad);
            os << cell;
        }
        os << "\n";
    }
    os << paint("  red = more faults than with one frame less (Belady's anomaly)", Tone::Muted) << "\n\n";

    // ASCII line chart: at most four series stay legible, so plot OPT, FIFO,
    // LRU and the best of the remaining policies; the table above has them all.
    std::vector<size_t> series;
    auto indexOf = [&](const std::string& key) {
        for (size_t a = 0; a < s.algorithms.size(); ++a) if (s.algorithms[a] == key) return static_cast<int>(a);
        return -1;
    };
    if (s.algorithms.size() <= 4) {
        for (size_t a = 0; a < s.algorithms.size(); ++a) series.push_back(a);
    } else {
        for (const char* key : {"OPT", "FIFO", "LRU"})
            if (indexOf(key) >= 0) series.push_back(static_cast<size_t>(indexOf(key)));
        long long bestSum = -1;
        size_t bestIdx = 0;
        for (size_t a = 0; a < s.algorithms.size(); ++a) {
            if (std::find(series.begin(), series.end(), a) != series.end()) continue;
            long long sum = 0;
            for (long long v : s.faults[a]) sum += v;
            if (bestSum < 0 || sum < bestSum) { bestSum = sum; bestIdx = a; }
        }
        while (series.size() < 4 && bestSum >= 0) {
            series.push_back(bestIdx);
            bestSum = -1;
        }
    }

    const int height = 12, colW = 6;
    const size_t n = s.frameCounts.size();
    std::vector<std::vector<std::string>> cells(height, std::vector<std::string>(n));
    double range = std::max<long long>(1, hi - lo);
    for (size_t a : series) {
        for (size_t i = 0; i < n; ++i) {
            int row = static_cast<int>(std::round((s.faults[a][i] - lo) / range * (height - 1)));
            cells[height - 1 - row][i] += symbolFor(s.algorithms[a]);
        }
    }
    os << paint("  Page faults vs number of frames", Tone::Bold) << "\n";
    for (int r = 0; r < height; ++r) {
        long long label = lo + static_cast<long long>(std::round((height - 1 - r) * range / (height - 1)));
        std::string axis = (r == 0 || r == height - 1 || r == height / 2) ? padLeft(std::to_string(label), 6) : std::string(6, ' ');
        os << "  " << axis << " |";
        for (size_t i = 0; i < n; ++i) os << center(cells[r][i], colW);
        os << "\n";
    }
    os << "  " << std::string(7, ' ') << "+" << std::string(n * colW, '-') << "\n  " << std::string(8, ' ');
    for (int f : s.frameCounts) os << center(std::to_string(f), colW);
    os << "  frames\n  Legend: ";
    for (size_t a : series) os << symbolFor(s.algorithms[a]) << "=" << s.algorithms[a] << "  ";
    os << "(letters side by side = equal faults)\n";
}

void belady(std::ostream& os, const std::vector<BeladyAnomaly>& anomalies) {
    if (anomalies.empty()) {
        os << "  " << paint("No Belady's anomaly", Tone::Good)
           << " in this range: faults never increased when frames were added.\n";
        return;
    }
    for (const auto& a : anomalies) {
        os << "  " << paint("ANOMALY", Tone::Bad) << "  " << padRight(a.algorithm, 7) << a.framesFrom
           << " frames -> " << a.faultsFrom << " faults, but " << a.framesTo << " frames -> "
           << paint(std::to_string(a.faultsTo), Tone::Bad) << " faults\n";
    }
    os << paint("  Stack algorithms (LRU, OPT) satisfy the inclusion property and can never show this.",
                Tone::Muted) << "\n";
}

void beladyHunt(std::ostream& os, const BeladyHunt& hunt) {
    if (!hunt.found) {
        os << "  No anomaly found in " << hunt.attempts << " random traces.\n";
        return;
    }
    os << "  Found after " << hunt.attempts << " random trace(s): " << paint(hunt.anomaly.algorithm, Tone::Bold)
       << " with " << hunt.anomaly.framesFrom << " frames = " << hunt.anomaly.faultsFrom << " faults, "
       << hunt.anomaly.framesTo << " frames = " << paint(std::to_string(hunt.anomaly.faultsTo), Tone::Bad)
       << " faults\n  Shrunk from " << hunt.originalLength << " to " << hunt.trace.size()
       << " references (pages renumbered). Trace: " << hunt.trace.toString() << "\n";
}

void workingSet(std::ostream& os, const WorkingSetReport& r) {
    os << "  Window (delta)      : " << r.window << " references\n";
    os << "  Average |WS|        : " << fixed(r.average, 2) << " pages   peak " << r.peak
       << "   90th percentile " << r.percentile90 << "\n";
    os << "  Frames available    : " << r.frames << "   WS exceeds memory "
       << percent(r.fractionAboveFrames, 1) << " of the time\n";
    os << "  Thrashing risk      : "
       << (r.thrashingRisk ? paint("HIGH - working set does not fit; expect thrashing", Tone::Bad)
                           : paint("low - working set fits in memory", Tone::Good))
       << "\n  Recommended frames  : >= " << r.percentile90 << " (covers 90% of the run)\n\n";

    if (r.sizes.empty()) return;
    const int maxCols = 72;
    int bucket = static_cast<int>((r.sizes.size() + maxCols - 1) / maxCols);
    std::vector<int> cols;
    for (size_t i = 0; i < r.sizes.size(); i += bucket) {
        int m = 0;
        for (size_t j = i; j < std::min(r.sizes.size(), i + bucket); ++j) m = std::max(m, r.sizes[j]);
        cols.push_back(m);
    }
    int top = std::max(r.peak, r.frames);
    double scale = top > 14 ? 14.0 / top : 1.0;
    int rows = static_cast<int>(std::ceil(top * scale));
    os << paint("  |WS(t)| over time  (# fits, X exceeds frames, - frame limit)", Tone::Bold) << "\n";
    for (int row = rows; row >= 1; --row) {
        double value = row / scale;
        int frameRow = static_cast<int>(std::round(r.frames * scale));
        os << "  " << padLeft(fixed(value, 0), 4) << " |";
        for (int c : cols) {
            double h = c * scale;
            if (h >= row - 0.5) os << (c > r.frames ? paint("X", Tone::Fault) : paint("#", Tone::Hit));
            else if (row == frameRow) os << paint("-", Tone::Muted);
            else os << ' ';
        }
        os << "\n";
    }
    os << "       +" << std::string(cols.size(), '-') << "> time (" << bucket << " ref/col)\n";
}

void stackDistance(std::ostream& os, const StackDistanceReport& r, int frames) {
    os << "  Mattson's stack algorithm: one pass gives LRU faults for EVERY memory size.\n";
    os << "  Cold misses: " << r.coldMisses << "   mean distance: " << fixed(r.meanDistance, 2)
       << "   median: " << fixed(r.medianDistance, 1) << "\n\n";
    long long peak = 1;
    for (long long v : r.histogram) peak = std::max(peak, v);
    size_t shown = std::min<size_t>(r.histogram.size(), 21);
    for (size_t d = 1; d < shown; ++d) {
        int bar = static_cast<int>(std::round(36.0 * r.histogram[d] / peak));
        os << "  d=" << padRight(std::to_string(d), 4) << "|"
           << paint(std::string(bar, '#'), static_cast<int>(d) <= frames ? Tone::Hit : Tone::Fault) << " "
           << r.histogram[d] << "\n";
        if (static_cast<int>(d) == frames)
            os << paint("  ------ " + std::to_string(frames) + " frames: distances above are LRU hits, below are faults",
                        Tone::Muted) << "\n";
    }
    if (r.histogram.size() > shown) os << paint("  ... longer distances omitted", Tone::Muted) << "\n";
    os << "\n  LRU faults by frames: ";
    for (size_t f = 1; f < r.lruFaultsByFrames.size(); ++f) os << f << "->" << r.lruFaultsByFrames[f] << "  ";
    os << "\n";
}

void inclusion(std::ostream& os, const InclusionReport& r) {
    os << "  " << padRight(r.algorithm, 7) << "M(" << r.frames << ") subset of M(" << r.frames + 1 << ")? ";
    if (r.holds) {
        os << paint("holds at every step", Tone::Good) << "\n";
        return;
    }
    auto list = [](const std::vector<int>& v) {
        std::vector<std::string> s;
        for (int p : v) s.push_back(std::to_string(p));
        return "{" + join(s, ",") + "}";
    };
    os << paint("VIOLATED", Tone::Bad) << " at t=" << r.violationStep << ": " << list(r.smallerSet)
       << " not in " << list(r.largerSet) << "\n";
}

void profile(std::ostream& os, const TraceProfile& p) {
    os << "  Length / distinct   : " << p.length << " / " << p.distinct << "  (pages " << p.minPage << ".."
       << p.maxPage << ")\n";
    os << "  Reuse ratio         : " << percent(p.reuseRatio, 1) << "   writes " << percent(p.writeFraction, 1)
       << "   sequential " << percent(p.sequentialFraction, 1) << "\n";
    os << "  Hot-set share       : top 20% of pages get " << percent(p.hotSetShare, 1) << " of refs\n";
    os << "  LRU stack distance  : mean " << fixed(p.meanStackDistance, 2) << ", median "
       << fixed(p.medianStackDistance, 1) << "\n";
    os << "  Hottest pages       : ";
    for (const auto& hp : p.hottestPages) os << hp.first << "(" << hp.second << ") ";
    os << "\n  Locality class      : " << paint(p.localityClass, Tone::Accent) << "\n\n";
    for (const auto& insight : p.insights) {
        auto lines = wrap(insight, 72);
        for (size_t i = 0; i < lines.size(); ++i) os << (i ? "    " : "  * ") << lines[i] << "\n";
    }
}

void heatmap(std::ostream& os, const Heatmap& h) {
    if (h.pages.empty()) return;
    const std::string ramp = " .:-=+*#%@";
    os << paint("  Locality heatmap: rows = pages, columns = time windows of " + std::to_string(h.windowSize) +
                    " reference(s)", Tone::Bold) << "\n";
    for (size_t r = h.pages.size(); r-- > 0;) {
        os << "  " << padLeft(std::to_string(h.pages[r]), 5) << " |";
        for (int c : h.counts[r]) {
            size_t idx = c == 0 ? 0 : 1 + static_cast<size_t>((ramp.size() - 2) * (c - 1) / std::max(1, h.maxCount - 1));
            idx = std::min(idx, ramp.size() - 1);
            os << (c ? paint(std::string(1, ramp[idx]), Tone::Accent) : std::string(" "));
        }
        os << "|\n";
    }
    os << "        +" << std::string(h.windowStart.size(), '-') << "+> time\n";
    os << paint("  density: '" + ramp.substr(1) + "' (low -> high)", Tone::Muted) << "\n";
}

void validation(std::ostream& os, const std::vector<ValidationOutcome>& outcomes) {
    int passed = 0;
    os << "  " << padRight("", 7) << padRight("Case", 31) << padRight("Policy", 8) << padLeft("Frames", 6)
       << padLeft("Expect", 8) << padLeft("Got", 6) << "   Source\n";
    os << "  " << std::string(88, '-') << "\n";
    for (const auto& o : outcomes) {
        if (o.passed) ++passed;
        std::string expect = std::to_string(o.testCase.expectedFaults);
        std::string got = std::to_string(o.actualFaults);
        if (o.testCase.expectedWriteBacks >= 0) {
            expect += "/" + std::to_string(o.testCase.expectedWriteBacks);
            got += "/" + std::to_string(o.actualWriteBacks);
        }
        os << "  " << (o.passed ? paint("[PASS]", Tone::Good) : paint("[FAIL]", Tone::Bad)) << " "
           << padRight(o.testCase.name, 31) << padRight(o.testCase.algorithm, 8)
           << padLeft(std::to_string(o.testCase.frames), 6) << padLeft(expect, 8) << padLeft(got, 6) << "   "
           << paint(o.testCase.source, Tone::Muted) << "\n";
    }
    os << "\n  " << (passed == static_cast<int>(outcomes.size()) ? paint("All", Tone::Good) : paint("Only", Tone::Bad))
       << " " << passed << "/" << outcomes.size()
       << " hand-calculated cases match the simulator (x/y = faults/write-backs).\n";
}

void encyclopedia(std::ostream& os) {
    for (const auto& key : allAlgorithmKeys()) {
        auto algo = createAlgorithm(key);
        os << "\n  " << paint("[" + key + "] " + algo->name(), Tone::Title) << "\n";
        os << "  " << paint("Family: " + algo->family() + "   Stack algorithm: " +
                                (algo->isStackAlgorithm() ? "yes (immune to Belady's anomaly)" : "no"),
                            Tone::Muted) << "\n";
        for (const auto& line : wrap(algo->description(), 74)) os << "    " << line << "\n";
    }
}

}  // namespace render
}  // namespace prs
