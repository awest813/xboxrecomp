# Gameplay, rendering and accuracy audit

Checkpoint 186, 2026-10-31. [HANDOFF.md](HANDOFF.md) is the operational state.
[The prior audit](archive/GAMEPLAY_RENDER_AUDIT_PRE185.md) keeps the detailed
checkpoint-164 repair evidence.

## Fixed this checkpoint

- **Pitch-camera depth of field.** The whole pitch view was blurred, foreground
  pitcher included. The composite draw (target 0x82C70000, stage program
  0x00078421) samples the scene (t0), a 160x120 blur (t1) and the scene depth read
  as A8R8G8B8 (t2); stage 3 is DEPENDENT_AR, a lookup table addressed by the depth
  texel's (A,R) bytes. Combiner rc0 computes `(1 - t3.a)*scene + t3.a*blur`. Only
  stage modes 1/4/5 were implemented, so t3 was (0,0,0,1) and every pixel took the
  blur. DEPENDENT_AR/GB (modes 15/16) and SET_SHADER_OTHER_STAGE_INPUT (0x1E78) now
  work in both the D3D11 shader and the software combiner; two exact-texel cases were
  added to `nv2a_render_smoke` (WARP and hardware). Run 258's selector is sharp.
- **Contiguous memory lifetime** (run184's exhaustion crash). See HANDOFF.

## Retained correctness work from both lineages

- Resident renderer: filtering/LOD bias/mip chains, NV2A fog, per-channel clear
  masks, 16-bit and swizzled render targets (player shadows), GPU clears, flip
  pacing on PGRAPH_INCREMENT (menus at exactly 60), block vertex-program
  interpreter bit-identical to the reference (191 M lanes).
- Software shared-edge coverage uses winding-normalized top-left rules.
- The complete 0x3DD40 event switch was checked against original x86 across 8,288
  cases; its boundary is preserved by `tools_patch_boundaries.py` and the fixed
  `recomp_0001.c` body is in this build.
- Heap/TIB allocation is serialized (overlapping TLS blocks crashed run180); real
  critical-section waits release the lifted CPU and RAM lease.

## Open: between-pitch stall and animation-lookup crash

After every pitch the pitcher paces the mound with the aim reticle up for 25–70 s
before the pitch selector returns (a few seconds on hardware). With a runner on first
the fast lineage saw it never return (run 249). Ruled out earlier: dispatch misses,
unlifted stubs, blocked guest threads, stuck pad state, guest clocks. Ruled out now:

- Disc I/O. During the gap only audio streams are read (crowd/vendor `.AST`, speech
  `PROJn` `.BIG`); no animation data loads.
- The audio clock. APUOUTPUT is exactly 48 kHz; the game mixes SFX/speech in software
  into one ring (voices 244–246), so seven active voices is expected.
- x87 precision. The title's `fldcw` sites are CRT save/restore in math helpers;
  nothing sets 24-bit precision, so the lifter's `double` model matches.

Run 256 (merged build) **crashed after its sixth pitch**: guest read of 0xFFFFFFFF in
`sub_00331FB0` at 0x331FF4. That site calls `sub_00329C40` then `sub_00329D20`,
which searches up to four animation banks (binary search, `sub_00326640`, lifted
correctly) and returns −1 when no bank holds the ID; the caller dereferences the
result without checking. The same pair is checked for −1 later in the function, and
the stalled main thread was sampled in the same subsystem (`sub_0032E100`).
Hypothesis: a required animation ID is missing from the loaded banks after certain
plays. The wait path idles until something else changes; the unchecked path crashes.
A probe at the exit of `sub_00329D20` (run 259, local generated-code patch) logged
**no** failed lookups across five pitches, so the stall is not a missing animation;
the run 256 crash is a rarer path that still needs its ID captured.

New evidence from runs 256/258/259:

- The ordinary wait is nearly constant: pitch-to-pitch 70–79 s with the soak's ~6 s
  of input and sleep, so about 64 s of waiting, with occasional short gaps (22 s,
  41 s) after balls in play. A near-constant wait reads like a game-side timeout
  expiring, not slow progress. No kernel wait with a finite timeout is involved.
- Run 259 reproduced the indefinite case live: pitch 5 was ball four, a runner went
  to first, and the "82 MPH 4-Seam Fastball" result banner never cleared. The
  pitcher holds the ball and paces; batter and catcher are set. Black, White, both
  sticks and both triggers do nothing (A/B/X/Y and D-pad were tried earlier).
- The host stack shows the main thread busy in per-frame work, not blocked.
- The in-game flow script has explicit wait commands: `cWaitForAnimation`,
  `cWaitForActiveActors` (factory `sub_00135AA0`, vtable 0x617658, name hash
  0xDC217F0C), `WaitForSpecificActor`, `WaitForSuitableBreakInTheAction,Event,TimeOffset`,
  `WaitForRelay`, `WaitForCutoff`, `WaitForInfield`. The animation library is EA's
  stategraph (`stategraph.cpp`, `pathlist.cpp`: transitions, warp lists, heading
  correction). The speech queue (`speechqueue.cpp`, `SPCH_*`) streams play-by-play
  from `PROJn` `.BIG` during the wait.
- Fixed after comparing other EA ports (runs 260–261), none of which changed the
  ~70 s gap: precise 1 ms guest clocks (KeTickCount on its own thread, interrupt
  time, timed waits, drift-free kernel timers; MVP 06 arms no kernel timers), the
  DirectSound interrupt (vector 0x35; MVP 06 never raises notifier status because it
  software-mixes everything), and `frndint` ignoring the rounding mode (the CRT's
  `floor`/`ceil` rounded to nearest at 22 sites). So the stall is not a coarse or
  frozen clock, not a missing audio interrupt and not floor/ceil.
- Another public port exists: maddog847's MVP 06 and MVP 2005 Xbox recomps on
  TGP482/XboxXBERecomp report gameplay, saves and audio working; the MVP 2005 one
  ships a source package (not downloaded). The MVP 2005 GameCube decomp documents
  `igflow.txt` and a fixed 1/60 s logic tick fed by cycle-counter deltas.
- Outcome decides the gap (run 258): after a **ball or strike** the wait is ~64 s;
  after a **ball in play** (hit to right, a 430 ft home run) the next selector comes
  quickly. Dense captures (run 262, `FB_DUMP_EVERY=10`) show the catcher's throw-back
  is caught normally; afterwards the pitcher keeps pacing to new spots with the aim
  reticle up while the pitch-selection HUD stays hidden. The "pale-blue flash" seen
  in run 258 is the home-run stat card over open sky, likely intended.
- `igflow.txt`/`feflow.txt` (RefPack in `datafile.big`) are UI screen flows, and the
  `WaitFor*` commands appear only in replay/cut-scene camera scripts
  (`script_ar_*`, `script_createaplayer`), so the post-pitch gate is in code.
- The runner/fielder AI state table at 0x621D3B–0x621F74 (`LEADOFFINITIAL`,
  `LEADOFFRETREAT`, `ENDPITCH`, `THROW_2_PITCHER`, `PREPITCH`, …) fits an
  "all actors settled" gate: the pitcher's walk back never registers arrival (a ~60 s
  fallback ends it), and a runner leading off first never settles (no fallback).
