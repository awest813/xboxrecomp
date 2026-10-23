# MVP 06 — Audited Plan to a Playable PC Port

Definitive plan, replacing the milestone sections of [../PLAN.md](../PLAN.md)
(which remains the living iteration log). Written 2026-10-10 after four
bring-up iterations and a study of the four reference ports
(burnout3, nfsu2-sw/nfsc-sw, halo-ce-universal, doaxvb-re).

## Expanded user objective and acceptance order

Get the game playable on Windows, then improve frames, game timing, rendering
accuracy, controls, gameplay and audio; audit and polish; improve nativeness
and build quality; then pursue full original-game accuracy. This supersedes
the earlier enhancement-first post-M6 direction. A title screen or one clean
run does not satisfy the goal.

After M6, compare fixed input sequences and saved scenarios with original
Xbox output: frame pacing/simulation time, images/effects, controller behavior,
rules/state, sound timing/mixing, save/reload and long-session stability.
Keep reproducible evidence for repairs. Desktop behavior and reproducible
release builds follow that functional pass. Full accuracy is an evidence-based
target, not an achieved percentage inferred from boot; original captures and
measurements will be needed for the final comparison stage.

PAUSED161/162 (2026-10-20, user request): no game/helper live. Release161
promotes512-word synchronous fallback only; exclusive4096/coarse2 ms and
RAM/GET/ISR/publication contracts unchanged. Native guarded256/512 publication
checks pass GIL0/1. Same-process normal160 warm4096/512/4096/512 samples
5.149/5.350/5.150/5.400 FPS establish a modest selector gain:5.1495→5.375,
+4.38%. All12 phase input audits pass; total resolves do not fall. No broad
gameplay speedup claim. Then selected512 passes two pitches/count2–0/pause/
resume, all ten downs exact and captures checked.160 stops after17,239.150 s/
3,763 paths, zero logged faults/lost-syncs;8,353 captures/maxgap3.089 s and
8,618 audio windows/3,231,807 attempts/zero rejects. Session muted; full
exhibition/save-reload/comfortable FPS and listening quality still open.

Default512 build161 links07:59:42,59,192,832 bytes. Run162 intro captured,
591 paths/zero logged failures, but game/helper already absent at checkpoint,
route incomplete and exit reason unavailable. Do not treat it as full161
acceptance. Resume with normal163 and captured exit status, complete frontend/
selector/two-pitch validation before further runtime changes. Next attribute
external flushes and investigate GPU residency with exact CPU/alias observers;
Mercenaries and xemu offer useful contracts, NFS/Burnout review incomplete.
Preserve all barriers. See HANDOFF checkpoint for evidence and launcher traps.

Historical experiment159 setup (completed by160):
Same-process4096/256/4096/256 fallback slices compare FPS/readback counts with
5 s settle/20 s samples, input audits and final restoration. Original default
4096, exclusive budget/RAM/GET/ISR/publication contracts unchanged. Guarded256
publication, walker/continuous producer and memory/host waits/IRQL/DMA pass
GIL0/1. No gain claimed; never relink live. Normal1584.45/4.50/4.65 FPS shows
no established FPS gain from the smaller allocation repair alone.

Earlier157 work retains compatible surfaces
across same-height depth-disabled HUD draws. Native HUD pair cost falls
7.85–10.77→2.53–3.02 ms; AMD/WARP packed/legacy each pass2,600 renderer draws.
Diagnostic selector3.80/3.849 FPS unchanged151→4.60/4.75 on153; current157
runtime exactly equals153. Normal158 stopped cleanly after three samples;
full gameplay acceptance pending. Reject155's
larger-height capacity reuse despite native gains: normal1562.498/1.850 FPS,
clean log and first count1–0 only; height reuse fully reverted. All budgets,
RAM/CPU-observer/alias/IRQ barriers and session mute preference remain.

Previous PAUSED checkpoint (2026-10-19, user request): Normal152 on
Release151 stops15:22:07 after713.483 s/3,427 paths, zero faults/lost-syncs;
355,170,383 words, software18,700 posted/completed. Movies/full frontend/
stadium/two pitches/count2–0/pause/resume pass and captures visually inspected.
All ten key-downs match helper commands exactly. Normal selector4.549/4.65 FPS,
accepted audio48,020/47,996 frames/s; diagnostic counters zero.357 audio
windows/133,363 attempts/zero rejects; host active/nonzero, SESSION MUTED.
Listening quality unverified. Next readback synchronization/remaining audio
stalls, then full exhibition/save-reload. No comfortable-FPS/full-accuracy claim.

Current151 repair: Release151 links14:58:23 (59,192,832 bytes), no new
warnings/later runtime edits. Prepare each32-sample slice
and track freshness across256-frame blocks:
prevent last-slice stops discarding earlier valid audio and middle-slice stops
replaying stale audio. Native before448/128 mismatches; after all256 patterns/
131,072 PCM comparisons, output/mute/stop/DMA/sink timing0/1 pass. Cycle/D3D151
pass33 seeds/37,762 lifts/seven ranges, memcpy44302D. Intro48k processed frames/s,
no empty observations/rejections and six preserved partial blocks. Diagnostic151
stops15:09:55 after646.801 s/3,384 paths, clean,244.99M words.323 audio windows/
120,392 attempts/zero rejects. User confirms playing; priority/FPS comparison
is confounded. Normal priority restored, no policy adopted. Normal152 acceptance
passes. Sample content changes only to preserve/zero the correct
slice; rates, queue/ownership, RAM/scheduler/IRQ barriers unchanged.

