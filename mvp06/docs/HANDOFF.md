# MVP 06 NCAA Baseball — Handoff

One-page state of the world for whoever picks this up (human or agent).
Companion documents: [ENGINEERING_LOG.md](ENGINEERING_LOG.md) (full history),
[../AGENTS.md](../AGENTS.md) (working instructions), [PLAYABILITY_PLAN.md](PLAYABILITY_PLAN.md)
(audited plan to playable). Repo root README covers build steps.

## What this is

A native Windows x64 port of MVP 06 NCAA Baseball (Xbox, 2006) built by static
recompilation with the xboxrecomp toolkit (upstream v0.11.0 + this project's
fixes, all in this repo). Game project: `mvp06/`. Everything runs from the
recompiled game code — no hand-written game logic.

## Current state (2026-10-23, branch `claude/game-fps-accuracy-5eb48f`, commit 8bfcdff)

**Playable in parts, at a usable frame rate.** Boot, both intro movies, the full
frontend, team/jersey/difficulty selection, the stadium and live pitching all work.
Pitches resolve as balls, strikes, walks and hits with fielding. Pause, replay and
box score work. No dispatch misses, unlifted stubs or crashes in current runs.

| Area | State |
|---|---|
| Frame rate | Pitch selector 30–37 FPS (gameplay target up to 60); menus paced at exactly 60. 20-minute soak stable at 26–36 FPS (run247). |
| Rendering | D3D11 with GPU-resident surfaces. Filtering/mips, fog, 16-bit and swizzled targets (player shadows), DOF chain all on the GPU. |
| Input | Keyboard → emulated USB pad works. A pitch is three short A presses ~0.75 s apart (select, start meter, stop meter). |
| Audio | APU delivers 48 kHz (APUOUTPUT). Listening quality not verified. |
| Saves | Not verified. |
| Full exhibition | Not reached: blocked by the between-pitch stall below. |

Title defaults live in `mvp06/src/main.c` (`envs[]`): the D3D11 renderer, GPU
residency, flip pacing, VBLANK, GIL, USB pad on port 2 and the keyboard. Leave
RECOMP_GPU_SCOPES off: with residency it only serializes the GPU worker.

## Open issues (in priority order)

1. **Between-pitch stall (playability blocker).** After every pitch the pitcher
   wanders around the mound for ~25–60 s before the selector returns (a few seconds
   on hardware). With a runner on first it can never return (run 249: >10 min).
   Reproduced in runs 249 and 250. Input still works: pause, replay and box score
   respond, but no gameplay button advances it. Ruled out: dispatch misses and
   unlifted stubs (RECOMP_STUB_TRACE=1), crashes, blocked guest threads, stuck pad
   state, slow guest clocks. Leads, untested:
   (a) A movement or distance test that never converges. The lifter keeps x87
   values as doubles and ignores the precision-control word.
   (b) A wait on DSound speech/stream completion that ends only on a timeout.
   Next: RAM-diff the pitcher/runner objects across the wait, or native-x86-compare
   the main-thread chain (sub_0012B960 → … → sub_0032E100; see history, runs 249–252).
2. **Pitch-camera blur.** The whole pitch view is blurred, foreground pitcher
   included; the replay camera is sharp. The DOF composite blends the scene with a
   160×120 blur using the depth buffer read as LIN_A8R8G8B8, drawn as a full-screen
   quad at screen z≈201 with depth test on (drawlog `mvp06/build/drawlog253.txt`).
   Check that draw's combiner/depth func and our depth-as-colour bytes. RECOMP_FRAME_TRACE
   does not reach hardware-rendered batches, so extend the draw log instead.
3. **Camera cuts** sometimes flash a full-screen pale-blue frame (run 249, frames
   256–257 and 308). Not yet investigated.
