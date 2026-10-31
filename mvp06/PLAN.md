# MVP 06 implementation plan

Checkpoint 186, 2026-10-31. Current state and commands: [docs/HANDOFF.md](docs/HANDOFF.md).
The original toolchain audit and milestone history are archived:
[checkpoint-185 lineage](docs/archive/PLAN_PRE185.md), [resident-renderer lineage](docs/archive/PLAN_FAST254_2026-10-23.md).

## Objective and order

Get in game at a steady 30 FPS or better, then make it accurate. Preserve original
gameplay and timing. Menus pace at exactly 60 (the title's own presentation rate);
gameplay may run up to 60.

## Done at checkpoint 186

- The resident D3D11 renderer and the runtime-correctness fixes are on one branch.
  The pitch selector runs 38–40 FPS at 54% host load (23–36 under heavier load).
- Contiguous memory is freed and reused (run184's arena exhaustion).
- The pitch-camera depth-of-field composite is correct (dependent texture stages).

## Next stages

1. **Between-pitch stall and the animation-lookup crash.** After each pitch the
   pitcher idles 25–70 s before the selector returns, indefinitely with a runner on
   first. Run 256 crashed after pitch 6 dereferencing a failed animation lookup
   (`sub_00329D20` returned −1 at 0x331FF4). Find which ID is missing from the
   loaded banks and why. See HANDOFF "Open issues".
2. **Gameplay FPS under load.** Matched comparison of `RECOMP_CS_GUEST_FASTPATH=1`
   (opt-in, native tests pass) in both orders with host load recorded; profile the
   GPU command worker with RECOMP_EXEC_TIMING=1 during batting/fielding, not only
   at the selector.
3. **Complete exhibition.** Pitching, batting, fielding, inning and game transitions,
   pause/replay/box score, then save, exit, restart and reload.
4. **Rendering accuracy.** Pale-blue flash on some camera cuts; remaining texture-stage
   modes (cube map, bump-env, dot products) if a draw log shows them; compare fixed
   scenes against original-console captures.
5. **Audio.** Listen for continuity and timing across a long session (output is
   48 kHz; audible quality is unverified).
6. **Full lift.** Replace the local `recomp_9999.c` preflight chunk and the
   hand-bumped dispatch count with a full disasm/lift cycle, then re-verify the
   0x3DD40 event switch.

## Acceptance discipline

Separate native correctness, runtime behaviour, scene-specific speed and broad
playability claims. Test the failed behaviour before and after, keep input and scene
provenance, and record crashes and deliberate stops explicitly. The source, the
defaults and the loaded EXE/MAP must agree before a sample is interpreted.