Previous full acceptance150 on older150: STOP14:53:49 after766.605 s/3,420 paths,
clean,347.9M words. Movies/full frontend/stadium, two pitches/count2–0 and
pause/resume PASS and visually inspected. Normal selector4.35/4.00 FPS, no
causal gain claim; accepted audio47,996/47,970 frames/s, zero diagnostic counters.
383 audio windows/143,046 attempts/zero rejects. Host active/nonzero but session
muted; listening quality unverified. Next remaining audio stalls/readback
synchronization. Keep4,096/
coarse2 ms,128 slots/128 MiB, GPU_YIELD off and every coherence/IRQ barrier.
150 has extra Enter inputs, so strictly unattended provenance is unproven;
helper-confirmed count/pause results remain valid. Priority candidate has no
controlled benefit established. Full exhibition, saves, comfortable
FPS and original accuracy remain open.

Historical paused150 (2026-10-12, user request): current Release150 links08:15:15
(59,192,320 bytes), no new warnings/later runtime edits; no game/helper live.
150 adds default-off RAM-wait/block-interval maxima/histograms, pipeline max
and diagnostic APU TID, reusing timestamps. No sample/cadence/RAM/priority
changes. Native APU/sink/DMA timing0/1 and cycle/D3D150 pass. Live150 pending.
Normal149 on148 STOP08:15:07 after521.363 s/3,417 paths, clean;188.0M words.
Movies/full frontend/stadium/selector135 and first pitch/count1–0 at212 pass.
User pauses before second selector; latest full two-pitch/pause remains146 on145.
149 selector3.90/3.649 FPS, no causal gain claim; accepted audio47,970/48,006
frames/s, zero rejects and all diagnostic counters zero.260 audio windows/
96,886 attempts/zero rejects; host active/unmuted/nonzero. Playback continuity/
quality remain open. Next normal150 full pitch-loop acceptance, then151 APU
wall-time histograms; NFSU2's priority2 is only a candidate, private helper
prepared/untested and no priority change made. Preserve all RAM/coherence/IRQ
contracts,4,096/coarse2 ms and128 slots/128 MiB. Full exhibition, saves,
comfortable FPS and original timing/rendering comparison remain open.

Historical paused148 (2026-10-11, user request): Release148 links18:59:19 (59,192,320
bytes), no new warnings/later runtime edits. No game/helper live. Device-queue
APU pacing adapts NFSU2's eight256-frame target inside twelve slots; samples,
RAM/coherence/IRQ contracts and fallback remain. Fake clocks/queue regression
fails before/passes after; sample ownership/mute/stop/DMA tests pass timing0/1.
Default-off diagnostics identify147 selector45,634 processed frames/s and
1.012089 s discarded in102 resets/20 s.148 intro48k; stadium46.1k with14 empty
observations, zero resets or rejected submissions: continuity/quality open.
Run148 STOP19:03:16 after224.089 s/3,325 paths, zero failures/lost-syncs,
36.1M words. Movies/full frontend/stadium visually pass; selector/pitches not
tested before pause. Latest full two-pitch acceptance146 uses older145.
Next unchanged148 normal run149, all diagnostics/GPU_YIELD off: selector
FPS/production and two pitches/pause/resume, then measure remaining long RAM
stalls before changing leases/headroom. No148 FPS gain claim. Cycle/D3D148
pass33 seeds/37,762 lifts/seven ranges, memcpy exclusive44302D. Keep4,096/
coarse2 ms,128 slots/128 MiB. Full exhibition, comfortable FPS, saves, audible
quality, original-game timing/rendering comparison remain open.

Historical accepted146 (2026-10-11): Release145 links18:11:51 (59,190,784 bytes), no new
incremental warnings. Run146 STOP18:38:27 after884.2 s/3,420 paths, clean;
367.7M words. No game live. Movies/full frontend/stadium, two pitches/count2–0
and pause/resume pass automatically and visually. Latest two-pitch acceptance146.
All timing/history/frame polling/NV2A tracing and GPU_YIELD off; FPS4.30/4.30,
timing counters/frequencies zero. No causal FPS gain claim.145's interleaved
reverse3.091 FPS off/3.012 on overlaps: experimental GPU_YIELD stays default
OFF. Readbacks/Map remain a measured bottleneck. Keep4,096/coarse2 ms and
128 texture slots/128 MiB. Audio442 windows/156,776 attempts/zero rejects;
host active/unmuted/nonzero, but accepted production45,717/45,616 frames/s
versus48,000. Measure RAM waits, throttle overruns and actual playback clock
before changing cadence/headroom; continuity/listening quality remain open.
32 distinct-value manual/automatic handoffs pass GIL and requests on/off,
retaining publication/bridge extra readers and raised IRQL; publication4K/
16K and scheduler145 pass. DMA/hybrid144 and cycle-D3D145 pass unchanged33
seeds/37,762 lifts/seven ranges, memcpy exclusive44302D. Full144 has15 existing
shift warnings matching110; separate ownership/immediate-count audit remains.
Full exhibition, comfortable FPS, saves and original comparison are open.

