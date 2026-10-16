#include "ui/App.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>

#include "algorithms/AlgorithmFactory.h"
#include "analysis/Analyzer.h"
#include "analysis/Validation.h"
#include "core/Simulator.h"
#include "core/Util.h"
#include "io/ReportWriter.h"
#include "io/TraceGenerator.h"
#include "ui/Console.h"
#include "ui/Renderer.h"

#ifdef _WIN32
#include <direct.h>
#endif

namespace prs {

using console::heading;
using console::paint;
using console::Tone;

namespace {

const char* DEFAULT_TRACE = "7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1";

// Belady hunt defaults: short random strings almost never show the anomaly
// (about 1 in 10,000); 50 references over 10 pages succeed roughly 1 time in 100.
constexpr int HUNT_LENGTH = 50, HUNT_PAGES = 10, HUNT_FRAMES = 9;
constexpr int MAX_FRAMES = 4096;

// Whole-string numeric parsing: "3abc" is an error, not 3.
int parseInt(const std::string& s) {
    try {
        size_t used = 0;
        int v = std::stoi(s, &used);
        if (used == s.size()) return v;
    } catch (...) {
    }
    throw std::invalid_argument("not a whole number: '" + s + "'");
}

double parseDouble(const std::string& s) {
    try {
        size_t used = 0;
        double v = std::stod(s, &used);
        if (used == s.size()) return v;
    } catch (...) {
    }
    throw std::invalid_argument("not a number: '" + s + "'");
}

bool ensureDir(const std::string& dir) {
    struct stat st;
    if (stat(dir.c_str(), &st) == 0) return (st.st_mode & S_IFDIR) != 0;
#ifdef _WIN32
    return _mkdir(dir.c_str()) == 0;
#else
    return mkdir(dir.c_str(), 0755) == 0;
#endif
}

std::string safeFileName(const std::string& name) {
    std::string out;
    for (char c : name) out += (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') ? c : '_';
    return out.empty() ? "trace" : out;
}

// "Scan pollution (database-like)" -> "scan-pollution-database-like"
std::string slug(const std::string& title) {
    std::string out;
    for (char c : toLower(title)) {
        if (c == '\'') continue;  // "Belady's" -> "beladys"
        if (std::isalnum(static_cast<unsigned char>(c))) out += c;
        else if (!out.empty() && out.back() != '-') out += '-';
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out;
}

struct Scenario {
    std::string title;
    std::string trace;
    int frames;
    std::string lesson;
};

const std::vector<Scenario>& scenarios() {
    static const std::vector<Scenario> s = {
        {"Silberschatz textbook string", DEFAULT_TRACE, 3,
         "The reference example: FIFO 15, LRU 12, OPT 9 faults."},
        {"Belady's anomaly", "1 2 3 4 1 2 5 1 2 3 4 5", 3,
         "FIFO: 9 faults with 3 frames but 10 with 4. Try the sweep / Belady lab."},
        {"LRU worst case (loop > memory)", "0 1 2 3 4 0 1 2 3 4 0 1 2 3 4 0 1 2 3 4", 4,
         "A loop one page larger than memory: LRU and FIFO miss on EVERY reference."},
        {"Strong locality (program phases)", "1 2 1 3 1 2 1 3 1 4 1 5 1 4 1 5 1 6 1 7 1 6 1 7 1 8 1 9", 3,
         "Page 1 stays hot while side pages shift: LRU matches OPT (9), FIFO keeps evicting the hot page (11)."},
        {"Scan pollution (database-like)", "1 2 1 2 1 2 10 11 12 13 14 1 2 1 2 15 16 17 18 1 2 1 2", 3,
         "One-off scans flush the hot pages out of LRU; ARC protects them."},
        {"Write-heavy workload", "1W 2 3W 4 1 2W 5 1 2 3W 4 5W 1W 3 2 4W 5 1", 3,
         "Dirty pages cost a disk write when evicted: compare write-backs of FIFO vs ESC/NRU."},
        {"Frequency matters (hot page)", "5 1 5 2 5 3 5 4 5 1 5 2 5 3 5 4 5 6 5 7", 3,
         "Page 5 is hot; LFU and ARC keep it, compare with FIFO."},
    };
    return s;
}

}  // namespace

App::App() : keys_(classicAlgorithmKeys()) {
    trace_ = parseTrace(DEFAULT_TRACE, "silberschatz");
}

int App::run(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    bool color = true;
    for (const auto& a : args)
        if (a == "--no-color") color = false;
    console::init(color);

    try {
        const bool onlyMenuFlags = std::all_of(args.begin(), args.end(), [](const std::string& a) {
            return a == "--menu" || a == "--no-color";
        });
        if (onlyMenuFlags) {
            interactive();
            return 0;
        }
        return runCli(args);
    } catch (const std::exception& e) {
        std::cerr << paint("error: ", Tone::Bad) << e.what() << "\n";
        return 1;
    }
}

int App::workingSetWindow() const {
    if (wsWindow_ > 0) return wsWindow_;
    return std::max(2, std::min(10, static_cast<int>(trace_.size()) / 3));
}

std::string App::exportAll(const std::string& dir, int sweepMax) {
    if (!ensureDir(dir)) throw std::runtime_error("cannot create directory '" + dir + "'");
    ReportBundle bundle = buildReportBundle(trace_, cfg_, keys_, sweepMax, workingSetWindow());
    const std::string base = dir + "/" + safeFileName(trace_.name()) + "_f" + std::to_string(cfg_.frames);
    writeHtmlReport(bundle, base + "_report.html");
    writeMarkdownReport(bundle, base + "_report.md");
    writeSummaryCsv(bundle.results, bundle.optFaults, base + "_summary.csv");
    writeStepsCsv(bundle.results, base + "_steps.csv");
    writeSweepCsv(bundle.sweep, base + "_sweep.csv");
    return base;
}

// =================================================================== CLI ==

void App::printUsage() {
    std::cout << R"(
Usage: pagesim [options]          (no options = interactive menu)

Input
  -t, --trace "7 0 1 2W ..."    reference string (W suffix = write, 0x.. = address)
  -f, --file PATH               read trace from a file
  -g, --generate PATTERN        uniform | locality | loop | sequential | zipf | mixed
      --length N  --pages N     size of the generated trace / address space
      --write-prob P --seed S   write probability (0..1) and RNG seed
      --page-size BYTES         page size for address translation (default 4096)
      --addr                    treat every number in the trace as a byte address

Simulation
  -n, --frames N                physical frames (default 3)
  -a, --algo LIST               comma list, 'classic' (FIFO,LRU,OPT) or 'all'
  -s, --steps                   print the frame table for every policy
  -v, --verbose                 print every decision with its reason
      --ma NS  --fault-ns NS    memory access / page-fault service time (EAT)

Analysis
      --sweep MAX | MIN-MAX     page faults for a range of frame counts
      --belady                  Belady's anomaly detection + random hunt
      --ws [DELTA]              working-set / thrashing analysis
      --profile                 trace profile, insights and locality heatmap
      --stack                   LRU stack-distance histogram (Mattson)
      --inclusion               inclusion-property (stack algorithm) check

Output
      --report [DIR]            write HTML + Markdown + CSV reports (default: reports)
      --html PATH               write only the HTML report
      --test                    run the validation suite (hand-calculated cases)
      --list                    describe every algorithm
      --no-color                disable ANSI colours
  -h, --help                    this help

Examples
  pagesim -t "1 2 3 4 1 2 5 1 2 3 4 5" -n 3 -a all --steps
  pagesim -g locality --length 200 --pages 16 -n 4 -a all --sweep 1-10 --report
  pagesim -f traces/scan_pollution.txt -n 3 -a lru,arc,opt --profile --ws 6
)";
}

int App::runCli(const std::vector<std::string>& args) {
    bool steps = false, verbose = false, belady = false, profile = false, stack = false, inclusion = false;
    bool wantWs = false, wantReport = false, haveTrace = false;
    std::string reportDir = "reports", htmlPath;
    int sweepMin = 0, sweepMax = 0;
    GeneratorConfig gen;
    bool generate = false;
    std::string traceText, traceFile;

    auto need = [&](size_t& i) -> std::string {
        if (i + 1 >= args.size()) throw std::invalid_argument("option " + args[i] + " needs a value");
        return args[++i];
    };
    auto toInt = parseInt;
    auto toDouble = parseDouble;
    auto optionalValue = [&](size_t& i) -> std::string {
        if (i + 1 < args.size() && !startsWith(args[i + 1], "-")) return args[++i];
        return "";
    };

    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "-h" || a == "--help") { printUsage(); return 0; }
        else if (a == "--no-color" || a == "--menu") {}
        else if (a == "--list") { render::encyclopedia(std::cout); return 0; }
        else if (a == "--test") {
            std::cout << heading("Validation suite");
            auto out = runValidation();
            render::validation(std::cout, out);
            return std::all_of(out.begin(), out.end(), [](const ValidationOutcome& o) { return o.passed; }) ? 0 : 2;
        }
        else if (a == "-t" || a == "--trace") { traceText = need(i); haveTrace = true; }
        else if (a == "-f" || a == "--file") { traceFile = need(i); haveTrace = true; }
        else if (a == "-g" || a == "--generate") {
            if (!parsePattern(need(i), gen.pattern)) throw std::invalid_argument("unknown pattern " + args[i]);
            generate = haveTrace = true;
        }
        else if (a == "--length") gen.length = toInt(need(i));
        else if (a == "--pages") gen.pages = toInt(need(i));
        else if (a == "--write-prob") gen.writeProbability = toDouble(need(i));
        else if (a == "--seed") { gen.seed = static_cast<unsigned>(toInt(need(i))); cfg_.seed = gen.seed; }
        else if (a == "--page-size") parse_.pageSize = toInt(need(i));
        else if (a == "--addr") parse_.addressMode = true;
        else if (a == "-n" || a == "--frames") cfg_.frames = toInt(need(i));
        else if (a == "-a" || a == "--algo") keys_ = parseAlgorithmList(need(i));
        else if (a == "-s" || a == "--steps") steps = true;
        else if (a == "-v" || a == "--verbose") verbose = true;
        else if (a == "--ma") cfg_.memoryAccessNs = toDouble(need(i));
        else if (a == "--fault-ns") cfg_.faultServiceNs = cfg_.writeBackNs = toDouble(need(i));
        else if (a == "--sweep") {
            std::string v = need(i);
            auto parts = split(v, "-:.");
            if (parts.size() == 1) { sweepMin = 1; sweepMax = toInt(parts[0]); }
            else if (parts.size() == 2) { sweepMin = toInt(parts[0]); sweepMax = toInt(parts[1]); }
            else throw std::invalid_argument("bad sweep range " + v);
        }
        else if (a == "--belady") belady = true;
        else if (a == "--ws") { wantWs = true; std::string v = optionalValue(i); if (!v.empty()) wsWindow_ = toInt(v); }
        else if (a == "--profile") profile = true;
        else if (a == "--stack") stack = true;
        else if (a == "--inclusion") inclusion = true;
        else if (a == "--report") { wantReport = true; std::string v = optionalValue(i); if (!v.empty()) reportDir = v; }
        else if (a == "--html") htmlPath = need(i);
        else throw std::invalid_argument("unknown option '" + a + "' (see --help)");
    }

