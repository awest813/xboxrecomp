# MVP 06 NCAA Baseball — Engineering Log

Chronological record of every bring-up iteration. Newest last. Each entry lists
what was observed, what was changed, and how the next run verified it.
Addresses are guest VAs unless marked host. Run logs live in `mvp06/run*.txt`;
build logs in `mvp06/build_log*.txt`.

---

## 2026-10-08 — Audit and Phase 0/1 (toolchain validation)

**Environment:** xboxrecomp v0.11.0 clone at repo root; game project at
`mvp06/`. Host: Python 3.14.3 + capstone 5.0.7, CMake 4.3.1, VS BuildTools 18
(MSVC 19.50), 583 GB free.

**Measured (not estimated):**
- ISO: 7,825,182,240 B dual-layer; `tools.xiso` reads it transparently
  (270 entries, ~2.06 GB of files; EA BIG archives, `.ast`/`.abk` audio,
  `.asf` WMA music, **`.vp6` movies** — not XMV).
- XBE: XDK **5849**, plain **D3D8** (not LTCG), entry `0x0031EFD1`, base
  `0x00010000`, `.text` 0x4BC4C4 (~4.97 MB), RTTI present, 153 kernel imports
  (153/153 routed per `tools.kernel_audit.coverage`).
- Disasm: **37,734 functions**; func_id: 8.5k vtable_scan + 6.2k vtable_thunk;
  ABI: 6,441 thiscall. Lift: **37,732/37,732 translated, 0 failed**, 4.27 M
  lines of C; unimplemented-instruction census: 386 sites / 155 functions
  (mostly data-island false positives; real items: one SSE2 block at
  `sub_0049AE36`/`sub_0049AF9A`, `psubq` at `sub_0057FB36`, `cpuid` ×5).
- First `.exe`: `mvp06_recomp.exe` (56.9 MB) after one lifter bug fix (below).

**Fixes:**
- Lifter (upstream-worthy): translator declared `_flags` only for
  jcc/setcc/cmovc; rep-prefixed `cmps`/`scas` string ops emit `_flags` loops
  without any of those → undeclared identifier (hit in `recomp_0011.c:
  sub_0023B026`, a false-positive data island). Added the string-op forms to
  `has_conditionals` in `tools/recomp/translator.py`. 230 tests pass.
- `0x00442F94` ICALL miss identified as a mid-function entry of an unrolled
  struct-copy routine (entry-point table at `0x00442F7x`, tail
  `jmp [edx*4+0x442FD8]`) — deliberately not seeded.

**First run:** boots, loads 17/17 sections, resolves 153 thunks, runs CRT init
and EA/XONLINE library init for 30 s+ without crashing; spins on 3 unresolved
ICALL VAs.

## 2026-10-08 — Iteration 1: seeding + dispatcher objects

- Seeded 6 runtime-discovered VAs (`tools.seed_from_log`), incl. 3 thread
  starts (`0x0031EF5D` — main-thread wrapper beside the entry point,
  `0x0042065E`, `0x0044F4E2`). 5 lifted; re-lift/rebuild.
- **Kernel dispatcher-object shadowing** (runtime, upstream-worthy): Xbox code
  waits on KEVENT/KSEMAPHORE embedded in guest memory via Ke* family; bridges
  resolved only handle tokens, so a guest object VA fell through to a bogus
  native handle → instant WAIT_FAILED → **175 M** KeWaitForMultipleObjects
  calls. Added `bridge_ke_object()` in `src/kernel/kernel_bridge.c`
  (shadow → tagged token → auto-create host event/semaphore seeded from the
  guest DISPATCHER_HEADER; serialized against the last-wins shadow insert).
  Both thread mailboxes resolved (type-1 sync event at `0x01181E00`,
  type-0 notification event at `0x00F7DE14`); spin vanished.

## 2026-10-08 — Iteration 2: vblank + pusher poll

- `RECOMP_VBLANK=1` required: D3D's vblank ISR (vector 3, routine
  `0x00580960`) connects, claims delivered interrupts, queues DPCs.
- **Pusher poll** `sub_005794E0` (device+0x3240 vs +0x3244 = PFIFO CACHE1
  pair; later verified inert) spins at device-create `0x0057F560`. Gen-patched
  idle (later reverted — see iteration 3). Device-create completed.
- Debug tooling built: `tools_sample.py` (thread RIP sampling + `/MAP`
  symbolication — x64 CONTEXT **Rip offset is 0xF8; 0xF0 is R15**),
  `tools_stackscan.py` (host-stack scan → guest call chains),
  `tools_threadtimes.py` (per-thread CPU), `tools_watchpoint.py` (DR0
  hardware write watchpoint; arm ≤1.5 s — piped stderr makes the game run
  faster than console output).