Historical142 (2026-10-11): Same Release141 (links16:23:06,58,485,248 bytes)
passes movies/full frontend/stadium, two pitches/count2–0 and pause/resume
automatically. STOP16:46:14 after855.8 s/3,420 paths, zero failures/lost-syncs;
324.2M words. No game live. Exact linear ARGB0x12 bulk decoding preserves
pitched rows/alpha, all cache/CPU-edit/palette/alias/RAM/ISR contracts.625,422
scalar pixel comparisons plus AMD/WARP2,546 renderer/79 pushbuffer draws each
and cycle/D3D141 pass. Native decoder2.450→0.0369 ms per640x480; live decode
1.31–1.38→0.545–0.555 s/20 despite more pixels (~65% lower cost/pixel).
Default-off3.70/3.65 FPS overlaps prior baselines; NO reliable FPS gain.
All diagnostic counters/frequencies are zero. Map remains6.63–6.68 s/20;
post-FPS profile shows driver synchronization and a busySleep(0) flag worker.
Next measure readback counts/bytes/scope breaks and polling, preserving CPU
observers/IRQ handoffs. Retain128 texture slots/128 MiB (zero selector misses),
4,096/coarse2 ms. Remaining R5G6B5 decode needs separate timing. Audio151,634
attempts/zero rejects, host active/unmuted/nonzero; elapsed/source/playback
clock and listening quality remain open. Full exhibition, comfortable FPS,
saves and original-game comparison remain open.

Historical139 (2026-10-11): Release139 passes movies/full frontend/stadium,
two pitches/count2–0 and pause/resume automatically. STOP16:02:53 after
867.9 s/3,427 paths, zero failures/lost-syncs;371.9M words. No game live.
XAudio2 now queues12 monitor blocks/64 ms rather than3/16 ms, matching
NFSU2; sample content/rate/mix cadence/filters unchanged. Actual-sink regression
fails before on block4, passes3,246 FIFO blocks/500 backpressure/wrap cycles/
host-failure retries after. APU monitor/mute/stop/DMA and cycle/D3D139 pass.
Live ZERO rejected/153,762 blocks versus4.6–4.8% before. Host signal remains
active/unmuted/nonzero; listening quality/playback timing open. Observed
selector3.95/4.15 FPS with diagnostic counters off; no causal speedup claim
without a controlled reverse repeat. Next separate texture compare/decode/
create costs and measure actual audio producer/source-clock timing. All RAM/
alias/palette/CPU-edit and original ISR barriers remain. Full exhibition,
comfortable FPS and original accuracy remain open.

Historical138 (2026-10-11): Release138 passes movies/full frontend/stadium,
two pitches/count2–0 and pause/resume automatically,891.7 s/3,426 paths,
zero failures/lost-syncs;329.6M words. STOP15:41:07; no game live. Matched
selector3.50/3.45 FPS equals136's corrected baseline. No speedup established.
Default-off aggregate timing and separate explicit NV2A tracing now exist;
native137/cycle-D3D138 pass.137 diagnostic windows measure Map5.99–6.45 s/20,
GPU RAM leases7.66–9.92 s, texture2.41–2.56 s, surface compare1.52–1.73 s.
Do not sum overlapping totals or interpret multi-thread reader time as a
percentage. Preserve4,096/coarse2 ms and coherence/original ISR barriers.
Next bounded audio candidate: native queue burst/order/wrap/backpressure test,
then NFSU2's12×256-frame headroom; current three blocks cover16 ms and traces
reject4.6–4.8% across136–138. Audible effect unverified. For frames, separate
texture compare/decode/create costs before considering validation reuse;
reference sampled/frame hashes do not prove exact coherence. Full exhibition,
comfortable FPS, sound quality/timing and original comparison remain open.

Historical136 (2026-10-11): on unchanged Release135, fully automatically passes
both movies/frontend/stadium, two pitches/count2–0 and pause/resume with both
tracing flags false (876.2 s/3,420 paths, zero failures/parser recovery).
Corrected matched selector is3.50/3.45 FPS. No game is live. Larger word budget remains REVERTED for lack
of an established gain; reverse repeats also vary. Expanded native regression
stays. Earlier FPS includes per-draw trace-flag file polling;136 disables that
diagnostic overhead. System-export-resolved profiles now identify GPU waits,
ordinary waits and log writes; measure durations before further budget tuning.

Historical experiment134: increase successful exclusive slices to
16,384 words, retain shorter synchronous retries and all observer/RAM barriers.
Native publication tests pass both limits, GIL on/off, plus RAM/DMA/scanner
tests. Runtime experiment is reverted for lack of an established benefit.
133's matched selector measures2.9/3.4/3.5 FPS on unchanged132. Its Windows
audio session is active, unmuted and nonzero; listening quality is unverified.

Accepted checkpoint132: zero GPU NOP padding no longer forces a
resolve; nonzero software observers retain their flush. A failing-before/
passing-after native fixture and AMD/WARP packed/legacy PB tests verify the
change. Resolve profiling in131 records2.9/3.1 FPS in the matched selector;
132 performance remains unproven. User-assisted132 passes both movies/full
frontend/stadium, returned selector, second pitch/count2–0 and pause/resume
with Kentucky/Texas A&M (1,083.8 s/3,427 paths, zero failures/parser recovery).
133 repeated the controlled route on unchanged132. Latest fully automated loop is136;
full exhibition, playable FPS and audible audio acceptance remain open.