    if (generate) trace_ = generateTrace(gen);
    else if (!traceFile.empty()) trace_ = loadTraceFile(traceFile, parse_);
    else if (haveTrace) trace_ = parseTrace(traceText, "cli", parse_);
    if (!haveTrace) std::cout << paint("(no trace given - using the Silberschatz textbook string)", Tone::Muted) << "\n";
    if (trace_.empty()) throw std::invalid_argument("trace is empty");
    if (cfg_.frames < 1 || cfg_.frames > MAX_FRAMES)
        throw std::invalid_argument("frames must be between 1 and " + std::to_string(MAX_FRAMES));
    if (sweepMax > MAX_FRAMES || (sweepMax > 0 && sweepMin < 1) || sweepMin > sweepMax)
        throw std::invalid_argument("sweep range must lie within 1.." + std::to_string(MAX_FRAMES));

    std::cout << heading("Input");
    render::traceSummary(std::cout, trace_);
    render::config(std::cout, cfg_, keys_);

    auto results = Simulator::runAll(keys_, trace_, cfg_);
    long long opt = optimalFaults(trace_, cfg_);

    if (steps || verbose) {
        for (const auto& r : results) {
            std::cout << heading("Simulation: " + r.key);
            if (steps) render::stepGrid(std::cout, r, 20);
            if (verbose) render::stepLog(std::cout, r);
        }
    }
    std::cout << heading("Comparison (" + std::to_string(cfg_.frames) + " frames)");
    render::comparison(std::cout, results, opt, cfg_);
    std::cout << "\n";
    render::faultBars(std::cout, results);

