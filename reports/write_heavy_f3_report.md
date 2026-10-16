# Page Replacement Analysis

- **Trace:** write_heavy (25 references, 7 distinct pages)
- **Frames:** 3
- **Generated:** 2026-10-16 15:55

```
1W 2 3W 4 1 2W 5 1 2 3W 4 5W 1W 3 2 4W 5 1 6 2W 7 1 3 6W 2
```

## Comparison

| # | Policy | Faults | Hits | Hit ratio | Fault ratio | Write-backs | EAT | vs OPT |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 1 | OPT | 14 | 11 | 44.00% | 56.00% | 7 | 6.72 ms | = |
| 2 | LFU | 17 | 8 | 32.00% | 68.00% | 5 | 7.04 ms | +3 |
| 3 | NRU | 18 | 7 | 28.00% | 72.00% | 5 | 7.36 ms | +4 |
| 4 | FIFO | 18 | 7 | 28.00% | 72.00% | 8 | 8.32 ms | +4 |
| 4 | MFU | 18 | 7 | 28.00% | 72.00% | 8 | 8.32 ms | +4 |
| 6 | RANDOM | 19 | 6 | 24.00% | 76.00% | 8 | 8.64 ms | +5 |
| 7 | ESC | 20 | 5 | 20.00% | 80.00% | 8 | 8.96 ms | +6 |
| 8 | CLOCK | 21 | 4 | 16.00% | 84.00% | 8 | 9.28 ms | +7 |
| 9 | ARC | 22 | 3 | 12.00% | 88.00% | 8 | 9.60 ms | +8 |
| 10 | LRU | 23 | 2 | 8.00% | 92.00% | 8 | 9.92 ms | +9 |
| 10 | AGING | 23 | 2 | 8.00% | 92.00% | 8 | 9.92 ms | +9 |

## Faults vs frames

| Policy | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---:|---:|---:|---:|---:|---:|---:|
| FIFO | 25 | 25 | 18 | 15 | 10 | 9 | 7 |
| LRU | 25 | 25 | 23 | 19 | 8 | 8 | 7 |
| OPT | 25 | 19 | 14 | 10 | 7 | 7 | 7 |
| CLOCK | 25 | 25 | 21 | 15 | 9 | 9 | 7 |
| ESC | 25 | 25 | 20 | 18 | 9 | 9 | 7 |
| NRU | 25 | 23 | 18 | 14 | 10 | 8 | 7 |
| AGING | 25 | 25 | 23 | 19 | 8 | 8 | 7 |
| LFU | 25 | 25 | 17 | 15 | 9 | 8 | 7 |
| MFU | 25 | 25 | 18 | 15 | 10 | 9 | 7 |
| RANDOM | 25 | 23 | 19 | 14 | 8 | 8 | 7 |
| ARC | 25 | 25 | 22 | 17 | 9 | 8 | 7 |

## Belady's anomaly

No anomaly detected.

## Working set

- Window: 8
- Average |WS|: 4.84, peak 7
- Thrashing risk: high

## Trace insights

Locality class: **weak locality**

- Median reuse distance 5.0 is larger than memory - expect high fault ratios; adding frames pays off until the reuse distance is covered.
- 36% of references are writes: dirty-aware policies (ESC, NRU) can cut costly write-backs.