Current frontier (2026-10-11): run94 renders both movies and the bright
stadium, accepts P1 input and advances the pitch/count to 1–0 with zero
misses/stubs/crashes/USB cancellations (1,300 s / 3,352 paths). Post-pitch
animation continues but return to pitching controls/completed exhibition is
unverified. Full cycle93 succeeds with 32 seeds and seven protected ranges,
including memcpy's exclusive 0x44302D end. Both post-pitch callbacks pass
17,280 original-x86 comparisons with controlled callees; run94 passes the
previously missed path. Signed-repeat sampling executes live. Build95 adds
native-tested pitched integer depth clears/tests/writes and partial-surface
storage width; run96 is accepting it. Remaining depth features, sampler
coverage, frame rate and pacing still limit playability. Run96 verifies live
depth clears/writes and a clean 1–0 count change (1,215.6 s / 3,413 paths).
Build97 adds reached signed-short/CMP vertex decoders, verified by native
pixel fixtures, and repeatable frame capture. Run98 stops at 1,175.5 s /
3,259 paths, clean but parked while polling a stale worker handle. Build99
repairs the unsynchronized kernel handle table after a native concurrency
test fails before and passes after. Run99 passes loading/vertex acceptance,
throws an 82 MPH fastball and changes the count to 1–0. It stops at 1,721.3 s /
3,415 paths, zero failures; pause input still works after the pitch. A full
pitch loop/exhibition is unverified. Actual game flips measure 0.6–0.7 FPS,
making the hardware renderer a current dependency for playability. Build100's
native-tested packed inline layout passes both movies/full frontend/bright
stadium in a clean 557.4-second run100. Build101 fixes the raw D3D11 combiner
decoder after 1,136 native rendered-pixel channel mismatches; 320 WARP draws
then pass. Builds102/103 integrate hardware with the physical DMA walker and
coherent surfaces, accepting movies/frontend/stadium/players and a pitch/count
change. Region readbacks and resident-surface reuse improve stadium output to
2.3 FPS, but pitching still measures 0.7 FPS. Build104's measured small-batch
selection and larger bounded texture cache reach pitching controls at 1.3 FPS
in a clean run104. Build105's partial CPU-edit uploads pass native tests and
movies/full frontend/stadium and an 83 MPH pitch/count change in a clean
1,353-second run105. Matched pitching improves to 1.6 FPS. Build106 caches
the per-command verbose flag after a native stack identifies repeated CRT
environment scans; native fixtures and movies/full frontend/stadium/pitching
controls pass, with matched pitching at 2.2 FPS. The next-pitch selector returns
and a second 84 MPH pitch advances the count to 2–0 in a clean 1,125.7-second
run106: the full pitch loop is accepted, while full exhibition remains open.
Build108 separates hardware-flag acknowledgments from rendering and consumes
actual pattern/semaphore progress after build107 exposes an early PGRAPH wait.
Native fixtures and both movies pass, but run108 then waits on an ignored
NOP(5) event at the title (1,152.2 s / 2,186 paths, zero failures). Build109
delivers the original PGRAPH ISR/DPC handshake and preserves pending status;
delayed-handler/cancellation/native graphics tests and both movies pass.
Run109 passes the full player frontend, a pitch/count change to 1–0 and
pause/resume, then stops with six known attract-exit thunk misses. Cycle110
adds that 11-byte callback as seed33 and fully lifts/builds 37,762 functions,
zero failed, with all seven boundaries. Its 1,231.9-second intro/frontend/demo
run has zero failures. Guest sampling measures only 1.8 vblanks/s and a flip
queue growing by tens of thousands, matching NFSU2's duplicate-trap and
dropped-vblank defects. Build111's explicit handler handoff, original W1C
acknowledgments and rational 60 Hz timer pass native before/after fixtures;
Run112 accepts both movies, the full player frontend, bright stadium and
players with a clean log. Original vblanks improve from 1.8 to 59.8 Hz and
flip retirement advances. Run112's matched pitching is 2.5 FPS. Build114 fixes
a native-reproduced partial-depth coherence bug in opt-in packed readback;
2,398 decoded draws per packed/legacy AMD/WARP device path pass. Run114 accepts
both movies/full frontend/stadium, the next-pitch selector at count 1–0, another
pitch/count 2–0 and pause/resume after it, then stops cleanly at 1,148.9 seconds /
3,425 paths. Original vblanks remain 59.994 Hz; matched pitching is 2.5 FPS.
The pitch reset takes minutes at this throughput. Full exhibition, audible
audio acceptance and meaningful performance improvement remain the frontier.
Run116 isolates a 4,096 CPU/GPU cutoff on build114 and repeats both movies,
frontend/stadium, two pitches to count 2–0 and pause/resume: 1,412.0 s / 3,429
paths, zero failures. Its short matched sample is 2.799 FPS versus 2.5; the
default remains 2,048. Build118 adds native-tested explicit render scopes:
2,545 draws pass per packed/legacy AMD/WARP path, including memory dependencies.
Groups of 32 reduce native repeated-draw cost by roughly 20×, but are not yet
used by the game. Safe integration must cover kernel-bridge memory access,
vertex/command reads, software fallback and fence/interrupt/flip boundaries.
Run118 accepts the synchronous default: both movies/frontend/stadium, first
pitch to 1–0 and pause/resume, stopped after 677.3 s / 3,420 paths with zero
failures. Matched pitching is 2.800 FPS and original vblanks 60.0 Hz. Build119
initializes the dirty rectangle to remove a compiler flow warning; the AMD
2,545-draw renderer/72-draw pushbuffer checks pass again. Its movie/title smoke
stops after 164.2 s / 2,214 paths, zero failures; no game remains live.
Build120 integrates RAM-excluded bounded scopes behind opt-in
`RECOMP_GPU_SCOPES=1`, keeping synchronous execution default. Native bridge/
wait/callback/DMA/IRQL tests pass with GIL on/off, and all four AMD/WARP
packed/legacy paths pass 2,546 renderer and 78 pushbuffer draws. Default-off
run120 passes movies/title; opt-in121 passes both movies/full frontend/stadium,
first pitch/count1–0 and pause/resume, stops after523.3 s /3,418 paths, clean.
Matched pitching is3.000 FPS versus118's2.800, a small sample gain. Build121
tries bounding synchronous fallback to retry grouping, but run122 parks on
D3D event0x58B80C with GET=PUT, completed NOP(5) and no RAM/GPU/CPU lock owner
in the captured snapshot. The log remains clean; stop649.8 s /3,294 paths.
That retry change is reverted in build122. Corrupt-stream limits persist across
slices and native tests pass, including all five actual host wait paths.
Run123 passes the final default-off movies/title/Start: 175.5 s / 2,214 paths,
zero failures; no game is live. Native20× repeated-draw
timings are still not a claim of20× game throughput. Full exhibition/playable
FPS/audio remain open; reproduce the command/event ordering before retrying.
Later126: two native-reproduced hazards are repaired (forced GPU exclusion
during BlockOnTime publication, and registered SetEvent waits lost to inline
clear). GIL-on/off memory/publication and event/WaitAll regressions pass. The
bounded synchronous retry STILL parks124 after pitching selector and125 at
jerseys, with clean dispatch logs and no RAM owner. It is reverted again in126;
keep scopes opt-in and the synchronous fallback unbounded. Saved buffers show
124's semaphore beyond its requested fence,125's behind; trace actual consumed
semaphore/NOP positions and event clear before further performance attempts.
Default-off126 passes movies/title/Start/full frontend/stadium, a pitch/count
1–0 and pause/resume;604.0 s/3,416 paths, zero failures, stopped12:39:59.
Post-resume3.300 FPS is a different phase, not a matched gain; batter-intro
2.100 FPS is excluded from pitching comparisons. No game is live. These fixes
establish no live FPS gain or completed exhibition.
Later129: the actual127 stall is traced to the bounded parser's runaway budget
accumulating across successive PUT submissions and skipping a fence/NOP5.
Reset the budget when PUT changes, preserving packet/CALL state and stationary-
submission corrupt-loop limits. Native continuous-producer regression fails
before at exactly4,194,304 words and passes afterward. Traced128 passes both
movies/full frontend/stadium, pitch/count1–0 and pause/resume;511.4 s/3,327 paths,
zero failures/lost-syncs,139.9 million consumed words. History verifies242
complete original fence/NOP/wait-return chains. Build129 removes the separate
retry flag: bounded retries belong to opt-in GPU scopes, with unbounded
default-off fallback. Untraced129 passes both movies/full frontend/stadium,
two pitches/count2–0 and pause/resume:851.4 s/3,420 paths, zero failures/lost-
syncs,280.5 million consumed words. The matched selector sample is3.100 FPS,
close to121's3.000, not an established speedup. This supersedes116 as latest
two-pitch acceptance; full exhibition/playable FPS/audible audio remain open.
Next measure resolve causes/slice limits, retaining native RAM/coherence/
interrupt barriers and comparing the same selector phase.
Default-off130 repeats both movies/title/Start on final129:151.8 s/2,214 paths,
zero failures/lost-syncs, stopped13:39:04. No game is live.
The handle race is established; its role in
run98's park is not.
See HANDOFF/ENGINEERING_LOG for
the current build and run, rather than treating older checkpoint prose below
as the present frontier.