4. **Preflight dispatch chunk.** `gen/recomp_9999.c` holds 0x5E440/0x56B60 and the
   dispatch-table count was bumped by hand to 37764. Run the full disasm/lift cycle
   to replace it. First copy `tools/abi_analysis/output` and `tools/func_id/output`
   from the main checkout (they are missing in this worktree).
5. Not yet checked: audio listening quality, saves, a full nine-inning exhibition.

## Measuring and driving the game

Private helpers in the gitignored `mvp06/build/` (run them with a Python that has
Pillow; plain `py -3` lacks it):
- `snap_run.py RUN [ENV=VAL…]`: launch, route the frontend to the pitch selector,
  save `snapRUN.png`, leave the game running.
- `soak.py PID RUN --minutes M [--nudge S] [--stop-when-stuck S]`: pitch repeatedly
  using the three-tap sequence; logs pitches and FPS every minute.
- `read_render_stats.py PID --seconds 20`: FPS and renderer totals.
- `watch_selector.py RUN SECS`: log when the pitch selector appears or disappears.
- `probe_keys.py PID RUN name=0xVK,…`: press keys and save after-captures.
- `test_pitch_loop.py PID RUN`: two-pitch/pause acceptance.
- `thread_state89.py PID`: guest thread registers, IRQL, GIL.

Repo tools: `tools_sample.py --pid` and `tools_stackscan.py --pid` attach without
killing the game. `tools_threadtimes.py` spawns a new instance.

Diagnostics (all default off): RECOMP_EXEC_TIMING=1 prints per-flip ms for build,
draw, trap, stall and idle. RECOMP_DRAW_LOG_FLIP=N, or RECOMP_DRAW_LOG_TRIGGER=<file>
(the flip after the file appears), logs every batch of one flip. Correctness
checks: RECOMP_VP_CHECK=1 and RECOMP_VP_BLOCK_CHECK=1 (vertex programs vs the
reference interpreter); RECOMP_TEXTURE_DECODE_CHECK=1. Comparison switches:
RECOMP_TEXTURE_FILTER=0, RECOMP_TEXTURE_WATCH=0, RECOMP_VP_BLOCK=0,
RECOMP_FLIP_PACING=0, RECOMP_ACK_BACKOFF=0.

## History (newest first)

Earlier checkpoints (runs 88–162, the software/scoped-readback renderer era) are
archived verbatim in [archive/HANDOFF_checkpoints_2026-10-11_to_2026-10-20.md](archive/HANDOFF_checkpoints_2026-10-11_to_2026-10-20.md).

### Update (2026-10-21, runs 253–254): camera blur investigation (in progress)

- The pitch camera blurs the whole scene, foreground pitcher included; the replay
  camera is sharp. Run 253 draw log (new RECOMP_DRAW_LOG_TRIGGER=<file>: logs the flip
  after the file appears), saved as build/drawlog253.txt: scene 82C70000 → 4-tap
  downsample to 82F55000 (336x240) → 82FF3000 (160x120) → a full-screen composite onto
  82C70000 with t0=scene, t1=blur, t2=depth 82D9C000 sampled as LIN_A8R8G8B8 (point).
  The quad is at screen z≈201 with depth test on and depth write off. Next: dump that
  draw's combiner and depth func, and check our depth-as-colour bytes and the quad's
  depth test against the scene depth (~16.4–16.6 M of 2^24).
- RECOMP_FRAME_TRACE (run 254) produced nothing: trace_current_batch is apparently
  not reached for hardware-rendered batches. Hook it into render_batch, or extend the
  draw log with the combiner/depth-func state.

### Update (2026-10-21, runs 249–252): pitching loop works; between-pitch stall found

- **Pitching works from a script.** One long A press only *selects* the pitch; the
  throw needs the meter presses too. Three short A presses ~0.75 s apart (soak.py
  `pitch_sequence`) select, start and stop the meter. Runs 249/250 each threw 5 pitches
  automatically: balls, a walk, a hit to right with the fielding/relay playing out,
  a run scoring, a new batter, and pause/replay/box score menus all working.
