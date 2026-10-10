# MVP 06 NCAA Baseball — Recomp (Windows PC port)

A native Windows x64 port of *MVP 06 NCAA Baseball* (original Xbox, 2006) by
**static recompilation**: the retail XBE's x86 machine code is lifted to C and
compiled with MSVC into an ordinary `.exe`. No emulator, no interpreter — the
game's own code runs natively against a compatibility runtime.

Built on [**sp00nznet/xboxrecomp**](https://github.com/sp00nznet/xboxrecomp)
(MIT; full upstream history preserved in this repo, with this title's fixes
layered on top — see [NOTICE](NOTICE) and [mvp06/docs/ENGINEERING_LOG.md](mvp06/docs/ENGINEERING_LOG.md)).

> **You must own the game.** No game code, assets, ISO, XBE, or generated C is
> in this repository, ever. The lifted C is mechanically derived from the
> game's own code and is rebuilt locally from your own disc dump.

## Status (2026-10-10 — bring-up in progress)

The port boots and initializes: kernel, threads, save partitions, USB gamepad
enumeration (OHCI), audio (emulated MCPX APU → XAudio2, DirectSound init,
DSP doorbells), and the software-NV2A GPU path (pushbuffer executed, 640×480
target cleared). It loads 189+ files from the disc including the frontend's
splash art, textures, and audio banks. Current frontier: EA's datafile/material
parsing (deterministic crash under repair — the root cause, a silently
non-copying CRT `memcpy` caused by jump-table decode gaps, is fixed at the
lifter level; verification in flight). Not yet playable — see
[mvp06/docs/PLAYABILITY_PLAN.md](mvp06/docs/PLAYABILITY_PLAN.md) for the
audited path to playable (first frame → menus → exhibition game → performance
tier via a D3D11 pushbuffer renderer).

## Repository layout

| Path | What |
|---|---|
| `mvp06/` | The game project: `CMakeLists.txt`, `src/main.c` (boot), `src/recomp_manual.c` (overrides), `config/seed_functions.json`, debug tools, and `AGENTS.md` + `docs/` |
| `src/`, `include/`, `tools/`, `templates/` | The xboxrecomp toolkit (upstream + this project's runtime/lifter fixes) |
| `mvp06/docs/` | **ENGINEERING_LOG.md** (full fix history), **HANDOFF.md** (current state), **PLAYABILITY_PLAN.md** (audited plan) |

## Building (Windows)

Prerequisites: Visual Studio 2022+ Build Tools (MSVC x64), CMake 3.20+,
Python 3.10+ with `capstone` and `pytest`. Then:

```bash
# 1. Extract your own disc dump (default.xbe + data/) into mvp06/game/
py -3 -m tools.xiso unpack "MVP 06 NCAA Baseball (USA).iso" -o mvp06/game/

# 2. Parse + disassemble + classify + ABI (first time only)
py -3 -m tools.xbe_parser mvp06/game/default.xbe --json mvp06/game/mvp06_analysis.json
py -3 -m tools.disasm mvp06/game/default.xbe --analysis-json mvp06/game/mvp06_analysis.json
# NOTE: re-applying mvp06's function-boundary patches after any disasm re-run
# is currently required — see mvp06/docs/HANDOFF.md "Boundary patches".
py -3 -m tools.func_id mvp06/game/default.xbe
py -3 -m tools.abi_analysis mvp06/game/default.xbe

# 3. Lift to C and build (lift ~10 min, build ~40 min)
py -3 -m tools.recomp mvp06/game/default.xbe --all --split 1000 --gen-dir mvp06/src/recomp/gen
cmake -S mvp06 -B mvp06/build
cmake --build mvp06/build --config Release

# 4. Run (from mvp06/)
cd mvp06 && RECOMP_VBLANK=1 ./build/Release/mvp06_recomp.exe
```

Everything else (`RECOMP_PB_EXEC`, `RECOMP_AC97_READY`, `RECOMP_USB`,
`RECOMP_APU_DSP_ACK`, …) is defaulted in `mvp06/src/main.c`.

## Contributing / working on this

Humans and AI agents: start with [mvp06/AGENTS.md](mvp06/AGENTS.md) (working
instructions, debugging loop, traps) and [mvp06/docs/HANDOFF.md](mvp06/docs/HANDOFF.md).

## License

Toolkit and runtime: MIT (see [LICENSE](LICENSE)), with LGPL-2.1+ portions
derived from xemu (see [LICENSES](LICENSES/) and [NOTICE](NOTICE)). Project
documentation under `mvp06/` is MIT as well. No affiliation with EA, Microsoft,
or the NCAA. Unofficial preservation project; supply your own game.