Latest checkpoint: build79's viewport-constant alias restores title/team
rendering through the CPU VP. Build80 repairs OHCI frame/MIE/DMA behavior;
build81 publishes guest KPCR IRQL and requests a 1 ms Windows audio timer.
Run81 reaches 254.9 s / 17,807 path attempts with zero misses/crashes/USB
cancellations and nonzero 48 kHz audio output. Run82 repeats cleanly for
427.9 s / 28,271 paths. Build83 adds combiners/four 2D texture stages/all
immediate attributes and corrects frontend brightness; run83 reaches 17,171
paths, clean. Native pixel checks pass. The earlier CPU-lock-only run77 still
crashed, so race elimination is not established. The Burnout comparison found
586 LAHF no-ops; the repair passes native float/NaN flag checks and full
lift/build84 succeeds. Run84 renders both intro movies, the complete main menu
and team selector; Start enters exhibition loading. It reaches 567.6 s /
2,874 paths before one omitted player-bitfield switch arm (0x7AE92) and a
pointer crash. The containing 0x7AE80..0x7AECB lift now passes 45,056 native-x86
comparisons; build85 succeeds and run85 passes that failure. At 266.1 s /
3,067 paths it exposes two indirect entries and one direct stub before a
new pointer crash. All three standalone bodies are seeded for full cycle86
(26 seeds). Full lift86 succeeds (37,755 functions, zero failures); Release
build86 succeeds. Run86 completes both movies naturally without input, then
reaches 1,107.5 s / 3,272 paths before two missing callbacks and a FFFFFFF4
read. Both are seeded for cycle87 (28 total). A native callback comparison
exposes scalar CVTSS2SI truncation; its repair passes 28,672 original-x86
comparisons. Full lift87 succeeds (37,757 functions, zero failures); its Release
build succeeds and run87 is accepting. Four-port/P1 mapping passes native OHCI
tests and run87 shows P1 with working menu input. Run87 renders the first live
stadium, pitcher card and manager huddle, then exposes one new batter-intro
predicate at 3,417 paths. The native-tested recovery passes 38,148 comparisons;
incremental preflight build88 succeeds (seed29, local chunk9999; full cycle88
pending). World rendering is too dark. First pitch and a completed exhibition
remain unverified. Run87 has zero crashes, direct-stub hits or USB cancellations.

