# MVP 06 NCAA Baseball — Handoff

One-page state of the world for whoever picks this up (human or agent).
Companion documents: [ENGINEERING_LOG.md](ENGINEERING_LOG.md) (full history),
[../AGENTS.md](../AGENTS.md) (working instructions), [PLAYABILITY_PLAN.md](PLAYABILITY_PLAN.md)
(audited plan to playable). Repo root README covers build steps.

## What this is

A native Windows x64 port of MVP 06 NCAA Baseball (Xbox, 2006) built by static
recompilation with the xboxrecomp toolkit (upstream v0.11.0 + this project's
fixes, all in this repo). Game project: `mvp06/`. Everything runs from the
recompiled game code — no hand-written game logic.

## Current status (2026-10-10, end of session)

**Boots through kernel/threads/saves, enumerates a USB pad (OHCI), brings up
audio (APU→XAudio2, DirectSound init, two DSP doorbells acknowledged),
executes the GPU pushbuffer (software NV2A: 640×480 target cleared, DMA
GET tracks PUT), and loads 189+ disc files (frontend splash, textures,
audio banks, materials).**

**Just landed and verified (handoff state):** the decode-convergence fix in
`tools/recomp/translator.py` — the reason CRT `memcpy` (sub_00442CF0)
silently copied nothing on jump-table paths. Three stacked defects fixed:
function boundary (extended to 0x443028 in `tools/disasm/output/functions.json`;
stale 0x442xx seeds removed), unreadable table shapes (overlapping-first-dword
and negated-index tables — lifter `_analyze_switch_table` fallback), and the
one-shot switch-resync (now iterated to convergence). Full-lift verified:
0 dead-code arms, all memcpy labels live. Verification runs: **first fully
clean dispatch log of the project** (240 s, zero `Failed to resolve VA`),
though boot now parks at an earlier wait (32 paths, audio banks, doorbells
acknowledged) and a second run crashed at a different site — behavior is
timing-dependent with no dispatch failures, pointing at the EA worker-race
class (NFSU2's GIL) as the next suspect.

**Next steps, in order:**
1. Run the rebuilt exe (command below) — expect the frontier to move past
   `DATA/mat.o` parsing; harvest any new `Failed to resolve VA` entries and
   seed them (see AGENTS.md step 4).
2. First visible frame (RECOMP_FB_WINDOW=1 is on): if flips stall, adopt the
   NFS fork's `nv2a_pb_exec.c` trap/semaphore handling (reference/nfsu2-sw).
3. XID pad attach completion (burnout3 fix-13 seeding pattern).
4. Performance tier: port `reference/nfsu2-sw/xboxrecomp/src/nv2a/
   nv2a_pgraph_d3d11.c` + their executor (pushbuffer → D3D11).

## How to build and run

Incremental (runtime/main/recomp_manual changes — minutes):
```bash
cmake --build mvp06/build --config Release
cd mvp06 && RECOMP_VBLANK=1 timeout 180 ./build/Release/mvp06_recomp.exe 2> run.txt
```

Full pipeline (after seed/lifter/disasm changes — ~55 min):
```bash
py -3 -m tools.disasm mvp06/game/default.xbe --seed-functions mvp06/config/seed_functions.json --analysis-json mvp06/game/mvp06_analysis.json
# then RE-APPLY the boundary patches (below!), then:
py -3 -m tools.recomp mvp06/game/default.xbe --all --split 1000 --gen-dir mvp06/src/recomp/gen
cmake --build mvp06/build --config Release
```

**Boundary patches — re-apply after every `tools.disasm` re-run** (it rewrites
`functions.json` from scratch and loses them). Until this is fixed upstream
in the disassembler (follow jump-table targets for function extents) or the
`--function-range` flag is ported (see Mercenaries' Recompile.ps1), apply:
- `sub_00442CF0` (CRT memcpy): end → `0x00443028`, and delete any function
  entries with start in (0x442CF0, 0x443028).
- `0x00094B90..0x00094C80` (missed SEH method): add as a function entry
  (start '0x00094B90', end '0x00094C80', section '.text').
- Seeds in `mvp06/config/seed_functions.json` must NOT contain entries
  inside [0x442CF0, 0x443028) (they cap the CFG recovery as stop-addresses).

`RECOMP_VBLANK=1` is required at runtime. main.c defaults: RECOMP_PB_EXEC=1,
RECOMP_AC97_READY=1, RECOMP_USB=1, RECOMP_USB_PADS=2, RECOMP_FB_WINDOW=1,
RECOMP_APU_DSP_ACK=0x82F20810,0x82F28810. Diagnostics env vars: see AGENTS.md.

## Where things are

| Thing | Path |
|---|---|
| Engineering log / this file / plan | `mvp06/docs/` |
| Game project (CMake, main.c, recomp_manual.c, seeds, tools) | `mvp06/` |
| Extracted disc + XBE + analysis JSON (gitignored) | `mvp06/game/` |
| Runtime debug tools | `mvp06/tools_sample.py`, `tools_stackscan.py`, `tools_threadtimes.py`, `tools_watchpoint.py` |
| Toolkit runtime (with our fixes) | `src/kernel/`, `src/apu/`, `src/usb/` + `tools/recomp/{lifter,translator}.py` |
| Reference ports (cloned, gitignored; study before inventing) | `reference/` — nfsu2-sw (EA/XDK5849, GIL+VK/GL+D3D11 pb renderers), burnout3 docs in upstream README links, doaxvb-re, doa3-re, mercenaries-re, ssx-tricky-pc, jsrf-recomp |
| Original ISO (gitignored) | `MVP 06 NCAA Baseball (USA)/` |

## Our changes vs upstream xboxrecomp v0.11.0 (all upstream-candidates)

Runtime (`src/`):
1. `bridge_ke_object()` — kernel dispatcher-object shadowing (`kernel_bridge.c`).
2. Contig low-64 KB reserve (`XBOX_CONTIG_LOW_RESERVE`, `xbox_memory_layout.c`).
3. GP/EP DSP memory untrap above the voice-register block (same file).
4. Rate-limited `[KEWAIT]/[KEMWAIT]/[KEOBJ]` debug logging (trim before PR).

Toolchain (`tools/recomp/`):
5. `_flags` declaration for rep-prefixed cmps/scas (`translator.py`).
6. `_analyze_switch_table` fallback probes: overlapping-first-dword tables
   (base+4) and negated-index downward tables (base−32) (`lifter.py`).
7. **Converged switch-resync decode** — iterate leader collection + resync
   until stable (`translator.py decode_function`); fixes silently-missing
   switch arms when tables stop the linear decode early.

Title-side (`mvp06/`): env-first boot, OHCI VEH routing, pushbuffer fence
`xbox_Nv2aMirrorFence(0x589A38, 0x2C, 0x30)`, DSP doorbells, saves/ dir, VEH
diagnostics (BUCKETS/PHYS0 dumps, DR-watchpoint reporting).

## Gotchas learned the hard way

See [../AGENTS.md](../AGENTS.md) "Conventions and traps" — the short list:
x64 CONTEXT Rip=0xF8 (0xF0 is R15); never write C via bash heredocs; direct
calls bypass `recomp_lookup_manual`; `.map` offsets are host offsets; guest
registers are TLS globals; piped stderr speeds the game up (watchpoint timing);
a leftover game process locks the exe (LNK1104).
