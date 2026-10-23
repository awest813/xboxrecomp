# Readback attribution and bounds optimization — 2026-10-23

The source now attributes external GPU resolves by executor caller and bounds
readback using triangles that can reach the viewport. Candidate166 is built
as build/Release/mvp06_recomp166.exe, with its own matching MAP. The default
executable and original live run164 PID15096 were not replaced or stopped.
The performance effect on a matched exhibition scene remains unverified.

## Attribution

Nv2aRenderObserver identifies clears, software draws, captures, pattern
progress markers, semaphore releases, software methods and flips. Each path
still performs its original mandatory flush. The backend records calls,
actual resolves and bytes; calls with no queued pixels are distinguished from
costly commits. Existing aggregate counters and stats layout remain intact.
No extra clocks, logging, waiting, yielding or coherence exceptions were added.
Counters follow the renderer's existing single-consumer model.

The separate observer-only build165 ran with GPU timing enabled. In one
20.002-second demo/gameplay interval (95 flips), external resolves were:

| Caller | Resolves | Share | Bytes |
| --- | ---: | ---: | ---: |
| Pattern progress marker | 1,054 | 58.3% | 277,015,320 |
| Software drawing | 570 | 31.5% | 547,683,760 |
| Clears | 184 | 10.2% | 37,846,060 |
| Semaphore / software method / flip / capture / other | 0 | 0% | 0 |

The sum1,808 matches the external-resolve aggregate. Software draws account
for the largest external byte volume, while pattern progress markers cause
the most external waits. Their ordering must remain: D3D observes consumed
progress before its fence. The zero additional semaphore/flip counts do not
mean those barriers can be removed; preceding observers already committed
the queued work in this interval.

Across all resolve reasons,165 timing reports8.074 seconds of flush work and
6.217 seconds in Map during that20-second interval. These are CPU wall-clock
diagnostics, not GPU timestamps. Nested draw/prepare/texture totals overlap.
The concurrently running164 session shares GPU resources, so these runs are
not clean FPS comparisons. Evidence:build/observer165_demo.json.

## Bounds repair

The old draw_bounds accumulated one rectangle over every vertex. Disconnected
triangles outside opposite viewport edges inflated the rectangle across
untouched visible RAM, even if neither triangle could reach a pixel. This
could also force a useless draw/readback for an entirely invisible batch.

The candidate validates every vertex as before, then excludes a triangle's
bounds only when they are wholly outside one clip edge. It keeps the existing
one-pixel raster-snapping margin, positive-W requirement and depth-width clip.
Triangles crossing the viewport remain included, even if their vertices lie
outside. It does not discard degenerates based on floating-point area, change
surface allocation, skip a RAM observer, or alter fence/interrupt publication.
Scope unions therefore retain all potentially modified pixels.

A direct original/candidate check uses the exact prior accepted renderer
snapshot (SHA256 E1206A483ADF71419745F7DDCE66BCF05BC0B30B88475CF0F301974608649C82).
For a tiny visible triangle plus disconnected offscreen triangles, the old
renderer reads1,024 bytes and fails the new bounds assertion. The candidate
reads144 bytes (6x6), an85.94% reduction in that fixture, with every pixel
checked. Scoped and synchronous cases pass. Entirely offscreen batches read
zero bytes, do not enlarge previously queued bounds, and still reject a NaN
vertex. This is a targeted correctness/performance result, not an in-game FPS
percentage.

## Validation and runtime status

- All four decoded renderer combinations pass: hardware/WARP x packed/legacy,
  2,605 accepted batches per run. The two deliberate unsupported/invalid input
  fallbacks are expected. Existing Z16/Z24, stencil-bit preservation, aliases,
  CPU edits, scope barriers and depth/HUD reuse regressions remain passing.
- Pushbuffer hardware and WARP observer tests pass; tagged counters identify
  the expected commit at each actual observer. Hardware and hybrid4096 suites
  also pass with the bounds candidate.
- The focused bounds check can be run with nv2a_render_smoke --bounds-only.
  Private control/candidate outputs are bounds166_control_result.txt and
  bounds166_candidate_result.txt in build/.
- Source/library and separately named game builds165/166 succeed. The four
  old memory-layout warnings reappear when165 rebuilds that file, matching
  the known C4324/C4456(x2)/C4702 sites. No new warning sites were observed.
- Separate165 and166 runs each reach live demo gameplay with2,652 PATH entries,
  zero logged crashes and zero direct-stub hits. Captures were visually
  inspected. These are partial attract/demo checks, not full exhibition or
  save/reload acceptance.166's demo shows field geometry, lighting, targets,
  score HUD and transitions. observer166_gameplay.json records its diagnostics.
- The165 diagnostic sample4.750 FPS and166 sample5.099 FPS are NOT a controlled
  speedup comparison: scene timing differs and164 remains concurrently live.
  Do not report a7.3% gameplay gain from them.
- Both separate diagnostic processes were deliberately terminated after
  captures:165 PID10652 and166 PID19196. Their retained exit records report
  0xFFFFFFFF from that controlled termination; these are not clean natural
  exits or crashes. Original164 PID15096 remains live on the original build.
- build165_166_hashes.json records executable/MAP/source identities. The
  default mvp06_recomp.exe still has build164 identity and its13:43:37 link.

Next run the candidate alone with the same teams/camera, all timing and trace
diagnostics off, and no input during matched20-second intervals. Complete
two-pitch/pause acceptance before promoting the candidate executable. Do not
relink or replace the live default executable. Further work should quantify
how often offscreen bounds inflated real batches, then reduce software-draw
interruptions or safely replace CPU clear/upload cycles while preserving all
RAM, alias, mask, depth, fence and interrupt visibility.
