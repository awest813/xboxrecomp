# AGENTS.md — working instructions for the MVP 06 recomp

The task: get MVP 06 NCAA Baseball (Xbox) playable on PC through the xboxrecomp
static-recompilation toolkit: in-game at a steady 30 FPS or better, then accurate.

**Start with [docs/HANDOFF.md](docs/HANDOFF.md) "Current state" and "Open issues".**
Checkpoint 186 (2026-10-31) merged the two diverged lineages: the GPU-resident
renderer branch (30–37 FPS, Oct 23) and the checkpoint-185 runtime-correctness
branch (heap/TLS race, critical-section waits, event switch). The pitch selector now
runs 38–40 FPS on a lightly loaded host. The top blockers are the between-pitch
stall and a related animation-lookup crash. History lives in
[docs/ENGINEERING_LOG.md](docs/ENGINEERING_LOG.md) and [docs/archive/](docs/archive/);
flags from the 2–5 FPS readback era (--gpu-scopes, cutoff 4096, fallback budgets)
do not apply to the resident renderer.

## Ground rules

- The repo root is the **toolkit** (upstream xboxrecomp v0.11.0 + our fixes). The
  **game project** is `mvp06/`. Only the game project produces an `.exe`.
- Never commit or distribute game data: `mvp06/game/`, the ISO, or the built
  `.exe`/`.map` (they embed recompiled game code). `mvp06/src/recomp/gen/` and
  `mvp06/build/` are gitignored for the same reason; do not force-add them.
- Keep runtime fixes (under `src/`) title-agnostic and upstream-shaped, with
  comments explaining the failure they fix. Title-specific behaviour belongs in
  `mvp06/src/main.c` or `mvp06/src/recomp_manual.c`.
- Keep new speed or accuracy paths switchable (an env var) and, where they replace
  an exact path, add a checking mode that compares against the reference (see
  RECOMP_VP_CHECK, RECOMP_VP_BLOCK_CHECK, RECOMP_TEXTURE_DECODE_CHECK).
- Never relink a running executable or overwrite its matching MAP: stop the owned
  test process first (`tasklist | grep mvp06_recomp`). Copy an EXE/MAP pair aside
  before replacing it if you need it as a comparison baseline.
- Contiguous graphics memory (window at 0x80000000) and its tiled alias are separate
  from low RAM. Frees of window addresses go to `xbox_ContiguousFree`, never the
  ordinary heap. Device models key on contiguous addresses (MVP 06's DSP doorbells,
  `RECOMP_APU_DSP_ACK`), so allocation order must stay deterministic.
- Preserve thread-local guest registers/TIBs and raised-IRQL ownership. A real
  blocking wait releases the lifted CPU and RAM lease only after copying its
  arguments (see `xbox_RtlEnterCriticalSection`).
- Acceptance for every change: build, run the native smoke tests, then a live run.
  Compare `run*_stderr.txt` file-open counts (`grep -c '\[PATH\]'`), dispatch misses
  and crash signatures with the previous run, and **look at the captures**. Draw
  counters alone have hidden visual regressions before.
- Performance claims: measure actual game flips (`read_render_stats.py`) in a named
  scene with matching EXE/MAP, and record host CPU load next to every sample. On this
  4-core laptop FPS tracks host load (the same build measured 23 FPS at 77–100% load
  and 39 FPS at 54%). Compare builds in both orders before claiming a gain.

## Commands (Git Bash, from repo root)

```bash
# Incremental build after runtime/main changes (minutes):
cmake --build mvp06/build --config Release --parallel 8 --target mvp06_recomp

# Native regressions (see HANDOFF for the full list and env matrix):
cmake --build mvp06/build --config Release --parallel 8 --target \
    heap_concurrency_smoke critical_section_smoke memory_scope_smoke \
    guest_scheduler_smoke nv2a_pb_smoke nv2a_ack_smoke nv2a_render_smoke nv2a_pb_scan_smoke

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
cmake --build mvp06/build --config Release --parallel 8

# Run (always from mvp06/; main.c supplies the title's env defaults):
cd mvp06 && ./build/Release/mvp06_recomp.exe 2> run.txt

# Toolkit tests (fast, no game files):
py -3 -m pytest tools/recomp -q
```

The boundary helper is authoritative for memcpy, SEH, menu/player switches,
pitching/post-pitch callbacks and the 0x3DD40 event switch. After a lift, check
that `sub_0003DD40` spans 0x3DD40–0x3DDF6 with its six switch arms inline (8,288
cases were compared against original x86). Generated-only patches are erased by a lift.

Native smoke tests build alongside the game under `mvp06/build/xboxrecomp/src/`.
Several are env-gated: run them with RECOMP_GIL=0 and =1 (and
`critical_section_smoke` also with RECOMP_CS_GUEST_FASTPATH=0/1).

Driving and measuring a live game: HANDOFF "Measuring and driving the game"
(`snap_run.py`, `soak.py`, `read_render_stats.py`, …, private in `mvp06/build/`).
They post key messages to the game window after checking its owner PID; test-key
stretching (`RECOMP_TEST_KEY_HOLD_MS`) stays 0 outside acceptance. Keyboard map:
Enter=Start, Z=A, X=B, A=X, S=Y, E=Black, Q=White, 1/3=LT/RT, arrows=D-pad,
numpad 4/6/2/8 = left stick, J/L/K/I = right stick. A pitch is three short Z
presses ~0.75 s apart. Preserve the user's audio mute preference.

## The debugging loop that works

