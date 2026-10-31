> Historical snapshot archived at checkpoint185, 2026-10-31. Live process, default and next-action statements below are historical. Start with [the current handoff](../HANDOFF.md).

# MVP 06 NCAA Baseball — Handoff

Checkpoint180 (2026-10-31): normal179 run180 crashed at startup with
0xC0000005 in the guest TLS lookup (sub_0031D5BF). Cause unresolved; normal179
startup and non-diagnostic FPS are unvalidated. Investigate thread TIB/TLS
initialization next; see PERFORMANCE_WORK176_179.md for the recorded failure.

Latest179: see PERFORMANCE_WORK176_179.md.178 warm reverse shows bounded
handoff improves selector3.7735→4.05FPS (~7.3%), unscoped resolves35.5% lower.
Native GIL0/1/publication/IRQL/DMA and walker partial/PUT expiry checks pass.
176/177 waits regressed startup due stale request; disabled/stopped.178 fixes
expiry at actual captured PUT. Two pitches/count2–0/pause/resume PASS, exact11
downs,3,462 PATH/no logged failures. Live178 PID18740 keeps yield1/us2000,
hold0/poll100/timing0. Never relink its EXE/MAP. Normal179 rebuilt with MVP
hardware defaults honoring overrides; toolkit defaults remain off.179's profile
matches accepted178 settings; no separate full179 exhibition claimed. Optional
GPU_OWNER_TRACE/native on/off tests and private reader ready; actual owner
attribution and normal179 no-diagnostic FPS next.30FPS not met; new work
uncommitted. Inspect current process/log before relying on historical PIDs.

Latest174/175: PERFORMANCE_WORK174.md records matched direct-buffer comparison.
Initial apparent gain fails warm reverse; runtime option/switch/stats-field
and mode-transition tests removed. Native probes retained; restored backend175
build and HW/WARP packed checks. Source snapshot/evidence under build/experiment174.
Live174 PID24804 remains staging0/hold0/poll100/timing0; do not relink EXE/MAP.
Selector frame107/error1.70;3,412 PATH/exact7 frontend downs/no logged failures.
Old171 deliberately stopped (exitFFFFFFFF), normal169 EXE unchanged.30FPS is
unachieved: samples3.25–3.85. Next reduce readback/ownership round trips with
verified visibility; report lists obligations before GPU residency can be safe.
New work remains uncommitted. No175 game executable built/promoted.

Latest173: see PERFORMANCE_WORK172_173.md. Live171 PID17020 remains confirmed,
hold0/poll100/timing0. Matched-MAP thread sample confirms rendering waits.
Driver-thread experiment removed; mapped default color textures slower for
large reads. New173 opt-in direct packed-UAV readback removes a staging copy,
keeps all visibility/completion rules, passes hardware/WARP exact512 and2,605
draw suites plus packed-off suites. Native ABBA inconclusive: no speedup claim.
173 separately built, not launched/promoted; default OFF. Live171/normal169
unchanged. Next matched gameplay comparison or reject the prototype.30FPS
remains unachieved. New code/tests/docs uncommitted; never relink live171.

Latest171 (2026-10-30): objective steady30FPS gameplay, then rendering/accuracy.
Read PERFORMANCE_WORK170_171.md. Exact block row comparisons pass614,784
oracle cases and four renderer modes, zero failures; native sparse-row~7x
is NOT a gameplay claim. Separate171 PID17020 is live, hold0/poll100,
timings off. Selector verified frame097;20s samples3.900/3.950FPS, zero audio
rejects, exact7 frontend downs,3,416 PATH/no logged failures. No whole-game
speedup or30FPS claim. Investigate larger readback/coherence costs next, retaining
all CPU/alias/fence visibility. Read selector171_gate.txt
and current process; no prior process was live on resume. Diagnostic170 was
deliberately stopped; its25.9intro/48.9title samples do NOT prove gameplay FPS.
Normal169 EXE unchanged; do not relink live171 EXE/MAP. Two-pitch/full-game
acceptance remain pending. New source/tests/docs remain uncommitted.

Latest169: normal Release/mvp06_recomp.exe rebuilt with adaptive idle flag
poll100us, zero/invalid fallback to old Sleep(0). Two-ms fast burst retains
WC progress. Matched168 ABBA/BAAB worker CPU93–94% lower, FPS roughly3.9–4.1,
no material FPS gain established. Bounds167 reduced readback~7% without reliable
FPS improvement. All native IRQ/trap/ownership tests pass. Live168 PID10620
passed two pitches/count2–0/pause/resume; key hold now0, poll100. Original164
PID15096 deliberately stopped as superseded AFTER acceptance. Do not relink
live168 EXE/MAP. Default169 is ready for next launch, while168 retains gameplay.
See [PERFORMANCE_WORK167_168.md](../PERFORMANCE_WORK167_168.md) and private
build/acceptance168_build169.json for hashes/input/log audit. First11 downs
match acceptance; subsequent inputs observed after resume. Full-game/save and
comfortable FPS remain open; existing GPU/RAM/IRQ contracts remain mandatory.

Latest165/166 work: observer attribution implemented;165 demo interval external
resolves58.3% pattern,31.5% software,10.2% clear. Bounds candidate166 ignores
wholly clipped triangles when forming readback unions: original1,024→144 bytes
for identical native fixture pixels; all four renderer modes pass2,605 batches.
Separate165/166 demo checks each2,652 PATH entries/no crashes/stubs, then both
diagnostics deliberately stopped. No matched FPS gain claimed. Original164
PID15096/default executable remain unchanged/live. Candidate is separately named
Release/mvp06_recomp166.exe, not promoted. See
[PERFORMANCE_WORK165_166.md](../PERFORMANCE_WORK165_166.md) for evidence/resume.

Performance investigation164: prior gains retained (renderer/memory source
hashes exactly161; live fallback512, yield/timing off, scopes/packed on).
Current post-pitch samples4.649/5.150/4.750 FPS, about63 resolves and20 MB
readback/16 MB upload per frame, similar to prior160 work per frame.
Earlier3.850 FPS was player introductions, not the prior stationary selector.
No matched regression established; current selector gate timed out. GPU sync
waits remain prominent; flag worker also consumes CPU. No runtime edits or
tuning writes; PID15096 live. See [PERFORMANCE_CHECK164.md](../PERFORMANCE_CHECK164.md).

Latest164 acceptance: manual Play Now route now reaches Florida at Texas,
stadium/player/manager introductions and interactive pitching at0–0. Live
screens visually checked; two-capture pitch selector check passes (error2.04).
No new logged crash/stub. Intro sample3.850 FPS/48,049 accepted audio frames/s,
zero rejects, no matched speedup claim. Two manual pitches and pause/resume
requested/pending. PID15096 remains live, unchanged164; never relink live.

Resumed164: user restored the window. PID15096 passed the prior loading crash
and visibly runs the pitching mini-game demo, 2,607 PATH entries, no logged
crash/unlifted stub. Demo sample4.850 FPS /47,997 accepted audio frames/s,
zero rejects; no matched performance claim. Automated Enter pulses reached
the window but did not exit demo; manual two-second Enter hold requested,
pending. Exhibition/two-pitch acceptance remains open; game still live.

Latest audit (2026-10-23): Release164 builds; winding-normalized software
top-left coverage and the complete 0x3DD40 event-switch lift pass native tests.
Unchanged161 run163 reaches an unlifted shared epilogue, then crashes during
mini-game loading. Run164 acceptance is incomplete: user stopped Computer Use
with Escape; the game was left running. Inspect its process/log/exit164.json
before further work and never relink live. Full evidence and resume steps:
[GAMEPLAY_RENDER_AUDIT.md](../GAMEPLAY_RENDER_AUDIT.md).

One-page state of the world for whoever picks this up (human or agent).
Companion documents: [ENGINEERING_LOG.md](../ENGINEERING_LOG.md) (full history),
[../AGENTS.md](../../AGENTS.md) (working instructions), [PLAYABILITY_PLAN.md](../PLAYABILITY_PLAN.md)
(audited plan to playable). Repo root README covers build steps.

## What this is

A native Windows x64 port of MVP 06 NCAA Baseball (Xbox, 2006) built by static
recompilation with the xboxrecomp toolkit (upstream v0.11.0 + this project's
fixes, all in this repo). Game project: `mvp06/`. Everything runs from the
recompiled game code — no hand-written game logic.

## Paused checkpoint (2026-10-20, Release161; partial run162)

User requested a stopping point. No game/test helper live. Release161 links
07:59:42,59,192,832 bytes; source/EXE/MAP hashes in build161_hashes.json. Default
synchronous failed-exclusion fallback512 words is promoted from the controlled
160 experiment. Exclusive slices remain4096/coarse2 ms; RAM/GET/ISR/publication
barriers unchanged. Guarded256/512 plus4K/16K publication tests pass GIL0/1.
Four known memory-layout warnings match107; no new warning sites.

Same-process normal160 warm4096/512/4096/512 samples:5.149/5.350/5.150/5.400 FPS,
20 s each after5 s settle. Mean5.1495→5.375 (+4.38%) at the pitch selector;
not a whole-game FPS guarantee. Earlier4096/256 and neighboring256/512 reverses
support512. All12 phase input audits pass; diagnostics/GPU_YIELD off. Unscoped
resolves36→24/flip, total resolves roughly63→64: no total readback reduction.
More tiny draws use scoped hardware; attribution of the gain remains an inference.

160 then holds512 and passes two pitches/count2–0/pause/resume16:29:06, with
visually checked captures and exactly ten helper downs. Stopped2026-10-19
21:00:42 after17,239.150 s/3,763 paths, zero misses/stubs/crashes/USB cancels/
lost-syncs.8,353 captures/maxgap3.089 s;8,618 audio windows/3,231,807 attempts/
zero rejects. Software536,261 posted/completed; word counter is uint32 and
may wrap, so do not report its final value as a lifetime total. Session muted;
no listening-quality or full-exhibition claim. Latest full acceptance is160
on159 with512 selected; the compatible same-height allocation fix is included.

162 on161 starts08:02:27 PID19708; intro capture01108:02:55 visually passes.
Last log08:03:06 has591 paths and zero logged failures; route never reaches
title or sends input. Game/helper already absent when stopping point requested;
no exit status/reason available. This is incomplete validation, not a clean
startup acceptance or a diagnosed regression. Relevant application events are
saved in events162_checkpoint.json; inspected reports concern other processes
and older watchdog dumps, not an established162 cause. No later runtime edits.

Resume: fresh normal163 on161, retain an owned-process handle/exit status, finish
frontend/selector/two-pitch/pause validation. Then attribute external flush sites
and pursue fewer GPU/RAM round trips. Study Mercenaries' compatible surface views
and xemu's dirty-page/CPU observers before adopting residency; NFS/Burnout review
is incomplete. Preserve all CPU/alias/fence/IRQ barriers and session mute.
experiment_fallback159.py assumes/restores4096: adapt to initial/final512 first.

## Historical experiment setup (2026-10-19, Release159; completed normal160)

Release159 links16:12:30,59,192,832 bytes, default fallback4096 unchanged.
Normal160 originally PID23988 from16:13:23, all diagnostics/GPU_YIELD off; never
relink live. After selector, same-process4096/256/4096/256 synchronous fallback
budgets,5 s settle/20 s FPS, key audit each phase and finally restore4096.
Only failed-exclusion synchronous progress changes; exclusive slices stay4096/
coarse2 ms, original software interrupt/GET/publication/RAM behavior stays.
Private setter verifies owned process/loaded PE+MAP stamp/ASLR/aligned module
data/expected value and readback. Guarded256/4K/16K publication, walker/continuous
producer and memory/host waits/raised-IRQL/DMA tests pass GIL0/1. Four unchanged
memory-layout warnings match build107; no new warning sites. No gain claimed.

