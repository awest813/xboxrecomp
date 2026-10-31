# Engineering log and evidence index

Current: checkpoint 186, 2026-10-31. Resume from [HANDOFF.md](HANDOFF.md). The long
chronological logs of both lineages are preserved whole:
[resident-renderer lineage to run 254](archive/ENGINEERING_LOG_FAST254_2026-10-23.md),
[checkpoint-185 lineage](archive/ENGINEERING_LOG_PRE185.md).

## 2026-10-31 — Stall audit against other EA ports (runs 259–261)

Two research passes: the local NFSU2/SSX Tricky/Mercenaries/DOA/JSRF references and
the web (maddog847's MVP 06 and MVP 2005 XBERecomp ports, the MVP 2005 GameCube decomp,
SSX 3 recomp, xemu issues 2403/904/2096). Applied the three fixes that matched known
EA-port bugs: precise 1 ms guest clocks with a dedicated KeTickCount thread and
drift-free kernel timers; APU interrupt delivery on vector 0x35 plus per-frame
consumption of latched `set_irq`; `frndint` honouring the guest RC (MVP 06's CRT
`floor` uses control word 0x173F, `ceil` 0x1B3F). Native dispatcher, memory-scope,
scheduler, heap, ack, critical-section (4 modes) and APU smokes plus 239 lifter tests
pass. Runs 260 and 261 pitch normally (no crash, 22–37 FPS under 80–100% host load),
but the between-pitch gap stays ~70–78 s: MVP 06 arms no kernel timers and never
raises APU notifier status (it software-mixes into one ring), and the floor/ceil
correction did not move it. An exit probe in `sub_00329D20` logged no failed
animation lookups. Pressing Black/White/sticks/triggers during the indefinite
runner-on-first wait does nothing. Next: identify the igflow wait command holding
the post-pitch flow (`cWaitForActiveActors` vtable 0x617658, hash 0xDC217F0C).

## 2026-10-31 — Checkpoint 186: lineages merged, DOF fixed, contiguous frees (runs 255–258)

**Audit.** Two lineages had grown from the same 2026-10-20 baseline snapshot
(`refs/baseline/main-import`). Branch `claude/game-fps-accuracy-5eb48f` (8bfcdff +
docs c4c08b2) reached 30–37 FPS with GPU-resident surfaces, flip pacing, a vertex
ring buffer, timer wakeups and a block VP interpreter. The checkpoint-185 lineage
(61a93f2 → 960363e) stayed on the readback renderer at ~4 FPS but fixed real runtime
bugs: the heap/TLS race, critical-section waits holding RAM, the 0x3DD40 event-switch
boundary, top-left raster coverage, observer attribution. Neither had the other's work.

**Merge.** Code files were three-way merged against the shared baseline rather than
git's older merge base (which reported add/add conflicts everywhere). Six hunks needed
hand resolution: title env defaults (resident profile kept, scopes off), the flag
worker's idle backoff (resident version kept; the duplicate `RECOMP_NV2A_ACK_POLL_US`
knob dropped), the clear path (GPU clear kept, observer attribution kept), observer
counting (now under `render_cs` and skipped when the renderer is off), and the
host-access hook alongside the GPU handoff API in `guest_memory.h`. The worktree's
generated code took the event-switch fix from the main checkout's `recomp_0001.c`
(the only differing generated file). The CS policy matches the resident lineage by
default; `RECOMP_CS_GUEST_FASTPATH` stays opt-in.

**Contiguous lifetime.** `MmFreeContiguousMemory` sent window addresses to the
ordinary heap, which never held them, so every D3D/DSound buffer leaked. Run184
exhausted the 64 MB window. The window now has a block table: first-fit reuse, page
granular, coalescing, `MmQueryAllocationSize` answered from it, zero-fill outside the
lock after `xbox_MemoryHostAccess`. `xbox_ContiguousAllocatedBytes` stays a lock-free
high-water mark (the renderer calls it while holding `render_cs`). Native fixture: 20
rounds allocating and freeing >32 MB each (old allocator: exhausted in round 2).

Run 255 hung on the boot loading screen. The first version made alignment gaps
reusable, which moved the DSound heap off 0x82F20000, so the DSP doorbells hardcoded
in `RECOMP_APU_DSP_ACK` were never acknowledged. Gaps are no longer reusable; with
that, allocation order matches the bump allocator until the title frees something,
and run 256 boots normally (frees and reuse visible from the first seconds).

**Depth of field.** The pitch camera blurred the whole frame because texture-stage
mode 15 (DEPENDENT_AR) was unimplemented: the focus lookup (t3, addressed by the
depth texel's A,R) read (0,0,0,1), so the composite took the blur everywhere.
Implemented DEPENDENT_AR/GB and SET_SHADER_OTHER_STAGE_INPUT in the D3D11 shader and
the software combiner; added exact-texel cases to `nv2a_render_smoke`. The draw log
now prints depth func, alpha test, stage program and each combiner stage's words.

**Measurements** (pitch selector, `read_render_stats.py`, host load in brackets):
run 256 23.8, 23.0 [77%]; run 257 (CS fast path on) 31.1, 24.7 [100%], 36.3, 31.6,
36.5 [80%]; run 258 (DOF fix, fast path off) 38.6, 39.5, 38.8, 39.1 [54%]. Gameplay
soak 256: 24.9, 28.9, 18.5, 26.5. Load dominates these numbers; no fast-path gain is
claimed. See HANDOFF for the run 258 soak.

**Stall and crash.** Run 256 crashed after its sixth pitch on an unchecked failed
animation lookup (`sub_00331FB0`, 0x331FF4). Ruled out for the between-pitch stall:
disc I/O, the audio clock, x87 precision. Details in
[GAMEPLAY_RENDER_AUDIT.md](GAMEPLAY_RENDER_AUDIT.md).

## Checkpoint-185 lineage reports (readback renderer)

| Report | Result |
| --- | --- |
| [164](PERFORMANCE_CHECK164.md) | Readback/worker profiling; no matched regression established |
| [165–166](PERFORMANCE_WORK165_166.md) | Observer attribution and tighter clipped-triangle bounds |
| [167–168](PERFORMANCE_WORK167_168.md) | Adaptive flag polling reduces worker CPU; no broad FPS gain |
| [170–171](PERFORMANCE_WORK170_171.md) | Exact block row comparisons; native speedup only |
| [172–173](PERFORMANCE_WORK172_173.md) | Driver/default-resource mapping experiments; no demonstrated gain |
| [174](PERFORMANCE_WORK174.md) | Direct packed readback rejected after reverse comparison |
| [176–179](PERFORMANCE_WORK176_179.md) | Bounded handoff; controlled 3.77→4.05 FPS selector gain |
| [180–182](PERFORMANCE_WORK180_182.md) | Heap race reproduction/repair; rejected completed-bridge experiment |
| [184](SYSTEM_PERFORMANCE_AUDIT184.md) | Whole-system profile, CS-wait repair, contiguous-arena failure |

Preserve failed experiments and attribution limits. New entries go at the top of this
file and link evidence rather than restating the live state everywhere.
