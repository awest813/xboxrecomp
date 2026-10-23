# Gameplay and rendering audit — 2026-10-23

Exhibition progression now verified on164: user manually navigated default
Florida at Texas setup, and live screenshots confirm team/difficulty menus,
stadium transition, Texas player introductions, Florida manager huddle, and
the interactive pitching screen at0–0. The pitch selector is independently
confirmed by two saved captures (window565/566, best mask error2.04 against
the prior accepted selector reference). Player models/uniforms, stadium,
shadows, strike-zone target, pitch controls and scoreboard are visible.
This is a visual functional check, not an original-console pixel comparison.
No new crash/unlifted-stub signature observed. Manual two pitches followed
by pause/resume were requested and remain pending; do not mark them passed.

A20.001058-second sample during the stadium/player-introduction segment
measured77 flips (3.850 FPS),961,024 accepted audio frames (48,048.658/s),
zero audio rejects and zero shader compilations. Readback1,735,663,964 bytes,
surface uploads1,290,221,764 bytes. The changing scene and manual inputs mean
this is not a matched benchmark. Evidence:build/audit164_exhibition_stats.json.
Runtime/build unchanged and PID15096 remains live; never relink live.

Further manual-input check: the user's held Enter press successfully left
Demo Mode and returned to the title screen (visually verified). The third
brief automated Enter pulse did not advance the title. Manual navigation
through Play Now/default exhibition settings has been requested; stadium
and interactive pitching acceptance are pending. Treat run164 as mixed
manual/automated input, not an unattended route or controlled input baseline.

Resumed validation after the user restored the window: run164 PID15096 is
alive and has progressed past run163's mini-game-loading failure into the
animated pitching mini-game demo. Live screenshots show pitching, changing
scores/targets, timer and HUD; 2,607 PATH entries and no crash/unlifted-stub
signatures were observed. Two automated Enter down/up pairs reached the
window but did not leave the demo. Their short duration can be missed by guest
input polling; the supported Computer Use API has no key-hold duration. A
manual roughly two-second Enter hold was requested and remains pending.
Exhibition/two-pitch acceptance remains incomplete. No further runtime edits
or relink were made while this game was live.

A 20.001234-second demo sample measured97 flips (4.850 FPS),960,000 accepted
audio frames (47,997.039/s), zero audio rejects and zero shader compilations.
Readback1,591,717,188 bytes and surface uploads1,158,121,344 bytes show that
GPU/RAM traffic remains substantial. These are demo-only observations with
no matched control, not a speedup or listening-quality claim. See private
build/audit164_demo_stats.json; no mute setting changed.

Release164 builds successfully, with no warning lines in build_log164.txt.
Initial checkpoint: live gameplay acceptance was incomplete; the user stopped Computer Use with
physical Escape during run164. No further UI input was sent. Run164 was left
running for the user; its launcher retains the child handle and writes the
actual exit code to build/exit164.json when it exits. Never relink while live.

## Confirmed defects and repairs

1. Software triangle coverage included both sides of a shared edge. A
   translucent quad on black produced 0x60C0C0C0 on its diagonal instead of
   0x40808080. Hardware and WARP already produced the correct result. The
   software rasterizer now applies winding-normalized top-left coverage.
   Regression checks cover triangle lists, strips, and quads, both windings,
   integer and half-pixel outer bounds, and untouched surrounding pixels.
   Software, hardware, WARP, and hybrid cutoff4096 all pass. This establishes
   coverage consistency, not complete NV2A rasterization accuracy or an FPS gain.

2. Unchanged Release161 run163 reached direct stub 0x0003DDF2, then crashed
   writing Xbox VA 0xBB55BC94. The final captured screen is pitching-mini-game
   high-score loading. There were 2,587 PATH entries versus partial run162's
   591; these represent different progress, not a performance comparison.
   Original-byte inspection proves 0x3DDC0 is an internal switch arm and
   0x3DDF2 is the shared POP ESI / RET 16 epilogue of 0x3DD40. Returning through
   the stub bypassed stack cleanup. Preserve 0x3DD40..0x3DDF6 as one function;
   exclude the following tables through 0x3DE50 from function discovery.
   The boundary helper now preserves this ownership. A targeted lift is
   installed in generated chunk1; a future full lift must run the boundary
   helper first, as usual. Older unused generated aliases remain until that
   full lift; no standalone seed was added for the interior arm.

The event-switch comparison executes the original x86 with controlled callees
and compares the lift across 8,288 cases: all six arms, default paths, memory,
callee arguments, registers, and RET 16 stack cleanup. All pass. This verifies
the local lift repair; it does not yet prove the later in-game crash is gone.
Use python -m mvp06.tools_verify_event_switch, then run
mvp06/tests/event_switch.cmd from the repository root on the configured MSVC
host. The --install option reapplies this tested body after a targeted lift.
The harness loads original bytes from the local XBE; no game bytes are stored
in the shared test source.

## Validation and limits

- Toolkit: 239 passed, 24 skipped, 44 subtests passed.
- Event switch: 53 nonoverlapping instructions, six local switch targets,
  original-x86 comparison passes 8,288 cases.
- Pushbuffer suite: software, hardware, WARP, and hybrid4096 pass; hardware
  and WARP each execute 91 GPU draws with zero fallback in the final suite.
- Existing decoded renderer: 2,600 GPU draws, zero failures (WARP legacy
  readback), including Z16/Z24, stencil preservation, aliases and scope barriers.
- Build/source hashes: build/build164_hashes.json. Logs and captures are
  private build artifacts; run163_stderr.txt contains the baseline crash.
- Run163's independent process observer returned a null exit code; do not
  call that a clean exit. The crash signature is directly present in the log.
- Run164 uses packed readback, scopes, cutoff4096 and diagnostics/GPU_YIELD
  off. No mute setting was changed. Full frontend, two pitches, pause/resume,
  complete exhibition, save/reload, and audible quality remain unverified on164.

Resume by checking whether run164 is still live and inspecting its log/exit
record. Continue acceptance before further runtime changes. In addition to
the exhibition route, reproduce idle-demo/minigame loading past run163's
failure. Compare PATH counts and crash/stub signatures, then visually inspect
stadium, player, HUD and pitch captures. Original-console comparison, near
clipping, stencil operations, culling/color masks and remaining unsupported
sampler/fog behavior still need a separate accuracy audit.