Normal158 on157 stopped16:11:45 after474.511 s/3,421 paths, clean; exactly six
key-downs, no pitches. Selector135/three samples4.45/4.50/4.65 FPS, all counters
off/zero, audio47,970/48,035/47,996 accepted frames/s, zero rejects. Smaller
allocation fix reduces native work but has no established normal FPS gain.
Latest full acceptance remains152 on151. Profiling158 failed before sampling
because cwd must be mvp06; do not cite its error file as stack evidence.

## Prior performance candidate (Release157; normal158)

User resumed FPS work. Current157 links16:03:08 (59,192,832 bytes), no warnings;
runtime renderer exactly equals153's same-height depth-toggle candidate. It
retains compatible color/depth resources when a HUD disables depth; compares
pending active depth separately so grouped HUD draws keep their original
barriers. Native control/candidate/candidate/control reduces same-height
depth/HUD pair7.85–10.77 to2.53–3.02 ms. AMD/WARP packed/legacy each pass2,600
renderer draws; prior153 PB79-draw and cycle/D3D audits remain applicable.
Diagnostic153 unchanged151 selector3.80/3.849 FPS;154 on1534.60/4.75 with
six exact helper key-downs and complete captures. Normal158's three samples
do not establish a normal FPS gain. Full acceptance on157 remains pending.

REJECT155 larger-height capacity reuse: native clipped-HUD timing improved,
but normal156 only2.498/1.850 FPS. STOP16:02:39 after521.226 s/3,418 paths,
clean; first pitch/count1–0 passed, helper deliberately stopped before second.
No full acceptance or normal FPS gain claimed for155. Height reuse and its
extra partial-depth preservation change are fully reverted; keep exact-height
allocation changes and pixel regressions. Latest full controlled acceptance
remains152 on151 below. Preserve session mute and all RAM/alias/IRQ barriers.

## Previous paused checkpoint (2026-10-19, Release151; normal run152)

Paused at the user's request. No game or test helper live. Normal152 stopped
15:22:07 after713.483 s/3,427 path attempts; zero dispatch misses, direct stubs,
crashes, USB cancellations or lost-syncs.355,170,383 command words execute;
software18,700 posted/completed in the pre-stop snapshot. Both movies/full
frontend/stadium pass. First count1–0 at17815:16:26, second selector30215:20:44,
count2–0 at31015:21:01, pause31315:21:07/resume31715:21:16 PASS, visually
inspected. All ten key-downs exactly match completed helper commands.
Normal selector4.549/4.65 FPS; accepted audio48,020/47,996 frames/s, no rejects,
all timing diagnostics zero.357 audio windows/133,363 attempts/zero rejects.
Host active/gain1/peak0.106567383 but SESSION MUTED; preserve user preference,
no listening-quality claim. Latest complete acceptance is152 on151.
Next investigate readback synchronization and remaining audio stalls, then
full exhibition and save/reload. Keep4,096/coarse2 ms,128 slots/128 MiB,
GPU_YIELD off and every publication/coherence/CPU-observer/IRQ barrier.

### Release151 repair and diagnostic evidence

Release151 links14:58:23 (59,192,832 bytes), no new warnings/later runtime
edits.152 PID13452 ran15:10:14–15:22:07 with all diagnostics/GPU_YIELD off.
It prepares each32-sample slice before processing and tracks all eight fresh
slices before256-frame submission. This preserves earlier valid hardware
audio when the final slice is off and prevents stale replay when earlier
slices are off. Standalone stopped/stale calls, mute/tone, software mixing,
device-queue pacing/ownership, RAM, scheduler and IRQ behavior remain.
Before regression:448 lost-channel/128 stale-channel mismatches. After:
all256 patterns/131,072 PCM comparisons, output/mute/stop/DMA/sink pass timing0/1.
Default-off diagnostics check actual off frames and nonzero partial blocks
that the old code erased; native enabled counts1,024/127 versus zero when off.
State size+8, existing offsets unchanged; fresh native layout confirms this.
Cycle/D3D151 pass33 seeds/37,762 lifts/seven ranges, memcpy exclusive44302D.
Intro20s processes48k frames/s, no empty observations/rejections, six preserved
partial blocks; frontend preserves22 more/20 s. Diagnostic151 stops15:09:55
after646.801 s/3,384 paths, zero failures/lost-syncs;244,989,269 words,
software13,146 posted/completed.323 audio windows/120,392 attempts/zero rejects,
session active but muted. User confirms playing:122 keyboard transitions
versus12 expected route edges. Priority0/2 reverse tool/API works and restored0;
its timings are confounded by user-controlled scenes, no priority policy adopted.
Normal152 acceptance passes with exact key-down provenance, as recorded above.

## Previous complete two-pitch acceptance (2026-10-19, Release150; run150)

STOP15014:53:49 after766.605 s/3,420 paths, zero misses/stubs/crashes/USB
cancellations/lost-syncs;347,889,013 words. Software posted17,197/completed17,196
in pre-stop snapshot. Both movies/full frontend/selector107, first count1–0
at15714:46:35, second selector30714:52:04, count2–0 at31614:52:23, pause319/
resume324 PASS; captures visually inspected. Normal selector4.35/4.00 FPS,
accepted audio47,996/47,970 frames/s, zero timing counters. No causal FPS gain
claim.383 audio windows/143,046 attempts/zero rejects; host active/gain1/
peak0.109955 but SESSION MUTED. Preserve preference; audible quality unverified.
150 input audit shows two extra Enter pairs before the expected route and
some omitted key-up traces (focus clearing can do this). Its helper-confirmed
pitch/count/pause evidence is valid; strictly unattended input is unproven.
150/151 are closed. No normal151/full exhibition/saves/comfortable FPS/original
accuracy claim. Normal152 now passes on151. Keep4,096/coarse2 ms,128 slots/128 MiB, GPU_YIELD off
and every RAM/publication/CPU-observer/alias/IRQ barrier. Priority candidate
has no controlled benefit established; normal priority restored.

## Historical paused checkpoint (2026-10-12, Release150; partial acceptance149)

Paused at user request. No game/helper live. Current Release150 links08:15:15
(59,192,320 bytes), no new warnings/later runtime edits. It only adds default-off
APU maxima/histograms for raw32-frame RAM acquisition and256-frame production
intervals, pipeline maximum and diagnostic thread ID. Existing timestamps are
reused; no cadence/sample/RAM/priority changes. APU output/sink/DMA tests pass
timing0/1; cycle/D3D150 pass33 seeds/37,762 lifts/seven ranges, memcpy44302D.
150 is built/native-tested, not live-tested.

Normal149 runs unchanged148. STOP08:15:07 after521.363 s/3,417 paths, zero
misses/stubs/crashes/USB cancellations/lost-syncs;188,023,527 words, software
posted11,226/completed11,225 in pre-stop snapshot. Movies003/021/full frontend
through difficulty070/stadium/selector135 pass. First selector20008:13:31,
A down/up08:13:33, count1–0 at21208:13:57 visibly passes (83 MPH). Stopped
while waiting for the second selector at user request; no second-pitch/pause
acceptance claimed. Latest complete two-pitch/pause run remains146 on older145.

Normal selector FPS3.90/3.649, still too slow; no causal gain claim. Accepted
audio47,969.716/48,005.793 frames/s, zero rejects; all APU/GPU timing counters
zero.260 audio windows/96,886 attempts/zero rejects, host active/unmuted/
peak0.129150391. Submission rate does not prove playback continuity/quality;
prior148 stadium playback46.1k/14 empty observations remains an open lead.

Next unchanged150 normal run150 (all timings/history/frame polling/NV2A
tracing/GPU_YIELD off), selector measurements and full two-pitch/pause test.
Then151 with --apu-timing and read_audio_timing150.py to measure long wall-time
stalls, including descheduling. NFSU2's APU relative priority2 is a candidate;
set_apu_priority150.py is prepared but untested, no priority changed. If data
supports it, measure0/2/0 in the same process and restore0. Keep4,096/coarse2 ms,
128 texture slots/128 MiB and all RAM/publication/observer/IRQ barriers. Full
exhibition, saves, comfortable FPS and original comparison remain open.

## Historical paused checkpoint (2026-10-11, Release148; partial run148)

Paused at the user's request. No game or test helper is live. Release148 links
18:59:19 (59,192,320 bytes), no new warnings and no later runtime edits. New
Windows audio pacing uses NFSU2's eight256-frame device-queue target (~43 ms)
inside the existing twelve-slot ring. It admits APU register writers once per
block and preserves sample bytes, rate, RAM leases and fallback behavior.
Default-off RECOMP_APU_TIMING measures deadline loss, waits and real voice
SamplesPlayed every2 s; read_audio_timing147.py checks loaded PE/MAP and uses
the voice snapshots' own clock. Queue-only GetState never supplies sample counts.

147 selector playback45,634 frames/s corresponds to102 deadline resets losing
1.012089 s/20 s. New native empty/full/pause regression fails before/passes
after; monitor/mute/stop/DMA and sink3,247 FIFO blocks/500 wrap/backpressure
cycles/14 host-failure retries pass with timing0/1. Cycle/D3D148 pass unchanged
33 seeds/37,762 lifts/seven ranges; memcpy exclusive44302D.

Run148 STOP19:03:16 after224.089 s/3,325 paths, zero misses/stubs/crashes/USB
cancellations/lost-syncs;36,129,492 words, software posted/completed4,462.
Movies003/021/full frontend through difficulty072/stadium104 visually pass.
Intro47,999 processed frames/s, no empty observations; stadium46,102 with14
empty observations, zero discarded deadline time or submission failures.
Audio112 windows/41,287 attempts/zero rejects, host active/unmuted/peak0.124512.
These stage samples are not a controlled speed comparison. Remaining stalls
can still exhaust the queue; continuous48k/listening quality remain open.

Stopped before selector/pitches at user request. Latest complete two-pitch/
pause acceptance is146 on older Release145. Resume with unchanged148 and a
normal run149 (APU/GPU timing/history/frame polling/NV2A tracing/GPU_YIELD off),
then selector FPS/production and two-pitch/pause acceptance. Afterward measure
long RAM waits before changing leases/headroom. Keep4,096/coarse2 ms,128 texture
slots/128 MiB and all coherence/publication/IRQ barriers. Full exhibition,
comfortable FPS, saves and original-game comparison remain open.

## Historical accepted status (2026-10-11, Release145; run146)

Release145 links18:11:51 (59,190,784 bytes), incremental build no new warnings.
Run146 STOP18:38:27 after884.2 s/3,420 paths, zero failures/lost-syncs;
367,699,808 words, software posted/completed20,983. No game live; never relink
live. Movies/full frontend/stadium, selector133, first selector196/count1–0
at205, returned selector365, second/count2–0 at375, pause378/resume384 PASS.
Six captures/log saved and pause/resume visually inspected. Latest two-pitch
acceptance is146. Timing/history/frame polling/NV2A tracing and GPU_YIELD off;
FPS4.30/4.30, all timing counters/frequencies zero. No causal FPS gain claim.

145's interleaved reverse gives3.091 FPS off/3.012 on, overlapping ranges;
experimental RECOMP_GPU_YIELD stays default OFF. Lease success49.185% off/
44.739% on, Map0.157843/0.164407 s per flip. Keep4,096/coarse2 ms and all
observer/IRQ barriers. Audio146442 windows/156,776 attempts/zero rejects,
host active/unmuted/peak0.101349. Accepted production45,717/45,616 frames/s
versus48,000; diagnostic145 ~43.2k. Actual playback/continuity/quality open.

32 distinct-value automatic/manual handoffs pass requests on/off and GIL
on/off, retaining extra publication/bridge readers and raised IRQL. Publication
4K/16K and scheduler pass145; DMA/hybrid144 and cycle-D3D145 pass33 seeds/
37,762 lifts/seven ranges, memcpy exclusive44302D. Full144 has15 existing shift
warnings matching110. Keep4,096/coarse2 ms;143 cutoff experiments overlap/
reverse. No runtime source edits after145 link. Next measure readback waits/
polling and audio RAM waits/throttle overruns/playback clock. Retain128 texture
slots/128 MiB. Full exhibition, comfortable FPS, saves and original comparison open.