1. **Crash?** Read `[CRASH]` in stderr: guest regs, guest VA of the fault, and the
   guest return-address stack (innermost first). Resolve the fault RVA against the
   loaded build's MAP, and guest addresses with `tools/disasm/output/functions.json`
   or `tools/disasm/output/asm/text.asm`.
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
   RECOMP_STUB_TRACE=1. A miss returns eax=0 and poisons everything downstream.
   If a miss is inside an existing function, inspect its jump tables before seeding.
5. **Compare with the reference ports** under `reference/` before inventing a fix.
   NFSU2's `CLAUDE.md` is an EA-title fix log that has repeatedly matched this
   project's bugs one-to-one.
6. **Verify visible output with captures.** `RECOMP_FB_DUMP=build/frameN/frame` plus
   `RECOMP_FB_WINDOW_DUMP_EVERY=120` saves `frame.windowNNN.bmp`. For one frame's
   draws: `RECOMP_DRAW_LOG_TRIGGER=<file>`, then create the file on the screen of
   interest. Each `[DRAW]` batch now also prints depth func, alpha test, stage
   program and every combiner stage's ICW/OCW words.
7. **Input/audio:** `RECOMP_INPUT_TRACE=1` logs guest USB report changes;
   `RECOMP_APU_TRACE=1` logs voice cursors and output peaks. The game mixes its SFX
   and speech in software into a ring played by voices 244–246, so seven active APU
   voices is normal; APUOUTPUT (48 kHz) is the delivered rate, not APUPROGRESS.

## Renderer and coherence model (read before touching `src/kernel/nv2a_render_d3d11.c`)

Committed pixels stay on the GPU. A tracked surface's pages are PAGE_NOACCESS
while the GPU copy is newer and PAGE_READONLY while equal, in both the contiguous
window and its tiled alias. Any observer faults into `resident_fault` (renderer
VEH), which materializes the dirty box; a write also untracks. Host-kernel I/O
cannot fault, so NtReadFile/NtWriteFile call `xbox_MemoryHostAccess` first; so does
the contiguous allocator before zero-filling a reused block. Invariant: a page is
tracked by at most one surface set (`claim`). Never skip a RAM commit merely because
the GPU copy is current; make it lazy instead. The `surf_env_lo/hi` envelope must
stay surface-only. Executor observers call `nv2a_render_observe(<reason>)` so
resolves stay attributable. Leave RECOMP_GPU_SCOPES off: with residency it only
serializes the worker.

Texture stages: modes 1 (2D), 4 (pass-through), 5 (clip plane), 15/16 (dependent
AR/GB, input stage from SET_SHADER_OTHER_STAGE_INPUT) are implemented in both the
D3D11 shader and the software combiner. Other modes (cube, bump-env, dot-product)
still produce (0,0,0,1); check a broken effect's `prog=` in the draw log first.

## Conventions and traps

- x64 CONTEXT offsets when sampling: Rip=0xF8, Rsp=0x98 (0xF0 is R15).
- Guest registers are TLS globals (`g_eax`, `g_esp`, …) declared `RECOMP_TLS`; a
  plain `extern` mismatches storage class silently. Guest host address = VA + 0x10000.
- The x87 stack is modelled as `double`; the precision-control word is not applied
  (nothing in MVP 06 sets 24-bit precision). The rounding-control bits are: `fist(p)`
  via `recomp_fist` and `frndint` via `recomp_frndint`, which the CRT's `floor`/`ceil`
  depend on.
- Guest clocks (KeTickCount, interrupt time, timed waits, kernel timers) come from
  `src/platform/host_clock.h`, not `GetTickCount`, whose 15.6 ms steps halved 60 Hz
  timers. RDTSC is scaled to 733.33 MHz.
- Direct calls in generated code cannot be intercepted via `recomp_lookup_manual`.
  Patch the generated body or the call sites, and note that a re-lift wipes it.
- `gen/recomp_9999.c` is a local preflight chunk (0x5E440/0x56B60) with a hand-bumped
  `g_recomp_table_size` (37764). A full lift replaces it.
- The `.map` file maps host addresses to `sub_XXXXXXXX` symbols; offsets after `+`
  are host offsets, not guest VAs.
- Write C with the Edit tool, not bash heredocs. Python patch scripts must open files
  with `encoding='utf-8'` and `newline=''`. `open(p, 'w')` truncates before writing.
- MSBuild under Git Bash: use `--parallel 8` (or `-j 8`), not `/m`. A leftover game
  process locks the exe and the link fails with LNK1104.
- All labels being live does not prove a lift correct. Check for overlapping
  decoded instructions and include the final return in the function boundary.
- IRQL must be published as a BYTE at each current TIB's fs:[0x24]; new TIBs start
  passive. OHCI DMA continues with MIE masked.
- The D3D8 API here is XDK 5849 plain D3D8. Device struct at `[0x589A38]`;
  pushbuffer fence pair at +0x2C/+0x30 (same as Burnout 3 and NFSU2).
- `tools_threadtimes.py` spawns a new game instance; use the `--pid` tools on a live run.
- `read_render_stats.py` hardcodes `game_flips` at `s_gpu`+271000 in nv2a_pb_exec.obj.
  `nv2a_pb_smoke --gpu-layout` prints the real offsets; fields added after `rc` are safe.

## Definition of done (project)

Playable: an exhibition game completable with visible rendering, audio and
controller input, at a steady 30 FPS or better in gameplay, and saves persist.
Stretch: Dynasty mode stability, then the enhancement program
(resolution/AA/texture replacement).