- MVP 2005 GameCube decomp (ptitty12/mvp2005-decomp, `tu_8019F8C0.cpp`,
  `tu_8015F164.cpp`): the next pitch waits only for the post-play presentation script
  players (viewports 1 and 5) to go idle; `cWaitForActiveActors` waits with no
  timeout for actor slots 2–14 to drop their goals; `cScriptWaitGoal` gives an actor
  an idle goal of `min + rng(1e6)*(max-min)*1e-6` seconds. MVP 06's
  `cScriptWaitGoal::slot11` is `sub_00139620` (vtable 0x616F44) and lifts faithfully;
  a duration probe is in local `recomp_0007.c`. Run 263 meant to exercise it routed
  into the replay viewer instead (detector false match, host at 100%), so it is
  unverified.
- XboxXBERecomp notes that CRT x87 math branches on ZF returned by a helper; our
  lifter keeps flags per function. MVP 06's `pow` (`sub_00445AC4`) branches after
  `call 0x44B1CD` on the stale x-exponent compare, so it mis-handles a special y with
  ordinary x. Real but rare; not the stall.
- Leading hypothesis: a flow wait on an actor or event (the pitcher's walk back to
  the rubber, or a speech/animation event) that never completes, ended by a timeout
  except when a runner is on base. Next: find the evaluation of hash 0xDC217F0C and
  the other wait commands, and log which one the post-pitch flow is blocked in.

## Other visible/runtime limits

- Some camera cuts flash a full-screen pale-blue frame (run 249). Not investigated.
- Texture-stage modes other than 1/4/5/15/16 (cube, bump-env, dot products) still
  produce (0,0,0,1).
- No original-console frame comparison quantifies remaining differences.

## Audit after the performance gate

1. Capture fixed camera/input scenarios on original Xbox and the port at the same
   simulation points.
2. Compare geometry transforms, viewport/near clipping, culling, colour masks and precision.
3. Cover Z16/Z24, stencil operations, overlapping targets and all CPU aliases.
4. Check texture addressing/filtering, formats, mip behaviour, fog, blending and effects.
5. Verify controller edges/analogs, pitch/bat/field timing, rules and pause behaviour.
6. Compare audio timing/mixing and save/reload across full exhibitions and long sessions.

Do not change rendering semantics merely to increase FPS.