## Historical accepted status (2026-10-11, Release141; run142)

Release141 links16:23:06 (58,485,248 bytes), no game live. Same EXE run142
automatically passes movies/full frontend/stadium, selector145, first pitch/
count1–0 at195, returned selector352, second/count2–0 at362, pause365/resume370.
STOP16:46:14 after855.8 s/3,420 paths, zero failures/lost-syncs;324,153,610
words, software issued/completed17,439. This is latest two-pitch acceptance.
Audio428 windows/151,634 attempts/zero rejects; host active/unmuted/peak0.099823.
Audible quality, continuous production and original timing remain unverified.

Measured texture stages identify changing linear ARGB0x12 pages, with ZERO
selector cache misses. Retain128 entries/128 MiB. Decode these exact-format
pages by copying visible width*4 bytes per pitched row, retaining all source/
palette validation, CPU edits, aliases and RAM/ISR barriers. Other formats keep
the scalar path.625,422 native scalar comparisons pass tight/padded/odd-width
rows, alpha, guards and CPU edits; native decoder2.450→0.0369 ms per640x480.
AMD/packed and WARP/legacy retain2,546 renderer and79 pushbuffer draws each;
cycle/D3D141 pass33 seeds/37,762 lifts/seven ranges, memcpy exclusive44302D.
No warnings or later runtime source edits.

Diagnostic live decode1.31–1.38→0.545–0.555 s/20 with more pixels (~65% lower
cost/pixel); texture total2.19–2.27→1.41–1.44 s. Default-off selector3.70/3.65
FPS, all timing counters/frequencies zero, overlaps older baselines: no reliable
FPS gain. Map still6.63–6.68 s/20. A post-FPS60-round profile shows GPU driver
waits and flag-worker2,093.75 CPU ms withSleep(0); sampled RIPs are not exact
waiting durations or call stacks. Evidence timing141_comparison.json and
render142/pitchloop142/final runtime/summary/stop/audio files in ignored build/.

Next measure readback counts/bytes/scope-break causes and polling costs while
preserving all CPU observers and interrupt handoffs. Measure remaining
swizzled R5G6B5 decode separately; do not infer steady eviction from loading
misses. Audio needs actual elapsed/source/playback-clock evidence. Keep4,096/
coarse2 ms; full exhibition, comfortable FPS, saves and original accuracy open.

## Historical139 checkpoint

Release139 links15:46:50 (58,484,736 bytes); no game live. Run139 automatically
passes both movies/full frontend/stadium, selector137, first pitch/count1–0
at218, returned selector370, second/count2–0 at379, pause382/resume387.
STOP16:02:53 after867.9 s/3,427 paths, zero failures/lost-syncs;371,857,489
words. This is latest two-pitch acceptance. Cycle/D3D139 and APU tests pass.

Repair verified audio submission loss: the monitor supplies256-frame blocks,
so three queued buffers covered16 ms despite1,024-frame storage per slot.
Use NFSU2's12 slots/64 ms, preserving samples, mix cadence, rate and filters.
The actual-sink fake-voice regression fails before on block4; after,3,246 FIFO
blocks survive500 completion/wrap/backpressure cycles and simulated host
failures/retries. APU mix/mute/stop/DMA fixtures pass. Native test opens no
device. Live139 records ZERO rejects/153,762 attempted blocks in435 trace
windows, versus4.6–4.8% before. Windows session active/unmuted/gain1.0/peak
0.1254. This verifies rejection pressure on the tested path; sound quality,
continuous production and original playback timing still need evidence.

Observed selector3.950/4.150 FPS (79/83 flips,20.001 s each), all diagnostic
timing/history/frame polling/NV2A tracing off. No controlled reverse repeat;
do not claim a causal speedup. Each sample has10 audio trace records,3,531/
3,523 attempts and zero rejects. Private reader --audio-log captures records
ending in its FPS interval; their~2 s windows can cross its edges.

Next: separate texture compare/decode/create timing while preserving RAM,
palette, alias and CPU-edit coherence. Audio diagnostics need actual elapsed
trace/source queue/playback-clock measurements; do not equate zero rejects
with continuous48 kHz output. Keep4,096/coarse2 ms and all original ISR/
observer barriers. Full exhibition, comfortable FPS and original accuracy open.

## Historical138 checkpoint

Release138 links15:26:06 (58,484,736 bytes); no game is live. Run138 fully
automatically passes movies/full frontend/stadium, selector146, first pitch/
count1–0 at217, returned selector391, second/count2–0 at402 and pause405/
resume411. STOP15:41:07 after891.7 s/3,426 paths, zero misses/stubs/crashes/
USB cancellations/lost-syncs;329,550,218 words. This is latest two-pitch
acceptance. Matched20.001 s samples3.500/3.450 FPS equal136's corrected
baseline: no gain established. Timing/history/frame polling/NV2A tracing off,
all timing counters/frequencies zero. Cycle/D3D138 audits and native137 pass.

Optional RECOMP_GPU_TIMING=1 now measures aggregate renderer/RAM costs, with
no history, per-draw I/O or public stats ABI change.137's six20 s selector
windows find readback Map5.99–6.45 s, commit/copy1.01–1.08 s, texture work
2.41–2.56 s and surface compare1.52–1.73 s. GPU holds RAM7.66–9.92 s/20;
reader-acquisition time sums25.42–30.76 s across threads. Totals overlap;
lifetime maxima219/218 ms include earlier scenes. These are diagnostic wall
times, not exact GPU utilization or a product FPS baseline. Keep4,096 words,
coarse2 ms check and all coherence/ISR barriers. See timing137_summary.json.

Execution no longer enables verbose NV2A diagnostics implicitly; explicit
RECOMP_NV2A_TRACE=1/RECOMP_PB_SCAN retain them. Same-process logging pairs
overlap/reverse; retain as quiet-default cleanup without a speedup claim.
137 timing/selector run stops15:25:58 after728.1 s/3,422 paths, clean; no pitches.

Next: native-test XAudio2 burst/order/wrap/backpressure, then assess NFSU2's
12-block queue (64 ms versus current three256-frame blocks/16 ms). Traces
show4.6–4.8% rejected submissions across136–138; this is not an audible-gap
measurement.138's Windows session is active/unmuted/gain1.0/peak0.1266.
Quality/timing open. For frames, split texture compare/decode/create timing;
do not port NFSU2's sampled hashes or Mercenaries' frame-generation checks as
exact coherence guarantees. Full RAM exclusion/write invalidation and hybrid
CPU depth observers must remain. Full exhibition/playable FPS/accuracy open.

## Historical135/136 checkpoint

Release135 links14:30:27 (58,481,664 bytes); no game is live.136 on that unchanged
EXE passes both movies/full frontend/stadium, first pitch/count1–0 at205,
returned selector366, second/count2–0 at376 and pause379/resume384 with both
tracing flags false. STOP15:03:21 after876.2 s/3,420 paths, zero failures/lost-syncs;
344.9M words consumed. This is latest fully automated two-pitch acceptance.
Never relink a live game.135 also automatically passes both
movies/frontend/stadium, first pitch/count1–0 at257, returned selector461,
second/count2–0 at472 and pause475/resume481. STOP14:48:44 after1,068.9 s/
3,416 paths, zero failures/lost-syncs;343.1M words.16,384-word budget experiment
is REVERTED;135 reverse repeat also
slows to2.45/2.75, so134's2.55/2.70 does not prove causality. No established
gain. Keep4,096/existing coarse2 ms check, all barriers and expanded native
publication coverage. Cycle/D3D135 audits pass.

Measurement trap: older private launchers enable RECOMP_FRAME_TRACE flag
polling even without recording. trace_current_batch opens that missing file
on every draw; sampled command worker has15/60 samples in ZwCreateFile.
Earlier FPS includes this diagnostic overhead. Launcher now omits the flag
unless --frame-trace is explicit;136 records both tracing flags false. Corrected
selector samples are3.50/3.45 FPS; no runtime speedup claim. Optional sampler
--system-exports resolves actual syscall stubs: GPU synchronization waits,
ordinary waits and log writes replace the prior CreateFile hotspot. GPU wait,
RAM exclusion, verbose NV2A
logging and busy flag-worker costs still require direct timing before tuning.

## Historical rejected experiment134

Release134 links14:19:37 (58,481,664 bytes); run134 stops14:29:46 after544.3 s/
3,410 paths, clean. Successful exclusive GPU slices allow16,384 words
instead of4,096, retaining the existing coarse2 ms clock check, all software/
RAM/coherence barriers and shorter synchronous retries. Native publication/
preemption/dispatcher regressions pass both limits, GIL on/off; RAM/DMA and
scanner tests pass. Cycle/D3D134 audits pass. Matched2.55/2.70 samples and a
variable135 reverse repeat establish no useful gain. Runtime experiment is
reverted; retain expanded native coverage. No pitch-loop acceptance on134.

Run133 on132 stops14:19:00 after515.3 s/3,413 paths, zero failures/lost-syncs.
Its same Florida/Texas selector measures2.9/3.4/3.5 FPS (last over20 s).
Windows Core Audio finds the game's session active, unmuted, volume1.0, peak
0.1278. The user did not check audio; audible quality/timing remain unverified.

## Accepted132 checkpoint (user-assisted; zero-NOP batching)

Release132 links13:51:50 (58,481,664 bytes). User-assisted132 passes
both movies/full frontend/stadium, count1–0, returned selector433, second
pitch/count2–0 at461 and pause491/resume507. STOP14:10:24 after1,083.8 s/3,427
paths, zero failures/lost-syncs;339.9M words consumed. Latest fully automated
two-pitch acceptance is129. This gameplay phase differs from controlled Florida/
Texas selector, so its2.1 FPS sample is not a matched performance comparison.

Only nonzero NO_OPERATION commands now flush before the original software
interrupt; zero padding leaves queued GPU pixels intact. New native fixture
fails before/passes after, including NOP5's committed-pixel observer. All79
PB draws pass AMD/WARP packed/legacy, plus hybrid and GIL on/off publication
tests. Scope, alias, fence, flip and clear barriers and4,096-word/2 ms limits
remain. Build131 resolve profiling found roughly27% external/28% scope-end/
45% unscoped commits, with no aliases in that scene. Its repeated selector
samples are2.9/3.1 FPS; no132 speedup is established. Cycle/D3D132 audits pass.

## Accepted build129 checkpoint (historical; runaway-budget repair)

Release129 links13:21:22 (58,481,664 bytes), explicit configure/build. The
actual run127 failure is traced to the parser's runaway counter: bounded
slices carried the four-million-word budget across successive submissions
without GET catching PUT. Valid busy traffic exhausted it, and recovery
skipped the pending fence0x6495/NOP5 at0x82A0BEB8. This was the parser's own
resync, not evidence of an external DMA GET writer. See saved127 memory
history/device/ring and ENGINEERING_LOG.

Reset the word budget when PUT changes; preserve packet/CALL state, unknown-
word protection and the bound for a stationary submission. Native continuous-
producer test fails before at exactly4,194,304 words, passes afterward and
retires semaphore/NOP5. Closed-loop/invalid-stream regressions still pass,
scopes/history enabled and disabled. NFSU2's synchronous reference uses the
limit per invocation, consistent with bounding one fixed submission.

Traced run128 passes both movies003/021, full frontend/difficulty069, stadium,
pitch/count1–0 at166 and pause/resume180/227. STOP13:21:13 after **511.4 s/
3,327 paths**, zero misses/stubs/crashes/USB cancellations/lost-syncs. Snapshot
consumes139.9 million words;242 complete original requested-fence/NOP/wait-
return chains are verified. No FPS measurement or two-pitch acceptance from
this traced run. Keep the earlier native-proven publication/dispatcher repairs.