- **Real playability bug (reproducible, runs 249 and 250):** after each pitch the
  game sits with the pitcher wandering around the mound before the selector returns.
  The gap varies: ~25 s, ~60 s, and after some pitches with a **runner on first it
  never returns** (>10 min, run 249). On hardware it is a few seconds. During the wait
  the screen shows the pitch name/speed ("81 MPH 4-Seam Fastball") with the aim
  reticle and the base-runner window. Nothing advances it: A/B/X/Y, D-pad, triggers.
  Start/pause, replay and box score do respond.
  Ruled out so far: dispatch misses and unlifted stubs (run 250 with
  RECOMP_STUB_TRACE=1: none); crashes; blocked guest threads (thread_state89.py: all
  helpers in normal waits, main thread running game logic); stuck pad triggers (pad
  reports all zero at rest); the guest clocks (RDTSC/KeQueryPerformanceCounter are
  real-time scaled). Guest call chain of the main thread during the wait:
  sub_0012B960 → 00127CF0 → 00127A80 → 002F0FC0 → 002F0ED0 → 00175570 → 0020F6E0 →
  002052A0 → 00334BB0 → 00331AE0 → 0032E100 (one sample).
  Leads, untested: (a) the pitcher's return-to-rubber movement never meets its
  distance test. The lifter keeps x87 values as doubles and ignores the precision
  control word, so a title that sets 24-bit precision gets different rounding.
  (b) A wait on a DSound speech/stream completion that only ends on a timeout.
  The next step is a RAM diff of the pitcher object across the wait, or comparing
  the lifted functions above against native x86.
- **Perf: block vertex-program interpreter.** `nv2a_vp_run_block` runs the
  pre-decoded program over 8 vertices at once (structure-of-arrays registers, the same
  float operations in the same order). The pool and small batches use it, and inputs
  the program never reads are not fetched. 191 M vertices compared bit-identical
  (RECOMP_VP_BLOCK_CHECK=1, run 250). Build time drops from 10.8–12.2 to 9.4–10.1
  ms/flip (run 251 vs 252, RECOMP_VP_BLOCK=0). The selector FPS samples (31–37 vs
  24–28) are confounded by flip stalls in run 252, so no FPS gain is claimed.

### Update (2026-10-21, runs 239–248): ~30–36 FPS, shadows, fog, 20-min soak

- **Speed**: pitch selector 33–37 FPS (run244–246), and over a 20-minute auto-play
  soak (run247) **26–36 FPS (mean ~30), no crash or hang, no downward drift**. The GPU
  command worker is the bottleneck (idle <1 ms/flip, RECOMP_EXEC_TIMING shows `idle=`).
  The guest thread's "busy" time is mostly DSound/KeStall spin-waits. Native rate:
  menus pace at exactly 60, so the gameplay target is up to 60 FPS (the earlier "30"
  inference was wrong).
- Changes: GPU clears for depth-only and partial-rect targets (`sync_target`, Z rect
  drawn with SV_Depth), which removed ~24 K CPU-clear readbacks; NV2A flag worker backs
  off to 250 µs high-resolution waits when idle (RECOMP_ACK_BACKOFF=0 keeps the spin;
  freed a full core); stderr progress markers throttled; VP pool wakes only parked
  workers and has a configurable threshold (RECOMP_VP_POOL_MIN).
- **Accuracy**: 16-bit/swizzled render targets on the GPU (CONVERTED surfaces in
  nv2a_render_d3d11.c). MVP's 256×256 swizzled R5G6B5 **player-shadow map** used to be
  rasterized pitch-linear in software, so the texture read was scrambled. **Player
  shadows now render.** Also: NV2A fog (FOG_MODE/PARAMS/COLOR, per vertex from oFog.x,
  formula per xemu/JSRF); CPU clears honour the per-channel colour mask.
