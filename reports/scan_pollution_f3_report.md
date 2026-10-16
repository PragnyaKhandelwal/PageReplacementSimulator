# Page Replacement Analysis

- **Trace:** scan_pollution (31 references, 15 distinct pages)
- **Frames:** 3
- **Generated:** 2026-10-16 15:55

```
1 2 1 2 1 2 10 11 12 13 14 1 2 1 2 15 16 17 18 1 2 1 2 19 20 21 22 1 2 1 2
```

## Comparison

| # | Policy | Faults | Hits | Hit ratio | Fault ratio | Write-backs | EAT | vs OPT |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 1 | OPT | 15 | 16 | 51.61% | 48.39% | 0 | 3.87 ms | = |
| 1 | LFU | 15 | 16 | 51.61% | 48.39% | 0 | 3.87 ms | = |
| 1 | ARC | 15 | 16 | 51.61% | 48.39% | 0 | 3.87 ms | = |
| 4 | FIFO | 21 | 10 | 32.26% | 67.74% | 0 | 5.42 ms | +6 |
| 4 | LRU | 21 | 10 | 32.26% | 67.74% | 0 | 5.42 ms | +6 |
| 4 | CLOCK | 21 | 10 | 32.26% | 67.74% | 0 | 5.42 ms | +6 |
| 4 | ESC | 21 | 10 | 32.26% | 67.74% | 0 | 5.42 ms | +6 |
| 4 | NRU | 21 | 10 | 32.26% | 67.74% | 0 | 5.42 ms | +6 |
| 4 | AGING | 21 | 10 | 32.26% | 67.74% | 0 | 5.42 ms | +6 |
| 4 | MFU | 21 | 10 | 32.26% | 67.74% | 0 | 5.42 ms | +6 |
| 4 | RANDOM | 21 | 10 | 32.26% | 67.74% | 0 | 5.42 ms | +6 |

## Faults vs frames

| Policy | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| FIFO | 31 | 21 | 21 | 21 | 21 | 19 | 17 | 17 | 17 | 17 | 17 | 17 |
| LRU | 31 | 21 | 21 | 21 | 21 | 17 | 15 | 15 | 15 | 15 | 15 | 15 |
| OPT | 31 | 18 | 15 | 15 | 15 | 15 | 15 | 15 | 15 | 15 | 15 | 15 |
| CLOCK | 31 | 21 | 21 | 21 | 21 | 19 | 17 | 17 | 17 | 17 | 17 | 17 |
| ESC | 31 | 21 | 21 | 21 | 21 | 17 | 17 | 17 | 17 | 17 | 17 | 17 |
| NRU | 31 | 21 | 21 | 21 | 21 | 19 | 17 | 17 | 17 | 17 | 17 | 17 |
| AGING | 31 | 21 | 21 | 21 | 21 | 17 | 15 | 15 | 15 | 15 | 15 | 15 |
| LFU | 31 | 18 | 15 | 15 | 15 | 15 | 15 | 15 | 15 | 15 | 15 | 15 |
| MFU | 31 | 21 | 21 | 21 | 21 | 21 | 19 | 19 | 19 | 19 | 17 | 17 |
| RANDOM | 31 | 23 | 21 | 21 | 18 | 17 | 17 | 16 | 16 | 16 | 15 | 15 |
| ARC | 31 | 18 | 15 | 15 | 15 | 15 | 15 | 15 | 15 | 15 | 15 | 15 |

## Belady's anomaly

No anomaly detected.

## Working set

- Window: 10
- Average |WS|: 5.71, peak 8
- Thrashing risk: high

## Trace insights

Locality class: **strong temporal locality**

- Median LRU stack distance is 2.0 (<= 3 frames): most re-references are recent, so recency-based policies (LRU, CLOCK, AGING, ARC) should approach OPT.
- 63% of references continue a sequential run: scan-resistant ARC protects the hot set from being flushed by scans.
- Skewed popularity: the hottest 20% of pages receive 61% of references - frequency-aware policies (LFU, ARC) are competitive.
