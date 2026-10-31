# Archived AGENTS.md (as of 2026-10-21)

The full AGENTS.md before the 2026-10-23 rewrite: run-by-run checkpoint notes for
runs 52–162 and the original debugging-loop narrative. Kept verbatim for the
evidence it cites; the current working instructions are in ../../AGENTS.md.

---

# AGENTS.md — working instructions for AI agents on the MVP 06 recomp

**CURRENT (2026-10-20): read docs/HANDOFF.md "Current state" first.** 23 FPS at the
pitch selector with GPU-resident surfaces (title defaults in main.c; scopes OFF).
Residency does not skip RAM commits: tracked surface pages are protected and any
observer faults into an on-demand materialize. The checkpoint notes below predate it;
their scope/budget/cutoff launch advice no longer applies.


PAUSED CHECKPOINT161/162 (2026-10-20, user request): no game or helper live.
Release161 links07:59:42,59,192,832 bytes; source/EXE/MAP hashes saved in
build161_hashes.json. Only synchronous failed-exclusion fallback now defaults
to512 words; exclusive slices stay4096/coarse2 ms, all RAM/GET/ISR/publication
contracts retained. Guarded256/512 and4K/16K race tests pass GIL0/1. Four
preexisting memory-layout warning sites match107; no new warning sites.

Normal160 on159 established the choice in the SAME process: warm reverse
4096/512/4096/512 samples5.149/5.350/5.150/5.400 FPS (20 s after5 s settle),
mean5.1495→5.375 (+4.38%). Earlier4096/256 and nearby256/512 comparisons support
512. All12 phase input audits match six route downs; timing/history/frame
polling/NV2A trace/GPU_YIELD off. Unscoped resolves fall36→24/flip but TOTAL
resolves do not fall (roughly63→64). More tiny draws take scoped hardware;
do not claim a broad4.4% gameplay gain or fewer total readbacks.

Then hold512: two pitches/count2–0/pause/resume PASS16:29:06 on160, captures
visually checked, all ten downs exactly match helper. STOP1602026-10-19 21:00:42
after17,239.150 s/3,763 paths; zero dispatch misses/stubs/crashes/USB cancels/
lost-syncs.8,353 captures span17,236.18 s with maxgap3.089 s;8,618 audio windows/
3,231,807 attempts/zero rejects. Software536,261 posted/completed. s_tot_words
is uint32 and may wrap: its final value is NOT a lifetime total. Session muted;
preserve preference, audible quality and full exhibition remain unverified.

Run162 on default512 Release161 starts08:02:27 PID19708, captures intro through
window01108:02:55, log ends08:03:06 with591 paths/zero logged failures. Game and
helper were already absent at checkpoint; exit status/reason unavailable.
Route log only says waiting for title, no injected downs. Do NOT call162 a
completed startup or clean gameplay acceptance. No runtime edits after161.
Resume with a fresh normal run163, track process EXIT status, complete frontend/
selector/two-pitch acceptance on161 before changing runtime again. Launch using
packed readback, scopes, cutoff4096, all diagnostics/GPU_YIELD off. Native161
publication tests passed; earlier159 walker/memory and157 renderer tests apply.
Next attribute external flushes, then reduce GPU/RAM round trips while retaining
CPU/alias/fence/interrupt visibility. Mercenaries surface compatibility and xemu
dirty-page observers are useful references; NFS/Burnout source review incomplete.
Never skip RAM commits merely because another project keeps GPU surfaces resident.
Private experiment_fallback159.py assumes/restores4096; adapt it to restore512
before reuse on161. Full exhibition/save-reload/comfortable FPS still open.

Historical performance work (2026-10-19):
Diagnostic153 on unchanged151 stopped15:44:03 after327.401 s/3,414 paths,
clean. Selector130 visually confirmed, exactly six route key-downs; timing-on
baseline3.800/3.849 FPS, Map6.419/6.611 s and surface2.987/2.757 s per20 s.
Release153 links15:45:05,59,192,832 bytes, no warnings. Diagnostic154 stopped
15:51:55 after347.234 s/3,425 paths, clean, exact six key-downs. GPU timing-on
selector4.60/4.75 FPS versus unchanged151's3.80/3.849; upload16.49/16.31 MB
per flip versus17.99/17.57. Captures visually pass. Renderer retains
compatible color/depth allocations across disabled-depth HUD draws; native
regression fails before (3 uploads vs1), passes after on AMD/WARP packed/legacy.
Native control/candidate/candidate/control packed-hardware benchmark reduces
depth/HUD pair7.853–10.773 to2.529–3.017 ms and upload volume by two thirds.
All2,570 renderer draws on AMD/WARP packed/legacy and79 PB draws each pass.
REJECTED155 experiment retained a larger allocation across shorter clip heights;
compare pending logical height separately and preserve depth outside the
current height before whole-resource uploads. New short-height Z16/Z24 EQUAL
regression catches missing preservation. Both-order native clipped-height
benchmark improves6.524–7.135 to2.219–2.735 ms/pair, upload262.144→122.88 MB
per100 pairs. Release155 links15:53:20,59,192,832 bytes, no warnings; all2,600
renderer draws per AMD/WARP packed/legacy plus79 PB draws each pass. Cycle/D3D
155 pass. But normal156 selector2.498/1.850 FPS regresses; reject capacity
reuse despite native gains. STOP15616:02:39 after521.226 s/3,418 paths, clean,
179,489,986 words/software11,867 completed before stop. First count1–0 at220
passes; helper deliberately stopped before second pitch. Seven expected downs,
all diagnostics zero. Existing selector template8.72 was a false negative;
unchanged151's verified selector153/130 matches2.10, same threshold/two captures.
Current157 source exactly matches153 renderer (same-height depth-toggle fix
only). Links16:03:08,59,192,832 bytes, no warnings; all2,600 renderer draws per
AMD/WARP packed/legacy pass. Keep new short-height pixel oracle but require
recreation for height changes. Normal158 stopped16:11:45 after474.511 s/3,421
paths, clean; selector135 and three samples4.45/4.50/4.65 FPS, accepted audio
47,970/48,035/47,996 frames/s, zero rejects/diagnostics; exact six key-downs.
No reliable normal FPS gain from157. No pitch input sent. Attempted profile
158 used wrong cwd and failed before sampling; do not cite it as a profile.

Historical159 experiment setup (completed by160 above): Release159 links16:12:30,59,192,832 bytes.
New private MAP-visible s_gpu_fallback_words default4096 (clamped64..4096)
only changes bounded synchronous slices when try-exclusive fails. Exclusive
scope budget stays4096/coarse2 ms and ISR/GET/publication/RAM barriers unchanged.
Native walker/continuous producer, guarded256 and4K/16K publication race,
memory/host waits/raised IRQL/DMA handoffs pass GIL0/1. Four existing memory-
layout warnings match build107 (C4324, C4456 twice, C4702); no new warning sites.
Normal160 originally PID23988 from16:13:23, all diagnostics/GPU_YIELD off. Setter
set_fallback159.py verifies owned path/loaded PE+MAP stamp/ASLR/aligned module
data/expected old value and readback; selftest4096→4096 passes. Queue controlled
4096/256/4096/256 selector test with five-second settles/twenty-second samples,
input audit each phase and finally restore4096. Do not relink live. No pitch
inputs queued yet. Latest full acceptance remains152 on151.
Latest full two-pitch acceptance remains152 on151.
Keep current budgets/coherence/IRQ barriers and preserve session mute preference.

Historical PAUSED checkpoint152 (2026-10-19, user request): no game or test helper live. Current
Release151 links14:58:23 (59,192,832 bytes), no runtime edits after link.
Normal152 STOP15:22:07 after713.483 s/3,427 paths, zero dispatch misses,
stubs/crashes/USB cancellations/lost-syncs.355,170,383 words, software18,700
posted/completed before stop. Movies/full frontend/stadium/two pitches/count2–0/
pause/resume pass; captures visually inspected. All ten key-downs exactly
match helper commands. Normal selector4.549/4.65 FPS, audio48,020/47,996
accepted frames/s, zero diagnostics with flags off; no causal speedup claim.
357 audio windows/133,363 attempts/zero rejects; active nonzero host signal,
SESSION MUTED. Preserve user preference; listening quality unverified.
Next inspect readback synchronization/remaining audio stalls, then full
exhibition and save/reload. Keep current budgets and all coherence/IRQ barriers.

