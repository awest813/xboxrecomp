# AGENTS.md — working instructions for AI agents on the MVP 06 recomp

The task: get MVP 06 NCAA Baseball (Xbox) playable on PC via the xboxrecomp
static-recompilation toolkit.

**Start with [docs/HANDOFF.md](docs/HANDOFF.md) "Current state" and "Open issues".**
As of 2026-10-23: live pitching at 30–37 FPS on the D3D11 renderer with GPU-resident
surfaces. The top blocker is the between-pitch stall, a long or indefinite wait
after each pitch. [docs/ENGINEERING_LOG.md](docs/ENGINEERING_LOG.md) is the full
history. Run-by-run checkpoint notes from runs 52–162 were moved verbatim to
[docs/archive/](docs/archive/). Their launch flags (--gpu-scopes, cutoff 4096,
fallback budgets) predate GPU residency and no longer apply.

## Ground rules

- The repo root is the **toolkit** (upstream xboxrecomp v0.11.0 + our fixes). The
  **game project** is `mvp06/`. Only the game project produces an `.exe`.
- Never commit or distribute game data: `mvp06/game/`, the ISO, or the built
  `.exe` (it embeds recompiled game code). The generated code (`mvp06/src/recomp/gen/`)
  and `mvp06/build/` are gitignored for the same reason. Runtime/lifter fixes are
  the shareable work.
- Keep runtime fixes (under `src/`) title-agnostic and upstream-shaped, with
  comments explaining the failure they fix, in the style of the existing code.
  Title-specific behaviour belongs in `mvp06/src/main.c` or `mvp06/src/recomp_manual.c`.
- Keep new speed or accuracy paths switchable (an env var) and, where they replace
  an exact path, add a checking mode that compares against the reference (see
  RECOMP_VP_CHECK, RECOMP_VP_BLOCK_CHECK, RECOMP_TEXTURE_DECODE_CHECK).
- Acceptance for every change: build, run the native smoke tests, then a live run.
  Compare `run*_stderr.txt` file-open counts (`grep -c '\[PATH\]'`), dispatch misses
  and crash signatures with the previous run, and **look at the captures**. Draw
  counters alone have hidden visual regressions before. For renderer/perf changes,
  measure selector FPS with `read_render_stats.py` and per-flip costs with
  RECOMP_EXEC_TIMING=1. Only claim a gain that survives scene variance.

## Commands (Git Bash, from repo root)

```bash
# Incremental build after runtime/main changes (minutes):
cmake --build mvp06/build --config Release

# Full pipeline after seed/lifter changes (~55 min total):
py -3 -m tools.disasm mvp06/game/default.xbe \
    --seed-functions mvp06/config/seed_functions.json \
    --analysis-json mvp06/game/mvp06_analysis.json
# Required between disasm and recomp (disasm overwrites these ranges):
py -3 mvp06/tools_patch_boundaries.py
py -3 -m tools.recomp mvp06/game/default.xbe --all --split 1000 \
    --gen-dir mvp06/src/recomp/gen
# Required after every lift: D3D publication/handler hooks and PCRTC W1C.
py -3 mvp06/tools_patch_d3d.py
# Refresh the VS source list before MSBuild when generated chunks change:
cmake -S mvp06 -B mvp06/build
cmake --build mvp06/build --config Release

# Run (always from mvp06/; main.c supplies the title's env defaults):
cd mvp06 && ./build/Release/mvp06_recomp.exe 2> run.txt

# Harvest new function entry points from a run log (then full pipeline):
cd .. && py -3 -m tools.seed_from_log mvp06/run.txt mvp06/game/default.xbe \
    --functions tools/disasm/output/functions.json \
    --seeds mvp06/config/seed_functions.json \
    --analysis-json mvp06/game/mvp06_analysis.json

# Toolkit tests (fast, no game files):
py -3 -m pytest tools/recomp -q
```

Native smoke tests build alongside the game. Examples:
`mvp06/build/xboxrecomp/src/kernel/nv2a_pb_smoke/Release/nv2a_pb_smoke.exe`; the
other kernel, USB, APU, video and input tests sit in sibling directories under
`build/xboxrecomp/src/`. Several are env-gated: run them with RECOMP_GIL=0 and =1.

Driving and measuring a live game: see HANDOFF "Measuring and driving the game"
(`snap_run.py`, `soak.py`, `read_render_stats.py`, `watch_selector.py`, …).
Keyboard map: Enter=Start, Z=A, X=B, A=X, S=Y, E=Black, Q=White, 1/3=LT/RT,
arrows=D-pad, numpad 4/6/2/8 = left stick, J/L/K/I = right stick. A pitch is
three short Z presses ~0.75 s apart.

## The debugging loop that works

1. **Crash?** Read `[CRASH]` in stderr: guest regs, guest VA of the fault, and the
   guest return-address stack (innermost first). Map addresses to functions with
   `tools/disasm/output/functions.json` or the asm listings in `tools/disasm/output/asm/`.
2. **Hang or spin?** `RECOMP_WATCHDOG_SECS=10` prints the stuck thread's guest
   registers. `tools_sample.py --pid PID` finds hot host/guest functions without
   killing the game. `tools_stackscan.py --pid PID` reconstructs guest call chains
   from the host stack via the `/MAP` file. `build/thread_state89.py PID` dumps guest
   thread registers, IRQL and the GIL owner. Capture these before stopping a parked run.
3. **Bad memory?** `RECOMP_KERNEL_WATCH=<va> RECOMP_KERNEL_WATCH_ALL=1` shows who
   changed a dword, by kernel-call interval. `tools_watchpoint.py` sets a hardware
   DR0 write watchpoint; arm it within ~1.5 s of launch.
