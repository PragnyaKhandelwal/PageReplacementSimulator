// Exports: self-contained HTML report (SVG charts, no internet needed),
// CSV tables for spreadsheets, and a Markdown summary.
#pragma once

#include <string>
#include <vector>

#include "analysis/Analyzer.h"
#include "core/Trace.h"
#include "core/Types.h"

namespace prs {

// Everything a full report contains, computed once.
struct ReportBundle {
    Trace trace;
    SimulationConfig config;
    std::vector<SimulationResult> results;  // with steps recorded
    long long optFaults = 0;
    SweepResult sweep;
    std::vector<BeladyAnomaly> anomalies;
    WorkingSetReport workingSet;
    StackDistanceReport stackDistance;
    TraceProfile profile;
    Heatmap heatmap;
    std::vector<InclusionReport> inclusion;
};

ReportBundle buildReportBundle(const Trace& trace, const SimulationConfig& config,
                               const std::vector<std::string>& keys, int sweepMaxFrames,
                               int workingSetWindow);

void writeHtmlReport(const ReportBundle& bundle, const std::string& path);
void writeMarkdownReport(const ReportBundle& bundle, const std::string& path);
void writeSummaryCsv(const std::vector<SimulationResult>& results, long long optFaults,
                     const std::string& path);
void writeStepsCsv(const std::vector<SimulationResult>& results, const std::string& path);
void writeSweepCsv(const SweepResult& sweep, const std::string& path);

}  // namespace prs
