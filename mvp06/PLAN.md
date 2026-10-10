# MVP 06 NCAA Baseball — Xbox → Windows Static Recompilation Plan

Status: **audited, Phase 0–1 in progress** (this document written 2026-10-08, toolkit v0.11.0)

Goal: a native Windows x64 port of MVP 06 NCAA Baseball (USA, retail) built with the
xboxrecomp static-recompilation toolkit, preserving original gameplay/art, then layered
enhancements (widescreen, HD, AA, texture replacement) per the remaster direction.

Legal: you must own the game. The ISO, the extracted XBE, and any built `.exe` that
embeds recompiled game code stay private — never distribute them. Only toolkit-side
changes (lifter fixes, runtime improvements) are shareable upstream.

---

## 1. Audit results (measured, not estimated)

### 1.1 The game (from `tools.xbe_parser`, saved in `mvp06/xbe_report.txt`)

| Property | Value | Consequence |
|---|---|---|
| Title ID | 0x4541008D (EA, retail) | no debug niceties |
| XDK | **5849** (all 9 libs @ 1.0.5849) | March-2005 era, standard libs |
| D3D variant | **plain D3D8** (no D3D8LTCG in lib list) | avoids the per-title LTCG hacks entirely — big win |
| Entry point | `0x0031EFD1` | already set in `mvp06/src/main.c` |
| Base / image | `0x00010000` / 6.98 MB | standard layout |
| `.text` | **0x4BC4C4 ≈ 4.97 MB** | Tier-3 "challenging" size per `docs/technical/candidate-games.md` |
| Code sections | db_sec, fe_sec, D3D, XGRPH, XONLINE, XNET, DSOUND, WMADEC, XPP, DOLBY, XON_RD | XDK libs statically linked; all carry code |
| RTTI | `$$XTIMAGE` + `.XTLID` present | C++ with RTTI; `tools/rtti` can recover classes |
| Kernel imports | **153** | vs Burnout 3's 147 — comparable breadth |
| Saves | `AvGet/SetSavedDataAddress`, `T:\` paths | rides existing path translation |
| Demand paging | `XeLoadSection` / `XeUnloadSection` | fe_sec/db_sec are lazily loaded by the game |

**Threading/concurrency imports (the headline risk):** `PsCreateSystemThreadEx`,
`NtResumeThread`, `KeWaitForMultipleObjects`, full DPC set (`KeInitializeDpc`,
`KeInsertQueueDpc`, `KeRemoveQueueDpc`), interrupts (`KeConnectInterrupt`,
`KeInitializeInterrupt`, `KeGetInterruptVector`, `HalGetInterruptVector`), kernel timers
(`KeSetTimer/Ex`, `KeCancelTimer`, `KeInitializeTimerEx`), IRQL manipulation
(`KfRaiseIrql` ×135 call sites, `KeRaiseIrqlToDpcLevel` ×114, `KfLowerIrql` ×135),
`KeSave/RestoreFloatingPointState`, `MmCreate/DeleteKernelStack`.
This is a genuinely multi-threaded title (audio/streaming/loading workers + XONLINE's
own thread machinery), and the runtime today is single-threaded cooperative.

**Exceptions:** `RtlRaiseException` + `RtlUnwind` imported; game SEH `__except` blocks
cannot currently run in lifted code — expect to work around crash paths.

**Oddball imports to watch:** `NtDeviceIoControlFile` / `NtFsControlFile` (possible
disc-medium / anti-piracy query — watch for a boot-time spin), `IdexChannelObject`,
`PhyInitialize` (Ethernet), `XcVerifyPKCS1Signature`/`XcModExp` (content signature
checks), XVOICE/XVOCREC (the voice-recognition pitch-calling feature — stub it).

### 1.2 The disc (`tools.xiso ls`, 270 entries ≈ 2.06 GB of files in the 7.8 GB image)

- **`data/*.big`** — EA BIG archives: `ghead.big` 379 MB, `models.big` 144 MB,
  `anims.big`, 40+ stadium `.big`s, etc. Plain files to the I/O layer; parsed by
  recompiled EA code, so no format work needed to run.
- **Audio** — EA `.abk` (banks) + `.ast` (streams), `.asf` music (WMA — the runtime has
  a host-side WMA/ASF decoder), `WMADEC` lib section in the XBE.
- **Movies** — ten **`.vp6` files** (EA's VP6 container, ~360 MB). *Not XMV* and not
  Media-Foundation-decodable; however EA's own VP6 decoder ships inside the game
  binary and gets recompiled like everything else, so playback is plausible without
  toolkit video support — treat as a stretch goal, skippable (black screen + proceed)
  for bring-up.
- `dashupdate.xbe` / `update.xbe` on disc — ignore, not the game.
- Dual-layer reads fine through `tools.xiso` (listing resolved files across the whole
  image; extraction of `default.xbe` verified by the parser: thunks resolved to 153
  real names, TLS sane — passes the svod-extraction.md gate criteria).

### 1.3 Pipeline run status (outputs already on disk)

| Stage | Result |
|---|---|
| `tools.xiso` extract XBE | done → `mvp06/game/default.xbe` (7,102,464 B) |
| `tools.xbe_parser --json` | done → `mvp06/game/mvp06_analysis.json` + `mvp06/xbe_report.txt` |
| `tools.disasm` (all code sections) | done, 226 s → **37,734 functions** (.text 32,217; fe_sec 2,192; db_sec 1,093; XDK libs the rest). Largest fn 25,577 B, median 76 B |
| `tools.func_id` | done — 8,533 vtable_scan + 6,210 vtable_thunk + 1,337 vtable_ctor + 1,746 xdk_caller; 25,356 unclassified (EA custom engine — no library signatures to match) |
| `tools.abi_analysis` | done — all 37,734 analyzed, **6,441 thiscall** (C++-heavy) |
| `tools.recomp --all` | **done** → `mvp06/src/recomp/gen/` (41 files, 38 chunks, 161 MB, **4,268,716 lines**). **37,732/37,732 translated, 0 failed.** 284 unresolved indirect-target stubs. Unimplemented-instruction census: **386 sites in 155 functions (0.4%)** — see §1.3.1 |
| Full ISO unpack | done — 268 files, 2.0 GB → `mvp06/game/` |
| `cmake -S mvp06 -B mvp06/build` | configures clean (VS 18 2026 generator, MSVC 19.50) |

#### 1.3.1 Lift-gap punch list (from `tools/recomp/output/summary.json`)

Only 155 of 37,732 functions contain unimplemented instructions, and much of that is
disassembler false positives on data (BCD/privilege forms like `aad`, `das`, `lldt`,
`verw` never occur in real game code). The genuinely load-bearing items:

| Item | Where | Action |
|---|---|---|
| SIMD block `movdqa/movdqu/psraw/psubw/paddw/pmulhuw` | `sub_0049AE36` + `sub_0049AF9A` (game code) | Extend lifter for these SSE2 forms (mechanical, testable via conformance suite) |
| `psubq` | `sub_0057FB36` (fe_sec frontend) | same |
| `cpuid` ×5 | `sub_0045A847`, `sub_0045E9F0` (CPU feature detect), `sub_00492D8E` | Hand-override in `recomp_manual.c` to report Xbox-class CPUID, or lift `cpuid` as host `__cpuid` |
| `xadd` ×1 | one site | check for lock-xadd atomic idiom; lift carefully |
| `in`/`out`/`sti`/`cli`/`hlt`/`wbinvd` clusters | mostly XONLINE/XNET/DOLBY lib regions + false positives | Expect to hand-override the few real hardware-touching functions |
| 284 unresolved stubs | `recomp_stubs_unresolved.c` | Surface at runtime as ICALL failures; resolve iteratively |

For scale: Burnout 3 — the toolkit's most mature target — is 22,097 functions; MVP 06
is **1.7× that**, with a custom EA engine rather than RenderWare.

### 1.4 Toolkit/runtime status (docs + source audit)

- **Kernel:** all 371 ordinals routed, 169 dedicated bridges. Solid: memory, file I/O
  (CdRom0/D:/T:/Z: path translation), sync objects, crypto, EEPROM, timing.
  **Single-threaded cooperative** — worker threads deferred, not preempted.
  *Measured for this title:* `tools.kernel_audit.coverage` reports **153/153 imports
  routed (100%), 0 unrouted, 0 missing argument-size entries** — no kernel-coverage
  blocker; the risk is behavioral (threading semantics), not missing surface.
- **D3D8→D3D11:** strong for standard API users — 4-stage FFP multitexture, register
  combiners, programmable vertex shaders (NV2A microcode→HLSL), 8-light T&L, fog,
  swizzling, UP-draw ring buffer. Weaker: render targets and cube/volume textures
  PARTIAL; DXN/DXT3A variants approximate; no point-sprite mentions (baseball
  particle effects may hit this).
- **Audio:** DirectSound buffers→64-voice mixer done; **streams stubbed** (commentary
  and crowd are streaming `.ast` — likely the main audio gap); DSP reverb/chorus
  stubbed; WMA host decode present.
- **Input:** XInput, 4 ports, rumble — done. No headset.
- **Video:** MF SourceReader pipeline (WMV/XMV-class). VP6 out of scope for it.
- **Lifter known gaps:** FS-segment prefixes dropped; some MMX/SSE forms lift to bare
  no-op comments; SEH `__except` non-executable; ~5% of indirect-only callbacks missed
  by disasm. Each of these will need watching in a 2005 EA title.

### 1.5 Host environment (verified today)

Python 3.14.3 + capstone 5.0.7 (installed during this audit), CMake 4.3.1, VS BuildTools
18 with x86/x64 VC tools at `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools`,
583 GB free disk. Project scaffolded at `mvp06/` from `templates/new-game` with entry
point/XBE path/game dir filled in.

### 1.6 Honest ecosystem context

No title is fully playable through pure recompilation yet. Burnout 3 (easier,
RenderWare) currently reaches engine init on its honest all-recomp path. Expect MVP 06
bring-up to be a research project with real unknowns, not a guided tour — the phases
below are ordered so the riskiest unknowns surface earliest and cheapest.

---

## 2. Risk register (ranked)

| # | Risk | Evidence | Mitigation |
|---|---|---|---|
| R1 | **Threading model** — game creates real worker threads (audio/streaming/loading, XONLINE) | PsCreateSystemThreadEx + DPC/timer/IRQL imports; 135+ IRQL call sites | Start with cooperative stubs (return handles, run inline). If audio/load paths deadlock, promote kernel threads to real Win32 threads at the toolkit level — the single biggest possible toolkit contribution of this project |
| R2 | **Scale** — 37.7k functions, 25k unclassified | disasm/func_id output | Mechanical (the lift doesn't care), but debugging surface is large; use Ghidra naming + RTTI recovery early to make traces readable |
| R3 | **C++/SEH** — exceptions + RTTI + 8.5k vtables | RtlUnwind, $$XTIMAGE, thiscall count | VEH bridge already partial; identify the few hot unwind paths and hand-override them |
| R4 | **SSE/MMX lift gaps** silently corrupting math | docs/pipeline/04-lifting.md | After lift completes: grep gen output for bare `/* mnemonic */` comments; fix lifter upstream (tests exist for this pattern) |
| R5 | **Streaming audio** (.ast commentary/crowd) | DSOUND streams stubbed | Buffer path may cover it if EA uses DSOUND buffers; else implement stream bridging on the mixer |
| R6 | **VP6 FMVs** | .vp6 files, MF can't decode | Game's own decoder is recompiled; if it works, done. Fallback: skip FMVs during bring-up |
| R7 | **Disc/medium queries at boot** | NtDeviceIoControlFile/NtFsControlFile imports | Watch first-boot trace; fake successful CDROM responses |
| R8 | **Point sprites / partial render targets** — ballpark effects, replays | d3d gaps doc | Address when rendering artifacts are seen; upstream improvements |
| R9 | Voice recognition features | XVOICE/XVOCREC libs | Stub — nonessential feature |
| R10 | Dolby Digital output | DOLBY section | Stub to stereo |

---

## 3. Plan

### Phase 0 — Assets & analysis *(≈ done)*

- [x] Verify ISO readable incl. layer 1 (`tools.xiso ls`)
- [x] Extract `default.xbe`; parse headers/imports (`mvp06/xbe_report.txt`)
- [x] Disassemble all code sections (37,734 functions)
- [x] `func_id` + `abi_analysis`
- [x] Scaffold `mvp06/` project, set entry/XBE/dir constants
- [x] Full unpack to `mvp06/game/` — 268 files, 2.0 GB, matches ISO listing
- [x] Kernel coverage audit: 153/153 imports routed (`tools.kernel_audit.coverage`)
- [x] Lift: 37,732/37,732 translated, 0 failed; gaps = 386 sites / 155 fns (§1.3.1)
- [x] `cmake` configure verified (VS 18 2026, MSVC 19.50)

### Phase 1 — Native build exists *(DONE 2026-10-08)*

- [x] Lift: 37,732/37,732 translated, 0 failed (§1.3.1 punch list stands)
- [x] Full MSVC build: **`mvp06/build/Release/mvp06_recomp.exe` (56.9 MB) links clean**
      after one local patch — lifter omits the `_flags` declaration in `repe scasd`
      pseudo-loops (hit in `recomp_0011.c:sub_0023B026`, a false-positive data island;
      patched locally, fix belongs upstream in the lifter with a conformance test)
- [x] First run (30 s timeout, `mvp06/first_run_*.txt`): **boots and executes guest
      code without crashing** — 17/17 sections loaded, 153 thunks resolved, CRT init
      runs, reaches EA/XONLINE library init. Hangs spinning on unresolved indirect
      calls; only **3 distinct unknown VAs** in the whole run:
      `0x0059F844` ×21,050 (XONLINE worker loop), `0x00442F94` ×1 (CRT),
      `0x005B753F` ×1 (XNET). All three sit exactly where a detected function ends —
      disassembler misses (indirect-only targets), not corrupt code.
- [ ] Upstream: lifter `_flags` declaration fix + SSE2 forms from §1.3.1
- [ ] Optional but high-value: Ghidra naming pass (`tools/ghidra_naming`)
- [ ] Optional: `tools/rtti` pass to name the C++ classes

### Phase 2 — Boot to first render *(the hard grind; weeks; STARTED)*

The loop is: run → harvest unresolved VAs from stderr → `tools/seed_from_log` →
re-lift → rebuild → run. Opening state: 3 VAs to seed (above). Also expect
thread-start routines to surface the same way (`PsCreateSystemThreadEx` targets are
pushed as arguments, never called — the documented #1 "built but did nothing" cause).

**Iteration 1 (2026-10-08):** seeded 6 VAs from the first run (the 3 ICALL targets +
3 thread-start routines: `0x0031EF5D` beside the entry point, `0x0042065E`,
`0x0044F4E2` — the game does create threads at boot). 5 of 6 lifted as functions.
`0x00442F94` is *not* a function: it is a mid-function entry point into an unrolled
struct-copy routine (table of entry points at `0x00442F7x`, tail `jmp [edx*4+0x442FD8]`)
called indirectly once in 30 s — the disassembler correctly refuses to seed it (would
split the containing function). Future fix: hand-written override of the containing
copy routine in `recomp_manual.c`. Also fixed upstream-worthy lifter bug: translator
declares `_flags` for rep-prefixed `cmps`/`scas` string ops (tests: 230 passed).

**Iteration 2 (2026-10-08):** three blockers fixed in sequence, each verified by a run:

1. **Kernel dispatcher objects** (runtime fix, upstream-worthy): Xbox code waits on
   KEVENT/KSEMAPHORE structs *embedded in guest memory* via `KeWaitForMultipleObjects`/
   `KeWaitForSingleObject`/`KeSetEvent`. The bridges only resolved handle tokens, so a
   guest-object pointer fell through to a bogus native handle → instant WAIT_FAILED →
   a 175M-call spin in worker #3. Added `bridge_ke_object()` in `kernel_bridge.c`
   (shadow → tagged-token → auto-created host object seeded from the guest
   DISPATCHER_HEADER; serialized against the last-wins shadow insert). Result: both
   thread mailboxes resolved (type-1 sync event + type-0 notification event), the
   spin vanished, and the game's wrapper waits now succeed.
2. **VBlank**: the GPU frame clock is opt-in — run with `RECOMP_VBLANK=1`. D3D's
   vblank ISR connects on vector 3, claims the (now delivered) interrupts, and queues
   its DPCs (drained by the timer thread).
3. **D3D pusher poll** (local gen patch): device-create spins at `0x0057F560` on
   `sub_005794E0` (pushbuffer PUT≠GET busy-poll) because nothing consumes pushbuffer
   commands. Patched `sub_005794E0` in `recomp_0035.c` to report idle (note: direct
   calls bypass `recomp_lookup_manual` — `RECOMP_ABI_CALL` is a plain call unless
   `RECOMP_ABI_CHECK` is defined — so the manual-override table can't intercept it;
   the gen patch is why). Device-create now completes (three AvSendTVEncoderOption
   calls are its last act, then it returns to EA's init).

**Iteration 3 (2026-10-09/10) — architecture corrected by studying the two
reference ports:**

- **Burnout 3** (github.com/sp00nznet/burnout3, docs/boot.md): does NOT
  replace guest D3D — the title's own D3D8LTCG runs against the toolkit's
  software NV2A pushbuffer executor (`RECOMP_PB_EXEC=1`), with small runtime
  bridges: `xbox_Nv2aMirrorFence` (pushbuffer handshake), frame counters,
  `RECOMP_AC97_READY`, `RECOMP_APU_DSP_ACK`, `RECOMP_USB=1`. Its fix 12
  (XPP owns physical page zero → contig-skip-page-zero) is the same fix this
  project landed independently. Playable end-to-end at 2–14 fps (software GPU).
- **halo-ce-universal** (github.com/cybersecurity/halo-ce-universal): the
  other extreme — full decompilation to C, native compilation, and a
  hand-written clean-room XDK surface (`port/include/xdk`) whose
  IDirect3DDevice8_* wrappers forward to a host OpenGL 4.5 renderer + SDL3.
  Fast, but only possible because a 400+-star decompilation effort exists.
  Not applicable to EA's engine — but it is the template for a future
  fast-graphics tier: intercept the ~30 EA→D3D8 direct-call entry points
  (enumerable; call-site patching machinery already proven here) and forward
  them to the toolkit's own xbox_d3d8/D3D11 layer.

**Changes landed this iteration:**
1. Reverted all three D3D stubs (pusher poll, drain cut, edge thunks) — the
   software-GPU path replaces them. (The thunks remain dormant in
   recomp_manual.c.)
2. main.c now follows the Burnout 3 boot pattern: env defaults
   (RECOMP_PB_EXEC, RECOMP_AC97_READY, RECOMP_USB, RECOMP_USB_PADS,
   RECOMP_FB_WINDOW), `xbox_OhciInit()`, `CreateDirectoryA("saves")`.
3. **Pushbuffer fence registered correctly**: `xbox_Nv2aMirrorFence(0x589A38,
   0x2C, 0x30)` — sub_0057D710 (chunked submitter) compares [dev+0x2C]
   (submitted) against the slot [dev+0x30] points at (engine-consumed,
   0x82815000 in the ring). Identical offsets to Burnout 3 (same XDK 5849).
   The first attempt (0x3240/0x3244) read the PFIFO CACHE1 pair, which the
   runtime docs say is inert — verified via RECOMP_KERNEL_WATCH that all of
   0xFD003240/44 and 0xFD400700 stay zero.
4. Runtime fix (upstream-worthy, matches Burnout 3 fix 12): contiguous
   allocator now reserves the low 64 KB of the 0x80000000 window
   (`XBOX_CONTIG_LOW_RESERVE`) so EA's 40 MB arena doesn't land on XPP's
   page-zero structures.

**State after these fixes:** with `RECOMP_VBLANK=1`, the software GPU
executes (a 640×480 target with pitch 2560 gets cleared; DMA GET tracks PUT),
splash.big loads, the EA bucket heap stays coherent (253 clean walks, no
0xCCCCCCCC), and the frontier crash moved to a read at guest 0xFED00000+0x10000
(eax=0xFED00000) — the **MCPX/APU aperture**, i.e. audio init. That is
Burnout 3's fixes 4–6 territory: AC97 codec readiness, APU MMIO routing
through the VEH fault handler (`apu_hook_handle_mmio`), and the DSP doorbell
ack (`RECOMP_APU_DSP_ACK`).

**Debugging tools built this iteration** (all under mvp06/): tools_sample.py
(thread RIP sampling → /MAP symbolication), tools_stackscan.py (host-stack
scan → guest call chains), tools_threadtimes.py (per-thread CPU to find
spinners), tools_watchpoint.py (hardware DR0 write watchpoint; arm early —
piped stderr makes the game run faster than console output). Runtime env
diagnostics used: RECOMP_KERNEL_WATCH(_ALL), RECOMP_PEEK(_CHAIN),
RECOMP_NV2A_TRACE, RECOMP_PB_EXEC_VERBOSE. x64 CONTEXT offsets: Rip=0xF8
(0xF0 is R15). Instrumentation left in generated code: walker log
(recomp_0022.c sub_0041A4D6), alloc log (loc_0041A978), transform log
(loc_0057E000) — rate-limited, trim before any upstream contribution.

Environment for runs: `RECOMP_VBLANK=1` (required), and main.c now defaults
the rest (RECOMP_PB_EXEC, RECOMP_AC97_READY, RECOMP_USB=1, RECOMP_USB_PADS=2,
RECOMP_FB_WINDOW=1). Optional: `RECOMP_KERNEL_LOG_BUDGET=5000000` (full
per-call kernel trace with guest ret=), `RECOMP_NV2A_TRACE=1` (pushbuffer
survey + [GPU] status + peek reporting), `RECOMP_KERNEL_WATCH=<va>
RECOMP_KERNEL_WATCH_ALL=1`, `RECOMP_PEEK`/`RECOMP_PEEK_CHAIN`.
Debug logging added in `kernel_bridge.c` (`[KEWAIT]`/`[KEMWAIT]`/`[KEOBJ]`,
rate-limited) — trim before any upstream contribution.

**Next milestone (iteration 4): audio bring-up.** The 0xFED00000 MCPX crash
needs the Burnout 3 treatment: verify `RECOMP_AC97_READY` reaches the codec
gate, wire APU MMIO access through the VEH (the template's
`apu_hook_handle_mmio` path), and identify the DSP-doorbell address for
`RECOMP_APU_DSP_ACK` from the spin (Burnout 3 observed theirs as `ebx+0x810`
at the spin site). After that: XID input seeding (Burnout 3 fix 13 —
table-reachable driver functions missing from the dispatch), then the
first visible frame.

**Iteration 4 (2026-10-10) — audio/input frontier, two more reference ports:**

Reference ports studied and cloned under `reference/`:
- **nfsu2-sw / nfsc-sw** (antoxa2584x; xboxrecomp fork, **XDK 5849**, EA):
  playable end-to-end. Pushbuffer executed by **Vulkan/OpenGL backends**
  (not the CPU rasterizer), **GIL** guest-lock for EA's worker races,
  KeTickCount thread, 59.94 Hz vblank, IRQL/GIL-retention tuning, VP6 movies
  via FFmpeg, saves under game/UDATA. Their CLAUDE.md is the EA-bring-up
  playbook (quoted offsets and failure modes match this project's bugs).
- **doaxvb-re** (NoRain211; "fully working"): different runtime — **D3D8 APIs
  replaced with native D3D11** (the halo-ce-universal pattern, but at binary
  level via the direct-call boundary). Fully playable with saves/audio/4 pads.

Fixes landed this iteration (each verified by the next run's progress):
1. **OHCI MMIO routing** in the VEH (title main.c): faults in the 0xFED00000
   controller block → `xbox_OhciHandleMmio`. Result: `[OHCI0] operational
   after 220 ms; device arriving on port 1`, XPP ISRs claim interrupts.
2. **APU init ordering**: env defaults must be set *before*
   `xbox_MemoryLayoutInit` — the APU model and its register unmap are both
   gated on RECOMP_AC97_READY at init time. (Was: defaults set at Step 5,
   after the gate had already been read as unset.) Result: APU up (XAudio2
   backend), first 90-second crash-free run.
3. **DSP doorbell**: observed via `RECOMP_WATCHDOG_SECS=10` (guest
   ebx=0x82F20810 at sub_005C234E's poll — DSound heap + 0x810, same shape
   as Burnout 3/Wreckless). Registered as RECOMP_APU_DSP_ACK. Result:
   "command 0x3 acknowledged", audio init passes.
4. **GP/EP DSP memory untrapped** (runtime fix, mirrors burnout3's
   fix/apu-dsp-memory-untrapped): only the 256-voice register block
   (0x0–0x30000) stays NOACCESS for the MMIO hook; 0xFE830000+ becomes plain
   RAM again so lifted-code memmove of DSP memory works. Result: the
   memmove crash gone; **file opens 32 → 171** (datafile.big + config parse).
5. **Crash frontier**: EA's number formatter receives garbage args
   deterministically; log shows `[ICALL] Failed to resolve VA 0x0042C2EE`
   (×528) and `0x00429BE3` (×146) at the crash — more undetected entry
   points, plus a new worker thread (0x42E428). 8 VAs seeded via
   tools.seed_from_log; full disasm→lift→build pipeline re-run.

NOTE on crashes vs races: the formatter crash reproduced bit-identically
twice (same RIP, same fault address, same path count) — deterministic, NOT
the NFSU2 GIL race class. The GIL remains a known future need for EA
stream workers (their fork's kernel_bridge.c carries a ~200-line
implementation to port if/when races appear).

Iterate the crash loop per `docs/pipeline/06-debugging.md`:

- [x] CRT init → `main()` reached (first run got through CRT into library init)
- [ ] Kernel-thunk level: verify all 153 imports resolve to bridges (several will be
      first-use: `IoCreateDevice`, `KeConnectInterrupt`, `IdexChannelObject` —
      cooperative stubs)
- [ ] D3D8 device creation through the API-intercept path; first clear/present
- [ ] `XeLoadSection` handling for fe_sec/db_sec (frontend/database code)
- [ ] EA frontend boots: title → main menu (VP6 intro skipped if needed)
- [ ] Threading decision point: if worker-thread deadlocks block progress, implement
      real-thread `PsCreateSystemThreadEx` in the toolkit (R1) — plan for this, it is
      the most likely deep-work item
- [ ] Controller navigates menus

### Phase 3 — Exhibition game playable *(weeks)*

- [ ] BIG archive loads (pure file I/O — verify `\Device\CdRom0\` translation on the
      big streaming reads)
- [ ] Stadium + players render (T&L/vertex-shader paths get their first real workout)
- [ ] Audio banks play; streaming commentary/crowd addressed (R5)
- [ ] Nine-inning game completes: batting/pitching/fielding, substitutions, stats
- [ ] Saves work through `T:\` translation; re-load verified
- [ ] Side-by-side validation against xemu (`docs/technical/xemu-debugging.md`) for
      logic regressions

### Phase 4 — Remaster layer *(only after Phase 3 is stable)*

Ordered cheapest-first, matching the enhancement goals:

1. **Resolution + widescreen**: host D3D11 swapchain already decoupled from the guest
   640×480 — expose windowed/fullscreen resolution selection; then aspect-ratio work:
   projection matrix hook (single runtime hook beats patching generated code), HUD
   safe-area scaling. Validate: no stretched 4:3 assets, score bug readable.
2. **AA**: MSAA on the D3D11 swapchain where cheap; SMAA/FXAA pass later.
3. **Texture replacement**: hash-based override at `d3d8_resources` texture-creation
   time (formats are already normalized there) → drop-in HD folder. AI-upscale the
   `.xsh`-sourced textures (uniforms, grass, dirt, ads, faces) offline.
4. **Post-processing**: color grading first (needs no depth); bloom/AO only after
   depth-buffer access is proven.
5. **HD UI**: scale/replace frontend assets once the FE asset pipeline is understood.
6. **60 FPS**: only after timing fidelity — measure the game's tick discipline first;
   MVP 06's gameplay logic stays authoritative regardless.
7. **Roster mods**: file-override layer at the I/O boundary (individual-file shadowing
   over BIG archives), external roster editor later.

### Phase 5 — Validation matrix

Dynasty mode (create team, recruit, sim games, multi-season), roster management,
save/load round-trips, all controller types, long-session stability, full postseason.
Fixes feed back into the toolkit where general.

---

## 4. What stays untouched (scope control)

Player models, animations, batting/pitching/fielding physics, commentary logic, Dynasty
logic — all remain the recompiled originals. Every enhancement lives in the host-side
layers (swapchain, texture path, post pass, I/O shim), never in patched game logic.
Exception process: a hand-override of recompiled code is allowed only as a last-resort
bug workaround, logged in `mvp06/src/recomp_manual.c` with a comment explaining why.

## 5. Contribution loop

This project will hit toolkit bugs the Burnout 3 work never exposed (SSE forms in 2005
EA code, threading, DSound streams). Every lifter/runtime fix goes upstream to
xboxrecomp with a test — that is the cheapest way to keep this port maintainable, and
it's explicitly what the project asks for. The Discord (`README.md`) has the maintainers.

## 6. Immediate next actions

1. Wait for lift + unpack jobs; fill in lift-coverage numbers in §1.3
2. Ghidra naming pass + `merge_names.py --apply`
3. Build the `.exe`, capture the first crash, begin the Phase 2 loop
4. If Phase 2 stalls on threading for more than a few sessions, pivot to implementing
   real kernel threads in the toolkit (with Burnout 3 as regression control)
