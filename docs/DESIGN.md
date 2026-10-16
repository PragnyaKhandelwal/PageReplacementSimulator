# Design Document – Page Replacement Algorithm Simulator

## 1. Goals

The synopsis has four objectives. Each one maps to a part of the code:

| Synopsis objective | Where it lives |
|---|---|
| Model page-table lookups, page faults, frame allocation | `core/PageTable`, `core/Simulator` |
| Implement and compare FIFO, LRU and Optimal | `algorithms/Classic.cpp`, plus 8 more policies |
| Measure faults, hits, fault ratio, hit ratio | `SimulationResult` (plus compulsory/capacity misses, write-backs, EAT) |
| Comparative report on the same trace and frame count | `analysis/Analyzer` (`rankResults`), `io/ReportWriter`, `ui/Renderer` |
| Validate with manually calculated cases (methodology step 7) | `analysis/Validation`, `tests/test_main.cpp` |

## 2. Architecture

The code is split into five layers. Each layer depends only on the layers below it.

```
          ui/  (App: menu + CLI, Renderer, Console)
            |
   io/ (TraceIO, TraceGenerator, ReportWriter)     analysis/ (Analyzer, Validation)
            |                                          |
            +--------------------+---------------------+
                                 |
                  core/ (Simulator, PageTable, Trace, Types)
                                 |
                algorithms/ (ReplacementAlgorithm + 11 policies)
```

* **Strategy pattern.** Every policy implements `ReplacementAlgorithm`. The simulator never
  knows which policy it is running.
* **Factory.** `createAlgorithm("lru")` builds a policy from a case-insensitive name or alias.
  This keeps the CLI, the menu, the analyses and the tests simple.
* **Value results.** `SimulationResult` and `StepRecord` are plain data. Renderers and report
  writers only read them, so the terminal, HTML, Markdown and CSV outputs always show the
  same numbers.

## 3. The simulation loop (`Simulator::run`)

This follows the flowchart in the synopsis:

```
for each reference r (page p, read/write):
    algorithm.onReference()
    frame = pageTable.lookup(p)                      // page-table lookup
    if frame != EMPTY:                               // HIT
        update lastAccess, frequency, R bit (+M bit on write)
        algorithm.onHit()
    else:                                            // FAULT
        classify: compulsory (first touch) or capacity (was evicted before)
        algorithm.onMiss()
        frame = first free frame
        if none:                                     // memory full
            victim = algorithm.selectVictim()        // policy decision + reason
            if victim dirty: writeBacks++            // disk write-back
            pageTable.unmap(victim page)
        load p into frame, reset its bits, pageTable.map(p, frame)
        algorithm.onLoad()
    algorithm.afterReference()                       // clock ticks (NRU, Aging)
    record a StepRecord (frame snapshot + reason)
```

The simulator owns the hardware state: the frames with their R/M bits, load time, last use
and use count. A policy that needs nothing more (FIFO, LRU, LFU, MFU) is a single
`selectVictim` method that reads this state. Policies with their own state use the hooks:

| Policy | Extra state | Hooks used |
|---|---|---|
| OPT | next-use array built in one backward pass (O(n)), next use per frame | `reset`, `onHit`, `onLoad` |
| Clock | clock hand | `selectVictim` clears R bits as it sweeps |
| ESC | clock hand | up to 4 passes over the (R,M) classes |
| NRU | reset interval | `afterReference` clears R bits (simulated clock interrupt) |
| Aging | n-bit counter per frame | `afterReference` shifts R into the counter's MSB |
| Random | seeded `mt19937` | – |
| ARC | lists T1, T2, B1, B2 and adaptive target p | `onHit`, `onMiss` (adapt p, choose victim), `onLoad` |

**Effective Access Time:** `EAT = ma + p × fault_service + w × write_back`. Here p is the
fault ratio and w is the fraction of references that forced a dirty write-back.

## 4. Design decisions worth knowing

* **Deterministic tie-breaking.** Ties go to the page that was loaded first, so every run
  can be reproduced and the hand-calculated cases are unambiguous. OPT is the exception:
  pages that are never used again are ordered by *least recent use*, because that order
  depends on the trace alone and not on memory size. Without this, OPT would break the
  inclusion property. The test suite caught exactly that bug.
* **Clock semantics.** Loading a page sets R = 1, because hardware sets the bit on every
  access. After an eviction the hand moves past the newly loaded frame.
* **ARC consistency.** ARC chooses its victim in `onMiss`, as in the paper's pseudo-code.
  ARC's invariants guarantee that it only evicts when memory is full. `selectVictim`
  throws if its directory ever disagrees with the physical frames, and the property tests
  run ARC on 180 trace/frame combinations without triggering it.
* **Explanations cost nothing when off.** Frame sweeps and Belady hunts run with
  `recordSteps = false`. In that mode the policies skip building reason strings, so a
  sweep stays fast.

## 5. Analysis modules

| Module | Idea | Use |
|---|---|---|
| `frameSweep` | rerun with 1..N frames | faults vs memory curve |
| `detectBeladyAnomalies` | faults(n+1) > faults(n) | spot the anomaly automatically |
| `huntBeladyAnomaly` | random search over seeded traces | find new anomalous strings |
| `checkInclusion` | M(n) ⊆ M(n+1) at every step | shows *why* LRU and OPT are immune |
| `analyzeStackDistances` | Mattson's LRU stack | LRU faults for every size in one pass |
| `analyzeWorkingSet` | sliding window of Δ references | thrashing risk, frames needed |
| `profileTrace` | reuse, sequentiality, skew, stack distance | locality class + recommendations |
| `buildHeatmap` | page × time window counts | shows phases (bands) and scans (diagonals) |

## 6. Testing strategy

1. **Oracle tests.** These are answers computed by hand or taken from textbooks
   (`Validation.cpp`). The same list is shown in the app through menu option 15.
2. **Invariants.** These must hold for any correct policy on any trace (e.g. faults ≥ OPT,
   cold misses = distinct pages).
3. **Differential testing.** Mattson's analytical LRU result must equal the simulated LRU
   result for frame counts 1..12.
4. **Theory tests.** LRU and OPT must satisfy the inclusion property. FIFO must show Belady's
   anomaly on the classic string.

## 7. Extending

To add a policy:

1. Subclass `ReplacementAlgorithm` in `Algorithms.h`.
2. Implement `selectVictim` and any hooks it needs.
3. Register its key in `AlgorithmFactory.cpp`.

The new policy then appears in the menu, the CLI, sweeps, reports and the property tests.