    if (sweepMax > 0 || belady) {
        int lo = sweepMax > 0 ? sweepMin : 1;
        int hi = sweepMax > 0 ? sweepMax : std::max(cfg_.frames + 3, 7);
        auto sw = frameSweep(trace_, keys_, lo, hi, cfg_);
        std::cout << heading("Frame sweep " + std::to_string(lo) + ".." + std::to_string(hi));
        render::sweep(std::cout, sw);
        std::cout << heading("Belady's anomaly check");
        render::belady(std::cout, detectBeladyAnomalies(sw));
    }
    if (belady) {
        std::cout << heading("Belady hunt (random search, FIFO)");
        render::beladyHunt(std::cout, huntBeladyAnomaly("FIFO", 20000, HUNT_LENGTH, HUNT_PAGES, HUNT_FRAMES, cfg_.seed));
    }
    if (wantWs) {
        std::cout << heading("Working set & thrashing");
        render::workingSet(std::cout, analyzeWorkingSet(trace_, workingSetWindow(), cfg_.frames));
    }
    if (profile) {
        std::cout << heading("Trace profile");
        render::profile(std::cout, profileTrace(trace_, cfg_.frames));
        std::cout << "\n";
        render::heatmap(std::cout, buildHeatmap(trace_, 70, 24));
    }
    if (stack) {
        std::cout << heading("LRU stack distances");
        render::stackDistance(std::cout, analyzeStackDistances(trace_, std::max(cfg_.frames + 4, 8)), cfg_.frames);
    }
    if (inclusion) {
        std::cout << heading("Inclusion property M(n) subset of M(n+1)");
        for (const auto& k : keys_) render::inclusion(std::cout, checkInclusion(k, trace_, cfg_.frames, cfg_));
    }
    if (wantReport) {
        std::string base = exportAll(reportDir, sweepMax);
        std::cout << heading("Reports");
        std::cout << "  " << paint(base + "_report.html", Tone::Accent) << "  (open in a browser)\n  " << base
                  << "_report.md\n  " << base << "_summary.csv, _steps.csv, _sweep.csv\n";
    }
    if (!htmlPath.empty()) {
        writeHtmlReport(buildReportBundle(trace_, cfg_, keys_, sweepMax, workingSetWindow()), htmlPath);
        std::cout << "\n  HTML report written to " << paint(htmlPath, Tone::Accent) << "\n";
    }
    return 0;
}

