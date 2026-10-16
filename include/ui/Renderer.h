// Terminal rendering of traces, simulations and analyses (ASCII only, so it
// works in every Windows / Linux console).
#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "analysis/Analyzer.h"
#include "analysis/Validation.h"
#include "core/PageTable.h"
#include "core/Trace.h"
#include "core/Types.h"

namespace prs {
namespace render {

void banner(std::ostream& os);
void traceSummary(std::ostream& os, const Trace& trace);
void config(std::ostream& os, const SimulationConfig& cfg, const std::vector<std::string>& keys);

// Classic textbook frame table, wrapped every `columns` references.
void stepGrid(std::ostream& os, const SimulationResult& result, int columns = 20);
// One line per reference with the reason behind every decision.
void stepLog(std::ostream& os, const SimulationResult& result, size_t limit = 0);
// A single step, used by the interactive walkthrough.
void singleStep(std::ostream& os, const SimulationResult& result, size_t index);
void resultSummary(std::ostream& os, const SimulationResult& result);
void pageTableStats(std::ostream& os, const PageTable& table, size_t limit = 10);

void comparison(std::ostream& os, const std::vector<SimulationResult>& results,
                long long optFaults, const SimulationConfig& cfg);
void faultBars(std::ostream& os, const std::vector<SimulationResult>& results);

void sweep(std::ostream& os, const SweepResult& sweep);
void belady(std::ostream& os, const std::vector<BeladyAnomaly>& anomalies);
void beladyHunt(std::ostream& os, const BeladyHunt& hunt);
void workingSet(std::ostream& os, const WorkingSetReport& report);
void stackDistance(std::ostream& os, const StackDistanceReport& report, int frames);
void inclusion(std::ostream& os, const InclusionReport& report);
void profile(std::ostream& os, const TraceProfile& profile);
void heatmap(std::ostream& os, const Heatmap& heatmap);
void validation(std::ostream& os, const std::vector<ValidationOutcome>& outcomes);
void encyclopedia(std::ostream& os);

char symbolFor(const std::string& key);

}  // namespace render
}  // namespace prs
