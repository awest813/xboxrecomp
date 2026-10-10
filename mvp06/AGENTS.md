# AGENTS.md — working instructions for AI agents on the MVP 06 recomp

Read [docs/HANDOFF.md](docs/HANDOFF.md) for current state and
[docs/ENGINEERING_LOG.md](docs/ENGINEERING_LOG.md) for history before making
changes. The task: get MVP 06 NCAA Baseball (Xbox) playable on PC via the
xboxrecomp static-recompilation toolkit.

## Ground rules

- The repo root (`C:\Users\allen\Documents\xboxrecomp`) is the **toolkit**
  (upstream xboxrecomp v0.11.0 + our fixes). The **game project** is `mvp06/`.
  Only the game project produces an `.exe`.
- Never commit or distribute game data: `mvp06/game/`, the ISO, or the built
  `.exe` (it embeds recompiled game code). Runtime/lifter fixes are the
  shareable work.
- Keep runtime fixes (under `src/`) title-agnostic and upstream-shaped, with
  comments explaining the failure they fix, in the style of the existing code.
  Title-specific behavior belongs in `mvp06/src/main.c` or
  `mvp06/src/recomp_manual.c`.
- After every fix: run, capture `run*_stderr.txt`, compare file-open counts
  (`grep -c '\[PATH\]'`) and crash signatures against the previous run. That
  comparison is the acceptance test for every change.

## Commands (Git Bash, from repo root)

```bash
# Incremental build after runtime/main changes (minutes):
cmake --build mvp06/build --config Release

# Full pipeline after seed/lifter changes (~55 min total):
py -3 -m tools.disasm mvp06/game/default.xbe \
    --seed-functions mvp06/config/seed_functions.json \
    --analysis-json mvp06/game/mvp06_analysis.json
# *** REQUIRED between disasm and recomp: re-apply the boundary patches —
# disasm rewrites functions.json from scratch and loses them (sub_00442CF0
# end -> 0x00443028 + remove contained entries + add 0x00094B90..0x94C80;
# exact steps in docs/HANDOFF.md "Boundary patches") ***
py -3 -m tools.recomp mvp06/game/default.xbe --all --split 1000 \
    --gen-dir mvp06/src/recomp/gen
cmake --build mvp06/build --config Release

# Run (always from mvp06/):
cd mvp06 && RECOMP_VBLANK=1 timeout 120 ./build/Release/mvp06_recomp.exe 2> run.txt

# Harvest new function entry points from a run log (then full pipeline):
cd .. && py -3 -m tools.seed_from_log mvp06/run.txt mvp06/game/default.xbe \
    --functions tools/disasm/output/functions.json \
    --seeds mvp06/config/seed_functions.json \
    --analysis-json mvp06/game/mvp06_analysis.json

# Toolkit tests (fast, no game files):
py -3 -m pytest tools/recomp -q
```

## The debugging loop that works

1. **Crash?** Read `[CRASH]` in stderr: guest regs, guest VA of fault, and
   the guest return-address stack (innermost first). Map addresses to
   functions with `tools/disasm/output/functions.json` (or the asm listings
   under `tools/disasm/output/asm/`).
2. **Hang/spin?** `RECOMP_WATCHDOG_SECS=10` prints the stuck thread's GUEST
   registers; `py -3 tools_threadtimes.py 20` finds CPU burners;
   `py -3 tools_stackscan.py 15` reconstructs the guest call chain from the
   host stack via the `/MAP` file.
3. **Bad memory?** `RECOMP_KERNEL_WATCH=<va> RECOMP_KERNEL_WATCH_ALL=1`
   (who changed a dword, by kernel-call interval); `tools_watchpoint.py`
   (hardware DR0 write watchpoint — arm within ~1.5 s of launch).
4. **Check for dispatch misses** before diagnosing logic:
   `grep "Failed to resolve VA" run.txt | sort | uniq -c` — each is a
   function the disassembler never found; seed them. A dispatch miss returns
   eax=0 to the caller and poisons everything downstream, and it looks like a
   game-logic bug.
5. **Compare with the reference ports** under `reference/` before inventing a
   fix. NFSU2's `CLAUDE.md` is an EA-title fix log that has repeatedly
   matched this project's bugs one-to-one (page zero, DSP doorbell, GP/EP
   memmove, GIL).

## Conventions and traps

- x64 CONTEXT offsets when sampling: Rip=0xF8, Rsp=0x98 (0xF0 is R15).
- Guest registers are TLS globals (`g_eax`, `g_esp`, ...) declared
  `RECOMP_TLS`; a plain `extern` mismatches storage class silently.
- Direct calls in generated code cannot be intercepted via
  `recomp_lookup_manual` — patch the generated body or call sites (note in
  the comment that re-lift wipes gen patches; re-apply or move the intercept
  into the runtime).
- The `.map` file maps host addresses to `sub_XXXXXXXX` symbols; offsets
  after `+` are host offsets, not guest VAs.
- `RECOMP_PEEK`/`PEEK_CHAIN` only accept guest RAM VAs (< 0x04000000) and
  only print while the pushbuffer survey ticks (needs RECOMP_NV2A_TRACE=1 or
  PB activity).
- Write C with the Edit tool, never bash heredocs (line-continuations get
  mangled). MSVC errors: check `mvp06/build_log*.txt`.
- A leftover game process locks the exe and fails the link with LNK1104:
  kill `mvp06_recomp` before rebuilding.
- The D3D8 API here is XDK 5849 plain D3D8. Device struct at `[0x589A38]`;
  pushbuffer fence pair at +0x2C/+0x30 (same as Burnout 3 and NFSU2).

## Definition of done (project)

Playable: exhibition game completable with visible rendering, audio, and
controller input; saves persist. Stretch: Dynasty mode stability, then the
enhancement program (resolution/AA/texture replacement per PLAN.md Phase 4).
