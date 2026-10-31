> Historical checkpoint report. Its process/default/next-action statements apply to that run. Current state: [HANDOFF.md](HANDOFF.md); report index: [README.md](README.md).

# Performance check — 2026-10-23, Release164

The previous performance improvements are still installed. Comparing the
3.850 FPS stadium-introduction sample with the prior5.35–5.40 FPS stationary
pitch-selector samples was not a valid regression comparison.

## Configuration and identity

- Source SHA256 for nv2a_render_d3d11.c is
  E1206A483ADF71419745F7DDCE66BCF05BC0B30B88475CF0F301974608649C82,
  identical to the promoted161 renderer (same-height depth-toggle reuse).
- Source SHA256 for xbox_memory_layout.c is
  928097F4E9B9AF8837A845C092B155D35A365033A2AB277FB71E2705F52FA85D,
  identical to161. The rejected larger-height reuse was not reintroduced.
- Read-only inspection validates owned PID15096, loaded PE against disk,
  matching MAP timestamp and ASLR-adjusted symbols. Live fallback budget512,
  GPU_YIELD0, renderer timing0. Startup confirms hardware D3D11, packed
  readback and RAM scopes enabled; cutoff4096 is recorded by launch164.
- No tuning writes, priority changes, runtime edits, relink or restart were
  performed during this investigation. The game remains live.

## Measurements

| Sample | FPS | Resolves/frame | Unscoped/frame | Readback MB/frame | Upload MB/frame |
| --- | ---: | ---: | ---: | ---: | ---: |
| Prior160, stationary selector,512 | 5.400 | 63.90 | 23.91 | 20.41 | 16.06 |
| Current164 liveA | 4.649 | 63.51 | 24.61 | 20.43 | 16.18 |
| Current164 liveB | 5.150 | 62.54 | 23.28 | 19.87 | 16.03 |
| Current164 liveC | 4.750 | 62.60 | 23.28 | 19.94 | 16.11 |

Each interval is about20 seconds. Current captures show the post-first-pitch
view at1–0 without the pitch selector. The selector gate timed out; despite
the private filename perf164_selector_a.json, sampleA is NOT a verified
stationary-selector sample. The previous user input and different game state
mean these are not a controlled build-to-build comparison. They establish
the current range and very similar work/traffic per frame, not proof that
every performance difference has been explained.

All three current samples compile zero shaders and accept approximately
47,971–47,996 audio frames/s with zero submission rejects. Timing diagnostics
remain zero/off; the --timing reader requests existing counters and does not
enable runtime instrumentation.

## Why it is still slow

The strongest current evidence is frequent GPU/RAM synchronization, consistent
with the previous agent's findings. LiveB performs6,442 resolves in20 seconds,
reading back2.047 GB and uploading1.652 GB. About23 unscoped draws and16
external observers per frame force substantial synchronization. Copy volume
alone does not establish a bandwidth bottleneck; many small GPU waits are
costly even when total bytes/s are modest.

A separate100-round RIP profile identifies the renderer worker24536 at
GPU CPU-synchronization waits in22 samples and single-object waits in23;
other samples include packed_readback, vertex programs, texture sampling and
memory-copy code. These are instruction-pointer observations, not unwound
call stacks or exact frame-time percentages. The profile is separate from
the FPS sampling intervals.

The flag worker16848 consumes5,093.75 ms of CPU during that profile and often
samples inside NtDelayExecution from its Sleep(0) loop. This is a secondary
CPU cost already present in older profiles, not evidence of a newly introduced
regression or proof that changing its cadence is safe. It advances hardware
and timeout-visible state, so it should not be slowed indiscriminately.

The next targeted investigation remains attribution of the roughly16 external
resolves/frame to clear, fence, software draw and flip callers. Distinguish
discardable overwritten results from data the CPU/alias/fence/IRQ observers
must see. Do not skip RAM commits or reintroduce the rejected height-capacity
optimization. A true regression comparison needs the same stationary game
state, no intervening input and controlled before/after order.

Private evidence:perf164_settings.json, perf164_selector_a.json,
perf164_live_b.json, perf164_live_c.json, profile164_live.txt and the prior
fallback160_final512_short_b.json. Read-only source inspection confirms the
external flush sites in clear_surface, software draws, pattern/semaphore
publication, software methods and flip handling.