4. **Check for dispatch misses and unlifted stubs** before diagnosing logic:
   `grep "Failed to resolve VA" run.txt | sort | uniq -c`, and run with
   RECOMP_STUB_TRACE=1 (prints `[UNLIFTED]` once per reached direct-call stub).
   A miss returns eax=0 and poisons everything downstream, which looks like a
   game-logic bug. If a miss is inside an existing function, inspect its jump
   tables before seeding: `0x442FCF` (memcpy), `0x5BEFD2` (XNET checksum), `0x7F052`
   and `0x7AE92` were omitted switch arms, not entry points.
5. **Compare with the reference ports** under `reference/` before inventing a fix.
   NFSU2's `CLAUDE.md` is an EA-title fix log that has repeatedly matched this
   project's bugs one-to-one (page zero, DSP doorbell, GP/EP memmove, GIL). The
   reference audit is in ENGINEERING_LOG, runs 79–82.
6. **Verify visible output with captures.** `RECOMP_FB_DUMP=build/frameN/frame` plus
   `RECOMP_FB_WINDOW_DUMP_EVERY=120` saves `frame.windowNNN.bmp` window captures.
   Create the directory first. For one frame's draws:
   `RECOMP_DRAW_LOG_TRIGGER=<file>`, then create the file once on the screen of
   interest. RECOMP_FRAME_TRACE (combiner state) currently fires only for software-
   rendered batches.
7. **Input/audio:** `RECOMP_INPUT_TRACE=1` logs guest USB report changes;
   `RECOMP_APU_TRACE=1` logs voice cursors and output peaks. Host key handling
   alone does not prove guest input; a pad report edge does.

## Renderer and coherence model (read before touching `src/kernel/nv2a_render_d3d11.c`)

Committed pixels stay on the GPU. A tracked surface's pages are PAGE_NOACCESS
while the GPU copy is newer and PAGE_READONLY while equal, in both the contiguous
window and its tiled alias. Any observer faults into `resident_fault` (renderer
VEH), which materializes the dirty box; a write also untracks. Host-kernel I/O
cannot fault, so NtReadFile/NtWriteFile call `xbox_MemoryHostAccess` first.
Invariant: a page is tracked by at most one surface set (`claim`). Never skip a
RAM commit merely because the GPU copy is current; make it lazy instead. The
`surf_env_lo/hi` envelope must stay surface-only: when it included texture-watch
pages, every vertex fetch took the renderer lock.

## Conventions and traps

- x64 CONTEXT offsets when sampling: Rip=0xF8, Rsp=0x98 (0xF0 is R15).
- Guest registers are TLS globals (`g_eax`, `g_esp`, …) declared `RECOMP_TLS`; a
  plain `extern` mismatches storage class silently. Guest host address = VA + 0x10000.
- The x87 stack is modelled as `double`; the precision-control word is not applied.
- Direct calls in generated code cannot be intercepted via `recomp_lookup_manual`.
  Patch the generated body or the call sites, and note in the comment that a re-lift
  wipes gen patches (re-apply them, or move the intercept into the runtime).
- A local preflight chunk `gen/recomp_9999.c` plus sorted dispatch-table entries
  can stand in for a full lift. Bump the hardcoded `g_recomp_table_size` by hand.
  A full lift removes the chunk.
- The `.map` file maps host addresses to `sub_XXXXXXXX` symbols; offsets after `+`
  are host offsets, not guest VAs.
- `RECOMP_PEEK`/`PEEK_CHAIN` only accept guest RAM VAs (< 0x04000000) and only print
  while the pushbuffer survey ticks (RECOMP_NV2A_TRACE=1 or RECOMP_PB_SCAN).
- Write C with the Edit tool, not bash heredocs: line continuations and `\n` get
  mangled. Python patch scripts must open files with `encoding='utf-8'`, and
  `newline=''` to keep CRLF. `open(p, 'w')` truncates before writing, so an encode
  error destroys the file.
- MSBuild: Git Bash mangles `/m`, so use `-j 8`. A leftover game process locks the
  exe and the link fails with LNK1104; kill `mvp06_recomp` first.
- After adding or removing generated chunks, run `cmake -S mvp06 -B mvp06/build`
  before building. MSBuild can keep the old item list for its current invocation.
- All labels being live does not prove a lift correct. Check for overlapping
  decoded instructions, and include the final return instruction in the exclusive
  function boundary (`memcpy` ends after `ret` at 0x44302C).
- VP viewport methods alias constants 58/59. IRQL must be published as a BYTE at
  each current TIB's fs:[0x24]; new TIBs start passive. OHCI DMA continues with MIE
  masked; do not put all DMA under the guest CPU lock.
- The D3D8 API here is XDK 5849 plain D3D8. Device struct at `[0x589A38]`;
  pushbuffer fence pair at +0x2C/+0x30 (same as Burnout 3 and NFSU2).
- APUPROGRESS "frames/s" is not the delivered audio rate; APUOUTPUT is (48 kHz).
- `tools_threadtimes.py` spawns a new game instance; use the `--pid` tools on a live run.
- `read_render_stats.py` hardcodes `game_flips` at `s_gpu`+271000 in
  nv2a_pb_exec.obj. Changing that struct's layout breaks it.

## Definition of done (project)

Playable: an exhibition game completable with visible rendering, audio and
controller input, and saves persist. Stretch: Dynasty mode stability, then the
enhancement program (resolution/AA/texture replacement per PLAN.md Phase 4).