// =========================================================== interactive ==

bool App::readLine(const std::string& label, std::string& out) {
    if (eof_) return false;
    std::cout << label;
    std::cout.flush();
    if (!std::getline(std::cin, out)) {
        eof_ = true;
        std::cout << "\n";
        return false;
    }
    out = trim(out);
    return true;
}

std::string App::ask(const std::string& label, const std::string& def) {
    std::string line;
    std::string full = "  " + label + (def.empty() ? "" : " [" + def + "]") + ": ";
    if (!readLine(full, line) || line.empty()) return def;
    return line;
}

int App::askInt(const std::string& label, int def, int lo, int hi) {
    while (!eof_) {
        std::string s = ask(label + " (" + std::to_string(lo) + "-" + std::to_string(hi) + ")", std::to_string(def));
        try {
            size_t used = 0;
            int v = std::stoi(s, &used);
            if (used == s.size() && v >= lo && v <= hi) return v;
        } catch (...) {
        }
        std::cout << paint("  Please enter a whole number between " + std::to_string(lo) + " and " +
                               std::to_string(hi) + ".", Tone::Warn) << "\n";
    }
    return def;
}

double App::askDouble(const std::string& label, double def, double lo, double hi) {
    while (!eof_) {
        std::string s = ask(label, fixed(def, def < 10 ? 2 : 0));
        try {
            double v = std::stod(s);
            if (v >= lo && v <= hi) return v;
        } catch (...) {
        }
        std::cout << paint("  Please enter a number between " + fixed(lo, 2) + " and " + fixed(hi, 2) + ".", Tone::Warn) << "\n";
    }
    return def;
}

