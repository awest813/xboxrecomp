# MVP 06 working instructions

Checkpoint185, 2026-10-31. The user requested a stopping point, refreshed documentation, handoff, commit and push. Do not automatically resume experiments after this handoff. Start with [docs/HANDOFF.md](docs/HANDOFF.md) when work resumes.

## Scope and current state

The objective remains steady 30 FPS in actual gameplay, then rendering and accuracy. It is not achieved. Build185 links; it has build/native coverage only. All game/test processes from this session are stopped. Historical PIDs and old “live” statements are not authority; inspect processes and exit records first.

The root is the shared toolkit; `mvp06/` is the game. Current priority is the contiguous-memory lifetime failure described in the handoff. Heap/TIB concurrency is repaired. CS contention releases CPU/RAM at passive level; `RECOMP_CS_GUEST_FASTPATH` defaults off and is unpromoted. The completed-bridge return-order experiment was rejected and removed.

## Ground rules

- Keep retail data, generated game code, game EXEs/MAPs, captures and local logs private. `mvp06/game/` and `mvp06/build/` are ignored. Do not add them with force.
- Keep shared runtime fixes title-agnostic. Title-specific defaults/hooks belong in `mvp06/src/`.
- Never relink a live executable or overwrite its matching MAP. Use a separately named candidate or deliberately retire the owned test process first.
- Preserve exclusive GPU slices of 4096 words/coarse 2 ms, synchronous fallback512, texture cache128 slots/128 MiB, and every RAM/publication/alias/GET/fence/IRQ contract unless a separately verified design replaces them.
- Contiguous graphics backing and its tiled alias are separate from low-RAM mirrors. Do not normalize graphics addresses into the ordinary heap when implementing frees.
- Preserve thread-local registers/TIBs and raised-IRQL ownership. Safe points cannot discard extra publication references. Real blocking waits need explicit argument-copy and resume boundaries.
- Run meaningful native regressions for runtime changes, then a deliberate game run with an exit observer. Compare PATH counts and crash/stub/miss signatures; counts alone do not prove correctness.
- Measure actual game flips in a verified scene. Use matching loaded PE/MAP, both comparison orders, stable settings and input provenance. GPU/memory timing off for FPS comparisons; distinguish diagnostic wall time from GPU execution and summed thread time.
- Record host contention. This session repeatedly observed 99–100% CPU load from concurrent work; do not claim a new FPS gain from those noisy comparisons.
- Use the computer-use skill for native UI input. Published framebuffer snapshots are useful renderer evidence. Do not revive private PostMessage input helpers. Focus loss must continue to clear keyboard state; test key stretching is opt-in and must return to0 outside acceptance.
- Preserve audio mute preference. Accepted sample rate does not prove audible continuity.

## Build and regeneration

```powershell
cmake -S mvp06 -B mvp06/build
cmake --build mvp06/build --config Release --target mvp06_recomp
```

After disassembly/lifter changes, run from the repository root:

```powershell
py -3 -m tools.disasm mvp06/game/default.xbe --seed-functions mvp06/config/seed_functions.json --analysis-json mvp06/game/mvp06_analysis.json
py -3 mvp06/tools_patch_boundaries.py
py -3 -m tools.recomp mvp06/game/default.xbe --all --split 1000 --gen-dir mvp06/src/recomp/gen
py -3 mvp06/tools_patch_d3d.py
cmake -S mvp06 -B mvp06/build
cmake --build mvp06/build --config Release --target mvp06_recomp
```

Do not regenerate merely to refresh documentation. The boundary helper is authoritative for memcpy, SEH, menu/player switches, pitching/post-pitch callbacks and the 0x3DD40 event switch. Reapply D3D publication/handler/PCRTC hooks after lifts; verify the event-switch body if regenerated. Generated-only patches can be erased by a lift.

## Tests and debugging

Native targets relevant to this checkpoint: `heap_concurrency_smoke`, `critical_section_smoke`, `memory_scope_smoke`, `guest_scheduler_smoke`, `nv2a_pb_scan_smoke`, `nv2a_surface_diff_smoke`. See the handoff for matrix settings and evidence. Lifter/toolkit changes also need `py -3 -m pytest tools/recomp -q` and relevant original-byte comparisons.

Guest addresses are32-bit; MAP function offsets are host offsets. Declare guest registers and `g_fs_base` with `RECOMP_TLS`. x64 CONTEXT RIP is0xF8 and RSP0x98. Resolve native faults using the loaded executable's base and matching MAP. Inspect `[CRASH]` and guest return addresses before inventing a workaround.

Direct generated calls bypass manual dispatch lookup. Check original function boundaries, overlapping decodes and complete returns; internal switch arms are not new function seeds. `RECOMP_FRAME_TRACE` reflects live combiner state; older raw-method snapshots can be stale.

Normal main defaults are in `src/main.c`; shared scheduling is in `../src/kernel/kernel_guest.c`, RAM ownership in `../src/platform/guest_memory.c`, and current allocation bugs in `../src/kernel/xbox_memory_layout.c` / `kernel_bridge.c`. [The documentation index](docs/README.md) links the historical evidence instead of embedding it here.