## 2026-10-09 — Iteration 3: architecture corrected by reference ports

Studied (cloned under `reference/`): **burnout3** (sp00nznet; software NV2A
pushbuffer executor + fence bridges; fix 12 "contig-skip-page-zero" =
our page-zero bug), **halo-ce-universal** (decomp + clean-room XDK + host GL),
later **nfsu2-sw / nfsc-sw** (antoxa2584x; xboxrecomp fork, XDK 5849, EA;
GIL, VK/GL pb_exec renderers, PGRAPH→D3D11 translator) and **doaxvb-re**
(NoRain211; D3D8 APIs → native D3D11; fully playable).

**Landed:**
- Reverted all three D3D stubs; `main.c` → Burnout-3 boot pattern.
- **Contig low-64 KB reserve** (`XBOX_CONTIG_LOW_RESERVE` in
  `xbox_memory_layout.c`): EA's 40 MB arena was the first contiguous
  allocation and landed on XPP's page-zero descriptors; its bucket-0 head
  then walked an XPP "RAM" string as block headers → 0xCCCCCCCC crash
  (poison traced to XPP allocator `sub_005E6A2B`, which fills every
  allocation with 0xCC).
- **Pushbuffer fence** registered correctly as
  `xbox_Nv2aMirrorFence(0x589A38, 0x2C, 0x30)`: `sub_0057D710` (chunked
  submitter) compares [dev+0x2C] (submitted) vs the slot [dev+0x30] points at
  (engine-consumed = 0x82815000 in the ring). Same offsets as Burnout 3.
  First attempt (0x3240/0x3244) read the PFIFO CACHE1 pair — all-zero,
  verified via RECOMP_KERNEL_WATCH.
- Result: software GPU executes (640×480 target, pitch 2560, cleared; DMA GET
  tracks PUT through 2.6 MB), `splash.big` loads, EA bucket heap coherent
  (253 clean walks).

## 2026-10-10 — Iteration 4: audio + USB frontier

- **OHCI MMIO routing** (title VEH): faults in the 0xFED00000 controller
  block → `xbox_OhciHandleMmio`. `[OHCI0] operational after 220 ms; device
  arriving on port 1`; XPP ISRs claim interrupts.
- **APU env ordering**: env defaults must be set **before**
  `xbox_MemoryLayoutInit` (both the APU model creation and the register unmap
  gate on RECOMP_AC97_READY at init time). Moved to WinMain top. APU up
  (XAudio2), first 90 s crash-free run.
- **DSP doorbell**: `RECOMP_APU_DSP_ACK=0x82F20810` — observed via
  `RECOMP_WATCHDOG_SECS=10` (guest ebx at `sub_005C234E`'s
  `cmp [ebx],0` poll; DSound heap + 0x810 — same shape as Burnout 3/Wreckless).
  "command 0x3 acknowledged"; audio init passes.
- **GP/EP untrap** (runtime; mirrors burnout3 `fix/apu-dsp-memory-untrapped`):
  only the 256-voice register block (0x0–0x30000) stays NOACCESS for the MMIO
  hook; `0xFE830000+` is plain RAM again so lifted-code `memmove` of DSP
  memory works (was: fatal at 0xFE830200/0xFE840200). **File opens 32 → 171**
  (datafile.big parsed, `datafile.txt` probes correctly fail with
  0xC0000034, frontend textures/audio banks load).
- **Crash frontier**: EA number formatter (`sub_00446F96` family) receives
  deterministic garbage args (value ASCII "  00", dest 0xBC8D0000).
  Bit-identical reproduction across runs — NOT a race (GIL not implicated).
  Log shows `[ICALL] Failed to resolve VA 0x0042C2EE` ×528 and `0x00429BE3`
  ×146 at the crash — more undetected entry points; failed dispatch returns
  eax=0 and downstream builds garbage. 8 VAs seeded (incl. new worker thread
  start 0x42E428); full pipeline re-run in flight.

## 2026-10-10 (later) — the memcpy jump-table root cause

- Seeded build: crash gone, 120 s stable, but a second **DSP doorbell**
  surfaced at `0x82F28810` (+0x8000 — one doorbell per DSP command block).
  List env `RECOMP_APU_DSP_ACK=0x82F20810,0x82F28810`. Paths → **181**,
  then a new crash walking `0xBB55BB55` poison (EA freed-memory pattern).