Release151 repair and prior diagnostic evidence:
151 prepares each32-sample slice and
tracks freshness across a256-frame block, fixing end-of-block loss of valid
hardware samples and mid-block replay of stale slices. Standalone stale stop,
mute/tone, software mix, queue pacing/ownership, RAM and IRQ contracts remain.
Native before:448/128 mismatched channels for last/middle-off cases; after:
all256 patterns/131,072 PCM comparisons, output/mute/stop/DMA/sink tests pass
timing0/1. Counters check1,024 off frames/127 preserved partial blocks when on,
zero when off. State size+8; existing offsets unchanged. Cycle/D3D151 pass.
read_audio_timing151.py adds off/front-end-stopped frames and partial blocks
to150 histograms/voice clock. Intro48k processed/s, no empty observations or
rejects, six preserved partial blocks/20 s; frontend preserves22 more/20 s.
Diagnostic151 STOP15:09:55 after646.801 s/3,384 paths, zero failures/lost-syncs;
244,989,269 words, software13,146 posted/completed.323 audio windows/120,392
attempts/zero rejects, active but session muted. User confirms playing during
151:122 keyboard transitions versus12 route edges. Treat151 as mixed-input
functional evidence, not controlled priority/FPS comparison. Priority helper
works,0/2 transitions verified and finally restored0; no source priority change.

Previous script-confirmed two-pitch acceptance is150 on previous Release150
(relinked2026-10-19). STOP14:53:49 after766.605 s/3,420 paths, zero failures/
lost-syncs;347,889,013 words, software posted17,197/completed17,196 before stop.
Both movies/full frontend/selector107 pass; first count1–0 at15714:46:35,
second selector30714:52:04, count2–0 at31614:52:23, pause319/resume324 PASS,
visually inspected. Normal selector4.35/4.00 FPS, audio47,996/47,970 accepted
frames/s, no causal FPS gain claim. Input audit finds two extra Enter pairs
before the expected route; some key-up traces are absent (focus clears state).
Do not label150 a strictly unattended input baseline. Its captured pitch/count/
pause results remain functional evidence. All APU/GPU timing counters zero; history/
frame polling/NV2A tracing/GPU_YIELD off.383 audio windows/143,046 attempts/
zero rejects; host active/nonzero but SESSION MUTED. Preserve that preference;
no listening-quality claim.150/151/152 games closed.

Normal152 acceptance passes; remaining long stalls need investigation. Do not infer
full accuracy from this local DSP-passthrough repair. Priority comparisons are
confounded by user input; no scheduling policy adopted. Keep4,096/coarse2 ms,
128 slots/128 MiB and every RAM/
coherence/IRQ barrier. Full exhibition/saves/comfortable FPS/accuracy open.
See ENGINEERING_LOG150–151 for evidence and native layout probe commands.

Historical PAUSED checkpoint150 (2026-10-12, user request): current Release150 links
08:15:15 (59,192,320 bytes), no new warnings/later runtime edits. No game or
test helper live.150 only extends default-off APU diagnostics: raw32-frame
RAM-wait and256-frame production-interval maxima/20 logarithmic bins, pipeline
maximum and diagnostic APU thread ID, using existing timing timestamps.
No pacing/sample/RAM/scheduler-priority change. APU output/sink/DMA tests pass
timing0/1. Cycle/D3D150 pass33 seeds/37,762 lifts/seven ranges; memcpy44302D.

Normal run149 on older Release148 STOP08:15:07 after521.363 s/3,417 paths,
zero misses/stubs/crashes/USB cancellations/lost-syncs;188,023,527 words,
software posted11,226/completed11,225 (snapshot before stop, not a park).
Movies003/021/full frontend/difficulty070/stadium/selector135 pass. First
selector20008:13:31, A down/up08:13:33, first count1–0 at21208:13:57 visibly
passes (83 MPH). User requests pause while waiting for second selector; do
not label the helper's explicit termination a regression. Second pitch/pause
acceptance is pending. Latest full two-pitch/pause run remains146 on145.

149 selector3.90/3.649 FPS (still slow; no causal gain claim), accepted audio
47,969.716/48,005.793 frames/s, zero rejects. All APU/GPU timing counters zero;
history/frame polling/NV2A tracing/GPU_YIELD off. Audio260 windows/96,886
attempts/zero rejects; host active/unmuted/peak0.129150391. Actual playback
continuity/listening quality still open (prior148 stadium46.1k/14 empties).

Resume unchanged150 with normal run150: --packed-readback --gpu-min-pixels 4096
--gpu-scopes, all diagnostics/GPU_YIELD off. Complete selector measurements and
two pitches/pause/resume. Then run151 --apu-timing and read_audio_timing150.py
for wall-time histograms (include descheduling, not exact CPU/lock attribution).
NFSU2 also raises its APU host thread to relative priority2; private
set_apu_priority150.py is prepared but UNTESTED and NO priority change was made.
Only test a same-process0/2/0 reverse if measured stalls support it, restoring0;
helper verifies PE/MAP, owned diagnostic APU TID and normal process class.
Keep4,096/coarse2 ms,128 slots/128 MiB and every RAM/coherence/IRQ barrier.
See ENGINEERING_LOG149–150; full exhibition/saves/comfortable FPS/accuracy open.

Historical PAUSED checkpoint148 (2026-10-11, user requested a pausing point): current
Release148 links18:59:19 (59,192,320 bytes), no new warnings. No game/test
helper live. No runtime edits after that link. Latest full two-pitch/pause
acceptance remains146 on older Release145;148 has partial diagnostic acceptance.
Run148 STOP19:03:16 after224.089 s/3,325 paths, zero misses/stubs/crashes/USB
cancellations/lost-syncs;36,129,492 words, software posted/completed4,462.
Both movies003/021/full frontend through difficulty072/stadium104 visually
pass. Stopped before selector or pitches, per user request.

Default-off RECOMP_APU_TIMING measures deadlines, throttle/RAM/pipeline time,
queue observations and sequenced SamplesPlayed snapshots every2 s (GetState
flags0, never reading SamplesPlayed from NOSAMPLESPLAYED). Baseline147 selector
45,634 processed frames/s;102 resets discard1.012089 s/20 s. Native fake clocks
and fake voice reproduce the defect and verify unchanged samples/ownership.
Windows producer now follows xa2_queued()/xa2_queue_target(): eight256-frame
blocks (~43 ms), inside twelve slots; admit APU register writers once per
block. Adapted from NFSU2; RAM lease remains after throttle, fallback unchanged.
New empty-queue/full-queue/pause fixture fails before/passes after. APU output,
sink3,247 FIFO blocks/500 cycles/14 host-failure retries and DMA pass timing0/1.
148 intro47,999 processed frames/s/no empty observations; stadium46,102 with
14 empty observations, no deadline resets/rejections. Audio112 windows/41,287
attempts/zero rejects; host active/unmuted/peak0.124512. This is partial progress,
not continuous48k or listening-quality proof. No148 FPS gain claim.

Resume with unchanged Release148, run149: launcher flags --packed-readback
--gpu-min-pixels 4096 --gpu-scopes (no --apu-timing), then
route_frontend, wait_pitch_selector131 and test_pitch_loop. Measure selector
FPS/accepted production with all diagnostic flags off; later opt-in APU timing
to measure long RAM stalls before changing leases/headroom. read_audio_timing147.py
is read-only and verifies loaded PE/MAP. Keep4,096/coarse2 ms,128 slots/128 MiB,
GPU_YIELD off and all observer/IRQ barriers. Cycle/D3D148 pass33 seeds/37,762
lifts/seven ranges, memcpy exclusive44302D. Full exhibition/saves/comfortable
FPS/audio continuity/original comparison remain open. See ENGINEERING_LOG147–148.

