# Gameplay performance experiments 167–168

The tighter triangle bounds reduce readback traffic, but the matched gameplay
measurements do **not** establish a frame-rate improvement. Keep the existing
512-word synchronous fallback and all publication, alias, fence and IRQ rules.

## Same-process bounds comparison (167)

Florida–Texas pitching selector matched two captured frames against existing
references (error 1.70). Seven input downs were unchanged throughout both
comparisons. Renderer/memory timing and GPU_YIELD were off; packed readback,
scopes and the 4096-pixel threshold stayed on. The old 164 process was suspended
only during sampling and resumed in each helper's finally block.

Each phase settled for five seconds and sampled for twenty seconds. First ABBA:
control 3.550, candidate 3.700, candidate 3.599, control 3.400 FPS. Warm reverse
BAAB: candidate 3.750, control 3.700, control 3.800, candidate 3.699 FPS. The
reverse run removes the apparent initial gain; do not report a pooled speedup.
Readback fell consistently from approximately 20.7 to 19.1–19.3 MB per flip
(about 7%). Audio remained near 48,000 accepted frames/s with zero rejections.
167 was deliberately stopped after profiling; original 164 resumed.

## Adaptive flag worker (168 acceptance; 169 default)

The old Sleep(0) loop consumes nearly one host CPU thread while idle. An opt-in
`RECOMP_NV2A_ACK_POLL_US` (0–1000, originally default 0) uses a high-resolution waitable
timer after two milliseconds without a write-combine request. Bursts retain
Sleep(0); acknowledgment logic, IRQ publication locks and masks are unchanged.
Unavailable timers fall back to the old loop. After acceptance, the default
became 100; explicit zero and invalid values retain the old loop.

A flat 100-microsecond delay was rejected: 128 acknowledgments took about
235 ms. The adaptive variant's QPC sample took 0.577 ms for 128 acknowledgments;
the control sample took 2.046 ms (scheduling variation, not a speed claim).
Idle worker CPU was below the process accounting resolution versus 218.75 ms
over approximately 516 ms. Native 60 Hz clock, 10,000 vblank handoffs, trap/DPC
races, blocked-executor flushes, ownership and restart checks passed in both
modes. Evidence: `build/ack168_qpc0.txt` and `build/ack168_qpc100.txt`.

The separate 168 executable also includes an opt-in, cached minimum keyboard
hold for slow guest polling. Normal input defaults to zero hold; focus clear
cancels pending releases. Native packet, expiry, physical hold, independent key,
focus and physical-pad merge checks pass (`build/input168_test.txt`). This is
an automation aid, not a performance fix.

The live 168 selector gate passed on two distinct frames (final error 4.04).
Same-process polling ABBA measured control 3.947, candidate 4.050, candidate
4.000, control 3.900 FPS. Warm reverse BAAB measured candidate 3.950, control
3.700, control 4.097, candidate 4.100 FPS. Each phase used five seconds to settle
and twenty seconds to sample. The timer was confirmed available. Flag-worker
CPU per sample was 17,796.875–19,234.375 ms at zero, versus 1,093.750–1,328.125
ms at 100 microseconds (roughly 93–94% lower). No repeatable material FPS gain
is established; this is a CPU-use improvement. Seven exact input downs stayed
unchanged, diagnostic timing was off, and the old process resumed in both runs.
Full data are `build/poll168_abba_*.json` and `build/poll168_baab_*.json`.

168 polling100 passed both pitches/count2–0, pause and resume. Matching two-frame
gates confirmed first count at frame379, second selector at546 (error2.04),
second count572, pause588 and resume596 (count/menu errors0.00). Captures were
visually checked. Input downs were the seven frontend keys, two Z pitches and
two Enter pause/resume keys. The test hold was then set from2500 to0 with a
guarded matching-MAP write; candidate168 remains live with ordinary input.

Original164 PID15096 was deliberately stopped after168 acceptance, superseded
by the validated2–0 session. This frees the normal executable for rebuilding;
the live168 executable/MAP must remain untouched. Default polling100 is a
CPU-use improvement, not a claim of comfortable FPS or full-game validation.
Native169 tests repeat default100, explicit0 and invalid fallback, retaining
all IRQ/trap/ownership checks. Evidence lives in `build/ack169_*.txt`.
The normal169 executable rebuilt successfully; four preexisting memory-layout
warning sites remain. Hashes, matched acceptance input prefix and zero logged
dispatch misses/direct stubs/crashes/lost sync/USB cancellations are recorded
in `build/acceptance168_build169.json`. Two additional Z downs appeared after
acceptance; they are outside the benchmark and acceptance input sequence.
The separately named168 binary (same runtime behavior with poll100 selected)
retains the live session;169's changed startup default has native validation,
not a separate full exhibition run.
Never relink a running executable or use a different executable's MAP file.
