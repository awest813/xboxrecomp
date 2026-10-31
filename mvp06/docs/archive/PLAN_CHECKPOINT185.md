# MVP 06 implementation plan

Checkpoint185,2026-10-31. Work is stopped for handoff. Current state and commands: [docs/HANDOFF.md](docs/HANDOFF.md). Detailed initial audit and original milestone history: [archive](docs/archive/PLAN_PRE185.md).

## Objective and order

Get in game to steady30 FPS, then improve rendering and accuracy. Preserve original gameplay and timing. Boot, a clean short run, native microbenchmarks, or a local selector gain do not satisfy this objective.

## Next stages

1. **Restore repeatable resource lifetime.** Fix contiguous allocation/free/query bookkeeping exposed by demo→exhibition arena exhaustion in184. Reproduce natively; preserve graphics aliases and reserved regions. Build185 itself is unlaunched.
2. **Establish a reliable gameplay baseline.** Fresh run with observed exit, normal timing/tracing off and CS fast path0. Verify selector and two-pitch/pause acceptance plus repeat demo/loading. Record host load; recent CPU saturation invalidates broad performance claims.
3. **Measure scheduling changes.** The actual CS wait repair passes native checks. Keep the optional uncontended CS policy off until matched same-process comparisons in both orders and gameplay checks establish its effect.
4. **Reduce system-wide GPU/RAM round trips.** Use the [whole-system audit](docs/SYSTEM_PERFORMANCE_AUDIT184.md). About59 resolves/flip persist in the diagnostic scene. A resident-surface/CPU-visibility design must cover all aliases, native API probes, CPU/software writes, renderer serialization, fences and IRQs. Do not skip commits or expand budgets to improve a counter.
5. **Verify steady30 FPS.** Measure sustained gameplay and frame pacing across pitching, batting, fielding, transitions and pause/resume, with simulation timing and audio continuity checked. Full exhibition and save/reload remain open.
6. **Rendering and accuracy audit.** Follow [the audit backlog](docs/GAMEPLAY_RENDER_AUDIT.md): original-console images, geometry/clipping, depth/stencil, texture/fog/blend behavior, input, gameplay rules, audio and persistence. Enhancements follow baseline fidelity.

## Acceptance discipline

Keep the complete objective intact. Separate native correctness, runtime behavior, scene-specific speed and broad playability claims. Test the failed behavior before/after, preserve input/scene provenance, and record crashes and deliberate stops explicitly. The source/defaults and the loaded EXE/MAP must agree before interpreting a sample.
