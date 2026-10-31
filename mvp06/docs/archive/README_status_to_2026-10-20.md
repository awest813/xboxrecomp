# Archived README status section (to 2026-10-20)

Moved verbatim out of the root README on 2026-10-23.

---

## Status (2026-10-20 — bring-up in progress; work paused)

**Both intro movies, the full Play Now frontend, stadium rendering, pitching
and pause/resume are verified.** Normal run160 on Release159 with the selected
512-word fallback confirms two
pitches/count2–0 and pause/resume, with all ten key-down inputs matching the
test helper exactly. Its **4-hour 47-minute run / 3,763 path attempts** has zero
dispatch misses, direct-stub hits, crashes, USB cancellations or parser resyncs;
8,353 captures have a maximum gap of3.1 seconds. A full exhibition remains unverified.

Compatible depth allocations now survive
same-height HUD draws; native transition cost falls from7.85–10.77 to2.53–3.02 ms,
with pixel/coherence regressions passing. Normal selector samples4.45–4.65 FPS
show no established FPS gain yet. A larger-height reuse experiment regressed
normal performance and was reverted. A controlled same-process reverse test
of synchronous fallback slices improves warm pitching-selector FPS from
**5.15 to5.38 (+4.4%)**. Release161 now defaults to512 words for that fallback;
exclusive GPU/RAM/interrupt barriers remain unchanged. Native publication tests
pass. Its first runtime test captured the intro but ended with no available
exit reason before frontend validation; repeat acceptance is the next step.
No game or test helper remains running at the requested stopping point.

Latest controlled warm pitching-selector samples are **5.35–5.40 FPS**, still
too slow for comfortable play; the measured gain applies to this selector test.
Exact linear ARGB bulk decoding reduces measured texture
decode work from1.31–1.38 to0.545–0.555 seconds per20-second sample, preserving
pitched rows, alpha, CPU edits and aliases.625,422 native scalar comparisons
and AMD/WARP coherence checks pass. Earlier samples included a diagnostic file check on
every draw; the test launcher now disables it unless frame tracing is explicit.
This corrects measurement overhead without claiming a runtime speedup. A larger
GPU word budget reduced resolves but gave no established benefit and is
reverted. Optional timing counters attribute roughly half of readbacks to
unscoped draws; recent diagnostic samples spend about ten seconds mapping
readbacks per20 seconds, alongside texture and surface work. A same-process
GPU-yield experiment measures3.09 FPS off versus3.01 on with overlapping
ranges; experimental yielding remains off. Native RAM/publication/raised-IRQL
checks pass. Exclusive GPU slices stay at4,096 words, synchronous failed-exclusion
fallback slices at512, and the software cutoff at4,096 pixels.
Execution now enables verbose NV2A diagnostics only when explicitly requested;
build138's rebuilt default matches its previous FPS baseline. No gain is claimed
for that logging cleanup.

The accepted runtime fixes a traced parser stall by resetting its runaway
budget for each new submission, and keeps zero-valued GPU NOPs inside batches.
Nonzero software interrupts still observe committed pixels. Failing-before/
passing-after native regressions cover both repairs; AMD/WARP tests retain
color, depth, alias and CPU-observer coherence checks. Grouped execution stays
opt-in through `RECOMP_GPU_SCOPES=1`, with RAM exclusion covering lifted code,
kernel bridges and DMA. Synchronous execution remains the default.

The game's Windows audio session sends a nonzero signal; run160's session was
muted, so listening quality remains unverified.
An XAudio2 queue repair increases headroom from16 to64 ms while preserving
mixed samples. A failing-before/passing-after native test verifies FIFO data,
buffer ownership, wrap and backpressure. Run160 records **zero rejected audio
blocks out of3,231,807**, versus roughly4.6–4.8% before. Timing counters
confirm that short-stall deadline resets discarded1.01 seconds in a20-second
pitching sample, with45.6k processed frames/s versus48k nominal. Release148
adapts NFSU2's device-clock queue pacing: eight256-frame blocks within twelve
storage slots. Native catch-up/backpressure/pause and sample-ownership checks
pass. Intro playback is48k frames/s; a stadium sample remains46.1k with14
empty-queue observations, despite zero resets or rejected submissions.
Normal run152 accepts48,020/47,996 frames/s in its selector samples.
The retained Release151 repair also fixes mixed active/off audio blocks: prepare each
32-sample slice so a late stop preserves earlier samples and an earlier stop
cannot replay stale data. A failing-before/passing-after native regression
checks all256 patterns/131,072 PCM channel comparisons. Diagnostic intro151
exercises the repaired path. Normal run152 completes controlled acceptance;
its pitch/count/pause/resume captures were also visually inspected.
Continuity and audible quality remain open; the stage samples are not a
controlled performance comparison. Next investigate readback synchronization
and remaining audio stalls, then full exhibition and save/reload acceptance.
Iteration details, rejected
experiments and acceptance captures are recorded in
[ENGINEERING_LOG.md](mvp06/docs/ENGINEERING_LOG.md).

