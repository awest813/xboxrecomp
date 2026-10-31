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

## Status against the milestones (2026-10-23)

| Milestone | Status | Evidence |
|---|---|---|
| M1 config parse | reached | runs 53/54 |
| M2 first frame | reached | run 52; intro movies render |
| M3 menus with audio | reached except listening check | full frontend route by script; APU delivers 48 kHz; audible quality unverified |
| M4 exhibition plays | **partial** | live pitches, walks, hits, fielding, runs, new batters, pause/replay (runs 249/250); blocked by the between-pitch stall (HANDOFF "Open issues" 1); saves unverified |
| M5 performance | reached for the pitching scene | D3D11 + GPU-resident surfaces: 4.3 → 30–37 FPS at the selector; 20-min soak stable (run 247); menus paced at 60 |
| M6 playable | not yet | needs M4: a full exhibition, saves |

Next, in order:
1. Fix the between-pitch stall.
2. Play a full half-inning, then a full exhibition.
3. Check save/reload.
4. Compare the pitch-camera DOF blur and the camera-cut artifacts with original
   output, and do an audio listening pass.

Run-by-run checkpoint notes that used to sit here are archived in
[archive/PLAYABILITY_PLAN_checkpoints_to_2026-10-20.md](archive/PLAYABILITY_PLAN_checkpoints_to_2026-10-20.md).

## Audit: where the port stood (historical, 2026-10-10)

Kept for its evidence; superseded by the status table above.


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

### M3 — Menus navigable with audio (reached; listening check open)
Run84 verifies the complete main menu, team/jersey selector and difficulty
screen after LAHF repair; run85 repeats that route. Guest USB input drives
transitions, with long presses needed under the slow CPU rasterizer. Nonzero
APU/XAudio2 output reaches 48 kHz with low block-drop counts; audible fidelity
and music/SFX timing still need acceptance. Check the displayed P3 controller
assignment against USB port mapping. Acceptance: navigate the frontend with
a pad and hear the intended menu music/SFX.

### M4 — Exhibition game loads and plays (partial: live pitching; between-pitch stall open)
Run85 opens stadium/dishday.big, player textures, signature-animation data
and crowd/overlay audio banks. Cycle86 adds three measured missing entries
before the next loading retry. The FIFO guest CPU lock, dispatcher objects,
IRQL publication and OHCI timing/order repairs are implemented; continue
acceptance for worker/USB lifetimes rather than re-porting the scheduler.
Next verify stadium output, pitching/batting/fielding, commentary/crowd and
save/reload. Acceptance: pitch an inning; no crash across a full half-inning;
saves persist under saves/.

### M5 — Performance tier (reached for the pitching scene, 2026-10-21)
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
- **GPU-resident surfaces instead of per-commit readback (2026-10-20)**: the
  remaining cost was latency between the GPU worker and the guest, not raw
  compute. Committed pixels stay on the GPU; page protection materializes them for
  any RAM observer, so RAM is never silently stale. 4.3 → 23.5 FPS. The scoped
  exclusive-RAM approach (RECOMP_GPU_SCOPES) stays available but off.
- **Native pacing from the title's own vblank handler (2026-10-21)**: FLIP_STALL
  waits on PGRAPH_INCREMENT as on hardware, instead of a host-side frame cap.
- **Speed paths keep a reference check**: each fast path that replaces an exact
  one (VP pre-decode/block, swizzled decode) ships with a compare mode and was
  run bit-identical over 10^7–10^8 samples before becoming the default.