Historical accepted146 (2026-10-11): Release145 links18:11:51 (59,190,784 bytes).
STOP14618:38:27 after884.2 s/3,420 paths, zero misses/stubs/crashes/USB
cancellations/lost-syncs;367,699,808 words, software posted/completed20,983.
No game live; never relink live. Both movies/full frontend/stadium pass,
selector133, first selector196/count1–0 at205, returned selector365, second/
count2–0 at375, pause378/resume384 PASS (visually inspected). Latest two-pitch
acceptance is146. Timing/history/frame polling/NV2A tracing and GPU_YIELD off.
FPS4.30/4.30, timing counters/frequencies all zero; no causal FPS gain claim.
145 same-process reverse3.091 FPS off/3.012 on overlaps: experimental
RECOMP_GPU_YIELD remains default OFF. Keep4,096/coarse2 ms,128 slots/128 MiB.
Audio442 windows/156,776 attempts/zero rejects; host active/unmuted/nonzero.
Exact normal-run accepted production45,717/45,616 frames/s versus48,000;
diagnostic145 ~43.2k. Playback clocks, continuity and listening quality open.
32 distinct-value automatic/manual handoffs pass with requests on/off and
GIL on/off; extra publication/bridge readers and raised IRQL remain protected.
Publication4K/16K and scheduler pass145; DMA/hybrid144 and cycle-D3D145 pass.
Keep4,096/coarse2 ms. Full144 build has15 existing shift warnings matching110;
145 incremental build has no new warnings. Next measure readback synchronization/
polling and audio RAM waits, throttle overruns and actual playback clock before
changing cadence/headroom. Full exhibition, comfortable FPS, saves and original
comparison remain open. See ENGINEERING_LOG continuation143–146.

Private test_pitch_loop.py opens build/pitchloopRUN.txt itself. Redirect its
external stdout/stderr to pitchloopRUN_helper.txt; redirecting to its own log
causes a Windows file-lock error before input.

Historical continuation142 (2026-10-11): Release141 links16:23:06 (58,485,248
bytes). Same EXE run142 STOP16:46:14 after855.8 s/3,420 paths, zero misses/
stubs/crashes/USB cancellations/lost-syncs;324,153,610 words. No game live;
never relink live.142 automatically passes movies/full frontend/stadium,
selector145, first pitch/count1–0 at195, returned selector352, second/count2–0
at362, pause365/resume370. This is latest two-pitch acceptance. Audio428 trace
windows/151,634 attempts/zero rejects; host active/unmuted/peak0.099823.
Listening/continuous production/playback timing remain open.

140 separates default-off texture lookup/compare/decode/resource-upload costs.
Selector has ZERO cache misses; retain128 slots/128 MiB despite loading-only
misses. Valid linear ARGB0x12 now copies visible width*4 bytes per pitched row
in render_texture_decode; other formats retain scalar decoding. All validation,
CPU-edit/palette/target aliases, RAM and original ISR barriers remain.625,422
scalar comparisons cover tight/padded/odd-width rows, alpha, guards and CPU
edits. Native benchmark2.450→0.0369 ms/640x480 texture; AMD/packed and WARP/
legacy2,546 renderer draws,79 pushbuffer draws each and cycle/D3D141 pass.
No warnings. No source changes after141 build. Seven ranges/33 seeds/37,762
lifts remain; memcpy exclusive44302D.

Live decode falls1.31–1.38→0.545–0.555 s per20 s despite more decoded pixels;
per-pixel cost ~65% lower. Diagnostic141 FPS3.90/4.10 is not a product baseline.
Default-off142 FPS3.70/3.65, all timing counters/frequencies zero, overlaps old
baselines: NO reliable FPS gain established. Keep4,096/coarse2 ms. Map remains
6.63–6.68 s/20 in diagnostics. After FPS,60-round reset profile shows GPU
driver waits and flag worker2,093.75 CPU ms usingSleep(0); not stack unwinding
or exact wait duration. Next measure readback counts/bytes/scope breaks and
polling costs without weakening coherence/IRQ contracts. Remaining swizzled
R5G6B5 decode needs separate timing before optimization. Audio needs elapsed/
source/playback-clock measurements. Full exhibition, comfortable FPS, saves
and original-game comparison remain open. Evidence timing141_comparison.json,
render142_pitch_first/repeat.json, pitchloop142 captures/log, final runtime/
summary/stop/audio142 files. Earlier checkpoints below are historical.

Historical continuation139 (2026-10-11): Release139 links15:46:50 (58,484,736
bytes). Run139 STOP16:02:53 after867.9 s/3,427 paths, zero misses/stubs/crashes/
USB cancellations/lost-syncs;371.9M words. No game live; never relink live.
139 fully automatically passes movies003/021, frontend/stadium/selector137,
first pitch/count1–0 at218, returned selector370, second/count2–0 at379,
pause382/resume387. Latest two-pitch acceptance is139. Full audio trace435
windows/153,762 attempted256-frame blocks has ZERO rejected, versus prior
4.6–4.8%. Windows session active/unmuted/peak0.1254. Listening/timing open.

Only runtime change after138: XAudio2 queue3→12 slots,64 ms at the monitor's
256-frame cadence, matching NFSU2. No sample/filter/rate/mix/renderer changes.
New xaudio2_queue_smoke fails before on fourth block; after3,246 FIFO blocks/
500 completion/wrap/backpressure cycles/host-failure retries pass. Existing
APU monitor/mute/stop and DMA tests plus cycle/D3D139 pass. Native fixture
opens no audio device. Incremental139 build log has no warnings. FPS3.95/4.15
observed, timing/history/frame polling/NV2A tracing off; no controlled reverse
repeat, so no causal FPS gain claim. Private read_render_stats.py --audio-log
collects records ending in the same FPS interval (their~2 s windows can cross
its edges). Two20-second samples total7,054 attempts/zero rejects.

Next: profile texture compare/decode/create separately, preserve all RAM/
alias/CPU-edit barriers. Audio needs actual trace duration/source queue and
playback-clock measurements: zero rejected blocks does not prove continuous
48 kHz production or audible quality. Do not expand headroom blindly or copy
reference limiter/filter changes. Keep4,096/coarse2 ms; full exhibition,
comfortable FPS and original-game comparison remain open. Earlier138/136
checkpoints below are historical.

Historical continuation138 (2026-10-11): Release138 links15:26:06 (58,484,736
bytes). Run138 STOP15:41:07 after891.7 s/3,426 paths; zero misses/stubs/crashes/
USB cancellations/lost-syncs;329.6M words. No game is live. Never relink live.
138 automatically passes movies/full frontend/stadium, selector146, first
pitch/count1–0 at217, returned selector391, second/count2–0 at402, pause405/
resume411. This is latest two-pitch acceptance. Matched20 s samples3.50/3.45
FPS match136: no FPS gain. History/frame polling/GPU timing/NV2A tracing off;
live timing counters/frequencies all zero. Native137 and cycle/D3D138 pass.

New default-off RECOMP_GPU_TIMING=1 records aggregate QPC renderer/guest-RAM
costs, no I/O/public stats ABI change. Default-off makes no QPC calls. Do not
use diagnostic-on FPS as a product baseline.137's six20 s selector samples
find Map wall time5.99–6.45 s, commit/copy1.01–1.08 s, texture2.41–2.56 s,
surface compare1.52–1.73 s; GPU leases7.66–9.92 s/20. Reader acquisition sums
across threads25.42–30.76 s; not an elapsed percentage. Draw/flush and nested
stage totals overlap. Maxima219/218 ms include earlier scenes. Keep4,096
words/existing coarse2 ms check/all RAM, fence, alias and original ISR barriers.
RECOMP_PB_EXEC no longer implies verbose diagnostics. Use RECOMP_NV2A_TRACE=1
or RECOMP_PB_SCAN for them; private launcher adds --nv2a-trace/--gpu-timing.
Same-process logging pairs overlap/reverse; no reliable gain established.
137 timing run STOP15:25:58 after728.1 s/3,422 paths, clean; no pitches.

