# Page Replacement Algorithm Simulator

**Operating Systems – Project Based Learning (B.Tech V Semester)**
Jaypee Institute of Information Technology · Submitted to: Twinkle Tyagi

| Name | Enrollment No. | Batch |
|---|---|---|
| Pragnya Khandelwal | 992401030360 | F8 |
| Pratistha Singh | 992401030361 | F8 |
| Rachna | 992401030362 | F8 |
| Anushka Jain | 992401030363 | F8 |
| Vivek Pareek | 992401030370 | F8 |

---

A C++14 virtual-memory lab. It replays a page reference trace against **11 page
replacement policies**. It models page-table lookups, page faults, frame allocation,
reference/dirty bits and disk write-backs. It measures every policy on the same input and
explains why each policy behaves the way it does.

The synopsis asked for FIFO, LRU and Optimal with hits, faults and ratios. This project
covers that and adds the extra modules below.

| Module | What it adds |
|---|---|
| **11 algorithms** | FIFO, LRU, OPT, Second-Chance Clock, Enhanced Second Chance (R,M), NRU, Aging, LFU, MFU, Random, **ARC** (adaptive, used in ZFS) |
| **Explainable simulation** | Every eviction carries a reason, e.g. *"evict page 7 from frame 0: next use farthest in the future (t=18)"* |
| **Dirty pages & EAT** | Read/write traces, write-back counting, and Effective Access Time from a configurable timing model |
| **Fault classification** | Compulsory (cold) vs capacity misses, plus extra faults vs OPT and an optimality % |
| **Frame sweep** | Faults for 1..N frames for every policy (ASCII chart in the terminal, small multiples in HTML) |
| **Belady's anomaly lab** | Detects the anomaly automatically, *hunts* random traces until it finds one, then **shrinks it to a minimal, hand-checkable string** |
| **Stack-algorithm checker** | Verifies the inclusion property M(n) ⊆ M(n+1) step by step, the theory behind Belady's anomaly |
| **Working set & thrashing** | Denning's working set W(t, Δ) over time, thrashing risk, and a recommended frame count |
| **Mattson stack distances** | One pass predicts LRU faults for *every* memory size, and is cross-checked against the simulator |
| **Trace profiler** | Locality class, reuse/sequential/hot-set metrics and plain-language insights about which policy fits |
| **Locality heatmap** | Page × time density map that shows program phases and scans |
| **Workload generator** | Uniform, locality (phases), loop, sequential, Zipf (80/20) and mixed (scan pollution) traces, seeded and reproducible |
| **Address translation** | Traces may contain hex virtual addresses (`0x1A3F`); page = address / page size |
| **Reports** | A self-contained **HTML report** (interactive SVG charts, dark/light theme, step tables) plus Markdown and CSV |
| **Animated replay** | Inside the HTML report: pick a policy, then play / pause / step / scrub through every decision with frames flashing green (hit) or red (fault) |
| **Validation suite** | 18 hand-calculated textbook cases plus more than a million automated property checks |
| **Two interfaces** | A menu-driven interactive lab with step-by-step walkthrough, and a scriptable CLI |

---

## Build

Needs only a C++14 compiler (tested with MinGW g++ 6.3 on Windows 10).

```bat
build.bat                    :: Windows, no make/cmake required
run.bat                      :: builds if needed, then starts the interactive lab
```

```sh
make            # or: mingw32-make   -> build/pagesim(.exe), build/pagesim_tests(.exe)
make test       # run the test suite

cmake -B build-cmake && cmake --build build-cmake    # CMake alternative
```

## Run

### Interactive lab

```
build\pagesim.exe
```

```
   1  Enter a reference string             2  Load trace from file
   3  Generate a synthetic workload        4  Classic scenarios (demo library)
   5  Configure frames & timing            6  Choose algorithms
   7  Run & compare policies               8  Step-by-step walkthrough
   9  Faults vs frames sweep              10  Belady's anomaly lab
  11  Working set & thrashing             12  Trace profile & heatmap
  13  Stack-algorithm check               14  Export reports (HTML/MD/CSV)
  15  Validation suite                    16  Algorithm encyclopedia
  17  Save current trace                   0  Exit
```

**Demo path for the viva:** `4` (pick a scenario) → `6` (type `all`) → `7` → `9` → `14`, then open the HTML file in `reports/` and press **Play** in the *Animated replay* section.

### Command line

```sh
# Textbook string, all policies, frame tables
pagesim -t "7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1" -n 3 -a all --steps

# Belady's anomaly: sweep 1..7 frames, inclusion check, random hunt
pagesim -f traces/belady_anomaly.txt -n 3 -a all --sweep 1-7 --inclusion --belady

# Synthetic workload with writes, full analysis + HTML/MD/CSV reports
pagesim -g zipf --length 300 --pages 20 --write-prob 0.3 -n 4 -a all --profile --ws --report

# Every decision with its reason
pagesim -f traces/scan_pollution.txt -n 3 -a lru,arc,opt --verbose

pagesim --test      # validation suite
pagesim --list      # algorithm encyclopedia
pagesim --help      # all options
```

### Trace file format

```text
# comments start with '#'; separators: spaces, commas, semicolons, newlines
7 0 1 2W 0 3        # W (or w, W:2, 2:w) marks a write -> sets the dirty bit
0x2004 0x3ABCW      # hex = virtual address, translated with --page-size (default 4096)
```

Sample traces live in [`traces/`](traces/): `silberschatz`, `belady_anomaly`, `lru_worst_case`,
`locality_phases`, `scan_pollution`, `write_heavy` and `address_trace`.