Run88 instead parks earlier during exhibition loading: 740.5 s / 2,824 paths,
zero misses/stubs/crashes/USB cancellations. Cycle89 consolidates all 29 seeds
and lifts 37,758 functions with zero failures; Release build90 succeeds after
refreshing the stale VS item list. Native
pixel tests verify secondary texture-Q defaults and the stadium's previously
ignored additive/constant-color blending; run90 renders both intro movies,
a bright live stadium and pitching view with inning/count/aiming UI. It is
stopped at 663.8 s / 3,351 paths after eleven misses of a new pitching callback
(0x256590); zero crashes/stubs/USB cancellations. Its full five-return body
is seed30, prepared for incremental preflight91. First successful pitch and
exhibition remain pending; depth and signed repeat-UV sampling need work.
The next loading wait needs a TLS/CPU-ticket owner snapshot before diagnosis.

## Audit: where the port actually stands

**Proven working (verified by runs, not assumption):**
- Toolchain end-to-end: cycle59 lifted 37,752 functions (0 failures), native exe
  builds and links (`/MAP` symbolication).
- Boot: CRT init, main-thread wrapper (`0x0031EF5D`), EA/XONLINE init,
  worker threads (3 spawned as real host threads), save/cache partitions,
  TDATA/UDATA probes.
- Kernel: 153/153 imports routed; dispatcher-object waits (KEVENT/KSEMAPHORE
  embedded in guest memory) resolve through `bridge_ke_object`.
- USB/OHCI: controller operational, device enumerating on port 1, XPP ISRs
  claiming interrupts.
- Audio: APU model up (XAudio2 backend), AC97 codec ready, DSP doorbell
  acknowledged at 0x82F20810, DirectSound voice setup past the GP/EP memmove
  (untrapped span).
- Graphics (correctness tier): software NV2A executing — 640×480 target with
  correct pitch cleared, DMA GET tracks PUT, fence handshake at
  dev+0x2C/+0x30.
- Assets: 171+ file opens incl. datafile.big, frontend splash/textures
  (grad.xsh), audio banks (SFXA1/2.ABK), mvporide.ini probe fails correctly.

**Known broken / in flight:**
- The follow-up memcpy audit fixed omitted index zero, stale overlapping
  instructions, and a truncated epilogue (exclusive end 0x44302D). Native
  differential check: 281,124 cases passed. A complete XNET checksum switch
  then got run51 past material parsing to `frontend/movies/easports.vp6`.
  Targets 0x432FBB and 0x49E807 now resolve after a full pipeline. The next
  IDCT entries 0x49D0C0/0x49DA06/0x49E116 now resolve; 6,144 native x86
  comparisons passed. Runs53/54 each reached five minutes without crashes
  or parsed dispatch misses.
- First visible frame is verified: framebuffer-window captures show the
  loading screen, advancing progress bar, and legal screen. Complete movie
  playback and menu navigation are still unverified. YUY2 support, NV097 primitive
  assembly and alpha fades produce a complete EA Sports intro frame in run55.
  Run61 reaches the MVP 06 title screen with readable P8 dialog text.
- Rendering tier is the CPU rasterizer (Burnout-3-class: 2–14 fps) — fine
  for bring-up, not for playability.
- Full cycle59 includes all six post-run56 callback/worker seeds (23 total).
  XID attach now runs; class GET_REPORT is
  implemented. APU DMA/output fixes are built, and actual voice cursors advance.
  Run58a produced nonzero output samples; audible playback/pacing remain
  unverified. OHCI done-queue ownership now passes its native regression;
  run60 records Start/A USB report edges and opens the second intro movie.
  Run61 reaches 600 s / 2,214 paths without a crash, dispatch miss or controller
  stop; native OHCI snapshots pass 5,400 acknowledged deliveries. Its readable
  save-check dialog stalls because ObReferenceObjectByHandle returned NULL
  despite a completed host thread. Guest thread-object views are implemented,
  built and pass a native lifecycle/polling regression. Run62 passes that poll
  and opens menu music/cache files, then exposes an internal switch arm and
  crashes. The complete 0x7F040..0x7F077 lift passes 4,099 native-x86 comparisons;
  the boundary helper preserves it and build63 succeeded. Run64 reached
  312.02 s / 2,567 paths with no crash/miss, past that switch, but the frontend
  draws black. Register-combiner alpha is the next rendering lead. A real
  duplicate/concurrent DPC-queue defect is repaired and passes its native
  regression; build65 includes it and run65 is accepting the change.
  Run65 now renders frontend controls and accepts a long A press into the
  Gators/Longhorns screen; a second press advances to the stars background.
  Short presses can miss the slow report poll. No combiner repair was needed
  for those visible responses. Full navigation, transition/transform correctness
  and audible playback remain unverified; run65 is live past 550 s, dispatch-clean.
  Untouched: memory-unit save paths beyond T:\ translation.
  Builds66/68 copy completed frames at flips and repair the DMA walker's
  dropped calls/wraps and premature GET publication. Native regressions pass;
  run68 reaches frontend/team selection without miss/crash/parser desync at
  four minutes. Run66's isolated unresolved 0x41FF34 is an internal RET;
  added caller diagnostics rather than seeding it. Run67 reproduces the USB
  wild-link crash despite DPC deduplication. A shared raised-IRQL gate now
  protects driver regions and ISR/DPC callbacks; native checks pass, with
  run69 ends at 318.1 s / 7,850 paths with zero misses/crashes/parser desync/
  IRQL warnings. A single run does not prove the intermittent USB fault gone.
  Build70 succeeds with survey-only DMA regression coverage. Guest transform
  mode6 led to the CPU VP implementation in build75 (native tests pass,
  runtime acceptance pending). Run75 again reproduces USB's stale stack
  request; build76 is compiling a full guest CPU lock after native scheduler
  acceptance. See the latest checkpoint above; exhibition remains unverified.