Next bounded audio lead: observed4.6–4.8% rejected submissions across136–138
does not prove audible gaps. The sink queues three256-frame blocks (16 ms);
NFSU2 already uses12 (64 ms). Add native burst/order/wrap/backpressure coverage
before changing it, then live signal/drop checks; quality/timing still open.
For frames, split texture compare/decode/create costs before optimizing.
NFSU2's sampled hashes and Mercenaries' frame-generation static validation
are references, not exact coherence proofs. Reuse requires proven full RAM
exclusion and invalidation at CPU/GPU/palette/alias writes. Do not disable
hybrid depth readback. Full exhibition/playable FPS/original comparison open.

Historical continuation136 (2026-10-11): Release135 links14:30:27 (58,481,664
bytes). Run136 STOP15:03:21 after876.2 s/3,420 paths, zero misses/stubs/crashes/
USB cancellations/lost-syncs;344.9M words. No game is live. Never relink a live game.
135 fully automatically passes movies/frontend/stadium, first pitch/count1–0
at257, returned selector461, second/count2–0 at472, pause475/resume481.
STOP14:48:44 after1,068.9 s/3,416 paths, zero failures/lost-syncs;343.1M words.
136 repeats fully automated movies/frontend/stadium, first/count1–0 at205,
returned selector366, second/count2–0 at376, pause379/resume384 with both
tracing flags false. This is latest two-pitch acceptance. The16,384-word budget
is REVERTED for lack of an established benefit:135's reverse repeat2.45/2.75
also falls below133's2.9–3.5, so a causal regression is not proved. Keep4,096.
Cycle/D3D135 audits pass; expanded publication test covers both limits.
Critical measurement trap: older launchers set RECOMP_FRAME_TRACE even when
not recording. That causes fopen on every draw; earlier FPS includes polling
overhead. Private launch_smoke99.py now omits it by default; --frame-trace is
required for flag-driven captures.136 has memory_trace=false/frame_trace=false
for corrected selector/profile measurements. All RAM/coherence/ISR barriers stay.
136's corrected selector samples are3.50/3.45 FPS. tools_sample.py now supports
--module-summary --system-exports to resolve actual ntdll/win32u syscall stubs;
run sampling after FPS measurement, since it briefly suspends threads. Current
leads are GPU synchronization/RAM waits, remaining verbose log writes and the
busy flag worker; no native FPS gain or full exhibition acceptance is claimed.

Historical experiment134: native publication/preemption/dispatcher tests pass
4,096/16,384 words, GIL on/off, plus RAM/DMA/scanner cases. Larger exclusive
budget records2.55/2.70 FPS; STOP14:29:46 after544.3 s/3,410 paths, clean.
Reverse135 also varies, so no causal speedup/regression established. Runtime
budget is reverted; keep the expanded native fixture. User handed testing
back; automated inputs remain authorized. Audio meter is active, unmuted and
nonzero; listening quality/timing remain unverified.

Continuation133 (historical): Release132 links13:51:50 (58,481,664
bytes). Run133 STOP14:19:00 after515.3 s/3,413 paths, zero failures/lost-syncs.
Its matched selector samples are2.9/3.4/3.5 FPS (last over20 s). Run132 completed
a user-assisted Kentucky/Texas A&M pitch loop: first count1–0, selector433,
second pitch/count2–0 at461, pause491/resume507. STOP14:10:24 after1,083.8 s/
3,427 paths, zero misses/stubs/crashes/USB cancellations/lost-syncs;339.9M
words consumed. User handed control back; automated inputs are authorized.
133 repeated the controlled Florida/Texas route on unchanged132 for measurement.
Zero NOPs now remain inside GPU batches; nonzero NOP5 still commits before
the original software observer. Native regression fails before/passes after,
and all79 PB draws pass AMD/WARP packed/legacy; publication tests pass GIL
on/off. Resolve/slice counters add no per-draw logging. Controlled build131
selector samples are2.9/3.1 FPS; no132 speedup is established. Latest fully
automated two-pitch acceptance is129; latest user-assisted acceptance is132.
Keep all coherence/ISR barriers and the
4,096-word/2 ms limits. Cycle/D3D132 audits pass;33 seeds/37,762 functions,
seven ranges including memcpy exclusive end0x44302D. The older checkpoints
below describe their own stopped runs, not the current live process.

Continuation129 (2026-10-11): Release129 links13:21:22 (58,481,664 bytes).
Run127's actual failure is traced: the bounded parser's accumulated runaway
budget expires and resync skips the requested semaphore/NOP5. A native busy-
producer regression fails at exactly4,194,304 words before repair; afterward
it exceeds that limit and retires both commands. Reset the word budget when
PUT changes, retaining packet/CALL state and unchanged-submission protection.
NFSU2's reference limit was per synchronous submission. Run128, with history
enabled, passes movies/full frontend/stadium, pitch/count1–0 and pause/resume:
511.4 s/3,327 paths, zero misses/stubs/crashes/USB cancellations/lost-syncs.
139.9 million words consumed;242 complete original fence/NOP/wait chains saved.
Opt-in RECOMP_GPU_SCOPES=1 now includes bounded synchronous retry; there is no
separate GPU_RETRY flag. Default-off retains unbounded synchronous fallback.
Keep the native-proven BlockOnTime RAM-reader and registered dispatcher-wait
repairs; they did not establish causality for every earlier live stall.
Optional RECOMP_MEMORY_TRACE=1 captures65,536 records; do not measure FPS with
it enabled. Required tools_patch_d3d.py restores THREE definition renames,
three PCRTC W1C stores and five observations in original BlockOnTime. Reapply
after every lift; cycle/D3D129 audits pass,33 seeds/37,762 functions/seven ranges.
Memcpy remains0x442CF0..0x44302D exclusive. Untraced129 passes both movies/full
frontend/stadium, first pitch/count1–0 at165, next selector360, second pitch/
count2–0 at375 and pause/resume388/395. STOP13:35:57 after851.4 s/3,420 paths,
zero failures/lost-syncs;280.5 million words consumed. Matched selector sample
132–147 is3.100 FPS, close to121's3.000; no established speedup. Latest two-
pitch acceptance is129. Full exhibition/playable FPS/audible audio remain open.
Default-off130 passes both movies003/021 and title/Start059 on unchanged129,
stops13:39:04 after151.8 s/2,214 paths, zero failures/lost-syncs. No game is live.
Never relink a live game. Next profile resolve causes/slice limits and compare
the same untraced pitching selector, retaining all RAM/coherence/ISR barriers.
The following126/122/119 checkpoints in older documents are historical.

Continuation122 (2026-10-11): Release122 links 11:53:31 (58,479,616 bytes).
RAM scopes are now integrated behind RECOMP_GPU_SCOPES=1, default off. Native
RAM/bridge/wait/callback/raised-IRQL/DMA tests pass with GIL on and off; AMD/
WARP packed/legacy pass 2,546 renderer and 78 pushbuffer draws. Scope-aware
command/vertex reads, clear/fallback/fence/flip barriers and bounded scanning
pass. Cycle120 and D3D120 audits pass. Default-off run120 passes both movies
and title/Start, stops 11:33:23 after 149.7 s / 2,214 paths, zero failures.
Opt-in run121 passes both movies/full frontend/stadium/pitching, first pitch
to 1–0 and pause/resume; stops 11:42:07 after 523.3 s / 3,418 paths, zero failures.
Matched pitching is 3.000 FPS versus118's 2.800. Build121's bounded synchronous
retry experiment parks run122 on D3D event0x58B80C (caller0x57D86F); stop
11:53:20 after 649.8 s / 3,294 paths, clean log but zero flips/draws. GET=PUT,
last NOP(5) completed, no RAM writer/reader or lifted CPU owner in snapshot.
Do not diagnose this as a RAM-lock deadlock without evidence. The retry change
is REVERTED in122; retain the original synchronous fallback, native-tested
bounded corrupt-stream limits, and opt-in scopes. Five actual Nt/Ke host waits
now pass nested-release/resume tests with GIL on/off. Final default-off run123
passes both movies and title/Start, stops 11:56:46 after 175.5 s / 2,214 paths,
zero failures. No game is live. Never relink a live game.
The older statement that the game never calls begin applies through119 and
to the default-off path. Latest accepted two-pitch run remains116.