Bounded synchronous retry now belongs to opt-in RECOMP_GPU_SCOPES=1; remove
the diagnostic RECOMP_GPU_RETRY flag. Default-off keeps the original unbounded
fallback. RECOMP_MEMORY_TRACE=1 is optional debugging history, off for FPS.
Required tools_patch_d3d.py restores three renames, three W1C stores and five
BlockOnTime observations after every lift. Cycle/D3D129 audits pass:33 seeds,
37,762 lifts/seven boundaries, memcpy exclusive end0x44302D.

Untraced129 passes movies/full frontend/stadium, first pitch/count1–0 at165,
next-pitch selector360, second83 MPH pitch/count2–0 at375 and pause/resume
388/395. STOP13:35:57 after **851.4 s/3,420 paths**, zero failures/lost-syncs;
280.5 million words consumed. Selector132–147 brackets31 flips in10.001 s:
**3.100 FPS**, close to121's3.000 sample, not an established speedup. This is
the latest two-pitch acceptance. Full exhibition, playable FPS and audible
audio acceptance remain open. Default-off130 passes both movies003/021 and
title/Start059 on unchanged129. STOP13:39:04 after **151.8 s/2,214 paths**,
zero failures/lost-syncs. No game is live. Never relink a live game.

Next performance experiment: identify why scopes still average about4.4 GPU
draws per packed resolve in the saved post-pitch sample. There are1,734
resolves/10 s,623 MB readback and522 MB CPU surface uploads. Compare slice
limits versus actual alias/clear/target/software barriers; retain native RAM,
coherence and original interrupt tests. Measure the same pitching selector
before and after, with history and detailed tracing disabled. Do not infer a
game FPS gain from grouped native draw timings or a different animation phase.

## Earlier build126; bounded retry rejected before causal trace

Release126 links12:28:35 (58,480,128 bytes), explicit configure/build. Default
execution remains synchronous; GPU scopes remain opt-in. The attempted bounded
synchronous retry is reverted again after two further clean-log event stalls.
Preserve the original unbounded synchronous fallback. Native-tested bounded
GPU-exclusive slices and corrupt-stream limits remain.

Two correctness fixes remain: BlockOnTime0x57D710 retains a RAM reader across
lifted preemption, preventing exclusive GPU work from forcing the CPU to stop
between GET checks and command publication; real host waits release all nested
readers. The dispatcher completes registered all-guest event/semaphore waits
under its mutex, so a later XDK inline clear cannot revoke a satisfied wait.
Native fixtures fail before/pass after for both hazards. GIL-on/off memory/
actual waits and publication tests pass; registered notification/synchronization
Set/clear and WaitAll cases, inline reuse, pulse, semaphores and mixed/abandoned
checks pass. Mixed native/guest waits still use the prior periodic path.
Cycle124/D3D124 audits preserve33 seeds/37,762 lifts/seven boundaries, memcpy
end0x44302D. Required tools_patch_d3d.py now renames three definitions, including
BlockOnTime, while preserving direct/indirect entry points.

Run124 passes movies/full frontend/stadium/pitching selector, then parks:
504.4 s/3,415 paths, zero misses/stubs/crashes/USB cancellations, zero flips/
draws, GET=PUT, no RAM/CPU owner, all7,698 traps complete. Saved requested fence
0xBB95 has its NOP5 patch and semaphore0xBBE7 is beyond it. With the dispatcher
fix, run125 still parks at jerseys:310.1 s/2,255 paths, clean, all3,846 traps
complete, GET=PUT=0x02890F44, requested0x5245 versus semaphore0x523F. This time
the semaphore is behind the requested fence. Both full device/command buffers
and frozen runtime/thread snapshots are saved in build/. Neither native fix
establishes live causality or a speedup; the retry experiment is rejected.

Default-off run126 visibly passes both movies (windows003/021), title/Start059
and the complete screen-checked frontend through difficulty070, stadium/team
introductions, a pitch/count1–0 at223 and pause/resume244/267. STOP12:39:59 after
**604.0 s/3,416 paths**, zero misses/stubs/crashes/USB cancellations. Post-resume
10 seconds shows33 flips/15,155 draws (3.300 FPS); this is a post-pitch sample,
not a matched pitching comparison or speedup. The earlier2.100 FPS sample
spans the batter introduction and is excluded from pitching comparisons.
No game is live. Final source/EXE126; latest accepted two-pitch run116 and
latest broad scoped gameplay acceptance121. Full exhibition, playable FPS
and audible audio acceptance remain open.

## Earlier build122; first scope integration

**Final build:** Release122 links 11:53:31 (58,479,616 bytes). Bounded
grouped draws are integrated behind opt-in `RECOMP_GPU_SCOPES=1`; default off.
The independent RAM gate covers lifted CPU leases, passive bridge buffer
access, native APU/OHCI DMA, framebuffer copies and callback preambles. Real
waits copy arguments before dropping RAM leases; callbacks resume only after
GPU commits. GPU exclusion is try-only, so a CPU polling at raised IRQL forces
the synchronous fallback instead of deadlocking. Slices retain command/CALL
state (4,096 words / 2 ms between methods); commit and release RAM before
executing nonzero software-trap parameters. Vertex/command aliases and all
clear/software/fence/flip/diagnostic observers flush first. Failed commits
are fatal. Scope pointers still must use the canonical DMA RAM mapping.

Native bridge/host/actual-dispatcher waits, callback continuation, foreign
raised-IRQL fallback and 2,000 coherent DMA transactions pass with GIL on/off.
Existing scheduler/IRQL/dispatcher/OHCI/APU-output/framebuffer tests pass with
scopes enabled; scheduler/IRQL/dispatcher also pass default off. AMD/WARP,
packed/legacy each pass 2,546 renderer and 78 full-pushbuffer draws; hybrid
and cycle/D3D120 audits pass. Default-off run120 passes both movies and
title/Start, stops 11:33:23 after **149.7 s / 2,214 paths**, zero failures.
Opt-in run121 passes both movies/full frontend/stadium/pitching, a pitch to
1–0 and pause/resume; stops 11:42:07 after **523.3 s / 3,418 paths**, clean.
Matched pitching is **3.000 FPS** versus 118's 2.800, a small short-sample gain.
The scoped repeated-pitch loop remains unverified; 116 is the latest accepted
two-pitch run. Native 20× grouped timings are not game performance evidence.

Build121's attempt to retry grouping between bounded synchronous slices
parks run122 on D3D event 0x58B80C at caller 0x57D86F. Stop 11:53:20 after
**649.8 s / 3,294 paths**, clean log but zero flips/draws in 10 seconds.
GET=PUT=0x02859170, posted/completed traps both 5,321, last NOP parameter 5.
There is no pending packet/CALL and all sampled RAM depths/GPU ownership and
CPU ticket ownership are idle. A command/event timing race is a lead, not
established causality. **The retry change is reverted in 122.** Preserve the
original synchronous fallback; only GPU-exclusive work is bounded. Corrupt-
stream/runaway limits now persist across slices and native tests pass. Five
actual Nt/Ke host wait paths pass nested-release/resumption under GIL on/off.
Final default-off run123 passes both movies and screen-checked title/Start
(window061), stops 11:56:46 after **175.5 s / 2,214 paths**, zero failures.
No game is live. Never relink a live game. Final source/build is 122; the latest
broad scoped gameplay acceptance is 121 and latest two-pitch acceptance 116.
Later unused-scope paragraphs describe 119.

**Earlier build:** Release119 links 10:58:34 (58,477,568 bytes), explicit
configure/build. Initialize the dirty rectangle to remove MSVC's new C4701
flow warning; AMD again passes 2,545 renderer and 72 pushbuffer draws. Run119
stops 11:03:01 after **164.2 s / 2,214 paths**, zero failures, both movies
visibly rendered and screen-checked title/Start passed. No game is live.
Never relink a live game. The renderer's new
explicit scopes are native-tested but the game does not call begin; all live
draws retain synchronous RAM commits. Run117 passes both movies/full frontend/
stadium and stops after 233.9 s / 3,357 paths with zero failures.
Run118 stops 10:58:26 after **677.3 s / 3,420 paths**, zero failures. It visibly
passes both movies, its screen-checked frontend and stadium; pitching controls
appear at window149, matched untraced pitching is **2.800 FPS**, and the first
pitch changes the count to 1–0 at window200. Pause/resume pass at windows266/305.
Original vblanks measure **60.0 Hz**. Full exhibition/audio remain open.

**Cutoff comparison accepted:** run116 on unchanged build114 stops 10:34:41
after **1,412.0 s / 3,429 paths**, zero misses/stubs/crashes/USB cancellations.
Both movies, full frontend/stadium, next-pitch selector at 1–0, second pitch to
2–0 and pause/resume pass. Matched untraced pitching is **2.799 FPS** versus
114's 2.5, a short sample; keep the 2,048 default pending broader comparisons.
The bounded readback polling experiment is fully reverted after <1% gain in
its last reverse timing pair with extra CPU spinning. No main build116.

**Larger performance lead:** native groups of 32 six-stage/four-texture draws
on 640×480 targets cost 0.029–0.049 ms/draw versus 0.782–1.060 ms synchronously
in the final pair. Reverse pairs reproduce the reduction. AMD/WARP, packed/
legacy each pass **2,545 renderer draws**. This is a native workload result;
live game grouping and its FPS gain are unverified. See the integration notes
below before changing the DMA worker.

**Earlier corrected-depth acceptance:** run114 stops 09:56:53 after **1,148.9 s / 3,425 paths**,
zero dispatch misses, direct stubs, crashes or USB cancellations. Both movies,
the complete player frontend, stadium, the pitch-to-next-pitch loop, a second
pitch/count 2–0 and pause/resume pass. Matched pitching is **2.5 FPS**, original
driver vblanks **59.994 Hz**. Full exhibition/audio acceptance remain open.
Run114 was stopped before subsequent benchmarks. Packed readback remains opt-in; its corrected live sample
does not establish a performance gain. Subsequent CPU scan and depth-copy
scheduling experiments are reverted after mixed/inconsistent benchmarks;
there is no main build115. The ordinary synchronous path retains build114's
depth-coherence behavior.

**Cycle110:** 33 seeds, 37,762/37,762 functions lifted, zero failed,
4,293,056 C lines / 38 chunks. All seven boundaries reapplied, including
memcpy's exclusive 0x0044302D end; USB probes restored and audit110 passes.
The recovered 0x0013D210 destructor thunk passes 10,000 original-x86
comparisons with a controlled tail callee. Release110 links 08:13:02,
58,470,400 bytes. Run110 stops 08:35:21 after 1,231.9 s / 4,984 paths,
zero misses/stubs/crashes/USB cancellations; both movies, title/naming route
and the batting demo visibly render. A long Start is posted during the demo;
the latest capture is still in demo mode, so do not claim that all six earlier
baseball attract-exit call sites have been independently re-exercised.

**Build111:** original driver sampling establishes only 1.8 vblanks/s and
48,358 added flip targets in ten seconds, retirement stuck at two
(`build/vblank110_intro.json`). NFSU2 documents both exact interrupt defects.
Explicit trap acknowledgment shares a lock with publication and runs before
the original DPC loops. Vblank status survives a queued DPC, and the driver's
three PCRTC acknowledgment stores use real W1C semantics, including the
masked-vblank branch. Rational 60 Hz deadlines avoid the old Sleep(10)/16 ms
drift. Native regressions fail before/pass after, including 64 trap races,
10,000 vblank handoffs and 60,000 clock periods. Release111 links 08:43:56
(58,472,448 bytes), explicit configure/build and fresh-lift/full-cycle audits
pass. Run111 inadvertently uses software rendering: both movies render,
original vblanks measure 59.97 Hz, and retirement advances. Stop after 221.9 s /
591 paths, zero failures. Its 4.0 movie FPS is not comparable to hardware
pitching. The launcher now selects and records the hardware backend explicitly.
Run112 PID28596 starts 08:47:39 on the same build with D3D11 active. Both movies
render (`frame.window003.bmp`, `frame.window026.bmp`); the complete screen-checked
frontend passes (`build/route112.txt`). Bright stadium and players render
(`frame.window090.bmp`, `frame.window109.bmp`), 3,414 paths and zero failures
so far. Original vblanks measure **59.8 Hz**, retirement advances and targets
stay near the current clock (`build/vblank112_intro.json`), versus run110's
1.8 Hz/duplicate queue. Stadium measures 2.2 FPS; matched pitching and the full
pitch loop remain open. Run112 stops 09:05:49 after **1,090.4 s / 3,425 paths**,
zero failures. Matched pitching measures **2.5 FPS**; an 83 MPH fastball changes
the count to 1–0 and pause/resume pass. The next-pitch selector remains unverified
despite active rendering and longer input holds; original vblanks stay near
60 Hz afterward. A native map-order experiment is reverted because it gives
no consistent speedup.