- Dispatch: preflight `gen/recomp_9999.c` adds 0x5E440 and 0x56B60 (entries of the
  runtime-filled method table at 0x6C3AF8); dispatch table count bumped to 37764 by
  hand. Seeds added to config/seed_functions.json; **run the full disasm/lift cycle**
  to replace the preflight, after copying tools/abi_analysis/output and
  tools/func_id/output from the main checkout (missing in this worktree).
- **Open playability issue**: after a walk the game sits in the pitch-aim phase
  (4-Seam reticle shown, runner on first) and no scripted key (A/B/X/Y, D-pad,
  triggers, black/white) advances it. The scripts can't drive MVP's pitch meter
  (pitch → aim → meter presses), so this may be input semantics, not emulation; no
  dispatch misses, crashes or blocked guest threads. Needs a human at the controller
  or a meter-aware script (soak.py / probe_keys.py exist).
- Audio: APUPROGRESS "frames/s" (~1000–1150) is not the delivered rate; APUOUTPUT
  shows 96000 frames per 2 s window, i.e. 48 kHz. Listening quality is unverified.

### Update (2026-10-21, runs 227–238): native pacing, 28.7 FPS

- **Flip pacing** (RECOMP_FLIP_PACING=1, title default): FLIP_STALL now waits for the
  title's vblank handler's PGRAPH_INCREMENT (0x40071C, READ_3D) writes, as on hardware
  (100 ms safety timeout). Menus went from ~120 FPS to exactly 60. **Gameplay's native rate
  is 30 FPS** (the handler presents every other vblank).
- Selector now **28.7 FPS** (run238), with a 3.8–4.5 ms/flip stall wait, i.e. close to
  the 30 cap. Two-pitch/pause acceptance PASS, captures clean.
- What helped: a fast swizzled 16/32-bit decode (62 M texels checked equal to the scalar
  sampler); a pre-decoded vertex-program interpreter (28 M vertices bit-identical,
  RECOMP_VP_CHECK=1); texture write-watching (RECOMP_TEXTURE_WATCH=0 disables);
  skipping unchanged stencil clears; a surface-only fast path in nv2a_render_memory_read.
  That last one is critical: the watch envelope made every vertex fetch take the lock.
- A VP thread pool (RECOMP_VP_THREADS) exists but covers only ~12 k verts/flip. Most
  vertices are in batches of fewer than 128, so its gain is marginal.
- Next: ~2–3 ms more headroom (memcpy in vertex assembly; software 16-bit render
  targets; batching the VP pool across draws), then a long soak test (repeated
  pitches, a full inning) for stability.

### State at 2026-10-20 (runs 201–226)