Earlier continuation (2026-10-11): Release119 links 10:58:34 (58,477,568 bytes),
explicit configure/build. It initializes the dirty rectangle to remove MSVC's
new C4701 flow warning; AMD passes 2,545 renderer and 72 pushbuffer draws again.
Run119 PID27312 stops 11:03:01 after 164.2 s / 2,214 paths, zero failures;
both movies visibly render and the screen-checked title/Start pass. No game is
live. The final source/build is 119; latest broad gameplay acceptance is 118,
latest two-pitch acceptance is 116. Never relink a live game.
Run118 stops 10:58:26 after 677.3 s / 3,420 paths, zero failures. It passes both
movies/full frontend/stadium, pitching controls, first pitch/count 1–0 and
pause/resume. Matched untraced pitching is 2.800 FPS; original vblanks 60.0 Hz.
The new renderer begin/flush/end API groups draws only in explicit scopes;
the game does NOT call begin and retains synchronous per-draw RAM commits.
AMD/WARP, packed/legacy each pass 2,545 renderer draws, including Z16/Z24,
blend, read-only depth, target changes, color/depth texture and palette aliases,
CPU edits after a flush, overlapping color/depth storage and rejected batches.
Full pushbuffer/hybrid and cycle/D3D audits pass. Groups of 32 native draws
measure 0.029–0.049 ms/draw versus 0.782–1.060 ms synchronously on 640×480
targets. This is NOT a game FPS gain. See HANDOFF for integration prerequisites:
the GIL alone does not exclude kernel-bridge RAM access while suspended.
All scope pointers must use the canonical RAM mapping; failure to flush is
fatal to the transaction, never permission to signal a fence or use stale RAM.

Run116 stops 10:34:41 after 1,412.0 s / 3,429 paths, zero misses/stubs/crashes/
USB cancellations. On unchanged build114 with cutoff 4,096, it passes both
movies, full frontend/stadium, two pitches to count 2–0 and pause/resume.
Matched untraced pitching is 2.799 FPS; keep the 2,048 default pending broader
measurement. No main build116. The bounded readback polling experiment passes
native pixels but gives <1% in the final reverse timing pair while spending
CPU, so it is fully reverted. Run117 stops after 233.9 s / 3,357 paths, clean,
with both movies/full frontend/stadium; build118 adds the native overlap check.

Previous accepted state: corrected Release114 links
09:37:06 (58,476,032 bytes); run114 stops 09:56:53 after **1,148.9 s / 3,425
paths**, zero misses/stubs/crashes/USB cancellations. D3D11 plus opt-in packed
readback passes both movies, the complete player frontend, stadium, pitching,
the next-pitch selector at count 1–0, a second pitch/count 2–0 and pause/resume.
The reset phase takes minutes at this speed; don't infer a gameplay stop from
short waits. Matched untraced pitching is 2.5 FPS, original vblanks 59.994 Hz.
Full exhibition and audible audio acceptance remain open.
The partial-depth regression passes Z16/Z24 on AMD/WARP, packed and legacy
(2,398 draws each); full pushbuffer/hybrid and cycle/D3D body audits pass.
Packed readback stays opt-in: build113's apparent 3.0 FPS preceded a real
depth-coherence repair and is not an accepted performance gain.
Run113 stopped 09:36:58 after 864.4 s / 3,422 paths, zero failures. A CPU edit
outside an earlier GPU triangle reproduced stale intermediate depth; build114
seeds that intermediate before partial CPU uploads. Whole-surface early
memcmp, grouped row scans and moving the preservation copy before the packed
readback wait give mixed/no consistent benefit and are reverted. No build115
main EXE is produced; the source retains the accepted build114 behavior.
Large-target benchmarks show 32² software ~0.71 ms versus GPU ~1.38 ms, unlike
the smaller surfaces used for the 2,048 cutoff. Run116 isolates the higher
cutoff; no polling or other runtime change is combined with it.
Run112 stopped 09:05:49 after 1,090.4 s / 3,425 paths, zero failures;
movies/full frontend/stadium/players, first pitch/count 1–0 and pause/resume
passed. The second-pitch transition remained unverified despite active
rendering and longer holds. Readback synchronization remains the measured
bottleneck. A map-order experiment gave no consistent gain and was reverted.
Build111 links 08:43:56 (58,472,448 bytes); its interrupt handoff and fresh/full
audits pass. Hardware run112 measures 59.8 Hz original vblanks versus 1.8 Hz
in run110. Run111's accidental software smoke stops after 221.9 s / 591 paths,
clean, with both movies and 59.97 Hz original vblanks. Its movie FPS is not a
matched pitching comparison. The launcher now explicitly records its backend.
Full cycle110 lifts 37,762/37,762 functions, zero
failed, 33 seeds, all seven boundaries (memcpy ends 0x0044302D). Audit110
passes; USB probes restored. Build110 links 08:13:02, 58,470,400 bytes.
Run110 stops 08:35:21 after 1,231.9 s / 4,984 paths, zero misses/stubs/crashes/
USB cancellations; both movies, title/naming route and the batting demo render.
Its original driver records only 1.8 vblanks/s; the flip target grows by 48,358
in ten seconds while retirement stays at two. NFSU2 documents the same repeated
PGRAPH trap and dropped-vblank defects. Build111 uses explicit handler completion
under the publication lock, protected queued vblank status and actual PCRTC W1C
stores, with phase-correct 60 Hz deadlines. Native FIFO-before-return and lost
vblank fixtures fail before/pass after; 10,000 vblank handoffs and clock/GIL/
queue checks pass. Reapply `tools_patch_d3d.py` after EVERY full lift; this is
now required for linking and interrupt correctness, not an optional probe.
Run109
stops 07:49:43 after 1,317.6 s / 6,269 paths: six known misses, no stubs,
crashes or USB cancellations. It passes both movies, Press Start, the full
player Play Now route, a pitch/count change to 1–0 and pause/resume. Second
pitch/full exhibition remain unverified in this build. Attract-demo exit
exposes six misses at the
11-byte destructor thunk 0x0013D210; its lift passes 10,000 original-x86
comparisons with a controlled tail callee; it is now in full cycle110.
No game is live at this checkpoint. Never relink while a game is alive.
All six earlier attract-exit call sites have not been re-exercised. Run108 stops
07:27:24 after 1,152.2 s / 2,186 paths,
zero failures, parked on D3D's NOP(5) event wait at the title logo. Build109
publishes the PGRAPH trap, delivers vector0x33 on the timer's guest stack/TIB,
and waits for the original handler's entry/exit. Protected pending-source,
delayed DPC, cancellation, AMD pushbuffer, DMA/GIL/IRQL and DPC tests pass;
full93 audit109 passes. Build107 separates NV2A flag servicing from rendering;
it exposes a real PGRAPH marker/semaphore wait (590 paths, zero failures).
Build108 consumes pattern-color/semaphore releases and disables the old
producer-count fence mirror during execution. Native before/after pattern,
AMD/WARP/software, blocked-render acknowledgment, DMA and GIL checks pass;
full93 audit108 passes. Generic DMA-context semaphore addressing remains open.
Run106 stops 06:59:07 after 1,125.7 s / 3,380 paths, zero failures. Both movies,
full frontend, stadium and pitching at 2.2 FPS pass. The next-pitch selector
returns at count 1–0, and another 84 MPH pitch changes it to 2–0: full pitch
loop plus a second pitch accepted. Pause/resume also works. Full exhibition
is unverified. Build106 removes per-parameter getenv scans.
Run105 stops 06:40:06 after 1,353.5 s / 3,423 paths, zero failures. Both movies,
full screen-checked frontend, stadium/players and an 83 MPH pitch/count 1–0
pass. Untraced pitching measures 1.6 FPS, versus build104's 1.3. Three traces
complete (692 draws each). Next-pitch controls remain unverified despite
continuing post-pitch animation and later input report edges. Build105 uploads
only CPU-edited color/depth bounds; WARP/AMD
coherence, cache, depth and hybrid fixtures pass. Full93 audit105 passes.
Run104 stops 06:16:36 after 582.4 s / 3,412 paths, zero failures, movies/full
frontend/stadium/pitching controls visible. Pitching improves to 1.3 FPS and
104 texture uploads/10 s versus build103's 0.7 FPS / 792 uploads, but full
surface uploads still total 1.48 GB/10 s; build105 reduces that to 241 MB/10 s.
Run103 stops 06:06:31 after 1,140.7 s /
3,413 paths, zero misses/stubs/crashes/USB cancellations. Both movies, full
frontend, bright stadium, player close-ups and a pitch/count change to 1–0
pass on hardware. Full pitch loop/exhibition remains unverified. Build103
narrows readbacks to conservative draw bounds and reuses unchanged resident
surfaces after RAM comparison. Stadium measures 2.3 FPS, players 1.2 FPS,
pitching 0.7 FPS; window refresh is not game throughput.