The requested full disasm/lift/build cycle for `0x00432FBB` and
`0x0049E807` is complete, with memcpy's exclusive end restored to
`0x0044302D`. Full cycle110 consolidates 33 seeds: **37,762 functions
lifted, zero failures**, seven protected ranges and a successful Release build.
Its 1,231.9-second movie/frontend/demo run is clean. The newly recovered
attract-exit destructor passes 10,000 native original-x86 comparisons.
The two newly reached post-pitch callbacks pass 17,280 original-x86 comparisons
with controlled callees.

The runtime now handles the reached vertex programs, four texture stages,
register combiners, P8 fonts, video sampling and pitched integer depth.
Reached signed-short and packed-normal vertex inputs are repaired. Build99
also fixes a native-reproduced concurrent handle-table defect; run99 passes
the earlier loading stall. That does not establish the cause of every park.
Build100 adds native-tested packed inline-vertex alignment and passes both
movies, the full frontend and stadium with a clean 557-second run. Build101
also repairs raw-word decoding in the existing D3D11 combiner translator;
320 rendered WARP draws pass comparison with the software evaluator.
Its 252-second movie/frame smoke run is clean. Build102 adds an opt-in decoded
D3D11 backend (`RECOMP_RENDERER=d3d11`), preserving P8/video, four texture
coordinates, combiners and coherent guest surfaces. Both WARP and AMD pass
1,952 native rendered draws each after coherence/cache tests. Run103 renders
both movies, the full frontend, stadium and players, then completes a pitch
and changes the count to 1–0 on hardware, with a clean 1,140.7-second run.
Build104 adds measured small-batch CPU/GPU selection and a larger bounded
texture cache, reaching the pitching controls in a clean 582-second run.
Build105 adds partial uploads of CPU-edited surfaces; both movies, the full
frontend, stadium and an 83 MPH pitch/count change pass in a clean 1,353-second
run. Build106 removes repeated environment scans from command dispatch;
movies/full frontend/stadium/pitching controls pass at 2.2 FPS.
Build108 separates flush acknowledgments from rendering and consumes real
pattern/semaphore progress, exposing D3D's ignored NOP(5) event wait at the
title. Build109 delivers PGRAPH software-method interrupts through the
original guest ISR/DPC; movies, the full frontend, pitching and pause/resume
pass. Build111 addresses measured duplicate flip traps and dropped vblanks,
using the corresponding NFSU2 driver handoff as a reference. Native failures
reproduce before the repairs and pass afterward. The original driver's live
vblank rate improves from 1.8 to 59.8 Hz, with flip retirement advancing;
both movies, the full frontend, bright stadium and players still render with
a clean dispatch log. Rendered FPS remains separate from this driver clock.
Build114 repairs partial CPU depth uploads around GPU-written regions; a
failing-before native pixel test now passes Z16/Z24 on AMD/WARP, packed and
legacy paths (2,398 decoded draws each). Opt-in packed color/depth readback
(`RECOMP_PACKED_READBACK=1`) passes movies/full frontend/stadium and two pitches
with pause/resume. Its corrected matched pitching sample is **2.5 FPS**;
no broad speedup is established. Software remains the default.

**Rendering throughput currently limits playability:** the actual game flip
counter measures about 0.6–0.7 FPS in software and build103's hardware path,
improving to 1.3 FPS with build104, 1.6 with build105, 2.2 with build106 and
2.5 in the corrected build114 sample. Window refresh
is independent of this rate. Rendering accuracy, timing, full gameplay,
audible audio acceptance and long-session stability still need work.
The [playability plan](mvp06/docs/PLAYABILITY_PLAN.md) brings the D3D11
pushbuffer backend forward alongside the remaining gameplay checks.
See [HANDOFF](mvp06/docs/HANDOFF.md) for current evidence and
[ENGINEERING_LOG](mvp06/docs/ENGINEERING_LOG.md) for the full fix history.

