# Archived PLAYABILITY_PLAN checkpoint notes (to 2026-10-20)

Moved verbatim out of PLAYABILITY_PLAN.md on 2026-10-23. Current status: ../PLAYABILITY_PLAN.md
"Status" and ../HANDOFF.md.

---

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