void App::pause() {
    std::string dummy;
    readLine(paint("\n  Press Enter to return to the menu...", Tone::Muted), dummy);
}

void App::showMenu() {
    std::cout << heading("Main menu");
    std::cout << "  Trace: " << paint(trace_.name(), Tone::Accent) << " (" << trace_.size() << " refs, "
              << trace_.distinctPages() << " pages)   Frames: " << paint(std::to_string(cfg_.frames), Tone::Accent)
              << "   Policies: " << paint(join(keys_, ","), Tone::Accent) << "\n\n";
    const char* items[][2] = {
        {"1", "Enter a reference string"},      {"2", "Load trace from file"},
        {"3", "Generate a synthetic workload"}, {"4", "Classic scenarios (demo library)"},
        {"5", "Configure frames & timing"},     {"6", "Choose algorithms"},
        {"7", "Run & compare policies"},        {"8", "Step-by-step walkthrough"},
        {"9", "Faults vs frames sweep"},        {"10", "Belady's anomaly lab"},
        {"11", "Working set & thrashing"},      {"12", "Trace profile & heatmap"},
        {"13", "Stack-algorithm check"},        {"14", "Export reports (HTML/MD/CSV)"},
        {"15", "Validation suite"},             {"16", "Algorithm encyclopedia"},
        {"17", "Save current trace"},           {"0", "Exit"}};
    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i += 2) {
        std::cout << "  " << paint(padLeft(items[i][0], 2), Tone::Title) << "  " << padRight(items[i][1], 36);
        if (i + 1 < sizeof(items) / sizeof(items[0]))
            std::cout << paint(padLeft(items[i + 1][0], 2), Tone::Title) << "  " << items[i + 1][1];
        std::cout << "\n";
    }
}

void App::interactive() {
    render::banner(std::cout);
    while (!eof_) {
        showMenu();
        std::string choice;
        if (!readLine("\n  Select> ", choice)) break;
        if (choice == "0" || toLower(choice) == "q" || toLower(choice) == "exit") break;
        try {
            int c = -1;
            try { c = std::stoi(choice); } catch (...) {}
            switch (c) {
                case 1: actionEnterTrace(); break;
                case 2: actionLoadFile(); break;
                case 3: actionGenerate(); break;
                case 4: actionScenarios(); break;
                case 5: actionConfigure(); break;
                case 6: actionSelectAlgorithms(); break;
                case 7: actionCompare(); break;
                case 8: actionWalkthrough(); break;
                case 9: actionSweep(); break;
                case 10: actionBelady(); break;
                case 11: actionWorkingSet(); break;
                case 12: actionProfile(); break;
                case 13: actionInclusion(); break;
                case 14: actionExport(); break;
                case 15: actionValidation(); break;
                case 16: actionEncyclopedia(); break;
                case 17: actionSaveTrace(); break;
                default: std::cout << paint("  Unknown option.", Tone::Warn) << "\n"; continue;
            }
        } catch (const std::exception& e) {
            std::cout << paint("  Error: ", Tone::Bad) << e.what() << "\n";
        }
        if (!eof_) pause();
    }
    std::cout << paint("\n  Goodbye!\n", Tone::Muted);
}