**Known risks (from reference-port evidence, not speculation):**
1. **EA worker races** — NFSU2 needed a guest lock (GIL) because its stream
   system hands unfilled blocks to workers. The handoff's timing-dependent
   material crash was not dispatch-clean (run48 missed memcpy arm 0x442FCF).
   Current evidence supports decode/dispatch defects first; investigate the
   GIL once those are resolved and a race is demonstrated.
2. **More dispatch misses** — every subsystem boundary has so far surfaced
   1–10. Converged quickly each time.
3. **D3D pushbuffer semantics** — NFSU2's executor needed NOP-trap delivery,
   pattern-color fences, semaphore releases, per-subchannel register shadow
   discipline. Our v0.11.0 executor lacks these; expect freezes at first
   Present/flip.
4. **Further input semantics** — table-reachable XID callbacks are seeded and
   Start/A report edges execute. OHCI ownership/acknowledgment regressions pass;
   game-level menu navigation remains to be verified.

## Milestones to playable

### M1 — Config parse completes (reached; clean acceptance in runs53/54)
The repaired memcpy and complete XNET switch let run51 pass datafile/material
parsing and open frontend/title and intro-movie archives. Run52 reached 300 s
without a crash (590 path attempts) and produced visible loading/legal screens.
Runs53/54 resolved the movie-path misses and met five-minute clean dispatch
acceptance. Later USB attach progress in run55 exposed a new callback miss.
Acceptance: no crash for 5 min; file-open count grows past ~200 (frontend
art/anims); `[GPU]` reports draws (not just clears).
Effort: 1–3 seed/build cycles (~1 h each, mostly build time).

### M2 — First visible frame (reached in run52)
Window captures verify the MVP 06 loading screen and legal screen. No
renderer/presentation change was required to reach this milestone. Remaining
intro work has reached a clean EA Sports frame in run55. Movie completion
is still unverified; the next runtime check tests APU cursor progress and XID
attach after DMA mapping and output repairs.

Original rendering-risk checklist for later frames:
The FB window (RECOMP_FB_WINDOW=1, already default) shows the EA/splash
screen. Likely blockers, in expected order: flip/Present semantics (adopt
NFSU2's `nv2a_pb_exec.c` trap/semaphore/fence handling — see M5 for the
rendering swap; for M2 the CPU rasterizer suffices), vblank frame counter
(`xbox_Nv2aFrameCounter(0x589A38, <swap-count offset>)` — find offset the
way Burnout 3 did: the XMV/present pacing read), missing combiner/texture
methods in the executor (burnout3 fixes 10/20/21/22 enumerate the classes).
Acceptance: non-black frame in the window; `RECOMP_FB_DUMP` produces images.

### M3 — Menus navigable with audio
Run84 verifies the complete main menu, team/jersey selector and difficulty
screen after LAHF repair; run85 repeats that route. Guest USB input drives
transitions, with long presses needed under the slow CPU rasterizer. Nonzero
APU/XAudio2 output reaches 48 kHz with low block-drop counts; audible fidelity
and music/SFX timing still need acceptance. Check the displayed P3 controller
assignment against USB port mapping. Acceptance: navigate the frontend with
a pad and hear the intended menu music/SFX.

### M4 — Exhibition game loads and plays
Run85 opens stadium/dishday.big, player textures, signature-animation data
and crowd/overlay audio banks. Cycle86 adds three measured missing entries
before the next loading retry. The FIFO guest CPU lock, dispatcher objects,
IRQL publication and OHCI timing/order repairs are implemented; continue
acceptance for worker/USB lifetimes rather than re-porting the scheduler.
Next verify stadium output, pitching/batting/fielding, commentary/crowd and
save/reload. Acceptance: pitch an inning; no crash across a full half-inning;
saves persist under saves/.

### M5 — Performance tier (interleavable from M2 onward)
The current executor already has CPU vertex programs, register combiners,
four texture stages and all immediate attributes. Introduce a decoded-batch
boundary and a Windows D3D11 backend while retaining that software path for
comparison. Use the NFS fork's modern executor/backend interface as a reference.
Its modern hardware
backends are `src/nv2a_gl` and `src/nv2a_vk`, built only outside Windows. Its
`src/nv2a/nv2a_pgraph_d3d11.c` is still the legacy path, close to ours; copying
that file does not provide a modern Windows backend. The executor's surface,
blend/depth, texture and transformed-vertex interfaces are the useful port
boundary. Their CLAUDE.md records presentation, culling, surface-key and vertex
fixes. Acceptance: 60 fps frontend / ≥30 fps gameplay.