**Build113:** opt-in synchronous packed color/depth readback maps one buffer
instead of two surfaces, preserving per-batch RAM coherence. AMD/WARP each
pass 2,392 decoded draws, including 512 exact ARGB/Z16/Z24 pixels; both full
pushbuffer suites and hybrid transitions pass. Repeated uncontended 640×480
surface benchmarks improve 32²/128² depth draws by about 18–19% on average;
small-surface timings remain mixed. Explicit configure/build links 09:21:16
(58,476,032 bytes), full-cycle/fresh D3D body audits pass. Run113 PID26492
starts 09:22:33 with D3D11 and RECOMP_PACKED_READBACK=1. Both movies
(`frame.window003.bmp`, `frame.window021.bmp`), the complete player frontend,
stadium and pitching render. Matched untraced pitching measures **3.0 FPS**
versus run112's 2.5; original vblanks measure **59.994 Hz**. The first pitch
changes the count to 1–0 (`build/pitch113_count.bmp`), zero failures at 3,419
paths. The render-worker profile still includes 20/60 win32u samples (15 in
the CPU synchronization wait). This is not full-game acceptance.

**Build114 source correction:** a new native fixture fails on build113's
packed path: a CPU depth edit far outside a prior GPU triangle causes the
whole intermediate upload copy to restore stale depth over that triangle.
Earlier tests edited inside the same bounding box and missed this case.
Seed the upload resource from current GPU depth before applying partial CPU
edits; whole-surface edits need no extra copy. Native verification passes
2,398 decoded draws on each of packed/legacy AMD/WARP, plus 72 pushbuffer
draws per device and hybrid transitions. Run113 stops 09:36:58 after 864.4 s /
3,422 paths, zero failures; first pitch/count 1–0 and pause/resume pass, second
pitch unverified. Explicit configure/build114 links 09:37:06 (58,476,032
bytes); full-cycle and fresh D3D body audits pass. Run114 PID30564 starts
09:37:44 with D3D11/packed readback. Both movies and the complete screen-checked
frontend pass. It then renders stadium/pitching, changes the first count to
1–0, returns to the next-pitch selector (`build/nextpitch114_selector.bmp`,
window440) and changes the count to 2–0 after another A press
(`build/nextpitch114_count.bmp`, window480). Pause and resume pass afterward.
The reset phase takes minutes at this throughput; this accepts the repeated
pitch loop after the 60 Hz interrupt repair. Matched pitching is 2.5 FPS, so
do not use build113's 3.0 FPS before its depth fix as an accepted speedup.
After ANY full lift, run
`py -3 mvp06/tools_patch_d3d.py` before configure/build. It renames only the
three original definitions (BlockOnTime0x57D710 and handlers0x580B60/0x5810D0)
and patches three W1C stores, preserving all game logic and direct/indirect
entry points. BlockOnTime retains a RAM reader through lifted preemption;
actual host waits release every nested reader. Fresh-lift body audit passes.

**Previous build109:** Release links 07:27:32 (58,470,912 bytes), explicit
configure/build and full93 audit109 pass. Run109 PID3008 starts 07:27:45 on
AMD hardware. Both movies visibly render (`frame.window003.bmp`,
`frame.window016.bmp`); Press Start, the full player frontend, a pitch/count
change to 1–0 and pause/resume pass. Stop 07:49:43 after 1,317.6 s / 6,269
paths, six misses at the newly recovered attract-exit thunk, zero stubs,
crashes or USB cancellations. Second pitch/full exhibition remain unverified.
Run108 stopped 07:27:24 after 1,152.2 s /
2,186 paths, zero failures, parked at the title logo on D3D's NOP(5) event.
Build109 implements the actual software-method interrupt, delivered through
the connected GPU ISR/vector0x33 and original DPC on the timer's guest stack.
Pending PGRAPH status survives flag servicing; the executor stalls until the
handler enters/exits, with shutdown cancellation and no fake event completion.
Routing fails before/passes after; 32 delayed trap fixtures, 72 AMD pushbuffer
draws, DMA/GIL/IRQL and concurrent DPC tests pass (`build/trap109_*.txt`).

**Previous build108:** Release links 07:07:46 (58,469,888 bytes), explicit
configure/build and full93 audit pass (`build/audit108.txt`). Run108 PID9180
starts 07:08:12 on AMD hardware. Both movies visibly render
(`frame.window003.bmp`, `frame.window011.bmp`), zero failures at 591 paths;
the subsequent title wait and stop are described above.
Build107's independent flag worker passes a blocked-render acknowledgment test
but exposes an early PGRAPH progress wait. The consumed ring releases fence199
after pattern0x507CB09C; the old mirror falsely reports201, and PGRAPH stays0.
Build108 latches the consumed pattern command and writes actual semaphore
releases, disabling producer-count mirroring with execution. Native pattern
routing fails before/passes after; wrapped sequences, subchannels, software,
AMD/WARP, DMA and guest scheduling checks pass. General DMA-context semaphore
addressing remains open; this uses the registered device-chain target.

**Previous build106:** Release links 06:40:14 (58,467,840 bytes), explicit
configure/build and full93 audit pass (`build/audit106.txt`). Run106 PID7556
starts 06:40:21 on AMD hardware. Both movies visibly render
(`frame.window003.bmp`, `frame.window009.bmp`), and the complete screen-checked
frontend route passes (`build/route106.txt`), followed by bright stadium/players
and standard pitching controls (`build/stadium106_dispatch.bmp`,
`build/pitch106_dispatch.bmp`). Matched pitching measures **2.2 FPS**, versus
build105's 1.6 (`build/fps106_pitch_untraced.txt`). Stop 06:59:07 after
**1,125.7 s / 3,380 paths**, zero misses/stubs/crashes/USB cancellations.
A pitch changes the count to 1–0
(`build/pitch106_count.bmp`); Start opens pause afterward
(`build/pause106_postpitch.bmp`), proving game-level input still responds.
Start resumes; the next-pitch selector eventually returns
(`build/nextpitch106_selector.bmp`) and a second 84 MPH pitch changes the
count to 2–0 (`build/nextpitch106_count.bmp`). **Full pitch loop accepted;
full exhibition remains open.** A native stack identifies CRT
environment scanning from the per-parameter verbose check. Cache that startup
flag, avoid the survey lookup when execution is already enabled, and read
renderer mode only during initialization. DMA, software, AMD/WARP full
pushbuffer, hybrid and 1,880 decoded-draw fixtures per device pass. The repeated
CRT hotspot is absent from the matched sixty-sample render-worker profile
(`build/sample106_pitch_modules.txt`). Main-thread samples still include D3D
write-combine flush polling (compiled +0x1F3 tests 0x100410 bit16,
`build/flush106_disasm.txt`); compare NFSU2's independent flag worker/poll-time
flush acknowledgment before changing DMA consumption or guest scheduling.

**Previous build105:** Release links 06:16:42 (58,467,840 bytes), configure/build
and full93 audit pass (`build/audit105.txt`). Run105 PID20484 starts 06:17:32
on AMD hardware. Both movies visibly render (`frame.window003.bmp`,
`frame.window019.bmp`), the full screen-checked frontend route passes, and
bright stadium/pitcher-introduction output renders (`build/stadium105_partial_upload.bmp`,
`build/players105_partial_upload.bmp`). Run105 stops 06:40:06 after **1,353.5 s /
3,423 paths**, zero misses/stubs/crashes/USB cancellations. An 83 MPH fastball
changes the count to 1–0 (`build/pitch105_count.bmp`), but return to next-pitch
controls remains unverified. Three full traces complete (692 draws each,
flips1707/1708/1709). An untraced stadium/player sequence measures 3.5 FPS
(`build/fps105_stadium_untraced.txt`); a concurrent traced sample is unsuitable
for comparison. Matched standard pitching measures **1.6 FPS**, versus build104's
1.3 (`build/fps105_pitch_untraced.txt`), with surface uploads reduced from
1.48 GB to 240,554,588 bytes/10 s (`build/renderstats105_pitch.txt`).
Build105 finds CPU-edited pixel
bounds in differing surface rows, uploads/converts just those regions and
retains untouched resident pixels. RAM comparisons, failure invalidation and
atomic color/depth readback remain. WARP/AMD each pass 1,880 differential/
coherence/cache draws, 72 forced-GPU pushbuffer draws and hybrid transitions.
The partial-upload fixture also checks untouched RAM and reduced transferred
bytes. Full pitch-loop/exhibition acceptance remains open.

**Previous build104:** Release links 06:06:38 (58,467,840 bytes), configure/build
and full93 audit pass. Run104 PID25372 starts 06:06:54 on AMD hardware.
It stops 06:16:36 after **582.4 s / 3,412 paths**, zero dispatch misses/stubs/
crashes/USB cancellations. Both movies, complete frontend, bright stadium and
standard pitching controls pass (`build/stadium104_hybrid.bmp`,
`build/pitch104_hybrid.bmp`). Pitching measures **1.3 FPS**, versus build103's
0.7; ten-second texture uploads fall to 104 from 792. However 1,345 surface
uploads transfer 1,483,846,656 bytes in that sample, motivating build105
(`build/renderstats104_pitch.txt`). Full pitch loop/exhibition remains open.
Build104 retains
tiny batches in software using measured clipped triangle-box work
(`RECOMP_GPU_MIN_PIXELS=2048`, 0 forces GPU) and expands the texture cache to
128 entries with a 128 MiB content budget. A forty-static-page reuse fixture
fails with 32 slots and passes after the change. WARP and AMD each pass
1,880 differential/coherence/cache draws plus 72 full-pushbuffer draws;
the CPU/GPU transition fixture passes too.

Run103 stops 06:06:31 after **1,140.7 s / 3,413 paths**, zero misses/stubs/
crashes/USB cancellations. Both movies, the complete screen-checked Play Now
route, bright stadium, player close-ups and a **pitch/count change to 1–0**
pass on hardware (`build/players103_hardware.bmp`, `build/pitch103_count.bmp`).
Readbacks cover conservative drawn bounds; RAM comparisons avoid redundant
surface uploads and read-only depth avoids a readback. Native CPU edits,
partial regions and render-target-as-texture fixtures pass. Stadium throughput
improves to 2.3 FPS versus build102's 0.3–0.4; players measure 1.2 FPS.
**Pitching still measures 0.7 FPS**, matching prior software, so practical
playability is not achieved. Five traces complete, including a full pitching
frame with 494 draws (490 to 32-bit color, four to 16-bit color). Run103's
ten-second pitching counters show 792 texture uploads, zero shader compiles
(`build/renderstats103_pitch.txt`). Static cache churn is a measured lead;
dynamic surface uploads still require coherence. Full pitch loop/exhibition
remains unverified.

