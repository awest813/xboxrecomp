> Historical checkpoint report. Its process/default/next-action statements apply to that run. Current state: [HANDOFF.md](HANDOFF.md); report index: [README.md](README.md).

# Matched gameplay test rejects direct packed buffers (2026-10-30)

The30FPS gameplay goal remains active and unachieved. Previous turn made
progress by implementing/testing an opt-in hypothesis; this turn completes
its actual gameplay comparison and removes it after contradictory results.

## Controls and native verification

174 adds a MAP-visible mode switch. Only packing output/view/staging resources
are recreated at readback boundaries; queued render targets, RAM shadows,
completion and observer ordering remain intact. A test-only control exercises
eight queued mode transitions with exact color/depth/stencil assertions and
actual direct-readback counters. Hardware/WARP packed/legacy each pass2,613
draws and zero failures. Private render174_*.txt retain results.

Old171 PID17020 was verified live with exactly seven frontend downs and then
deliberately stopped (exit171.json recordsFFFFFFFF).174 separately built and
launched PID24804, packed/scopes/4096, poll100, timing off, hold2500 during
frontend. Default Florida–Texas/home jersey/Varsity reaches the pitching
selector: two-capture gate frame107/error1.70. Hold then restored0 using a
matching PE/MAP guarded write. Normal169 executable remains unchanged.

## Same-process measurement

Each phase settles5s and measures20s, with no input. Seven exact frontend
downs remain unchanged throughout. Direct phases require actual direct
readbacks; all recorded direct deltas equal packed-readback deltas. Control
deltas are zero. No shader compiles and zero audio rejections in all8 phases.

| Order | Mode | FPS |
| --- | --- | ---: |
| ABBA1 | staging |3.250 |
| ABBA2 | direct |3.697 |
| ABBA3 | direct |3.850 |
| ABBA4 | staging |3.350 |
| BAAB1 | direct |3.300 |
| BAAB2 | staging |3.600 |
| BAAB3 | staging |3.247 |
| BAAB4 | direct |3.397 |

The first pass's apparent gain disappears in the reverse pass. Do not pool
these into a speedup claim. Work stays around65–66 resolves/flip and19–20MB
readback/flip. Evidence: build/direct174_abba/baab_*.json and their text logs.
The first helper labels the mode `bounds` in its summary only; it actually
writes/validates s_direct_packed_readback, as the transition and JSON files show.

## Result and current state

Reject and remove the runtime direct-buffer option, its mode switch/counter,
extra stats ABI field and mode-transition-only tests. Preserve a private source
snapshot in build/experiment174/ with all measurement evidence. Keep useful
native capability/throughput probes and the exact row-bounds optimization.
Restored backend rebuild175 succeeds and hardware/WARP packed tests pass.
No runtime or executable promotion. Do not relink live174 executable/MAP.

174 remains live in staging mode0, test hold0, poll100. Final guarded state
read verifies zero new direct readbacks. At audit3,412 PATH entries, exactly
seven frontend downs, no logged crash/stub/dispatch miss/lost sync/USB cancel.
171 had3,416 PATH entries at its audit; counts differ by resource routing,
and both reach the same checked selector. No full exhibition or two-pitch
acceptance is claimed for174. Current gameplay range3.25–3.85FPS is far below30.

## Larger change required

The next substantive investigation must reduce forced GPU/RAM round trips,
not just alter their copy destination. Keeping GPU-resident surfaces needs a
new, verified CPU-visibility mechanism, not skipped pattern/fence observers.
Relevant current code: graphics allocations use g_contig_mapping at0x80000000
and the tiled alias at0xF0000000. The28 low-RAM mirror views instead use
g_mapping_handle; do not conflate these backing stores.

A protection-based prototype would need all public aliases protected, a
private writeback view to commit before exposing public pages, serialized
renderer/context access from faulting readers, safe interaction with guest RAM
locks, and explicit barriers for host APIs that probe guest buffers in kernel
mode (which may return access errors rather than a user-mode fault). It must
also cover shutdown, overlapping targets/pages, software writes, texture
aliases and GPU-worker software-method waits. None of this is implemented or
validated yet. Existing scoped ownership/commit rules remain mandatory.
