# MVP 06 — Audited Plan to a Playable PC Port

Definitive plan, replacing the milestone sections of [../PLAN.md](../PLAN.md)
(which remains the living iteration log). Written 2026-10-10 after four
bring-up iterations and a study of the four reference ports
(burnout3, nfsu2-sw/nfsc-sw, halo-ce-universal, doaxvb-re).

## Audit: where the port actually stands

**Proven working (verified by runs, not assumption):**
- Toolchain end-to-end: 37,734 functions lifted (0 failures), native exe
  builds and links (56.9 MB, `/MAP` symbolication).
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
- The memcpy decode-convergence fix just landed (three stacked lifter/
  boundary defects — see ENGINEERING_LOG "2026-10-10 (final)"). The full
  lift is verified clean (0 dead-code arms); the runtime verification run
  was still in the build queue at handoff. Expect the datafile/mat.o crash
  chain to be gone or moved.
- No visible frame yet (frontend parse precedes first present, presumably).
- Rendering tier is the CPU rasterizer (Burnout-3-class: 2–14 fps) — fine
  for bring-up, not for playability.
- Untouched: VP6 movies (EA container; NFSU2 decodes theirs with FFmpeg),
  memory-unit save paths beyond T:\ translation, XID pad attach completion.

**Known risks (from reference-port evidence, not speculation):**
1. **EA worker races** — NFSU2 needed a guest lock (GIL) because its stream
   system hands unfilled blocks to workers. MVP's crashes so far are
   deterministic (bit-identical reproduction), so this is not blocking yet;
   expect it once streaming begins (frontend movies/audio streams).
2. **More dispatch misses** — every subsystem boundary has so far surfaced
   1–10. Converged quickly each time.
3. **D3D pushbuffer semantics** — NFSU2's executor needed NOP-trap delivery,
   pattern-color fences, semaphore releases, per-subchannel register shadow
   discipline. Our v0.11.0 executor lacks these; expect freezes at first
   Present/flip.
4. **XID input completion** — burnout3 needed table-reachable driver entries
   seeded (fix 13); our enumeration started but pad attach not yet confirmed.

## Milestones to playable

### M1 — Config parse completes (frontier; verification in flight)
The memcpy fix (the silent no-copy root cause) plus seed convergence on the
datafile path; then first frontend init completes.
Acceptance: no crash for 5 min; file-open count grows past ~200 (frontend
art/anims); `[GPU]` reports draws (not just clears).
Effort: 1–3 seed/build cycles (~1 h each, mostly build time).

### M2 — First visible frame
The FB window (RECOMP_FB_WINDOW=1, already default) shows the EA/splash
screen. Likely blockers, in expected order: flip/Present semantics (adopt
NFSU2's `nv2a_pb_exec.c` trap/semaphore/fence handling — see M5 for the
rendering swap; for M2 the CPU rasterizer suffices), vblank frame counter
(`xbox_Nv2aFrameCounter(0x589A38, <swap-count offset>)` — find offset the
way Burnout 3 did: the XMV/present pacing read), missing combiner/texture
methods in the executor (burnout3 fixes 10/20/21/22 enumerate the classes).
Acceptance: non-black frame in the window; `RECOMP_FB_DUMP` produces images.

### M3 — Menus navigable with audio
Frontend renders and responds; XID pad attach completes (burnout3 fix 13
seeding pattern if needed); menu audio plays through the APU/XAudio2 path
(voice creation already works). Acceptance: navigate main menu with a pad;
hear menu music/SFX.

### M4 — Exhibition game loads and plays
Loading a stadium (`data/stadium/*.big`), lineup, and gameplay sim. Expect:
EA stream workers (risk 1 — port NFSU2's GIL block, ~200 lines + dispatch
wiring), audio streaming for commentary/crowd (.ast — DirectSound streams),
save-game write on exit (T:\ translation exists). Acceptance: pitch an
inning; no crash across a full half-inning; saves appear under saves/.

### M5 — Performance tier (interleavable from M2 onward)
Swap the CPU rasterizer for the NFS fork's PGRAPH→D3D11 translator
(`reference/nfsu2-sw/xboxrecomp/src/nv2a/` + their `nv2a_pb_exec.c`), which
drives the toolkit's existing D3D11 layer on Windows. Their CLAUDE.md lists
the exact per-fix rationale (culling-after-blit, surface keying, FVF
defaults, depth handling). Acceptance: 60 fps frontend / ≥30 fps gameplay.

### M6 — "Playable" declared
Full 9-inning exhibition with audio, pads, saves. Then the enhancement
program from PLAN.md Phase 4 (resolution/aspect, MSAA/SMAA, texture
replacement, color grading, roster mods via I/O shadowing) — all host-side,
per the original remaster direction.

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
- **No GIL yet** — crashes have been deterministic; port NFSU2's GIL when a
  race is actually observed (their implementation is self-contained).
- **Runtime fixes stay title-agnostic** — four so far, all upstream-shaped.
