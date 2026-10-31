# Engineering log and evidence index

Current checkpoint185,2026-10-31. The long chronological bring-up log is preserved in [ENGINEERING_LOG_PRE185.md](archive/ENGINEERING_LOG_PRE185.md); its old PIDs, defaults and next actions are historical. Resume from [HANDOFF.md](HANDOFF.md).

## Current work

- [180–182](PERFORMANCE_WORK180_182.md): allocator race reproduction/repair and rejected completed-bridge experiment.
- [Whole-system184 audit](SYSTEM_PERFORMANCE_AUDIT184.md): single-instance pipeline timings, host contention, real CS-wait repair, opt-in CPU policy and final contiguous-arena failure.
- Build185: current source links; native heap/critical-section/memory/scheduler checks pass at the scopes described in the handoff.185 itself has not run. All game/test processes are stopped for handoff.

## Earlier measured work

| Report | Result |
| --- | --- |
| [164](PERFORMANCE_CHECK164.md) |Readback/worker profiling; no matched regression established |
| [165–166](PERFORMANCE_WORK165_166.md) |Observer attribution and tighter clipped-triangle bounds |
| [167–168](PERFORMANCE_WORK167_168.md) |Adaptive flag polling reduces worker CPU; no broad FPS gain |
| [170–171](PERFORMANCE_WORK170_171.md) |Exact block row comparisons; native speedup only |
| [172–173](PERFORMANCE_WORK172_173.md) |Driver/default-resource mapping experiments; no demonstrated gain |
| [174](PERFORMANCE_WORK174.md) |Direct packed readback rejected after reverse comparison |
| [176–179](PERFORMANCE_WORK176_179.md) |Bounded handoff; historical controlled3.77→4.05 FPS selector gain |

Preserve failed experiments and attribution limits. No report here proves steady30 FPS, complete exhibition, reliable persistence or full accuracy. New entries should link evidence rather than prepend another full live-state history to every document.
