# Bounded CPU/GPU handoff: measured gain, 30 FPS still open

Commit checkpoint (2026-10-31): the subsequent normal179 launch, run180,
exited with access violation 0xC0000005 after startup. Private exit180.json
records PID14312 and exit code3221225477. The fault maps to sub_0031D5BF,
where the guest TLS lookup reads through a zero FS:[4] value. The cause is
unresolved; this run does not validate normal179 startup or gameplay FPS.
Investigate thread TIB/TLS initialization before resuming normal-profile
measurements. This failure does not replace the separate178 acceptance above.

2026-10-30. Previous goal turn made progress: it completed matched direct-buffer
comparisons and rejected an unproven runtime option. At this turn's start174
PID24804 was confirmed live. Goal remains steady30FPS in actual gameplay, then
rendering/accuracy work. Current results do not achieve that goal.

## Mechanism and discovered regression

An ordinary SwitchToThread safe point can reacquire the CPU/RAM reader before
the GPU claims exclusion. New `RECOMP_GPU_HANDOFF_US=1..2000` permits a bounded
acknowledgment wait after releasing both guest CPU and RAM leases. It requires
GPU_YIELD, passive-level preemption and the sole lifted reader. Other retained
bridge/DMA readers cause a skip. The GPU acknowledges only after acquiring real
exclusive ownership. Missing work times out; no ownership/completion is invented.
No allocation, coarse sleep or handle is used in the wait. Toolkit default0.

176's naive startup regressed to0.5FPS:2,050 waits versus4 accepts in a2-second
sample.177's reader guard alone still regressed:2,468 waits versus2 accepts.
Both waits were disabled live, and both builds were deliberately stopped before
exhibition (2,186 PATH each, no logged crash/direct stub, exitFFFFFFFF).

The actual defect was an advisory request surviving synchronous fallback which
had already consumed PUT.178 expires the request when the actual walker reaches
its captured endpoint; partial slices retain it. A canceled wait returns promptly.
This neither releases a GPU lease nor publishes a hardware fence. An owned scope
still commits before RAM exclusion ends. Native tests cover masked PUT, partial
walks, more than four million continuous words, ring/call/GET semantics,32 safe
handoffs, nested/publication readers, raised IRQL, missing GPU, canceled requests
and2,000 coherent DMA writes. Eight GIL0/1 × off/legacy/handoff/invalid cases pass.
GPU budgets remain4096 words/coarse2ms; fallback remains512 words.

## Matched gameplay comparison on178

Separate178 PID18740 uses packed/scopes/4096, poll100, timing off. Default
Florida–Texas/home jersey/Varsity reaches selector frame141/error1.70. All
benchmark phases retain exactly seven frontend downs, zero audio rejections,
no shader compiles and the same scene. Each settles5s and samples20s.

Mode A: GPU_YIELD0/handoff0. B: advisory yield1/handoff0. C: yield1/handoff2000.

| Sequence | FPS in order |
| --- | --- |
| A B C C B A |3.098,3.600,4.000,4.000,3.750,3.700 |
| C B A A B C (warm reverse) |4.100,3.750,3.800,3.747,3.850,4.000 |

Warm reverse A averages3.7735FPS; C averages4.05 (+7.327%). B averages3.8;
thus the bounded wait also improves over advisory yield alone. In that reverse
run, unscoped resolves/flip fall27.245→17.568 (35.519%); scoped resolves rise
20.967→23.549. This is a scene-specific gain, not a claim across the whole game.
Counters record roughly3,200 real accepted handoffs per20s, with more than a
million skipped attempts due to other readers. Identifying those readers is
the next useful ownership investigation. Do not infer their identity from counts.

Evidence: private build/handoff178_abccba_*.json, handoff178_cbaabc_*.json and
their text logs. Guarded setters validate loaded PE, matching MAP, own executable,
symbol alignment and expected old value. Each comparison restores mode A.

## Gameplay acceptance and promotion

Mode C then passes two pitches/count2–0 and pause/resume. Two-capture gates:
first count375/error0.00, second selector528/error0.99, second count585/error0.00,
pause633/error0.00, resume641/error0.00. Captures visually checked. Exactly11
downs match seven frontend, two Z pitches and two Enter pause/resume actions.
Live178 retains yield1/handoff2000/poll100, with test hold restored0.

Final audit has3,462 PATH entries (174's selector-only audit3,412), no logged
crash/direct stub/dispatch miss/lost sync/USB cancellation. Increased coverage
includes the two-pitch route; this is not a full exhibition/save validation.
Private acceptance178_build179.json stores comparison summary, input audit,
live state and latest hashes. Never relink live178 EXE/MAP.

Normal179 executable rebuilt successfully. MVP's main now defaults the tested
D3D11/packed/scopes/4096/yield1/handoff2000/vblank profile before initialization,
while honoring explicit user overrides. Other titles retain toolkit handoff0
and GPU_YIELD off. Setting RECOMP_GPU_YIELD=0 disables MVP handoff.179's profile
is equivalent to the accepted178 settings;179 itself has build/native validation,
not a separate full exhibition. Its normal non-diagnostic FPS remains to measure.

## Optional ownership diagnostics in179

`RECOMP_GPU_OWNER_TRACE=1` records up to64 active reader thread IDs, native
acquisition callers, publication callback tags and lease age. Default off; records
never control locks. x64 record size32 has a compile-time check; readers use two
equal even sequence snapshots and ignore empty/transient records. No new lock
or allocation is introduced. Native lease/scheduler suites pass with tracing on,
plus GIL0/1 × yield0/1 with tracing off. Private read_owners179.py is prepared
for matching-MAP inspection; actual in-game owner attribution is still pending.

All new work remains uncommitted. Current game is live178, not179. The next
goal turn should inspect current state, measure the normal profile without
diagnostic traces, and use owner attribution to choose a larger reduction in
RAM/GPU round trips. Steady30FPS, full exhibition and rendering/accuracy audit
remain open; do not substitute the7.3% selector gain for goal completion.