---

## Sample output

```
== Comparison (3 frames) =====================================================
  #  Policy   Faults  Hits     Hit%   Fault%  Cold   Cap   WB         EAT   vs OPT  Optimal
  -----------------------------------------------------------------------------------------
  1  OPT           9    11   55.00%   45.00%     6     3    0     3.60 ms        =   100.0%
  2  LRU          12     8   40.00%   60.00%     6     6    0     4.80 ms       +3    75.0%
  3  FIFO         15     5   25.00%   75.00%     6     9    0     6.00 ms       +6    60.0%

  Verdict: best practical policy LRU with 12 faults, 3 above the optimal 9.
```

```
  FIFO - First-In First-Out (3 frames)
  Ref      | 7   0   1   2   0   3   0   4   2   3   0   3   2   1   2   0   1   7   0   1
  ---------+--------------------------------------------------------------------------------
  Frame 0  | 7   7   7   2   2   2   2   4   4   4   0   0   0   0   0   0   0   7   7   7
  Frame 1  |     0   0   0   0   3   3   3   2   2   2   2   2   1   1   1   1   1   0   0
  Frame 2  |         1   1   1   1   0   0   0   3   3   3   3   3   2   2   2   2   2   1
  ---------+--------------------------------------------------------------------------------
  Evicted  |             7       0   1   2   3   0   4           2   3           0   1   2
  Result   | F   F   F   F   H   F   F   F   F   F   F   H   H   F   F   H   H   F   F   F
```

```
== Belady's anomaly check ====================================================
  ANOMALY  FIFO   3 frames -> 9 faults, but 4 frames -> 10 faults
== Inclusion property M(n) subset of M(n+1) ==================================
  FIFO   M(3) subset of M(4)? VIOLATED at t=6: {1,2,5} not in {2,3,4,5}
  LRU    M(3) subset of M(4)? holds at every step
  OPT    M(3) subset of M(4)? holds at every step
```

Example HTML reports generated from the sample traces are in [`reports/`](reports/).

---

## Findings (data-backed)

| Access pattern | Winner (besides OPT) | Why |
|---|---|---|
| Textbook string (Silberschatz) | LRU 12 < FIFO 15 | temporal locality rewards recency |
| Belady string, 3 → 4 frames | LRU/OPT improve; FIFO, CLOCK, ESC, MFU get **worse** | only stack algorithms are immune |
| Loop of 5 pages, 4 frames | RANDOM 13 (OPT 10) | every deterministic policy, LRU and FIFO included, evicts exactly the page needed next: 25/25 faults |
| Hot set + one-off scans | **ARC = OPT (11)**, LRU/FIFO 15 | ARC's T1/T2 split stops scans flushing hot pages |
| Zipf (skewed popularity) | **LFU**, then ARC | frequency captures the stable hot set |
| Write-heavy (`write_heavy.txt`) | LFU 17 faults; NRU and LFU 5 write-backs vs 8 for FIFO/LRU | NRU ranks clean pages first; ESC's pass over R bits did not help on this trace |

---

## Validation

`pagesim --test` checks the simulator against hand-calculated answers. These include
Silberschatz (FIFO 15 / LRU 12 / OPT 9), Belady's string (FIFO 9 → 10), GeeksforGeeks
examples, and hand-traced Clock, LFU, MFU and ESC cases with write-back counts.

`pagesim_tests` adds property checks over 36 generated traces × 5 frame sizes × 11
policies (1,132,502 checks in total):

- hits + faults = references; no policy ever beats OPT; cold misses = distinct pages
- no duplicate pages in frames; the referenced page is always resident afterwards
- Mattson's stack-distance prediction equals the simulated LRU result for every frame count
- LRU and OPT satisfy the inclusion property and never show Belady's anomaly
- the Belady hunter returns a minimal anomalous trace (deleting any reference removes the anomaly)
- parser, generator determinism, working-set analysis and the algorithm factory

## Project structure

```
PageReplacementSimulator/
├── include/ & src/
│   ├── core/         Types, Trace, PageTable, Simulator (the engine)
│   ├── algorithms/   ReplacementAlgorithm interface, 11 policies, factory
│   ├── analysis/     sweep, Belady, working set, Mattson, inclusion, profile, heatmap, validation
│   ├── io/           trace parser/loader, workload generator, HTML/Markdown/CSV reports
│   └── ui/           ANSI console, terminal renderer, interactive menu + CLI
├── tests/            unit + property tests
├── traces/           sample reference strings
├── reports/          generated reports
├── docs/DESIGN.md    architecture and design decisions
├── build.bat · run.bat · Makefile · CMakeLists.txt
```

See [docs/DESIGN.md](docs/DESIGN.md) for the architecture, the simulation loop and how each algorithm is implemented.

## References

1. A. Silberschatz, P. B. Galvin, G. Gagne – *Operating System Concepts*, 10th ed. (Virtual Memory)
2. A. S. Tanenbaum, H. Bos – *Modern Operating Systems* (Ch. 3, Memory Management)
3. L. A. Belady – *A study of replacement algorithms for a virtual-storage computer*, IBM Systems Journal, 1966
4. R. L. Mattson et al. – *Evaluation techniques for storage hierarchies*, IBM Systems Journal, 1970
5. P. J. Denning – *The working set model for program behavior*, CACM, 1968
6. N. Megiddo, D. S. Modha – *ARC: A Self-Tuning, Low Overhead Replacement Cache*, USENIX FAST 2003
7. GeeksforGeeks – Page Replacement Algorithms (FIFO, LRU, Optimal)
