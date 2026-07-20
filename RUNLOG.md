# RUNLOG

| # | Profile | delay_ms | miss % | overhead | Change & why |
|---|---------|----------|--------|----------|---------------|
| 1 | A | 40  | 7.87%  | 1.02x | Baseline naive sender/receiver (forward once, no recovery). |
| 2 | B | 60  | 40.33% | 1.02x | Baseline naive, harsher profile — confirms lateness + loss both hurt. |
| 3 | A | 40  | 6.80%  | 1.26x | Added seq+type header, XOR/FEC parity in groups of 5, forward-on-arrival receiver. |
| 4 | B | 60  | 100%   | 1.26x | Same code — receiver bug (crash), fixed. |
| 5 | A | 40  | 6.80%  | 1.26x | Bug fixed, retested. |
| 6 | B | 60  | 37.07% | 1.26x | Bug fixed, retested. Still high — mostly lateness, not loss. |
| 7 | A | 60  | 1.53%  | 1.26x | Raised delay to absorb jitter. |
| 8 | A | 80  | 1.13%  | 1.26x | Raised delay further; diminishing returns = residual is double-loss-in-group, not lateness. |
| 9 | B | 90  | 4.53%  | 1.26x | Raised delay to absorb B's larger jitter range. |
| 10| B | 110 | 3.53%  | 1.26x | More delay; same diminishing-returns pattern as A. |
| 11| A | 80  | 0.20%  | 1.40x | GROUP_N 5→3: smaller groups cut double-loss probability. VALID. |
| 12| B | 110 | 1.07%  | 1.40x | GROUP_N=3 at same delay. Just over cap. |
| 13| B | 110 | 0.80%  | 1.57x | Tried GROUP_N=2 instead — valid but costs more overhead. |
| 14| B | 120 | 0.73%  | 1.40x | Kept GROUP_N=3, +10ms delay instead — better than GROUP_N=2 on every axis. |
| 15| A | 70  | 0.53%  | 1.40x | Checking margin at lower delay. |
| 16| A | 60  | 0.93%  | 1.40x | Only 0.07pp margin — too risky for unseen profiles. |
| 17| A | 120 | 0.13%  | 1.40x | Standardizing on single delay_ms=120 for both profiles — comfortable margin. |
| 18| B | 120 | 0.73%  | 1.40x | Confirms 120ms is safely VALID on both A and B. **Final config.** |

**Final config:** `GROUP_N=3`, `delay_ms=120` — VALID on both profiles with margin (A: 0.13%, B: 0.73%, both well under the 1% cap; overhead 1.40x, well under 2.0x).