Build104 keeps tiny batches on CPU (RECOMP_GPU_MIN_PIXELS=2048 clipped
triangle bounding-box pixels; 0 forces hardware), based on native timings.
Texture cache grows from 32 to 128 entries with a 128 MiB content budget:
run103 had 792 uploads/10 s with no shader compiles, and a 40-page reuse
fixture fails before/passes after. WARP and AMD each pass 1,880 differential/
coherence/cache draws plus 72 full-pushbuffer draws. The hybrid transition
fixture also passes. Build104's measured gain is recorded above.
Software remains default; hardware is opt-in. Exact D32_FLOAT binary depth
encoding preserves Z16/Z24 bits; host D24 failed on AMD despite passing WARP.
Full93 audit still passes (audit105.txt). See HANDOFF for live outcome.

Previous build101 links 05:06:46. Run101 stops at 05:10:59
after 252.0 s / 591 paths, zero misses/stubs/crashes/USB cancellations. Both
intro movies visibly render; two independent frame traces complete. That game
stops before subsequent builds. Never relink a live acceptance game.
Run100 stops at 05:06:15 after 557.4 s / 3,323 paths, zero failures. Both
movies/full frontend/bright stadium pass. Build101 adds raw D3D11 combiner
decoder repairs, verified by rendered WARP pixels: 1,136 mismatched channels
before, zero after in 320 draws. All 336 A8 + four RHW draws pass. This
prepared hardware rendering; build101's physical DMA path used software.
Run99 stops at 04:56:34 after 1,721.3 s / 3,415 paths, zero failures. Both
movies/frontend/stadium pass; an 82 MPH pitch changes the count to 1–0 and
Start still opens pause afterward. Next-pitch/exhibition is unverified.
Actual game flips measure 0.6–0.7 FPS. Build100's packed inline alignment
passes failing-before/passing-after pixels and all prior renderer checks.

Build99 fixes a native-reproduced kernel handle-table
race (58,871 stale/stolen observations before, zero after in 128,000 parallel
allocations). Run99 passes loading/vertex acceptance. Run98 stops clean but
parked at 1,175.5 s /
3,259 paths, polling a thread token whose table slot is NULL. Causality of
that park remains to be established. `RECOMP_HANDLE_TRACE=1` logs table
publication/removal and NtClose callers. See HANDOFF for current evidence.

Run96 advances the pitch/count to 1–0 with zero
misses/stubs/crashes/USB cancellations, stopped at 1,215.6 s / 3,413 paths.
Depth clears/writes execute live; return to the selector is unverified.
Full cycle93 succeeds: 32 seeds, seven preserved ranges, 37,761 lifted,
zero failed. Both post-pitch callbacks pass 17,280 original-x86 comparisons
with controlled callees. Build95 adds native-tested integer pitched Z16/Z24S8
clears/tests/writes and format-based surface width. Build97 adds reached
signed-short/CMP vertex streams and repeatable flag-triggered frame capture;
the complete renderer suite passes. Run98 was stopped before build99 linked.
See HANDOFF for evidence and remaining gaps.

Read [docs/HANDOFF.md](docs/HANDOFF.md) for current state and
[docs/ENGINEERING_LOG.md](docs/ENGINEERING_LOG.md) for history before making
changes. The task: get MVP 06 NCAA Baseball (Xbox) playable on PC via the
xboxrecomp static-recompilation toolkit.

## Ground rules

- The repo root (`C:\Users\allen\Documents\xboxrecomp`) is the **toolkit**
  (upstream xboxrecomp v0.11.0 + our fixes). The **game project** is `mvp06/`.
  Only the game project produces an `.exe`.
- Never commit or distribute game data: `mvp06/game/`, the ISO, or the built
  `.exe` (it embeds recompiled game code). Runtime/lifter fixes are the
  shareable work.
- Keep runtime fixes (under `src/`) title-agnostic and upstream-shaped, with
  comments explaining the failure they fix, in the style of the existing code.
  Title-specific behavior belongs in `mvp06/src/main.c` or
  `mvp06/src/recomp_manual.c`.
- After every fix: run, capture `run*_stderr.txt`, compare file-open counts
  (`grep -c '\[PATH\]'`) and crash signatures against the previous run. That
  comparison is the acceptance test for every change.

## Commands (Git Bash, from repo root)

```bash
# Incremental build after runtime/main changes (minutes):
cmake --build mvp06/build --config Release

# Full pipeline after seed/lifter changes (~55 min total):
py -3 -m tools.disasm mvp06/game/default.xbe \
    --seed-functions mvp06/config/seed_functions.json \
    --analysis-json mvp06/game/mvp06_analysis.json
# Required between disasm and recomp (disasm overwrites these ranges):
py -3 mvp06/tools_patch_boundaries.py
py -3 -m tools.recomp mvp06/game/default.xbe --all --split 1000 \
    --gen-dir mvp06/src/recomp/gen
# Required after every lift: D3D publication/handler hooks and PCRTC W1C.
py -3 mvp06/tools_patch_d3d.py
# Refresh the VS source list before MSBuild when generated chunks change:
cmake -S mvp06 -B mvp06/build
cmake --build mvp06/build --config Release

# Run (always from mvp06/):
cd mvp06 && RECOMP_VBLANK=1 timeout 120 ./build/Release/mvp06_recomp.exe 2> run.txt

# Harvest new function entry points from a run log (then full pipeline):
cd .. && py -3 -m tools.seed_from_log mvp06/run.txt mvp06/game/default.xbe \
    --functions tools/disasm/output/functions.json \
    --seeds mvp06/config/seed_functions.json \
    --analysis-json mvp06/game/mvp06_analysis.json

# Toolkit tests (fast, no game files):
py -3 -m pytest tools/recomp -q
```

## The debugging loop that works

1. **Crash?** Read `[CRASH]` in stderr: guest regs, guest VA of fault, and
   the guest return-address stack (innermost first). Map addresses to
   functions with `tools/disasm/output/functions.json` (or the asm listings
   under `tools/disasm/output/asm/`).
2. **Hang/spin?** `RECOMP_WATCHDOG_SECS=10` prints the stuck thread's GUEST
   registers; `py -3 tools_threadtimes.py 20` finds CPU burners;
   `py -3 tools_stackscan.py 15` reconstructs the guest call chain from the
   host stack via the `/MAP` file.
3. **Bad memory?** `RECOMP_KERNEL_WATCH=<va> RECOMP_KERNEL_WATCH_ALL=1`
   (who changed a dword, by kernel-call interval); `tools_watchpoint.py`
   (hardware DR0 write watchpoint — arm within ~1.5 s of launch).