void App::actionEnterTrace() {
    std::cout << heading("Enter reference string");
    std::cout << "  Separate pages with spaces or commas. Add W for a write (e.g. 3W), hex = address.\n";
    std::string text = ask("Pages");
    if (text.empty()) return;
    Trace t = parseTrace(text, "manual", parse_);
    if (t.empty()) throw std::invalid_argument("no pages entered");
    trace_ = t;
    render::traceSummary(std::cout, trace_);
}

void App::actionLoadFile() {
    std::cout << heading("Load trace file");
    std::cout << "  Sample traces live in the traces/ folder.\n";
    std::string path = ask("Path", "traces/silberschatz.txt");
    trace_ = loadTraceFile(path, parse_);
    if (trace_.empty()) throw std::invalid_argument("file contains no references");
    render::traceSummary(std::cout, trace_);
}

void App::actionGenerate() {
    std::cout << heading("Generate synthetic workload");
    const auto& patterns = allPatterns();
    for (size_t i = 0; i < patterns.size(); ++i)
        std::cout << "  " << i + 1 << ". " << padRight(patternName(patterns[i]), 12)
                  << paint(patternDescription(patterns[i]), Tone::Muted) << "\n";
    GeneratorConfig g;
    g.pattern = patterns[askInt("Pattern", 2, 1, static_cast<int>(patterns.size())) - 1];
    g.length = askInt("Length", 40, 1, 1000000);
    g.pages = askInt("Distinct pages in address space", 10, 1, 100000);
    if (g.pattern == Pattern::Locality || g.pattern == Pattern::Mixed)
        g.workingSetSize = askInt("Working-set / hot-set size", 4, 1, g.pages);
    if (g.pattern == Pattern::Zipf) g.zipfExponent = askDouble("Zipf exponent", 1.0, 0.0, 5.0);
    g.writeProbability = askDouble("Write probability", 0.0, 0.0, 1.0);
    g.seed = static_cast<unsigned>(askInt("Seed", 42, 0, 2147483647));
    trace_ = generateTrace(g);
    render::traceSummary(std::cout, trace_);
}

void App::actionScenarios() {
    std::cout << heading("Classic scenarios");
    const auto& list = scenarios();
    for (size_t i = 0; i < list.size(); ++i)
        std::cout << "  " << i + 1 << ". " << padRight(list[i].title, 36)
                  << paint(list[i].lesson, Tone::Muted) << "\n";
    int pick = askInt("Scenario", 1, 1, static_cast<int>(list.size()));
    const Scenario& s = list[pick - 1];
    trace_ = parseTrace(s.trace, slug(s.title));
    cfg_.frames = s.frames;
    std::cout << "\n";
    render::traceSummary(std::cout, trace_);
    std::cout << "  Frames set to " << s.frames << ". " << paint(s.lesson, Tone::Accent) << "\n";
}

void App::actionConfigure() {
    std::cout << heading("Configuration");
    cfg_.frames = askInt("Physical frames", cfg_.frames, 1, 4096);
    cfg_.memoryAccessNs = askDouble("Memory access time (ns)", cfg_.memoryAccessNs, 0.0, 1e9);
    cfg_.faultServiceNs = askDouble("Page-fault service time (ns)", cfg_.faultServiceNs, 0.0, 1e12);
    cfg_.writeBackNs = askDouble("Dirty write-back time (ns)", cfg_.writeBackNs, 0.0, 1e12);
    cfg_.nruResetInterval = askInt("NRU: clear R bits every N references", cfg_.nruResetInterval, 1, 1000000);
    cfg_.agingBits = askInt("Aging: counter bits", cfg_.agingBits, 1, 32);
    cfg_.agingInterval = askInt("Aging: references per tick", cfg_.agingInterval, 1, 1000000);
    cfg_.seed = static_cast<unsigned>(askInt("Random policy seed", static_cast<int>(cfg_.seed), 0, 2147483647));
    wsWindow_ = askInt("Working-set window delta (0 = auto)", wsWindow_, 0, 1000000);
    parse_.pageSize = askInt("Page size in bytes (address traces)", static_cast<int>(parse_.pageSize), 1, 1 << 30);
    std::cout << "\n";
    render::config(std::cout, cfg_, keys_);
}

