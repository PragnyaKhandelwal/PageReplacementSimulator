# Page Replacement Analysis

- **Trace:** silberschatz (20 references, 6 distinct pages)
- **Frames:** 3
- **Generated:** 2026-10-16 15:55

```
7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1
```

## Comparison

| # | Policy | Faults | Hits | Hit ratio | Fault ratio | Write-backs | EAT | vs OPT |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 1 | OPT | 9 | 11 | 55.00% | 45.00% | 0 | 3.60 ms | = |
| 2 | LRU | 12 | 8 | 40.00% | 60.00% | 0 | 4.80 ms | +3 |
| 2 | AGING | 12 | 8 | 40.00% | 60.00% | 0 | 4.80 ms | +3 |
| 4 | LFU | 13 | 7 | 35.00% | 65.00% | 0 | 5.20 ms | +4 |
| 4 | ARC | 13 | 7 | 35.00% | 65.00% | 0 | 5.20 ms | +4 |
| 6 | CLOCK | 14 | 6 | 30.00% | 70.00% | 0 | 5.60 ms | +5 |
| 6 | ESC | 14 | 6 | 30.00% | 70.00% | 0 | 5.60 ms | +5 |
| 6 | NRU | 14 | 6 | 30.00% | 70.00% | 0 | 5.60 ms | +5 |
| 9 | FIFO | 15 | 5 | 25.00% | 75.00% | 0 | 6.00 ms | +6 |
| 9 | MFU | 15 | 5 | 25.00% | 75.00% | 0 | 6.00 ms | +6 |
| 9 | RANDOM | 15 | 5 | 25.00% | 75.00% | 0 | 6.00 ms | +6 |

## Faults vs frames

| Policy | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---:|---:|---:|---:|---:|---:|
| FIFO | 20 | 15 | 15 | 10 | 9 | 6 |
| LRU | 20 | 17 | 12 | 8 | 7 | 6 |
| OPT | 20 | 13 | 9 | 8 | 7 | 6 |
| CLOCK | 20 | 15 | 14 | 9 | 9 | 6 |
| ESC | 20 | 15 | 14 | 9 | 9 | 6 |
| NRU | 20 | 16 | 14 | 9 | 8 | 6 |
| AGING | 20 | 17 | 12 | 8 | 7 | 6 |
| LFU | 20 | 15 | 13 | 9 | 7 | 6 |
| MFU | 20 | 15 | 15 | 12 | 8 | 6 |
| RANDOM | 20 | 16 | 15 | 12 | 8 | 6 |
| ARC | 20 | 15 | 13 | 9 | 7 | 6 |

## Belady's anomaly

No anomaly detected.

## Working set

- Window: 6
- Average |WS|: 3.80, peak 5
- Thrashing risk: high

## Trace insights

Locality class: **strong temporal locality**

- Median LRU stack distance is 3.0 (<= 3 frames): most re-references are recent, so recency-based policies (LRU, CLOCK, AGING, ARC) should approach OPT.
