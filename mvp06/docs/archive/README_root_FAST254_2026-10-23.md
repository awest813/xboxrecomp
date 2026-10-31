# MVP 06 NCAA Baseball — Recomp (Windows PC port)

A native Windows x64 port of *MVP 06 NCAA Baseball* (original Xbox, 2006) by
**static recompilation**: the retail XBE's x86 machine code is lifted to C and
compiled with MSVC into an ordinary `.exe`. The game's CPU code runs natively
against a compatibility runtime implementing Xbox kernel, graphics and audio
behavior.

Built on [**sp00nznet/xboxrecomp**](https://github.com/sp00nznet/xboxrecomp)
(MIT; full upstream history preserved in this repo, with this title's fixes
layered on top — see [NOTICE](NOTICE) and [mvp06/docs/ENGINEERING_LOG.md](mvp06/docs/ENGINEERING_LOG.md)).

> **You must own the game.** No game code, assets, ISO, XBE, or generated C is
> in this repository, ever. The lifted C is mechanically derived from the
> game's own code and is rebuilt locally from your own disc dump.

## Status (2026-10-23 — partly playable, work in progress)

**Live pitching runs at 30–37 FPS** on a D3D11 renderer that keeps the game's
surfaces resident on the GPU. That is up from 4–5 FPS with the earlier
software/readback path. Menus run at exactly the console's 60 FPS. A 20-minute
auto-play soak was stable.

Verified: boot, both intro movies, the full frontend, stadium, pitching and
fielding. Balls, strikes, walks, hits, runs and new batters all play out. Pause,
replay and box score work. Rendering includes texture filtering and mipmaps, fog,
player shadows and the game's depth-of-field chain.

Not there yet:
- After each pitch the game waits far too long, and with a runner on base
  sometimes indefinitely. This blocks a full exhibition.
- The pitch camera looks over-blurred.
- Saves and audio listening quality are unverified.

[HANDOFF](mvp06/docs/HANDOFF.md) has the current state and open issues,
[PLAYABILITY_PLAN](mvp06/docs/PLAYABILITY_PLAN.md) the milestones, and
[ENGINEERING_LOG](mvp06/docs/ENGINEERING_LOG.md) the full fix history.

## Repository layout

| Path | What |
|---|---|
| `mvp06/` | The game project: `CMakeLists.txt`, `src/main.c` (boot), `src/recomp_manual.c` (overrides), `config/seed_functions.json`, debug tools, and `AGENTS.md` + `docs/` |
| `src/`, `include/`, `tools/`, `templates/` | The xboxrecomp toolkit (upstream + this project's runtime/lifter fixes) |
| `mvp06/docs/` | **HANDOFF.md** (current state, open issues), **ENGINEERING_LOG.md** (full fix history), **PLAYABILITY_PLAN.md** (milestones), `archive/` (older checkpoint notes) |

## Building (Windows)

Prerequisites: Visual Studio 2022+ Build Tools (MSVC x64), CMake 3.20+,
Python 3.10+ with `capstone` and `pytest`. Then:

```bash
# 1. Extract your own disc dump (default.xbe + data/) into mvp06/game/
py -3 -m tools.xiso unpack "MVP 06 NCAA Baseball (USA).iso" -o mvp06/game/

# 2. Parse + disassemble + classify + ABI (first time only)
py -3 -m tools.xbe_parser mvp06/game/default.xbe --json mvp06/game/mvp06_analysis.json
py -3 -m tools.disasm mvp06/game/default.xbe --seed-functions mvp06/config/seed_functions.json --analysis-json mvp06/game/mvp06_analysis.json
# Required after every disasm run; preserves memcpy's final return:
py -3 mvp06/tools_patch_boundaries.py
py -3 -m tools.func_id mvp06/game/default.xbe
py -3 -m tools.abi_analysis mvp06/game/default.xbe

# 3. Lift to C and build (lift ~10 min, build ~40 min)
py -3 -m tools.recomp mvp06/game/default.xbe --all --split 1000 --gen-dir mvp06/src/recomp/gen
# Required after every lift: D3D publication/handler hooks and PCRTC W1C stores.
py -3 mvp06/tools_patch_d3d.py
cmake -S mvp06 -B mvp06/build
cmake --build mvp06/build --config Release

# 4. Run (from mvp06/)
cd mvp06 && ./build/Release/mvp06_recomp.exe
```

The title's runtime settings (D3D11 renderer, GPU residency, flip pacing, USB pad,
keyboard, …) are defaulted in `mvp06/src/main.c`; any `RECOMP_*` already in the
environment wins. Keyboard: Enter=Start, Z/X/A/S = A/B/X/Y, arrows = D-pad,
numpad 4/6/2/8 = left stick. To pitch, tap Z three times about 0.75 s apart:
select, start the meter, stop it.

## Contributing / working on this

Humans and AI agents: start with [mvp06/AGENTS.md](mvp06/AGENTS.md) (working
instructions, debugging loop, traps) and [mvp06/docs/HANDOFF.md](mvp06/docs/HANDOFF.md).

## License

Toolkit and runtime: MIT (see [LICENSE](LICENSE)), with LGPL-2.1+ portions
derived from xemu (see [LICENSES](LICENSES/) and [NOTICE](NOTICE)). Project
documentation under `mvp06/` is MIT as well. No affiliation with EA, Microsoft,
or the NCAA. Unofficial preservation project; supply your own game.