**Previous build102:** Release links 05:35:12 (58,464,256 bytes), configure/build
and full93 audit pass. Run102 PID19920 starts 05:36:09 with actual AMD hardware
via `RECOMP_RENDERER=d3d11`. Both movies and animated main menu visibly render;
menu throughput is 3.5 completed guest frames/s. Zero dispatch misses, stubs,
crashes, USB cancellations or hardware fallback draws at the menu checkpoint.
The stadium subsequently renders (`build/stadium102_hardware.bmp`), with
33 supported-path fallback attempts. Stop 05:42:01 after 352.5 s / 3,305 paths,
zero dispatch/stub/crash/USB failures. Its whole-surface transfer baseline
measures 0.3–0.4 FPS in the stadium sequence, motivating build103.

The new decoded D3D11 backend preserves the existing DMA walker and CPU VP,
four homogeneous texture coordinates, P8/video decoding and raw combiners.
Shader specializations exclude animated constants; texture/palette snapshots
detect in-place writes. Per-batch uploads/readbacks keep guest RAM coherent
and commit only after both mappings succeed. This is a costly baseline,
pending throughput measurement in the same pitching scene. Software remains
default; `RECOMP_RENDERER=warp` selects the test device. Unsupported draws
fall back before committing RAM.

Native fixtures pass on both WARP and actual AMD: 1,795 differential combiner
draws / 57 shader compiles plus 72 complete pushbuffer draws, zero failures
or fallbacks. AMD exposed 1–2-unit errors in normalized D24 depth that WARP
missed. Raw screen-Z interpolation plus D32_FLOAT encoding of integer Z/2^N
preserves all 16/24 bits exactly and keeps guest stencil bytes. All eight
comparisons, range endpoints, fractional rounding, affine unequal-W depth,
pitch/masks/alpha and prior fixtures pass. See ENGINEERING_LOG and
`build/render102_*_final.txt`.

**Previous build101:** Release links at 05:06:46; run101 PID17740 starts 05:06:47
and stops at 05:10:59 after **252.0 s / 591 paths**, zero misses/direct stubs/
crashes/USB cancellations. Both intro movies visibly render; two independent
frame traces complete (`build/frame101_traces_all/`). That game stops before
build102 links. Full93 audit still passes (`build/audit101.txt`).
Run100 stops at 05:06:15 after 557.4 s / 3,323 paths, zero failures. Both
movies, full frontend and bright stadium pass (`frame100/frame.window219.bmp`).
Build101 repairs the existing D3D11 combiner decoder's raw input/destination/
final-word/channel/special-register encoding. Native rendered WARP pixels
fail before (1,136 channel mismatches) and pass after (320 draws / 1,280
channels); all 336 A8 + four RHW draws pass. **The live physical DMA path
still uses software rasterization.** This is a prerequisite for acceleration.
Build102 implements the broader backend and surface/RAM coherence; live
stadium and throughput acceptance remain pending at this checkpoint.

Run99 stops at 04:56:34 after **1,721.3 s /
3,415 paths**, zero misses/stubs/crashes/USB cancellations. Both movies and
the full frontend pass; bright stadium/player output is captured. An **82 MPH
4-seam fastball changes the count to 1–0** (`build/pitch99_82mph.png`). Start
opens the pause menu afterward (`frame99/frame.window805.bmp`), proving input
still reaches the game; return to the next-pitch selector remains unverified.
Actual completed game flips measure **0.6–0.7 FPS**, independently of the
60 Hz presentation window. Hardware rendering is now a measured playability
dependency. Final handle trace: 273 publications, 253 removals, 20 live entries,
zero identity errors (`build/handles99_final_live.txt`).

Build100 includes packed inline-vertex component alignment, verified by new
failing-before/passing-after pixel fixtures, all existing renderer/depth tests
and two independent frame captures. Explicit CMake configure and incremental
Release build pass; full93 seed/body/range audit is unchanged (`build/audit100.txt`).
New FTRACE fields report array mode and every active input's layout. Movie-path
and stadium acceptance pass in run100. No packed inline inputs were observed in run99;
do not claim this repair resolves its remaining mesh defects.

**Previous build99:** PID3580 starts at 04:27:52, linked 04:27:00.
Run98 stopped at 04:24:11 after 1,175.5 s / 3,259 paths, zero misses/stubs/
crashes/USB cancellations, parked during exhibition loading. The main thread
polls `sub_0042082C` from `sub_0012C0F0`; guest object `0x8013C790` retains
token `0x4800000D`, whose table entry is NULL (`build/handles98_wait.txt`).
The CPU ticket advances; audio continues. This is not proof of APU starvation.

Kernel handle publication/removal was unsynchronized while host bridges run
outside the guest CPU lock. Extracted the unchanged implementation into
`src/kernel/kernel_handles.c`; 32 concurrent native workers reproduce 58,871
stolen/closed-handle observations in 128,000 allocations. Shared/exclusive
table locking eliminates them; token semantics and thread-object lifetime
fixtures pass (`build/handles99_before.txt`, `build/handles99_final.txt`).
Build99 includes the repair and optional `RECOMP_HANDLE_TRACE` publication/
close logging. Run99 reaches the bright stadium, matchup overlays and player
draws at 3,293 paths, zero failures (`build/stadium99_handles_vertices.png`).
Three repeatable traces complete: 38/368/368 draws, flips 749/750/751
(`build/frame99_trace2.txt`). Active input declarations include types 5/6
with no unsupported sampled types. Player close-ups and the first pitch pass;
complete pitch-loop acceptance remains. A successful rerun alone
will not prove the run98 stale token was caused by this race.

Run99 reaches the standard pitching UI at 3,412 paths. Actual completed game
frames measure about **0.7 FPS** (7 flips/10 s, `build/fps99_pitch.txt`), so
the software renderer severely limits playability and apparent animation
progress. Packed inline stride/alignment is repaired in source and passes
new failing-before/passing-after pixel tests (`build/inline100_final.txt`);
it is **not in build99**. No packed inline inputs were seen in that
live sample. Build100 links after run99 stops. New FTRACE fields
identify array mode and per-attribute layout for remaining mesh defects.

**Previous build:** incremental Release build97 succeeds (linked 04:04:23).
Run98 PID2748 starts at 04:04:35 on 2026-10-11 to accept reached signed-short
and CMP 11/11/10 vertex input decoding. The complete renderer/depth suite and
new pixel fixtures pass (`build/vertices97_final.txt`); the fixture fails
before the repair. Active indexed draws in run96 confirm both missing types.
Run98 passes the screen-checked Play Now driver before the loading park.

Run96 stops at 04:03:51 after 1,215.6 s / 3,413 paths, with a clean 1–0 count
change and zero misses/stubs/crashes/USB cancellations. Depth clears/writes
execute in actual guest zeta RAM and the standard pitching view is visible
(`build/pitch96_depth_count.png`). A full pitch loop/exhibition remains
unverified; earlier player-mesh gaps persist. Source/header comparisons
identify missing vertex inputs, pending run99 visual acceptance.

From build97, FTRACE consumes each flag file when armed. Create a fresh flag
for another frame in the same game. A native two-capture fixture passes;
do not use `build/rearm_trace96.py` with the new MAP (its done flag is gone).

Run96 reaches both movies, all Play Now screens and the bright stadium with
zero failures at 3,294 paths (`build/stadium96_depth.png`). Transition trace:
116 draws / 114 depth enabled, flip798; zeta RAM contains both clear and drawn
Z values (`build/frame96_trace.txt`, `build/depth96_ram.txt`). Player/shadow-atlas
and full pitch-loop acceptance are next.

**Latest result:** run94 advances the first pitch/count to 1–0 with zero
dispatch misses, direct stubs, crashes or USB cancellations, then continues
animating without returning to the pitch selector during this sample.
Stopped at 03:42:31 after 1,300.0 s / 3,352 paths; evidence is
`build/pitch94_clean_count.png`, `build/thread94_postpitch.txt` and
`build_sample94_postpitch.txt`. A complete pitch-to-next-pitch loop and
completed exhibition remain unverified. The CPU ticket advances and main
thread stays active; this does not establish an APU-lock deadlock.

Build95's native tests fail before/pass after pitched integer Z16/Z24S8
clear/compare/write support and format-based color storage width. Covered:
16.5M viewport Z, all eight comparisons, write/test masks, separate pitches,
stencil preservation, inclusive clear rectangles, alpha discard, affine Z
through a real VP with unequal W, zeta switching and repeated frame clears.
All prior renderer fixtures pass (`build/depth95_final.txt`). Floating or
swizzled depth, hardware-exact rounding, W-buffering, stencil operations,
polygon offset and near clipping remain incomplete. Run96 accepts live depth
clear/writes and stadium output; player/exhibition acceptance continues.

**Latest checkpoint:** run91 renders both intro movies, the bright stadium,
batter introduction and an 82 MPH 4-seam fastball; its count changes from 0–0
to 1–0 (`build/pitch91_82mph.png`). P1 team/jersey/difficulty and pitch-meter
input work. Stop at 02:44:06, 890.2 s / 3,325 paths: two post-pitch misses,
0x00032710/0x001D8240, zero crashes/direct-stub hits/USB cancellations.
A complete pitch loop and completed exhibition remain unverified.

The two new entries have complete original-byte/mechanical-lift audits:
0x32710..0x32885 (112 instructions, two ret 4 paths, five known direct callees)
and 0x1D8240..0x1D8500 (161 instructions, one return, six known direct callees).
Both post-pitch bodies now pass original-x86 comparisons with controlled
callees: 13,440 cases for 0x32710 and 3,840 for 0x1D8240. Complete object
writes, call sequence/arguments, timers, queue branches and stack/registers
agree; run94 passes the previously missed post-pitch path. Seeds now total 32 and the
boundary helper protects seven ranges. Disasm92 consolidates seed30 with
identical entry/range sets to the preflight DB; new disasm93 includes both
post-pitch seeds. All seven ranges and USB hooks are restored in full93;
CMake is explicitly configured before the successful Release build.

Cycle93 disasm/lift/audit now pass: 37,761/37,761 lifted, zero failed,
4,293,039 C lines; every seed and protected range is present. Native-tested
bodies and the three recent mechanical bodies match. Chunk9999 is removed,
USB probes restored, CMake configured; Release build93 succeeds with /MP4.
Final renderer smoke passes, including signed repeat and depth-state logging.

The negative-repeat-UV sampler repair passes failing-before/passing-after
native tests (`build/sampler92_final.txt`), including projective/swizzled and
linear fixed-function paths, clamp preservation and large finite coordinates.
It is included in build93 and executes in run94's bright stadium. Other sampler
modes and accurate pacing remain incomplete. MVP 06 uses pitch-meter buttons;
Rock and Fire analog pitching belongs to MVP 07. Keyboard right stick I/J/K/L
is already implemented for batting/fielding; left stick is numpad4/6/2/8.

The read-only TLS helper passes a live stadium snapshot in run91
(`build/thread91_stadium.txt`); no repeat clean loading wait occurred. The
late trace captures nine postprocessing/UI draws through flip998
(`build/frame91_trace.txt`), not a full world draw frame. The richer stadium
trace remains run90's 623-draw `build/frame90_trace.txt`.

Previous checkpoint: run90 renders a bright live Disch-Falk stadium
(`build/stadium90_matchup.png`), batter introduction and pitching view with
inning/count/aiming UI (`build/pitching90_first.png`). P1 input works. Stopped
at 663.8 s / 3,351 paths after eleven misses of 0x256590; zero crashes,
direct-stub hits or USB cancellations. First successful pitch/exhibition
remain unverified; depth and texture sampling need work.

Seed30 recovers the standalone 0x256590..0x256F4E body: 645 instructions,
five returns, 22 direct callees already dispatched and a virtual call.
Complete lift has no unsupported instructions/dead arms; no whole-body
native comparison is claimed. Installed in local chunk9999 and sorted dispatch
for preflight91. Explicit CMake configure precedes its incremental build,
which succeeds. Run91 PID16680 starts at 02:29:15 on 2026-10-11 for acceptance.
Added the fifth preserved range to the boundary helper;
the existing 0x256C6F conditional alias is internal. No full cycle91 yet.