- The persistent `Failed to resolve 0x0042F94 / 0x442EEC / 0x442E74 /
  0x442D60` family finally decoded: **`sub_00442CF0` is MSVC CRT `memcpy`**,
  and those VAs are its *own jump-table targets* (`jmp [edx*4+0x442E3C]` and
  the backward tail `jmp [edx*4+0x442FD8]`). The lifter already emits
  computed `goto`s for intra-function switch tables (`_analyze_switch_table`
  — same code as the NFS fork), but the **disassembler's function boundary
  stopped at 0x442EDF/0x442F92**, orphaning the tail blocks; with targets
  outside the function the switch detection truncates to nothing and the
  jump becomes an unresolvable ICALL. Consequence: every memcpy whose path
  took a jump-table route silently **copied nothing and returned 0** — the
  plausible root cause of the datafile-parse garbage, the "  00" formatter
  args, and the poison walks.
- Fix applied (bring-up shortcut): extended `sub_00442CF0` to end 0x442FE8
  in `tools/disasm/output/functions.json` (covers both tables and all tail
  blocks; one contained pseudo-function removed), and added the missed SEH
  function at `0x94B90..0x94C80` the same way. Re-lift + build in flight.
- **Upstream-worthy follow-ups:** (a) disassembler should extend function
  extents to include jump-table targets (the detector already recovers the
  tables for xrefs); (b) the seed tool's "alias inside" entries were a
  symptom of the same boundary bug.

## 2026-10-10 (final) — the decode-convergence fix (memcpy now fully correct)

Three stacked defects explained every 0x442Exx dispatch miss; each fix
exposed the next:

1. **Function boundary** (fixed in functions.json, ranges extended to
   0x443028): the disassembler's sub_00442CF0 ended before memcpy's tail
   blocks and tables. Stale seed entries (0x442D60/EEC/F10/F94/FBC family,
   added when they looked like missing functions) were removed from both
   `config/seed_functions.json` and functions.json — the 0x442FBC
   "vtable_thunk" was a stop-address capping the CFG recovery.
2. **Table shapes** (fixed in lifter `_analyze_switch_table`): (a) tables
   whose FIRST dword overlaps the preceding jmp instruction
   (0x442D50/0x442EDC read 0x9000442D — probe at base+4); (b) negated-index
   tables growing DOWN from the base (0x442F88 — probe at base−32). The
   fallback scans 8 dwords at each probe for a run of ≥2 consecutive
   in-function targets; emitted comparisons are unchanged.
3. **Decode convergence** (fixed in translator `decode_function`): the
   linear decode stops at the first undecodable byte (a table), so the
   backward-path jump instructions only appear AFTER the first resync — but
   switch leaders were collected exactly once, before resync, so
   second-order tables never triggered a second resync. Now iterated
   (≤4 passes) until leaders and decode agree. Isolated test:
   sub_00442CF0 translates with all labels live (0x442F8C/0x442FE8/0x443014)
   and zero dead-code arms.

This class (silent no-copy memcpy) is the plausible root of the datafile
garbage, the formatter crash, and the poison walks.

**Verification runs with the converged decode (handoff state):**
- 240 s run: **no crash, zero `Failed to resolve VA` entries** (first fully
  clean dispatch log of the project) — but boot parked at an earlier wait
  (32 paths, SFX banks loading, both DSP doorbells acknowledged).
- A second, shorter run crashed at a different site (read of 0x8A078808 via
  0x003B5E38, material-parse region) — the behavior is now
  **timing-dependent** between a wait and a crash, with no dispatch failures
  in either. That pattern points at the EA worker race class (NFSU2's GIL)
  as the next suspect, per the reference-port evidence.


## Reference-port architecture comparison

| Port | Approach | Status |
|---|---|---|
| Burnout 3 (sp00nznet) | guest D3D8LTCG + software NV2A rasterizer | playable alpha, 2–14 fps |
| NFSU2/NFSC (antoxa2584x) | xboxrecomp fork; pb_exec → Vulkan/OpenGL + PGRAPH→D3D11 translator; GIL | playable end-to-end |
| halo-ce-universal (cybersecurity) | full decomp + clean-room XDK → host GL/SDL | full port (needs decomp) |
| doaxvb-re (NoRain211) | D3D8 APIs → native D3D11 at the binary boundary | fully playable |
| **MVP 06 (this project)** | xboxrecomp v0.11.0; software NV2A now, D3D11 PGRAPH planned | boot → audio init → config parse |
