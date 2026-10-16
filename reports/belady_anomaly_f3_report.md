# Page Replacement Analysis

- **Trace:** belady_anomaly (12 references, 5 distinct pages)
- **Frames:** 3
- **Generated:** 2026-10-16 15:55

```
1 2 3 4 1 2 5 1 2 3 4 5
```

## Comparison

| # | Policy | Faults | Hits | Hit ratio | Fault ratio | Write-backs | EAT | vs OPT |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 1 | OPT | 7 | 5 | 41.67% | 58.33% | 0 | 4.67 ms | = |
| 2 | FIFO | 9 | 3 | 25.00% | 75.00% | 0 | 6.00 ms | +2 |
| 2 | CLOCK | 9 | 3 | 25.00% | 75.00% | 0 | 6.00 ms | +2 |
| 2 | ESC | 9 | 3 | 25.00% | 75.00% | 0 | 6.00 ms | +2 |
| 2 | MFU | 9 | 3 | 25.00% | 75.00% | 0 | 6.00 ms | +2 |
| 2 | RANDOM | 9 | 3 | 25.00% | 75.00% | 0 | 6.00 ms | +2 |
| 7 | LRU | 10 | 2 | 16.67% | 83.33% | 0 | 6.67 ms | +3 |
| 7 | NRU | 10 | 2 | 16.67% | 83.33% | 0 | 6.67 ms | +3 |
| 7 | AGING | 10 | 2 | 16.67% | 83.33% | 0 | 6.67 ms | +3 |
| 7 | LFU | 10 | 2 | 16.67% | 83.33% | 0 | 6.67 ms | +3 |
| 7 | ARC | 10 | 2 | 16.67% | 83.33% | 0 | 6.67 ms | +3 |

## Faults vs frames

| Policy | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---:|---:|---:|---:|---:|---:|
| FIFO | 12 | 12 | 9 | 10 | 5 | 5 |
| LRU | 12 | 12 | 10 | 8 | 5 | 5 |
| OPT | 12 | 9 | 7 | 6 | 5 | 5 |
| CLOCK | 12 | 12 | 9 | 10 | 5 | 5 |
| ESC | 12 | 12 | 9 | 10 | 5 | 5 |
| NRU | 12 | 12 | 10 | 10 | 5 | 5 |
| AGING | 12 | 12 | 10 | 8 | 5 | 5 |
| LFU | 12 | 12 | 10 | 8 | 5 | 5 |
| MFU | 12 | 12 | 9 | 10 | 5 | 5 |
| RANDOM | 12 | 11 | 9 | 7 | 5 | 5 |
| ARC | 12 | 12 | 10 | 7 | 5 | 5 |

## Belady's anomaly

- **FIFO**: 3 frames -> 9 faults, 4 frames -> 10 faults
- **CLOCK**: 3 frames -> 9 faults, 4 frames -> 10 faults
- **ESC**: 3 frames -> 9 faults, 4 frames -> 10 faults
- **MFU**: 3 frames -> 9 faults, 4 frames -> 10 faults

## Working set

- Window: 4
- Average |WS|: 3.33, peak 4
- Thrashing risk: high

## Trace insights

Locality class: **cyclic / looping**

- Looping pattern whose reuse distance (median 4.0) exceeds the 3 frames: LRU and FIFO evict exactly the page needed next (worst case). RANDOM and OPT keep part of the loop resident.