Previous checkpoint: run87 renders the first live stadium at Disch-Falk
Field, Florida/Texas matchup, pitcher card and manager huddle
(`build/stadium87_matchup.png`). P1 keyboard input is verified. It is stopped
at 739.1 s / 3,417 paths after ten misses of a new batter-intro predicate,
0x239420; zero crashes, direct-stub hits or USB cancellations. Its complete
lift passes 38,148 original-x86 comparisons and is seeded (29 total), installed
in local chunk9999 with a sorted dispatch entry for incremental preflight88.
Build88 succeeds; run88 PID23524 starts at 01:28:05 on 2026-10-11. Its first
build invocation needed a retry after VS project
regeneration picked up the new chunk. No full disasm/lift88 yet. FTRACE now
includes complete RC stages/constants, decoded textures and four UV sets.
Run88 is stopped at 740.5 s / 2,824 paths, parked on a black exhibition-load
screen with zero misses, direct-stub hits, crashes or USB cancellations. It
does not reach the recovered predicate. Thread samples are saved in
`build/sample88_wait.txt` / `build/stack88_wait_final.txt`. The stadium is very
dark; first pitch/completed exhibition remain unverified.

Cycle89 disasm/lift consolidates all 29 seeds, with the same entry set/ranges
as the preflight DB and all four protected boundaries restored. Full lift:
37,758/37,758, zero failures; all seed dispatch rows and native-tested body
identities pass (`build/audit89.txt`). Chunk9999 is removed; USB hooks are
restored in chunk37. Build89's first invocation fails on a stale source item
for removed chunk9999. Explicit CMake configure and build90 succeed, reusing
the compiled objects and rebuilding the changed kernel runtime.

Two independent renderer defects now have failing-before/passing-after native
pixel tests: secondary VP texture Q defaults were zero for XY-only writes;
nonstandard blending discarded the destination. Runtime90 handles ordinary
blend factors/equations and method 0x34C's constant, preserving the common
fade fast path. Signed equations remain partial. These runtime edits happened
after the kernel compiled in build89 and are included in successful build90.
Run90 verifies a substantial live brightness improvement. PID28888 starts
at 02:12:36 on 2026-10-11, renders both intro movies and accepts Start; its
dispatch log is initially clean, then exposes the new pitching callback.
Its one-frame trace captures 623 real 3D draws through flip677, with full
RC/texture/UV state (`build/frame90_trace.txt`). Negative repeat UVs are being
clamped to zero in the existing sampler, a measured next accuracy lead.
`build/thread_state89.py PID` reads MAP-derived per-thread guest TLS/IRQL/
stack and CPU ticket state without suspending or writing; live run90 checks
pass (after reading/retrying the ticket pair together). Use it before changing
the MAP. NFSU2's APU-lock starvation is a lead for
the clean loading wait, not an established cause.

Earlier cycle86/87 recovery: run86 completes both movies naturally, then reaches
1,107.5 s / 3,272 path attempts before two missing exhibition callbacks
(0x157ED0 nine times, 0x2CC3A0 ten times) and a FFFFFFF4 read. Zero direct-stub
hits or USB cancellations. Both callbacks are now seeds (28 total).
Disasm87 adds exactly those entries and splits the oversized 0x2CC0A0 alias
at 0x2CC3A0; no entries removed. All four protected ranges are reapplied.
A native comparison exposed CVTSS2SI incorrectly truncating like CVTTSS2SI.
The scalar conversion repair passes 28,672 original-callback comparisons
across four rounding modes, and toolkit tests pass 239 / 24 skipped / 44 subtests.
Full lift87 succeeds (37,757 functions, zero failures); Release build87
succeeds. Run87 PID1760 starts at 01:09:29 on 2026-10-11 for acceptance.
The four-port OHCI repair also passes native root-hub,
arrival and DMA/interrupt tests; MVP defaults to root port 3 (P1). Run87
visibly shows P1 in the Florida/Texas selector (`build/teams87_p1.png`) and
accepts keyboard Advance through jersey/difficulty. Full exhibition and
original pacing remain unverified.

Earlier checkpoints: build79 repairs the
viewport-method alias of VP constants 58/59, restoring title/team rendering
(`build/frontend79.png`, `frontend79b.png`). Build80 unifies OHCI register/HCCA
frame time, continues DMA with MIE masked and publishes frames after DMA.
Build81 publishes IRQL to each guest TIB at fs:[0x24] and requests a balanced
1 ms Windows audio timer period. **Run81: 254.9 s / 17,807 path attempts, zero
dispatch misses/crashes/USB cancellations**; nonzero audio output reaches
96,000 frames per two seconds with 0–2 dropped blocks. Run82 repeats cleanly
for 427.9 s / 28,271 paths; run83 reaches 563.0 s / 17,171 paths with zero misses,
crashes or cancellation traces. The earlier CPU-lock-only run77 still crashed;
these samples do not prove race elimination. Build83 adds real combiners,
four independent 2D texture stages, alpha tests and all immediate attributes.
Native pixel tests pass, including four distinct UV sets. Frontend brightness
is corrected. Build84 restores the complete main menu and team selector;
exhibition remains incomplete. Build82's
one-frame draw trace remains available:
launch with RECOMP_FRAME_TRACE=build/trace82.flag, then create that empty flag
after reaching the desired menu. Runs82/83 are deliberately stopped. The
Burnout comparison exposed 586 LAHF no-ops; upstream's float-flag repair is
ported and native UCOMISD/FUCOMIP/NaN tests pass, alongside all 65,536 ADD8
flag cases. Full lift84 succeeds (37,752 functions, zero failures, 586 LAHF
sites repaired). Build84's repaired chunk3 rebuild/link succeeds at 23:19.
Run84 runs 567.6 s / 2,874 path attempts. Both intro movies, the complete main
menu and Florida/Texas selector render (`build/mainmenu84.png`, `teams84.png`).
Start advances from jersey selection into exhibition loading (`dishday.big`,
player textures and crowd/overlay audio banks). That exposes one dispatch miss
at 0x0007AE92, then an AA55AA55 pointer crash. This is an omitted switch arm
inside sub_0007AE80, not a new function. The boundary helper now also preserves
0x0007AE80..0x0007AECB; the complete four-arm lift passes 45,056 native-x86
comparisons, including defaults and stack/register preservation. Build85
succeeds; run85 passes that miss/crash, reaches 266.1 s / 3,067 paths and
exposes standalone entries 0x171D40/0x2C8AD0 plus direct stub 0x288EB0,
then crashes reading FFFFFFFE. All three are seeded for full cycle86 (26
seeds). Disasm86 adds exactly those three entries, changing no old ranges;
full lift86 succeeds (37,755 functions, zero failures). Player/parameter-copy
lifts match their 45,056/8,192-case native-tested bodies; memcpy/IDCT/frontend
checks pass. Full build86 succeeds; run86 PID4056 starts at 00:11:31 on
2026-10-11 with dispatch/direct-stub/USB tracing and no watchdog.
Run84 has zero direct-stub hits and zero USB
cancellation traces. Some uncommon flag producers remain an accuracy audit item.
RECOMP_STUB_TRACE=1 now logs [UNLIFTED] once per executed direct-call stub;
it is enabled in run84, alongside dispatch-miss checking.
Native IRQL, scheduler, OHCI, APU-output and VP checks pass. Toolkit tests:
238 passed / 24 skipped / 44 subtests. Do not use RECOMP_WATCHDOG_SECS for
long acceptance: it exits at its timeout. See ENGINEERING_LOG for runs79–84 and the requested
NFSU2/Burnout3/Mercenaries comparison.
The old ignored `build/combiner_snapshot.py` reads unhandled-method storage;
after build83 its combiner values are stale. Use draw-time RECOMP_FRAME_TRACE.

New modules: kernel_guest (FIFO CPU lock), kernel_dispatcher (guest-resident
event/semaphore state), nv2a_vertex_program (CPU interpreter). PIC mapping:
USB0x31/IRQL26, GPU0x33, private KINTERRUPT IRQL at+12. A full lift emits
scheduler hints automatically; existing chunks can use
`py -3 -m tools.recomp.scheduler_hints mvp06/src/recomp/gen`.
Temporary generated USB cancellation hooks disappear on regeneration; they
are diagnostic. Mixed host/guest pulses and full APC semantics remain partial.
The expanded user objective is recorded in PLAYABILITY_PLAN.md.
The project CMakeLists is now exempt from the local-log *.txt ignore rule so
its scheduler compile definition and build wiring are reviewable/shareable.

**Boots through kernel/threads/saves, enumerates a USB pad (OHCI), brings up
audio (APU→XAudio2, DirectSound init, two DSP doorbells acknowledged),
executes the GPU pushbuffer (software NV2A: 640×480 target cleared, DMA
GET tracks PUT), and loads frontend/audio/material assets. First visible
frame is now verified in the framebuffer-window captures: MVP 06 loading
screen, advancing progress bar, then the legal screen.**

**Follow-up audit:** the earlier clean 240 s run (run47, 32 path attempts)
was real, but run48's material-parse crash had two dispatch misses at
`0x00442FCF` inside memcpy. Its labels were live while the lift still had
overlapping stale instructions, an omitted zero-index arm, and a truncated
final epilogue. These are now fixed; memcpy's correct exclusive end is
`0x0044302D`. A local x86 differential harness passed 281,124 cases against
the original routine and libc, checking bytes, return value, stack, saved
registers, and DF. Toolkit tests: 234 passed, 24 skipped, 44 subtests passed.

The repaired memcpy exposed another truncated switch in XNET checksum
`sub_005BEF58`: the eight-dword fallback omitted upper arms of its fifteen-arm
table (miss at `0x005BEFD2`). The fallback now reads complete contiguous runs
in either direction, bounded at 256 entries. Both functions were regenerated
locally and the Release exe rebuilt. **The next run (run51) passed material
parsing and opened `frontend/title.big` and `frontend/movies/easports.vp6`.**
It reached 180 s without a crash, with 590 path attempts and logged GPU draws.
Run52 completed their full disasm/lift/Release-build cycle, with the boundary
helper preserving memcpy's end at `0x0044302D`. Both targets resolve, and the
regenerated memcpy body exactly matches the differential-tested version.
The next full cycle (run53) added all three MMX IDCT entries installed by
the On2 initializer: `0x0049D0C0`, `0x0049DA06`, and `0x0049E116`.
It lifted 37,744 functions with zero failures. Native x86 versus lifted
comparisons passed 6,144 cases with initialized IDCT constants, complete
buffers, saved registers and stack checks. **Runs53/54 each reached 300 s
with 590 path attempts, no crash and zero parsed dispatch misses.**

Runtime fixes now sample YUY2/UYVY movie textures, use the correct NV097
primitive codes and independent quad groups, and apply diffuse-alpha fades.
**Run55's window captures show a complete, unobscured EA Sports intro frame**
(`build/frame55/frame.window012.bmp`). Loading/legal screens also remain
visible. The movie then holds on that frame; playback completion is unverified.

OHCI now resolves physical queue addresses into the contiguous window while
preserving static image reply buffers. USB reaches SET_CONFIGURATION, exposing
missing XID attach callback **0x005E5684**. Its adjacent detach callback
**0x005E5065** is also seeded (17 seeds total). Run55: 300 s, 590 path attempts,
no crash, one attach-callback miss. Keyboard fallback is implemented and tested
at the host input layer, but guest report delivery remains unverified.

The APU's physical helpers previously read low-RAM aliases; live run56 memory
confirms the real voice tables are in contiguous backing. Physical voice/SGE/sample accesses now resolve there, and
the Windows XAudio2 output preserves the hardware mix and submits 256 samples
per eight APU frames. **Diagnostic correction:** run55a's zero debug cursors
were unpopulated fields, not proof of frozen playback. The corrected register
trace in run57 confirms voices244–246 advance. Run58a later produced 47,708
nonzero output samples (peak 11,567); audible playback/pacing remain unverified.
Cycle56's
disassembly/boundary patch and lift are complete (37,746 functions, zero
failures); the Release build and final audio rebuild succeeded. EA worker races remain a risk,
not the demonstrated current blocker.