void App::actionSelectAlgorithms() {
    std::cout << heading("Choose algorithms");
    for (const auto& k : allAlgorithmKeys()) {
        auto a = createAlgorithm(k);
        std::cout << "  " << padRight(k, 8) << padRight(a->name(), 32) << paint(a->family(), Tone::Muted) << "\n";
    }
    std::cout << "\n  Enter a comma separated list, 'classic' (FIFO,LRU,OPT) or 'all'.\n";
    keys_ = parseAlgorithmList(ask("Algorithms", join(keys_, ",")));
    std::cout << "  Selected: " << paint(join(keys_, ", "), Tone::Accent) << "\n";
}

void App::actionCompare() {
    std::cout << heading("Run & compare");
    render::traceSummary(std::cout, trace_);
    auto results = Simulator::runAll(keys_, trace_, cfg_);
    std::string show = toLower(ask("Show frame tables for each policy? (y/n)", trace_.size() <= 40 ? "y" : "n"));
    if (show == "y" || show == "yes") {
        for (const auto& r : results) {
            std::cout << "\n";
            render::stepGrid(std::cout, r, 20);
        }
    }
    std::cout << heading("Comparison (" + std::to_string(cfg_.frames) + " frames)");
    render::comparison(std::cout, results, optimalFaults(trace_, cfg_), cfg_);
    std::cout << "\n";
    render::faultBars(std::cout, results);
}

void App::actionWalkthrough() {
    std::cout << heading("Step-by-step walkthrough");
    std::string key = canonicalKey(ask("Algorithm", keys_.front()));
    auto algo = createAlgorithm(key);
    if (!algo) throw std::invalid_argument("unknown algorithm");
    PageTable table;
    SimulationResult r = Simulator::run(*algo, trace_, cfg_, table);
    std::cout << "  " << paint(algo->name(), Tone::Bold) << " - " << algo->family() << "\n";
    std::cout << paint("  Enter = next step, a = show all remaining, q = stop\n\n", Tone::Muted);
    bool all = false;
    for (size_t i = 0; i < r.steps.size(); ++i) {
        render::singleStep(std::cout, r, i);
        if (!all && i + 1 < r.steps.size()) {
            std::string cmd;
            if (!readLine("", cmd)) return;
            cmd = toLower(cmd);
            if (cmd == "q") break;
            if (cmd == "a") all = true;
        }
    }
    std::cout << "\n";
    render::stepGrid(std::cout, r, 20);
    std::cout << "\n" << paint("  Page table after the run (most faulted pages first)", Tone::Bold) << "\n";
    render::pageTableStats(std::cout, table, 12);
}

void App::actionSweep() {
    std::cout << heading("Faults vs frames");
    int lo = askInt("From frames", 1, 1, 4096);
    int hi = askInt("To frames", std::max(lo + 1, std::min(12, trace_.distinctPages() + 1)), lo, 4096);
    auto sw = frameSweep(trace_, keys_, lo, hi, cfg_);
    render::sweep(std::cout, sw);
    std::cout << "\n";
    render::belady(std::cout, detectBeladyAnomalies(sw));
}

