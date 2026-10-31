# MVP 06 handoff — checkpoint 186

2026-10-31. Branch `claude/game-performance-audit-accuracy-8d9717`. One-page state of
the world; [../AGENTS.md](../AGENTS.md) has the working rules and debugging loop,
[ENGINEERING_LOG.md](ENGINEERING_LOG.md) the history.

## Current state

**In-game above 30 FPS on the merged build.** The two lineages that diverged from
the 2026-10-20 baseline are merged: the GPU-resident D3D11 renderer (branch
`claude/game-fps-accuracy-5eb48f`) and the checkpoint-185 runtime fixes (heap/TLS
race, critical-section waits, event-switch boundary, raster rules).

| Area | State |
|---|---|
| Frame rate | Pitch selector 38.6–39.5 FPS at 54% host load (run 258). 7-minute pitching soak (run 258): 32.0, 31.7, 27.6, 27.9, 39.7, 29.9 FPS, mean ~31.5, no crash. Under 77–100% host load the same scene drops to 23–31. Menus pace at exactly 60. |
| Rendering | Resident surfaces, filtering/mips, fog, 16-bit/swizzled targets, correct pitch-camera depth of field (dependent texture stages, new this checkpoint). |
| Memory | Contiguous frees are reclaimed (run184's 64 MB exhaustion fixed); heap/TIB allocation serialized. |
| Input | Keyboard → emulated USB pad. A pitch is three short A presses ~0.75 s apart. |
| Audio | 48 kHz output; the game software-mixes SFX/speech into one ring. Listening quality unverified. |
| Saves / full exhibition | Not verified; blocked by the between-pitch stall. |

Title defaults live in `mvp06/src/main.c` (`envs[]`): D3D11, GPU residency, every
batch on the GPU, packed readback, flip pacing, VBLANK, GIL, USB pad on port 2,
keyboard, DSP doorbells. RECOMP_GPU_SCOPES stays off. `RECOMP_CS_GUEST_FASTPATH=1`
(keep the lifted CPU across uncontended Enter/Leave) is opt-in: native tests pass in
all GIL × policy modes and run 257 played fine with it, but no matched comparison
separates its effect from host load.

Accuracy fixes after run 258 (from other EA ports): guest clocks at 1 ms from the
performance counter (`src/platform/host_clock.h`; KeTickCount on a dedicated thread,
drift-free kernel timers), APU interrupt delivered on vector 0x35
(`RECOMP_APU_IRQ=0` disables), and rounding-mode-aware `frndint`
(`recomp_frndint`; lifter emits it, the 22 local generated sites are patched).
Runs 260–261 pitch normally with them. `recomp_0015.c` also carries a local
`[ANIMMISS]` probe in `sub_00329D20` (generated-only; a re-lift removes it).

No game or helper process is left running. `mvp06/build/Release/mvp06_recomp.exe`
is build 261 (merge + contiguous + DOF + the fixes above). `mvp06/build/fast254_mvp06_recomp.exe/.map`
is the pre-merge resident build kept for comparison.

## Open issues (priority order)

1. **Between-pitch stall.** After every pitch the pitcher paces the mound ~64 s
   (pitch-to-pitch 70–79 s in soaks 256–261) before the selector returns; with a
   runner on first it never returns (reproduced live, run 259). Ruled out: dispatch
   misses, stubs, blocked threads, pad state and every button, disc I/O, the audio
   clock (48 kHz), x87 precision, failed animation lookups (probe, run 259), clock
   resolution, the DirectSound interrupt and floor/ceil rounding (all fixed, runs
   260–261, no change). Next: find which igflow wait command (`cWaitForActiveActors`,
   `cWaitForAnimation`, `WaitForSuitableBreakInTheAction`, …) the post-pitch flow is
   parked in; consider comparing with maddog847's MVP 2005 XBERecomp source package.
   Details in [GAMEPLAY_RENDER_AUDIT.md](GAMEPLAY_RENDER_AUDIT.md).
2. **Animation-lookup crash.** Run 256 faulted after pitch 6 reading 0xFFFFFFFF in
   `sub_00331FB0` at 0x331FF4: `sub_00329D20` (search up to four animation banks)
   returned −1 and the caller dereferenced it. The stall's main-thread sample is in
   the same subsystem. Working hypothesis: an animation ID the play needs is missing
   from the loaded banks. Next: log ID and bank pointers on every −1 return.
3. **Camera-cut flash.** Some cuts show one pale-blue full-screen frame (run 249).
4. **Preflight dispatch chunk.** `gen/recomp_9999.c` (0x5E440/0x56B60) and the
   hand-bumped table count (37764) should be replaced by a full disasm/lift cycle;
   re-verify the 0x3DD40 event switch afterwards (`recomp_0001.c` currently carries it).
5. Not yet checked: audio listening quality, saves, a nine-inning exhibition,
   batting/fielding FPS.

## Measuring and driving the game

Private helpers in the gitignored `mvp06/build/`, run with a Python that has Pillow
(`/c/Users/allen/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`;
plain `py -3` lacks it):
- `snap_run.py RUN [ENV=VAL…]`: launch, route the frontend to the pitch selector,
  save `snapRUN.png`, leave the game running.
- `soak.py PID RUN --minutes M --nudge 0`: pitch whenever the selector is detected;
  logs pitches and FPS every minute. The selector detector compares the HUD region
  with `pitch134_selector_before.bmp` and `pitchRUN_selector_before.bmp`; since the
  DOF fix the old reference scores ~9.3 (threshold 8), so copy a current selector
  capture to `pitchRUN_selector_before.bmp` before starting.
- `read_render_stats.py PID --seconds 20`: FPS and renderer totals.
- `test_pitch_loop.py PID RUN`, `watch_selector.py RUN SECS`, `thread_state89.py PID`.
Repo tools: `tools_sample.py --pid` and `tools_stackscan.py --pid` attach without
killing the game. Always record host CPU load next to an FPS sample.

Diagnostics (default off): RECOMP_EXEC_TIMING=1 (per-flip build/draw/trap/idle ms);
RECOMP_DRAW_LOG_FLIP=N or RECOMP_DRAW_LOG_TRIGGER=<file> (every batch of one flip,
with depth func, stage program and combiner words); RECOMP_VP_CHECK=1,
RECOMP_VP_BLOCK_CHECK=1, RECOMP_TEXTURE_DECODE_CHECK=1. The first 48 contiguous
allocations/frees are always logged as `[CONTIG]`.

## Native checks

```bash
cmake --build mvp06/build --config Release --parallel 8 --target heap_concurrency_smoke \
    critical_section_smoke memory_scope_smoke guest_scheduler_smoke nv2a_pb_smoke \
    nv2a_ack_smoke nv2a_render_smoke nv2a_pb_scan_smoke
```

All pass at checkpoint 186: heap fixture (now with the contiguous lifetime cycle) at
RECOMP_GPU_SCOPES=0/1; `critical_section_smoke` at GIL 0/1 × fastpath 0/1;
`nv2a_render_smoke` on WARP and `--hardware` (2607 draws, 0 failures, including the
dependent-read cases); memory scope, scheduler, pushbuffer, ack and scan smokes.
`nv2a_pb_smoke --gpu-layout` reports flips=271000, draws=270872 (unchanged).