Carry the MVP font repair through that port: the current reference's
`Nv2aTexture` and transformed `Nv2aBatch` path omit palette address/length,
and its CPU sampler has no P8 case. Its raw batch does carry `pal_va[4]`.
A Windows backend taking transformed batches therefore needs palette fields,
four texture coordinate sets and combiner state added to that interface.
Its raw-batch interface retains all 16 attributes and palette addresses and
is a better starting point for a full GPU pipeline. Mercenaries supplies a
Windows D3D11 surface/depth/presentation reference, while its global guest
registers and synchronous thread execution must not replace our TLS workers.
Preserve native P8-alpha, packed-video, viewport, four-coordinate combiner
and primitive/fade regressions during integration.

Build101 repairs raw combiner input/destination/final-word/channel encoding
in the existing D3D11 helper. A WARP differential test fails before and passes
after (320 draws / 1,280 float-channel comparisons); 336 A8 + four RHW draws
also pass. This is a prerequisite, not a completed backend. Still audit
simultaneous RGB/alpha writes, mapping/clamping and mux/constant/final flags;
preserve homogeneous Q and coherent color/depth resources when connecting
the live physical DMA walker.

Build102 implements that decoded-batch D3D11 path, with software remaining
default. WARP and actual AMD each pass 1,795 differential combiner draws and
72 complete pushbuffer draws, zero failures/fallbacks. Raw screen Z and
D32_FLOAT power-of-two encoding retain all guest integer depth bits after
AMD exposed a WARP-missed D24 rounding defect. Per-batch guest-RAM
upload/readback preserves coherence; it is an expensive initial contract.
Run102 renders both movies and the animated main menu on hardware, measuring
3.5 actual game frames/s in the menu. Stadium/pitching acceptance and matched
throughput comparison are in progress. M5's performance target is not met.

Run103 subsequently passes both movies, full frontend, stadium/player output
and a pitch/count change to 1–0 in a clean 1,140.7-second sample. Conservative
region readbacks and RAM-checked surface reuse improve stadium throughput to
2.3 FPS, with players at 1.2 FPS; pitching remains 0.7 FPS. A native draw-size
benchmark favors CPU at 16x16 and GPU at 32x32 and larger. Build104 uses a
2,048-pixel clipped triangle-box threshold (0 forces GPU) and expands the
texture cache to 128 entries / 128 MiB after a reproducible forty-page reuse
failure. WARP and AMD each pass 1,880 differential/coherence/cache draws plus
72 full-pushbuffer draws and the hybrid transition fixture. Live acceptance
passes in run104 through pitching controls, with 1.3 FPS and 104 texture
uploads/10 s versus 0.7 FPS and 792 in build103. Whole surface uploads still
transfer 1.48 GB/10 s. Build105 uploads just CPU-edited pixel bounds, passing
native coherence/depth/cache/hybrid tests and movies/full frontend/stadium.
An untraced stadium/player sequence measures 3.5 FPS; matched pitching
acceptance continues. M5's performance target and M6 remain unmet.

### M6 — "Playable" declared
Full 9-inning exhibition with audio, pads and persistent saves. Then follow
the expanded user objective above: frame pacing/timing, rendering, controls,
gameplay/audio accuracy, audit/polish, desktop nativeness and build quality,
then original-game parity. The older enhancement program in PLAN.md remains
optional and must not displace those acceptance requirements.

## Effort estimate (order-of-magnitude, single experienced agent)

| Milestone | Wall time | Dominated by |
|---|---|---|
| M1 | hours | seed/build cycles |
| M2 | 1–3 days | pb_exec semantics + method coverage |
| M3 | 1–2 days | XID seeding + APU voices |
| M4 | 3–7 days | GIL port + streaming audio + stabilization |
| M5 | 2–4 days | D3D11 translator port + tuning |
| M6 | 1–2 days | validation |

The reference ports' histories (Burnout 3: 23 fixes to races; NFSU2:
comparable, plus a scheduler rework) say the tail risk is M4 stability, not
any single unknown.

## Decision log (so far)

- **Software NV2A first, host-GPU later** — correctness before speed;
  matches burnout3 and NFSU2 bring-up order. The D3D8-API-replacement route
  (doaxvb) remains a fallback if pushbuffer semantics prove pathological,
  using the already-proven direct-call intercept machinery.
- **Measured renderer throughput now gates playability (run99)** — the
  pitching scene completes 7 guest flips in 10 s (~0.7 FPS), independently
  of the 60 Hz window refresh. Both movies, frontend, stadium and an 82 MPH
  first pitch/count are accepted with clean dispatch. Bring M5's hardware
  backend work forward alongside remaining M4 pitch-loop/stability checks;
  a full exhibition at this throughput cannot establish practical playability.
  Preserve the native-tested software path as a correctness reference.
  Mercenaries' translator depends on its expanded D3D8 internals, vertex
  shader/combiner helpers, surface manager and title-specific diagnostic
  globals; it is a reference rather than a drop-in file replacement.
- **Guest CPU lock now required** — clean-dispatch runs63/67/70–73/75 reproduce
  USB using a request after its submitting thread reuses its stack. PIC priority,
  dispatcher-state and wait-boundary repairs do not eliminate it. Build76 ports
  the reference scheduling contract, with native acceptance tests.
- **Runtime fixes stay title-agnostic** — title addresses and overrides remain
  in the MVP project; shared scheduling/hardware/lifter behavior stays in the toolkit.
