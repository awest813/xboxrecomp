# Gameplay, rendering and accuracy audit

Checkpoint185,2026-10-31. Performance and resource stability come first. Steady30 FPS, a complete exhibition and original-console fidelity are not achieved. [HANDOFF.md](HANDOFF.md) is the current operational state; [the prior audit](archive/GAMEPLAY_RENDER_AUDIT_PRE185.md) preserves the detailed164 repair evidence.

## Retained local correctness work

- Software shared-edge coverage uses winding-normalized top-left rules.
- The complete0x3DD40 event switch was checked against original x86 across8,288 cases and its boundary is preserved by `tools_patch_boundaries.py`.
- D3D11 raw-combiner, texture, depth, stencil/alias, partial upload, scoped commit and packed-readback fixtures provide local pixel evidence. Earlier hardware/WARP suites pass2,605 draws per mode; these are historical results, not a new185 rerun.
- Exact block row comparisons pass614,784 oracle cases; their sparse-row speedup is not a whole-game FPS claim.
- Heap/TIB and real critical-section wait regressions now have native before/after evidence. This improves runtime foundations without proving gameplay accuracy.

## Current visible/runtime limits

Stadium, players, HUD, pitch meter and pause menu render, but recent captures still show coarse/pixelated player output and missing/black geometry during some transitions. No original-console frame comparison quantifies those errors. Run182 proves one pitch/count1–0, observed pause and resumed published output; the second pitch remains unverified. Run184 fails during exhibition loading after contiguous-arena exhaustion, so no184 selector or two-pitch acceptance exists.

The crash follows allocator exhaustion and a poison pointer, not the earlier event-switch epilogue or TLS signature. Do not repair a generated game branch to mask failed allocation. Preserve the failure evidence and fix ownership/lifetime at the correct layer.

## Audit after the performance gate

1. Capture fixed camera/input scenarios on original Xbox and the port at the same simulation points.
2. Compare geometry transforms, viewport/near clipping, culling, color masks and precision.
3. Cover Z16/Z24, stencil operations, overlapping targets, software/GPU transitions and all CPU aliases.
4. Check texture addressing/filtering, supported formats, mip behavior, fog, blending and effects.
5. Verify controller edges/analogs, pitch/bat/field timing, rules/state progression and pause behavior.
6. Compare audio timing/mixing and save/reload across full exhibitions and long sessions.

Do not change rendering semantics merely to increase FPS. Follow [SYSTEM_PERFORMANCE_AUDIT184.md](SYSTEM_PERFORMANCE_AUDIT184.md) for the coupled scheduling/readback bottleneck and [PLAYABILITY_PLAN.md](PLAYABILITY_PLAN.md) for completion gates.