**Pitch selector 4.3 → 23 FPS, two-pitch/count 2–0/pause/resume PASS** (run226,
`test_pitch_loop.py`), captures visually checked against the baseline. Work is
in the `claude/game-fps-accuracy-5eb48f` worktree, uncommitted; the main
checkout's uncommitted state it started from is ref `refs/baseline/main-import`
(`git diff refs/baseline/main-import` shows only this session's changes).

The fast path is now the title default (`mvp06/src/main.c`): RECOMP_RENDERER=d3d11,
RECOMP_GPU_RESIDENT=1, RECOMP_GPU_MIN_PIXELS=0, RECOMP_PACKED_READBACK=1,
RECOMP_VBLANK=1, RECOMP_GIL=1. Leave RECOMP_GPU_SCOPES off: with residency
the exclusive RAM scope only serializes the GPU worker against the guest.

What changed, by measured gain (selector FPS):
| Change | FPS |
|---|---|
| baseline (scopes, synchronous per-batch readback) | 4.3 |
| GPU-resident surfaces + page-protection materialize, scopes off | 7.25 |
| surface cache (12 parked sets), direct render-target sampling, apply() skip | 8.85 |
| bound-target snapshot, sub-rect views, no fail-path materialize | 11.65 |
| GPU present ring, height-agnostic keys, depth transfer between sets | 12.1 |
| faster protection pass | 12.5 |
| **timer thread woken by NV2A traps / DPC inserts** (was a 15.6 ms Sleep tick) | 16.95 |
| depth sampled as ARGB built on GPU (compute pass) | 18.45 |
| **vertex ring buffer** (WRITE_DISCARD per draw stalled the driver) | 23.5 |
| texture filtering + mipmaps (accuracy; ~2 ms) | 23.15 |

**Coherence model (read before touching the renderer).** Committed pixels stay
on the GPU. Each tracked surface's pages are PAGE_NOACCESS while the GPU copy
is newer and PAGE_READONLY while RAM, shadow and GPU agree, in the contiguous
window AND its tiled alias. Any access — lifted code, presenter, software
raster, DMA — faults into `resident_fault` (renderer VEH, runs under the
renderer lock), which materializes (reads back the dirty box); a write also
untracks so the next draw compares/uploads. RAM is never silently stale: an
observer always gets current pixels, the commit just happens on demand.
Host-kernel I/O cannot fault, so NtReadFile/NtWriteFile call
`xbox_MemoryHostAccess` first. Invariant: a page is tracked by at most one
surface set (`claim`). GPU-side clears (`nv2a_render_clear`) and the depth-as-
texture pass preserve RAM-owned stencil. See the comment blocks in
`src/kernel/nv2a_render_d3d11.c`.

Accuracy: the renderer point-sampled everything. It now honours
SET_TEXTURE_FILTER (BOX/TENT min/mag, NEARESTLOD/TENT_LOD mips, LOD bias) and
mirror/border address modes, with full mip chains uploaded for swizzled/DXT
textures. MVP 06's depth-of-field chain (back buffer → 336×240 4-tap → 160×120
half-texel taps → depth composite) is built for bilinear filtering; with point
sampling the scene was sharper than the game intends. RECOMP_TEXTURE_FILTER=0
restores point sampling for comparison. No xemu/hardware capture was compared.

Diagnostics added (all default off): RECOMP_EXEC_TIMING=1 (per-flip build/draw/
trap ms), RECOMP_DRAW_LOG_FLIP=N (every batch of flip N), RECOMP_TEXTURE_POINT_FORMAT,
`[D3D11] materialize by reason / fault sites / switched-to targets / texture
materializes / draw failures` every 20,000 draws in resident mode.

Remaining per-frame cost (~43 ms, GPU command worker is the critical path):
renderer ~16 ms (texture memcmp of every bound texture ~4 ms; one 256×256
swizzled R5G6B5 decode ~5 ms via the scalar sampler), CPU vertex programs and
batch build ~14 ms, parsing ~13 ms. 16-bit render targets still fall back to
software (~4 draws/flip). Next candidates: page-protect texture sources instead
of memcmp, a fast swizzled-16-bit decoder, GPU R5G6B5 targets, vertex programs
on the GPU. Open: full exhibition, saves, audio listening quality.

## How to build and run

Incremental (runtime/main/recomp_manual changes — minutes):
```bash
cmake --build mvp06/build --config Release
cd mvp06 && RECOMP_VBLANK=1 timeout 180 ./build/Release/mvp06_recomp.exe 2> run.txt
```

Full pipeline (after seed/lifter/disasm changes — ~55 min):
```bash
py -3 -m tools.disasm mvp06/game/default.xbe --seed-functions mvp06/config/seed_functions.json --analysis-json mvp06/game/mvp06_analysis.json
py -3 mvp06/tools_patch_boundaries.py
py -3 -m tools.recomp mvp06/game/default.xbe --all --split 1000 --gen-dir mvp06/src/recomp/gen
py -3 mvp06/tools_patch_d3d.py
cmake -S mvp06 -B mvp06/build
cmake --build mvp06/build --config Release
```