4. **Check for dispatch misses** before diagnosing logic:
   `grep "Failed to resolve VA" run.txt | sort | uniq -c` — each is a
   function the disassembler never found; seed them. A dispatch miss returns
   eax=0 to the caller and poisons everything downstream, and it looks like a
   game-logic bug.
   If a miss is inside an existing function, inspect its indirect jump tables
   before seeding it as a separate function. `0x442FCF` (memcpy) and
   `0x5BEFD2` (XNET checksum) were omitted switch arms, not entry points.
5. **Compare with the reference ports** under `reference/` before inventing a
   fix. NFSU2's `CLAUDE.md` is an EA-title fix log that has repeatedly
   matched this project's bugs one-to-one (page zero, DSP doorbell, GP/EP
   memmove, GIL).
6. **Verify visible output** with captures, not just draw counters:
   `RECOMP_FB_DUMP=build/frame52/frame` saves GPU surfaces. Set
   `RECOMP_FB_WINDOW_DUMP_EVERY=300` to also save the window's converted
   pixels every 300 refreshes as `<prefix>.windowNNN.bmp`. Create the output
   directory first. Run52 proved the loading and legal screens this way.
   Run55 shows an unobscured EA Sports intro frame after YUY2, primitive and
   alpha-fade repairs. Runs56/57 advance to `mvpintro.vp6` and the MVP title
   screen. Run61 verifies readable dialog text after P8 palette sampling.
   Cycle59 fully lifts/builds six further seeded entries (23 seeds total,
   37,752 lifted functions, zero failures). Its full lift removed the ignored
   preflight `gen/recomp_9999.c` chunk automatically.
7. **Input/audio:** main defaults `RECOMP_KEYBOARD=1`; Enter=Start, Z=A,
   X=B, A=X, S=Y, arrows=D-pad. `RECOMP_INPUT_TRACE=1` logs guest USB report
   changes; `RECOMP_APU_TRACE=1` logs voice cursors and output sample peaks.
   The corrected cursor trace reads actual voice registers. Older debug-struct
   cursor fields were never populated; their zeroes did not prove a freeze.
   Host key handling alone does not prove guest input. Native regression
   tests now cover OHCI done-list ownership until acknowledgment. Run60
   records Start/A USB report edges; game-level responses still need checking.
   Its later input stop was the guard counting acknowledged WDH reports;
   the repaired guard and P8 font sampling pass native tests. Run61 reached
   600 s / 2,214 paths with zero dispatch misses/crashes/controller stops,
   readable title text, and more than 5,400 acknowledged OHCI status writes.
   Its save-check stall was XAPI polling a NULL thread object after the
   underlying host thread exited. Run62 tests the new guest thread-object
   views (SignalState +4, ExitStatus +0x120), including NtClose/ref lifetime.
   Run62 passes that poll and opens menu music/cache files, then exposes
   internal switch arm 0x7F052 and crashes. The helper now preserves the
   containing function 0x7F040..0x7F077; a local complete lift passes 4,099
   native-x86 comparisons. Build63 succeeded; run64 reached 312.02 s / 2,567
   paths with zero misses/crashes, past the save check and switch failure.
   Do not seed the internal arm separately. Its frontend now draws black;
   compare register-combiner alpha with the sampler's unconditional diffuse
   alpha multiplication before changing fades. `build/combiner_snapshot.py`
   reads the current unhandled combiner words by MAP/ASLR.
   Run63 separately crashed during USB init with a wild endpoint head.
   A locked DPC queue now rejects duplicate KDPC insertion and implements
   cancellation; its concurrency regression passes. Build65 includes it;
   run65 now renders Sign In/Advance controls and accepts a long A press into
   the Gators/Longhorns screen (window110). Short presses may miss the slow
   guest USB poll. A second long press transitions to a stars background;
   further menu transitions/rendering/pacing need tracing. No combiner change
   was required for that response; do not disable alpha fades based on the
   earlier black captures. Run65 is still live past 550 s with zero misses or
   crashes. A full guest GIL is not implemented.
   Run65 was finally stopped at about 906 s / 4,019 paths, clean. Builds66/68
   add copy-at-flip presentation and a persistent physical DMA walker: CALL,
   RETURN, ring jumps/wrap, partial packets and GET-after-consumption. Native
   regressions pass; run68 reaches completed frontend/team captures without
   misses/crashes/parser desync at four minutes. Run66's one miss **0x41FF34**
   is a RET inside sub_0041FF26, not a normal new entry; inspect the new guest
   register/stack diagnostic before seeding anything. Run67 reproduced the USB
   wild-link crash with zero misses; DPC deduplication alone did not fix it.
   Raised IRQL now gates protected driver regions and ISR/DPC callbacks;
   native nesting/concurrency and OHCI ISR checks pass. Run69 ends at 318.1 s /
   7,850 paths, zero misses/crashes/parser desync/IRQL warnings. This single run
   does not prove the intermittent USB fault gone. Build70 succeeds after a
   survey-only DMA integration correction. Guest transform mode 6 selects
   vertex programs, whose uploads are still ignored; port the reference VP
   path next while retaining P8, video and fades. Exhibition remains unverified.
   Later checkpoint: run74 reaches 612.9 s / 37,308 path attempts, clean, with
   readable team selection. Run75 reproduces the stale USB stack request at
   590 paths. PIC mapping, guest dispatcher state and a passive wait barrier
   are real repairs but insufficient. Build76 is compiling a full FIFO guest
   CPU lock with 52,137 entry/backedge safe points. Native tests cover exclusion,
   nested waits, pure polls, ISR lock order and raised-IRQL retention. The
   translator emits hints on future lifts; existing chunks can be instrumented
   with `py -3 -m tools.recomp.scheduler_hints mvp06/src/recomp/gen`.
   RECOMP_GUEST_SCHEDULER is enabled; RECOMP_GIL=0 is diagnostic. VP uploaded
   program/pixel tests pass; runtime acceptance waits for a stable run. Check
   build76 and test frontend/team advancement. Mixed host/guest pulses and
   full APC semantics remain incomplete.
   Build76 succeeded. Run76 was cut off by the diagnostic watchdog (_exit(3)
   after its timeout); run77 then reproduced USB's stale request with zero
   misses at 590 paths. The full CPU lock alone is insufficient. Build78
   succeeds with RECOMP_WAIT_TRACE; run78 is diagnosing waits. Audit OHCI's
   inconsistent register/HCCA clocks, MIE-masked hardware progress and
   frame-publication/WDH ordering. Hardware must progress while guest IRQL is
   raised; do not simply lock all DMA under the CPU lock.
   New native checks: kernel's `nv2a_pb_scan_smoke/Release/nv2a_pb_scan_smoke.exe`
   and `irql_smoke/Release/irql_smoke.exe`; video's
   `framebuffer_present_smoke/Release/framebuffer_present_smoke.exe`.
   Nonzero audio samples were observed in run58a, without audible validation.
   Native regression executables live under `build/xboxrecomp/src/`: input's
   `Release/xinput_keyboard_test.exe`; kernel's
   `nv2a_pb_smoke/Release/nv2a_pb_smoke.exe` and
   `thread_object_smoke/Release/thread_object_smoke.exe`; USB's
   `ohci_dma_smoke/Release/ohci_dma_smoke.exe`; APU's DMA/output smoke tests.
   Both sample/stack tools accept `--pid` to inspect an existing run without
   killing it. Stack scanning clips reads to the mapped stack region.
   Kernel's `dpc_queue_smoke/Release/dpc_queue_smoke.exe` covers duplicate
   insertion, cancellation, capacity/wrap and concurrent producers.

## Conventions and traps

