# MVP 06 NCAA Baseball — Windows recompilation

A native Windows x64 port of the original Xbox game, built with static recompilation and the [xboxrecomp toolkit](https://github.com/sp00nznet/xboxrecomp). The runtime replaces Xbox kernel, GPU, audio and input behavior while the lifted game code runs natively.

Supply your own disc dump. Retail game data, generated game C, XBE/ISO files and built game executables stay private. Toolkit/runtime sources and tests are shared. The existing synthetic conformance fixture is not a retail game image.

## Current checkpoint — 2026-10-31

Work is stopped for the requested handoff. **Steady 30 FPS is not achieved.** Build185 links successfully but has not been launched. No game or test helper is left running at this checkpoint.

Retained changes serialize heap/TIB/stack allocation and release CPU/RAM ownership during actual passive critical-section waits. Native concurrency and scheduling tests pass. The optional critical-section CPU fast path remains off by default and has no accepted gameplay FPS comparison.

The next blocker is memory lifetime: run184 exhausted the 64 MB contiguous arena after idle demos and exhibition loading, then crashed. The contiguous allocator never reclaims frees routed through the ordinary heap. This remains unfixed. Do not mistake it for the earlier TIB race or claim the scheduling experiment caused it without a controlled reproduction.

Earlier controlled run178 measured a scene-specific selector improvement from 3.77 to 4.05 FPS with bounded GPU handoff and passed two pitches plus pause/resume. Recent 1–4 FPS samples occurred under heavy host CPU contention and establish no additional gain. Full exhibition, saves, sustained pacing and original-console accuracy remain open.

Start with [the handoff](mvp06/docs/HANDOFF.md), [working instructions](mvp06/AGENTS.md), and [the whole-system audit](mvp06/docs/SYSTEM_PERFORMANCE_AUDIT184.md). [The documentation index](mvp06/docs/README.md) separates current guidance from historical evidence.

## Build and run

Windows prerequisites: MSVC x64 build tools, Windows SDK, CMake, Python and your own extracted game under `mvp06/game/`. For an existing generated checkout:

```powershell
cmake -S mvp06 -B mvp06/build
cmake --build mvp06/build --config Release --target mvp06_recomp
$env:RECOMP_CS_GUEST_FASTPATH = '0'
Push-Location mvp06
.\build\Release\mvp06_recomp.exe
Pop-Location
```

Check for a live executable before rebuilding; preserve its EXE and matching MAP. Build185 is ready for the next deliberate run, not runtime-accepted. Full extraction/lift commands are in [the handoff](mvp06/docs/HANDOFF.md).

MVP defaults select D3D11, packed readback, RAM scopes, a 4096-pixel cutoff, vblank, GPU yield and a bounded 2000-microsecond handoff. Explicit environment overrides win. Toolkit-wide yield/handoff and the new CS fast path stay opt-in. Normal input does not stretch key presses.

Controls: Enter=Start, Z=A, X=B, A=X, S=Y, arrows=D-pad; numpad 4/6/2/8=left stick and J/L/K/I=right stick. Preserve the user's audio mute preference.

## Repository

| Path | Contents |
| --- | --- |
| `mvp06/` | Title boot code, manual hooks, seeds, plans and handoff |
| `src/` | Shared runtime |
| `tools/` | Extraction, disassembly, lifting and analysis |
| `tests/` | Native and toolkit regressions |
| `mvp06/game/`, `mvp06/build/` | Private ignored assets and local evidence |

## License

Toolkit/runtime: MIT, with LGPL-2.1+ xemu-derived portions; see [LICENSE](LICENSE), [LICENSES](LICENSES/) and [NOTICE](NOTICE). Full upstream history is preserved. This is an unofficial preservation project, unaffiliated with EA, Microsoft or the NCAA.