**Boundary patches — re-apply after every `tools.disasm` re-run** (it rewrites
`functions.json` from scratch and loses them). Until this is fixed upstream
in the disassembler (follow jump-table targets for function extents) or the
`--function-range` flag is ported (see Mercenaries' Recompile.ps1), run
`py -3 mvp06/tools_patch_boundaries.py` from the repo root. It applies:
- `sub_00442CF0` (CRT memcpy): exclusive end → `0x0044302D` (size 829), and
  delete any function entries with start in (0x442CF0, 0x44302D).
  The old 0x443028 boundary cuts off the last arm's epilogue; its `ret` is
  at 0x44302C, immediately before the next function at 0x44302D.
- `0x00094B90..0x00094C80` (missed SEH method): add as a function entry
  (start '0x00094B90', end '0x00094C80', section '.text').
- `0x0007F040..0x0007F077` (frontend constant-return switch): preserve all
  five arms of its 28-entry table. Do not seed internal arm 0x7F052.
- `0x0007AE80..0x0007AECB` (player bitfield switch): preserve all four arms
  and ret 4. Do not seed internal arm 0x7AE92.
- `0x00256590..0x00256F4E` (pitching callback): preserve all five returns;
  conditional alias 0x256C6F is internal.
- `0x00032710..0x00032885` (post-pitch callback): preserve both ret 4 paths.
- `0x001D8240..0x001D8500` (post-pitch callback): include the final return
  immediately before the next function.
- Seeds in `mvp06/config/seed_functions.json` must NOT contain entries
  inside these switch ranges (they cap CFG recovery as stop-addresses).

main.c sets these defaults unless the environment already has them:
RECOMP_PB_EXEC=1, RECOMP_VBLANK=1, RECOMP_GIL=1, RECOMP_RENDERER=d3d11,
RECOMP_GPU_RESIDENT=1, RECOMP_GPU_MIN_PIXELS=0, RECOMP_PACKED_READBACK=1,
RECOMP_FLIP_PACING=1, RECOMP_AC97_READY=1, RECOMP_USB=1, RECOMP_USB_PADS=2,
RECOMP_USB_PORT=2, RECOMP_FB_WINDOW=1, RECOMP_KEYBOARD=1,
RECOMP_APU_DSP_ACK=0x82F20810,0x82F28810.
Window keys: Enter=Start, Z=A, X=B, A=X, S=Y, arrows=D-pad. Input reports:
`RECOMP_INPUT_TRACE=1`; APU cursor/output trace: `RECOMP_APU_TRACE=1`.
Left stick: numpad4/6/2/8; right stick: J/L/K/I. MVP 06 pitching uses the
on-screen buttons/meter; the right stick handles Load and Fire batting and
fielding throws. Pitching: three short Z (A) presses ~0.75 s apart select the
pitch, start the meter and stop it; one long press only selects (run 249).

## Where things are

| Thing | Path |
|---|---|
| Engineering log / this file / plan | `mvp06/docs/` |
| Archived checkpoint notes (runs 52–162) | `mvp06/docs/archive/` |
| Private launch/route/measure helpers (gitignored) | `mvp06/build/*.py` (see "Measuring and driving the game") |
| Game project (CMake, main.c, recomp_manual.c, seeds, tools) | `mvp06/` |
| Extracted disc + XBE + analysis JSON (gitignored) | `mvp06/game/` |
| Runtime debug tools | `mvp06/tools_sample.py`, `tools_stackscan.py`, `tools_threadtimes.py`, `tools_watchpoint.py` |
| Toolkit runtime (with our fixes) | `src/kernel/`, `src/apu/`, `src/usb/` + `tools/recomp/{lifter,translator}.py` |
| Reference ports (cloned, gitignored; study before inventing) | `reference/` — nfsu2-sw (EA/XDK5849, GIL, modern VK/GL and legacy D3D11 paths), burnout3 docs in upstream README links, doaxvb-re, doa3-re, mercenaries-re, ssx-tricky-pc, jsrf-recomp |
| Original ISO (gitignored) | `MVP 06 NCAA Baseball (USA)/` |

## Our changes vs upstream xboxrecomp v0.11.0 (all upstream-candidates)

Runtime (`src/`):
1. `bridge_ke_object()` — kernel dispatcher-object shadowing (`kernel_bridge.c`).
2. Contig low-64 KB reserve (`XBOX_CONTIG_LOW_RESERVE`, `xbox_memory_layout.c`).
3. GP/EP DSP memory untrap above the voice-register block (same file).
4. Rate-limited `[KEWAIT]/[KEMWAIT]/[KEOBJ]` debug logging (trim before PR).
5. Keyboard controller fallback with focus-loss release and physical-pad merge.
6. OHCI physical queue/static reply-buffer translation; optional report trace.
7. YUY2/UYVY sampling, correct NV097 primitive assembly and common UI alpha fade.
8. APU physical DMA translation and Windows hardware-mix/sample-cadence repair.
9. Repeated framebuffer-window captures; native smoke tests under `tests/`.
10. Raw D3D11 combiner input/output/final/channel decoding, physical DMA
    integration, coherent resident targets, partial CPU uploads and packed
    readback, verified by AMD/WARP pixels against the portable evaluator.
11. Independent guest RAM exclusion (`platform/guest_memory.*`) across CPU,
    passive bridges and native DMA, actual wait release/resumption, bounded
    optional renderer scopes and command/vertex/fence/flip coherence barriers.
    Default off; rejected synchronous-retry experiments reverted in126.

10a. D3D11 renderer with GPU-resident surfaces (`nv2a_render_d3d11.c`): page-
    protection materialize (VEH) for every RAM observer, 12-set surface cache,
    render targets and depth sampled directly, GPU clears, present ring, vertex
    ring buffer, 16-bit/swizzled targets, filtering/mips/LOD bias, fog. Host I/O
    hook `xbox_MemoryHostAccess` (`platform/guest_memory.*`, `kernel_file.c`).
10b. Flip pacing on PGRAPH_INCREMENT; kernel timer thread woken on NV2A traps
    and DPC inserts; NV2A flag-worker idle backoff (`nv2a_pb_exec.c`,
    `nv2a_ack.c`, `kernel_bridge.c`).
10c. Vertex programs: pre-decoded and 8-lane block interpreters with reference
    checks, worker pool (`nv2a_vertex_program.c`, `nv2a_pb_exec.c`).

Toolchain (`tools/recomp/`):
12. `_flags` declaration for rep-prefixed cmps/scas (`translator.py`).
13. `_analyze_switch_table` fallback: complete overlapping/unused-first-dword
   tables (base+4), and downward tables including index zero (`lifter.py`).
14. **Converged switch-resync decode** — iterate leader collection + resync
   until stable (`translator.py decode_function`); fixes silently-missing
   switch arms when tables stop the linear decode early.
15. Resync removes stale instruction starts covered by recovered instructions
   (`disasm.py`); otherwise bogus operations remain between real ones.

Title-side (`mvp06/`): env-first boot, OHCI VEH routing, pushbuffer fence
`xbox_Nv2aMirrorFence(0x589A38, 0x2C, 0x30)`, DSP doorbells, saves/ dir, VEH
diagnostics (BUCKETS/PHYS0 dumps, DR-watchpoint reporting).

## Gotchas learned the hard way

See [../AGENTS.md](../AGENTS.md) "Conventions and traps" — the short list:
x64 CONTEXT Rip=0xF8 (0xF0 is R15); never write C via bash heredocs; direct
calls bypass `recomp_lookup_manual`; `.map` offsets are host offsets; guest
registers are TLS globals; piped stderr speeds the game up (watchpoint timing);
a leftover game process locks the exe (LNK1104).