void App::actionBelady() {
    std::cout << heading("Belady's anomaly lab");
    std::cout << "  1. Check the current trace for anomalies (all policies, frames 1..N)\n"
                 "  2. Hunt: search random traces until a policy shows the anomaly\n"
                 "  3. Load the textbook anomaly string (1 2 3 4 1 2 5 1 2 3 4 5)\n";
    int mode = askInt("Mode", 1, 1, 3);
    if (mode == 3) {
        trace_ = parseTrace("1 2 3 4 1 2 5 1 2 3 4 5", "belady-classic");
        mode = 1;
    }
    if (mode == 1) {
        int hi = askInt("Max frames", std::max(5, std::min(10, trace_.distinctPages())), 2, 512);
        auto sw = frameSweep(trace_, allAlgorithmKeys(), 1, hi, cfg_);
        render::sweep(std::cout, sw);
        std::cout << "\n";
        render::belady(std::cout, detectBeladyAnomalies(sw));
    } else {
        std::string key = canonicalKey(ask("Policy to hunt (FIFO, CLOCK, LFU, MFU, RANDOM, ...)", "FIFO"));
        if (key.empty()) throw std::invalid_argument("unknown algorithm");
        int len = askInt("Search length (shrunk afterwards)", HUNT_LENGTH, 5, 500);
        int pages = askInt("Distinct pages", HUNT_PAGES, 2, 100);
        std::cout << "  Searching...\n";
        BeladyHunt h = huntBeladyAnomaly(key, 20000, len, pages, std::min(pages, HUNT_FRAMES), cfg_.seed);
        render::beladyHunt(std::cout, h);
        if (h.found) {
            std::string use = toLower(ask("Make this the current trace? (y/n)", "y"));
            if (use == "y" || use == "yes") {
                trace_ = h.trace;
                cfg_.frames = h.anomaly.framesFrom;
            }
        }
    }
}

void App::actionWorkingSet() {
    std::cout << heading("Working set & thrashing");
    int window = askInt("Window delta (references)", workingSetWindow(), 1, std::max(1, static_cast<int>(trace_.size())));
    render::workingSet(std::cout, analyzeWorkingSet(trace_, window, cfg_.frames));
}

void App::actionProfile() {
    std::cout << heading("Trace profile");
    render::traceSummary(std::cout, trace_);
    std::cout << "\n";
    render::profile(std::cout, profileTrace(trace_, cfg_.frames));
    std::cout << "\n";
    render::heatmap(std::cout, buildHeatmap(trace_, 70, 24));
    std::cout << heading("LRU stack distances");
    render::stackDistance(std::cout, analyzeStackDistances(trace_, std::max(cfg_.frames + 4, 8)), cfg_.frames);
}

void App::actionInclusion() {
    std::cout << heading("Stack-algorithm check");
    std::cout << "  A policy is a stack algorithm if the pages held with n frames are always a subset of\n"
                 "  those held with n+1 frames. Such policies can never exhibit Belady's anomaly.\n\n";
    for (const auto& k : allAlgorithmKeys()) render::inclusion(std::cout, checkInclusion(k, trace_, cfg_.frames, cfg_));
    std::cout << paint("\n  Note: a single trace can only disprove the property; LRU and OPT hold it for all traces.",
                       Tone::Muted) << "\n";
}

void App::actionExport() {
    std::cout << heading("Export reports");
    std::string dir = ask("Output folder", "reports");
    std::string base = exportAll(dir, 0);
    std::cout << "\n  Written:\n  " << paint(base + "_report.html", Tone::Accent)
              << "   <- open in any browser\n  " << base << "_report.md\n  " << base << "_summary.csv\n  "
              << base << "_steps.csv\n  " << base << "_sweep.csv\n";
}

void App::actionValidation() {
    std::cout << heading("Validation suite");
    render::validation(std::cout, runValidation());
}

void App::actionEncyclopedia() {
    std::cout << heading("Algorithm encyclopedia");
    render::encyclopedia(std::cout);
}

void App::actionSaveTrace() {
    std::cout << heading("Save trace");
    std::string path = ask("File path", "traces/" + safeFileName(trace_.name()) + ".txt");
    saveTraceFile(trace_, path);
    std::cout << "  Saved " << trace_.size() << " references to " << paint(path, Tone::Accent) << "\n";
}

}  // namespace prs
