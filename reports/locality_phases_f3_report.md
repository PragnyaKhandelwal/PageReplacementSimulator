# Page Replacement Analysis

- **Trace:** locality_phases (30 references, 9 distinct pages)
- **Frames:** 3
- **Generated:** 2026-10-16 15:55

```
1 2 1 3 2 1 2 3 1 2 7 8 7 9 8 7 8 9 7 8 4 5 4 6 5 4 5 6 4 5
```

## Comparison

| # | Policy | Faults | Hits | Hit ratio | Fault ratio | Write-backs | EAT | vs OPT |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 1 | FIFO | 9 | 21 | 70.00% | 30.00% | 0 | 2.40 ms | = |
| 1 | LRU | 9 | 21 | 70.00% | 30.00% | 0 | 2.40 ms | = |
| 1 | OPT | 9 | 21 | 70.00% | 30.00% | 0 | 2.40 ms | = |
| 1 | CLOCK | 9 | 21 | 70.00% | 30.00% | 0 | 2.40 ms | = |
| 1 | ESC | 9 | 21 | 70.00% | 30.00% | 0 | 2.40 ms | = |
| 1 | NRU | 9 | 21 | 70.00% | 30.00% | 0 | 2.40 ms | = |
| 1 | AGING | 9 | 21 | 70.00% | 30.00% | 0 | 2.40 ms | = |
| 1 | MFU | 9 | 21 | 70.00% | 30.00% | 0 | 2.40 ms | = |
| 9 | ARC | 10 | 20 | 66.67% | 33.33% | 0 | 2.67 ms | +1 |
| 10 | RANDOM | 12 | 18 | 60.00% | 40.00% | 0 | 3.20 ms | +3 |
| 11 | LFU | 23 | 7 | 23.33% | 76.67% | 0 | 6.13 ms | +14 |

## Faults vs frames

| Policy | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| FIFO | 30 | 24 | 9 | 9 | 9 | 9 | 9 | 9 | 9 |
| LRU | 30 | 24 | 9 | 9 | 9 | 9 | 9 | 9 | 9 |
| OPT | 30 | 18 | 9 | 9 | 9 | 9 | 9 | 9 | 9 |
| CLOCK | 30 | 24 | 9 | 9 | 9 | 9 | 9 | 9 | 9 |
| ESC | 30 | 24 | 9 | 9 | 9 | 9 | 9 | 9 | 9 |
| NRU | 30 | 22 | 9 | 9 | 9 | 9 | 9 | 9 | 9 |
| AGING | 30 | 24 | 9 | 9 | 9 | 9 | 9 | 9 | 9 |
| LFU | 30 | 26 | 23 | 23 | 18 | 16 | 16 | 11 | 9 |
| MFU | 30 | 24 | 9 | 9 | 9 | 9 | 9 | 9 | 9 |
| RANDOM | 30 | 22 | 12 | 13 | 9 | 9 | 9 | 9 | 9 |
| ARC | 30 | 24 | 10 | 10 | 10 | 10 | 10 | 10 | 9 |

## Belady's anomaly

- **RANDOM**: 3 frames -> 12 faults, 4 frames -> 13 faults

## Working set

- Window: 10
- Average |WS|: 4.20, peak 6
- Thrashing risk: high

## Trace insights

Locality class: **strong temporal locality**

- Median LRU stack distance is 3.0 (<= 3 frames): most re-references are recent, so recency-based policies (LRU, CLOCK, AGING, ARC) should approach OPT.
- 41% of references continue a sequential run: scan-resistant ARC protects the hot set from being flushed by scans.