Run56 reached 323 s without a crash, opened `mvpintro.vp6`, and exposed
0x432FC0/0xE3020. Their preflight lifts let run57 reach the **MVP 06 title
screen** and 2,205 parsed path attempts. Its dialog text appears as white
blocks. Class GET_REPORT now returns a real input report; class SET_REPORT
is accepted (rumble data is still discarded). The OHCI transfer test passes.
Run57 exposed report callback 0x5E4C1F and worker 0x3DACEB; adjacent copy
callback 0x5E4C71 is also seeded. Run58 reached 167 s / 2,214 path attempts
without a crash or parsed miss. Run58a exposed callback 0x42C31C and crashed
with a read at FFFFFFFC; the callback returns EAX=0, so the miss is not an
established cause. There are now 23 seeds. Cycle59 disassembly finished in
247.86 s, the boundary helper restored memcpy/SEH, and the full lift completed:
37,752 functions, zero failures, 4,273,165 lines / 38 files. Release build
succeeded at 20:03, followed by the final USB runtime rebuild. All five native
input/GPU/OHCI/APU regressions pass against the rebuilt sources.
The ignored
`gen/recomp_9999.c` preflight chunk is removed automatically by a full lift.

Run59 was stopped at 403.61 s / 590 paths with no crash or parsed misses,
but its main thread was blocked in XInput initialization. A native regression
reproduced OHCI overwriting an HCCA done list while the driver still owned it.
The runtime now holds later completions until WDH acknowledgment, preserves
that held queue during acknowledgment, and defers new TD walks while WDH is
set. The multiple-batch ownership regression passes. Run60 records Start/A
USB report edges and opens `mvpintro.vp6`. It was stopped at 347.62 s / 2,214
paths with no crash or parsed misses. Reports later stopped because the
stuck-source guard counted repeated WDH values even when acknowledged.
That defect is repaired; a real-controller-loop regression now passes 250
acknowledged deliveries while retaining the unacknowledged-source limit.
The final USB rebuild includes this latest change. Run61's live OHCI snapshot
passed 5,428 acknowledgments and continued advancing; Start report edges
were followed by opening the second intro movie. It was stopped at
600.35 s / 2,214 paths, with zero dispatch misses, crashes or controller stops.

The dialog font is confirmed P8 (0x0B), with a valid ARGB palette at live
VA 0x8104E3C0. P8/palette-method sampling is implemented and its native
alpha/rendering regression passes. **Run61 verifies readable in-game text**
(`build/frame61/frame.window053.bmp`): "Checking Xbox hard disk for MVP 06
NCAA Baseball saves," with "Press START" behind it.

The save-check stall is a demonstrated kernel-object bug. Main spins in
XAPI GetExitCodeThread (0x320980), inspecting SignalState at object+4 and
ExitStatus at +0x120. ObReferenceObjectByHandle returned success with NULL,
so the game always read an unfinished thread. Live host-handle inspection
proved token 0x48000014's thread had already exited with status 1. Added
guest-addressable thread views refreshed from the real host thread; references
survive NtClose using a duplicated handle, and object-taking waits/priorities
resolve that view. The new native thread-object regression passes. Incremental
Release build succeeded; run62 began at 20:14:59 for game-level acceptance.

**Run62 passes the completed-thread poll**, opens `MENU10.ASF`, starts further
workers and reads `CACHE/LocalCache00.bin`. Its final audio interval has
70,011 nonzero samples (peak 5,273), without audible validation. At about
216 s / 2,231 paths, it encounters **one dispatch miss at 0x0007F052**, then
crashes reading 0xAC8BFFFD in sub_00089520. The miss is an arm of existing
**sub_0007F040**, whose truncated end 0x7F05A hid the 28-entry/five-target
switch. Its correct exclusive code end is **0x0007F077** (table at 0x7F078).
The complete local lift passes **4,099 native-x86 comparisons**, all indexes,
defaults, stack, callee-saved registers and DF. The boundary helper now
preserves this range too. Chunk3 is regeneratively replaced; Release build63
succeeded. Do not seed 0x7F052 as a standalone entry.

Run63 instead crashed during USB init, at about 40 s / 590 paths, with zero
dispatch misses: sub_005E9B62 followed an endpoint head of 0xFFFFFFF0 and read
guest 0x7FFFFFF8. Run64 used the same exe and passed that point, the save check
and the old frontend switch failure. It was stopped at **312.02 s / 2,567 paths,
zero dispatch misses/crashes**. Menu music/cache files open and draws continue,
but window060 onward is black. Draws carry diffuse **0x007F7F7F** while the
sampler always multiplies texture alpha by diffuse alpha; combiner alpha is
the next rendering lead, not yet an established fix.

The reference audit also found a real DPC-queue defect: duplicate insertion
always succeeded and multiple producers shared an unlocked ring. Added a
locked queue with KDPC.Inserted, actual removal, FIFO drain, immediate capacity
reclamation on cancellation and clearing Inserted before callbacks. The native
regression passes 1,000 duplicate attempts and 4,000 concurrent insertions,
cancellation, wrap/capacity and preserved arguments. This does not yet prove
the cause of run63's USB crash. Release build65 succeeded; **run65 PID11388**
began at **20:30:37** for acceptance. Thread-object add-reference/close lifetime
also passes its refreshed regression.

**Run65 now verifies game-level Advance and a visible frontend.** Window056
shows Sign In/Advance controls over the title; a ten-second A press produces
guest report down/up and advances to window110, showing Gators/Longhorns,
Back/Random/Advance and the menu track "Get Comfortable" / Wide Eyed.
Short two-second presses can miss the guest poll during slow rendering.
The earlier black-window/alpha lead is not a demonstrated cause: no combiner
change was made, and the programmed alpha still multiplies diffuse and texture
alpha. Preserve those fades. A second long A press transitions to the stars
background; later batches include offscreen geometry. At the latest check,
run65 remains live at **550 s / 3,071 paths**, zero dispatch misses/crashes.
Menu transitions, transforms and pacing need tracing; audio is still not
audibly validated. Native snapshots show OHCI continues slowly (2,611+
acknowledgments), despite occasional one-second samples with no change.

**Next steps, in order:**
Record a fresh run number and explicit backend/readback/cutoff settings; never
relink a live game. The private `build/launch_smoke99.py` records these settings.

1. Trace D3D command/event ordering before expanding scope opportunities.
   Runs122/124/125 park at original0x57D86F on event0x58B80C with GET=PUT and
   no RAM owner. Publication exclusion and registered Set/clear races now have
   native fixes, but125 still stalls; neither explains the live failure yet.
   Saved124/125 device and full ring buffers include requested descriptors and
   NOP5 patches;124's semaphore is beyond its fence,125's is behind. Record
   actual consumed semaphore values/command positions and NOP parameters,
   alongside the original event-clear/store position and GET/PUT. Compare
   0x57D710/0x580E60 with NFSU2. Do not artificially signal the event or infer
   a RAM-lock deadlock. Keep bounded synchronous retry reverted in126.

   The first scope implementation is now integrated and native-tested, with
   one clean live pitch/pause/resume acceptance at 3.000 FPS. Keep it opt-in
   until repeated pitch loops and longer runs pass. Preserve the independent
   RAM lease around passive bridge access and native DMA; copy wait arguments
   before releasing it. GPU try-exclusive must never wait for a polling CPU.
   Commit and release RAM before software traps; flush every RAM/fence/flip
   observer, preserve canonical aliases and fail fatally on commit errors.
   Run memory_scope_smoke (GIL on/off), bounded scanner and AMD/WARP packed/
   legacy differential tests for changes to this contract. Game FPS still
   needs a substantial improvement; native 20× grouping is not that evidence.

   Existing backend evidence: software pitching throughput is 0.6–0.7
   completed game frames/s. Runs99/100 already accept both movies, Play Now,
   bright stadium and reached packed vertices. Build102's decoded-batch path
   now passes WARP and actual AMD pixel comparisons, including raw combiners,
   four homogeneous coordinates and integer depth. Build103 accepts hardware
   movies/frontend/stadium/players and a pitch/count change, but pitching still
   ran at 0.7 FPS. Build104 reaches 1.3 FPS with fewer texture uploads, and
   build105 reaches 1.6 with partial CPU-edit uploads. Build106 reaches 2.2 after
   removing environment scans from method dispatch. Build108 separates PFB
   flush acknowledgments from rendering and consumes real pattern/semaphore
   progress, following NFSU2. Build109 supplies the reached PGRAPH NOP interrupt
   handshake. Accept its frontend/pitching performance and
   preserve DMA_GET-after-consumption. Profile synchronous
   readbacks, texture decoding, software 16-bit targets and CPU
   vertex execution. Keep software
   as the comparison oracle. Full cycle110
   is complete: 33 seeds, seven ranges, 37,762 lifted, zero failed; runtime-only
   repairs do not need full regeneration.
2. Preserve runs114/116's accepted two-pitch loop and pause/resume, then
   advance batters/innings and complete an exhibition
   with controller input, audio and persistent saves. The stadium is now
   captured and brightness improves in run90; depth/render accuracy remain incomplete. The intro, main menu,
   team/jersey selector and difficulty screen are visible. Keyboard: Start
   opens the title, A chooses jersey-number names/Play Now/team/jersey, Start
   advances jersey to difficulty, A starts loading. Screen pacing is slow;
   allow transitions. Four-port/P1 mapping is accepted by actual guest report
   delivery, menu advancement and the displayed P1 slot in run87.
3. Continue runtime acceptance for intermittent USB lifetime faults. The FIFO
   CPU lock, IRQL-byte publication and OHCI timing/order repairs are implemented;
   multiple clean samples are encouraging, not proof of elimination. For a
   dispatch-clean loading wait, capture `build/thread_state89.py PID` and stacks
   before stopping. Compare the owner with NFSU2's APU-lock starvation fix;
   its relevance is still a lead. Audit x87 PC=24 rounding against Burnout
   fix17 for a reached numeric divergence; a float cast loses exponent range.
4. Correct remaining renderer gaps alongside the Windows D3D11 backend.
   Preserve P8, video, fades, independent texture coordinates
   and combiner semantics; the reference transformed batch alone loses some
   of those. Mercenaries' Windows rendering path is useful, but its global
   guest registers/synchronous threads do not match this runtime. Use FTRACE
   for live draw state; the old unhandled-combiner snapshot is stale.

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

`RECOMP_VBLANK=1` is required at runtime. main.c defaults: RECOMP_PB_EXEC=1,
RECOMP_AC97_READY=1, RECOMP_USB=1, RECOMP_USB_PADS=2, RECOMP_FB_WINDOW=1,
RECOMP_KEYBOARD=1, RECOMP_APU_DSP_ACK=0x82F20810,0x82F28810.
Window keys: Enter=Start, Z=A, X=B, A=X, S=Y, arrows=D-pad. Input reports:
`RECOMP_INPUT_TRACE=1`; APU cursor/output trace: `RECOMP_APU_TRACE=1`.
Left stick: numpad4/6/2/8; right stick: J/L/K/I. MVP 06 pitching uses the
on-screen buttons/meter; the right stick handles Load and Fire batting and
fielding throws. A long Z hold then a second press throws a pitch in run91.

## Where things are

| Thing | Path |
|---|---|
| Engineering log / this file / plan | `mvp06/docs/` |
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

See [../AGENTS.md](../../AGENTS.md) "Conventions and traps" — the short list:
x64 CONTEXT Rip=0xF8 (0xF0 is R15); never write C via bash heredocs; direct
calls bypass `recomp_lookup_manual`; `.map` offsets are host offsets; guest
registers are TLS globals; piped stderr speeds the game up (watchpoint timing);
a leftover game process locks the exe (LNK1104).
