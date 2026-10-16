#include "core/Simulator.h"

#include <chrono>
#include <stdexcept>

#include "algorithms/AlgorithmFactory.h"
#include "core/Util.h"

namespace prs {

SimulationResult Simulator::run(ReplacementAlgorithm& algorithm, const Trace& trace,
                                const SimulationConfig& config) {
    PageTable scratch;
    return run(algorithm, trace, config, scratch);
}

SimulationResult Simulator::run(ReplacementAlgorithm& algorithm, const Trace& trace,
                                const SimulationConfig& config, PageTable& pageTable) {
    if (config.frames < 1) throw std::invalid_argument("number of frames must be at least 1");

    const int frameCount = config.frames;
    std::vector<Frame> frames(frameCount);
    pageTable.clear();
    algorithm.reset(frameCount, trace, config);

    SimulationResult result;
    result.key = algorithm.key();
    result.algorithm = algorithm.name();
    result.frames = frameCount;
    result.references = static_cast<long long>(trace.size());
    if (config.recordSteps) result.steps.reserve(trace.size());

    const bool explain = config.explain && config.recordSteps;
    const auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < trace.size(); ++i) {
        AccessContext ctx;
        ctx.time = static_cast<long long>(i);
        ctx.ref = trace[i];
        ctx.trace = &trace;
        ctx.explain = explain;

        const int page = ctx.ref.page;
        algorithm.onReference(frames, ctx);

        StepRecord step;
        step.index = ctx.time;
        step.ref = ctx.ref;

        // 1. Page-table lookup.
        int frameIdx = pageTable.lookup(page);
        pageTable.recordAccess(page);

        if (frameIdx != EMPTY) {
            // 2a. Page hit: refresh the hardware bits.
            Frame& f = frames[frameIdx];
            f.lastAccess = ctx.time;
            ++f.frequency;
            f.referenced = true;
            if (ctx.ref.isWrite()) f.dirty = true;
            algorithm.onHit(frames, frameIdx, ctx);

            ++result.hits;
            step.hit = true;
            step.frameUsed = frameIdx;
            if (explain) step.reason = cat("page ", page, " already resident in frame ", frameIdx);
        } else {
            // 2b. Page fault.
            ++result.faults;
            step.faultKind = pageTable.everLoaded(page) ? FaultKind::Capacity : FaultKind::Compulsory;
            if (step.faultKind == FaultKind::Compulsory) ++result.compulsoryFaults;
            else ++result.capacityFaults;

            algorithm.onMiss(frames, ctx);

            // 3. Free frame available?
            int target = EMPTY;
            for (int k = 0; k < frameCount; ++k) {
                if (frames[k].empty()) { target = k; break; }
            }

            if (target == EMPTY) {
                // 4. Memory full: ask the policy for a victim.
                Victim v = algorithm.selectVictim(frames, ctx);
                if (v.frame < 0 || v.frame >= frameCount || frames[v.frame].empty())
                    throw std::logic_error(algorithm.key() + " selected an invalid victim frame");
                target = v.frame;
                step.victimPage = frames[target].page;
                step.victimDirty = frames[target].dirty;
                ++result.evictions;
                if (step.victimDirty) ++result.writeBacks;
                pageTable.unmap(step.victimPage);
                if (explain) {
                    step.reason = cat("evict page ", step.victimPage, " from frame ", target,
                                      step.victimDirty ? " (dirty -> write back)" : "",
                                      ": ", v.reason);
                }
            } else if (explain) {
                step.reason = cat("free frame ", target, " available");
            }

            // 5. Load the page and update the page table.
            Frame& f = frames[target];
            f = Frame{};
            f.page = page;
            f.loadTime = ctx.time;
            f.lastAccess = ctx.time;
            f.frequency = 1;
            f.referenced = true;
            f.dirty = ctx.ref.isWrite();
            pageTable.map(page, target);
            algorithm.onLoad(frames, target, ctx);
            step.frameUsed = target;
        }

        algorithm.afterReference(frames, ctx);

        if (config.recordSteps) {
            step.frames.reserve(frameCount);
            for (const Frame& f : frames) step.frames.push_back(f.page);
            result.steps.push_back(std::move(step));
        }
    }

    const auto end = std::chrono::steady_clock::now();
    result.runtimeMicros = std::chrono::duration<double, std::micro>(end - start).count();
    result.pageTableLookups = pageTable.lookups();

    // EAT = ma + p * fault_service + w * write_back, with p the fault ratio
    // and w the fraction of references that forced a dirty write-back.
    if (result.references > 0) {
        const double p = result.faultRatio();
        const double w = double(result.writeBacks) / result.references;
        result.effectiveAccessNs =
            config.memoryAccessNs + p * config.faultServiceNs + w * config.writeBackNs;
    }
    return result;
}

std::vector<SimulationResult> Simulator::runAll(const std::vector<std::string>& keys,
                                                const Trace& trace,
                                                const SimulationConfig& config) {
    std::vector<SimulationResult> results;
    for (const auto& key : keys) {
        auto algo = createAlgorithm(key);
        if (!algo) throw std::invalid_argument("unknown algorithm: " + key);
        results.push_back(run(*algo, trace, config));
    }
    return results;
}

}  // namespace prs