- Latest checkpoint: full build87 succeeds and run87 renders the first live
  stadium, pitcher card and manager huddle; P1 input is accepted. It stops at
  739.1 s / 3,417 paths after ten misses at 0x239420, no crash/direct-stub/USB
  cancellation. Native-tested predicate recovery passes 38,148 comparisons
  and build88 succeeds as a local preflight (seed29, chunk9999, sorted dispatch).
  Run88 PID23524 starts at 01:28:05 on 2026-10-11 for acceptance.
  No full disasm/lift88 yet; consolidate after useful progress. Full generation
  removes chunk9999. FTRACE now includes RC stages/constants, decoded textures
  and all four UV sets. World rendering is very dark; first pitch unverified.
  Earlier runs81/82/83 have zero dispatch
  misses/crashes/USB cancellation traces (run82: 427.9 s / 28,271 paths;
  run83: 17,171 paths). These are encouraging samples, not proof that the
  intermittent USB race is eliminated. Read the latest HANDOFF before running.
- VP viewport methods alias constants 58/59. IRQL must be published as a BYTE
  at each current TIB's fs:[0x24], alongside native TLS state. New TIBs start
  passive. OHCI DMA continues with MIE masked; do not put all DMA under the
  guest CPU lock. FmNumber/HCCA share real elapsed time; FmRemaining is partial.
- RECOMP_FRAME_TRACE=<flag path> records one frame when that file appears.
  Create it only after reaching the screen of interest. Draw-time state is
  stronger evidence than a boot/default snapshot. Build83 implements the
  scale-two combiner/four 2D stages/all immediate attributes. Many missing
  widgets are also offscreen or transparent. LAHF emitted 586 no-ops; its
  repair passes native float/NaN and all 65,536 ADD8 flag checks. Full lift84
  succeeds; build84's repaired chunk3 rebuild/link succeeds at 23:19. Run84
  renders both intro movies, the complete main menu and team selector. Start
  advances jersey selection to exhibition loading. Run84 ends at 567.6 s /
  2,874 paths with one miss at 0x7AE92, then an AA55AA55 pointer crash; zero
  direct-stub hits or USB cancellation traces. This is another internal switch
  arm: helper now also preserves 0x7AE80..0x7AECB. Its complete lift passes
  45,056 native-x86 comparisons. Build85 succeeds; run85 passes that failure,
  then ends at 266.1 s / 3,067 paths with new misses 0x171D40/0x2C8AD0 and
  direct stub 0x288EB0, followed by a FFFFFFFE pointer read. These are verified
  standalone bodies and are seeded for full cycle86 (26 seeds). Disasm86
  adds only those three bodies; full lift86 succeeds (37,755 functions, zero
  failures). Full build86 succeeds; run86 ends at 1,107.5 s / 3,272 paths,
  after both movies complete naturally without input. Its two new misses
  (0x157ED0/0x2CC3A0) precede a FFFFFFF4 read; zero direct-stub hits or USB
  cancellations. Both entries are now seeded (28 total); disasm87 adds only
  those entries and splits 0x2CC0A0 at the second one. A native callback audit
  exposes scalar CVTSS2SI truncation; the repair passes 28,672 comparisons
  across four rounding modes. Full lift87 succeeds (37,757 functions, zero
  failures); Release build87 succeeds. Run87 PID1760 starts at 01:09:29
  on 2026-10-11 for acceptance. Toolkit tests pass
  239 / 24 skipped / 44 subtests. OHCI now exposes four ports, with MVP's
  RECOMP_USB_PORT=2 default selecting P1's root port 3; native arrival/reset/
  DMA/IRQ tests pass. Run87 verifies P1 in the team selector and keyboard
  advancement through jersey/difficulty (`build/teams87_p1.png`). Native
  parameter-copy comparisons pass 8,192 cases; its generated body matches.
  Do not seed 0x7AE92 separately. Some uncommon flag producers, fog evaluation, cube/volume/
  bump samplers, depth and near clipping remain incomplete.
- Enable RECOMP_STUB_TRACE=1 when investigating logic. Generated direct-call
  stubs now print [UNLIFTED] once per reached target, preserving their stack
  cleanup. Zero indirect dispatch misses alone does not cover these stubs.
  Inspect original bytes/ownership before seeding an interior address.
- `build/combiner_snapshot.py` reads unhandled-method storage and is stale
  for combiner registers handled since build83. Use RECOMP_FRAME_TRACE.
- The reference audit is in ENGINEERING_LOG, runs79–82. Burnout 3's current
  docs/boot.md follows the external toolkit; its in-tree runtime and old
  CLAUDE.md are stale. NFSU2 matches the TLS/CPU-lock model; Mercenaries does
  not. Preserve P8/video/fades and physical resolution when porting rendering.

- x64 CONTEXT offsets when sampling: Rip=0xF8, Rsp=0x98 (0xF0 is R15).
- Guest registers are TLS globals (`g_eax`, `g_esp`, ...) declared
  `RECOMP_TLS`; a plain `extern` mismatches storage class silently.
- Direct calls in generated code cannot be intercepted via
  `recomp_lookup_manual` — patch the generated body or call sites (note in
  the comment that re-lift wipes gen patches; re-apply or move the intercept
  into the runtime).
- The `.map` file maps host addresses to `sub_XXXXXXXX` symbols; offsets
  after `+` are host offsets, not guest VAs.
- `RECOMP_PEEK`/`PEEK_CHAIN` only accept guest RAM VAs (< 0x04000000) and
  only print while the pushbuffer survey ticks (needs RECOMP_NV2A_TRACE=1 or
  RECOMP_PB_SCAN; execution alone no longer implies diagnostics).
- Write C with the Edit tool, never bash heredocs (line-continuations get
  mangled). MSVC errors: check `mvp06/build_log*.txt`.
- A leftover game process locks the exe and fails the link with LNK1104:
  kill `mvp06_recomp` before rebuilding.
- After adding/removing generated chunks, explicitly run
  `cmake -S mvp06 -B mvp06/build` before `cmake --build`. MSBuild can retain
  the old item list for its current invocation even when CMake's glob check
  regenerates the project: build88 omitted a newly added chunk; build89 tried
  to compile the removed chunk9999. A retry reuses completed object files.
- All labels being live does not prove a lift correct. Check for overlapping
  decoded instructions and include the final return instruction in the
  exclusive function boundary (`memcpy` ends after `ret` at 0x44302C).
- The D3D8 API here is XDK 5849 plain D3D8. Device struct at `[0x589A38]`;
  pushbuffer fence pair at +0x2C/+0x30 (same as Burnout 3 and NFSU2).

## Definition of done (project)

Current checkpoint: run88 parks in exhibition loading, 740.5 s / 2,824 paths,
zero misses/stubs/crashes/USB cancellation traces. Cycle89 fully lifts all
29 seeds (37,758 functions, zero failures), restores four boundaries and USB
hooks, removes chunk9999 and passes `build/audit_cycle89.py` body/dispatch
checks. The first build89 invocation fails on a stale chunk9999 source item;
explicit CMake configure and build90 succeed, including the renderer Q-default
and ordinary blend repairs (native pixel checks pass). Run90 PID28888 starts
at 02:12:36 on 2026-10-11 and renders both intro movies, a bright live stadium
and the pitching UI. Stopped at 663.8 s / 3,351 paths with eleven new misses
at 0x256590; zero crashes/stubs/USB cancellations. Seed30 has a complete
645-instruction/five-return lift, no unsupported/dead code and known direct
callees, installed in chunk9999 for preflight91 (build succeeds; run91 PID16680
starts at 02:29:15 on 2026-10-11). No full
cycle91 or whole-body native comparison claimed. Boundary helper now also
preserves 0x256590..0x256F4E: five ranges total. The 623-draw richer trace is
`build/frame90_trace.txt`; negative repeat UVs are incorrectly clamped and
depth remains absent. Read-only `build/thread_state89.py PID` passes live boot/intro
checks and must use the loaded executable's MAP. Capture the GIL owner
and IRQL before stopping another parked run. NFSU2 APU-lock starvation is a
lead, not a demonstrated cause. A successful pitch/exhibition remain unverified.

Playable: exhibition game completable with visible rendering, audio, and
controller input; saves persist. Stretch: Dynasty mode stability, then the
enhancement program (resolution/AA/texture replacement per PLAN.md Phase 4).
