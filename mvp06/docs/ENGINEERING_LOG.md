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
  0x003B5E38, material-parse region). The initial handoff described both logs
  as dispatch-clean and suggested the EA worker-race class. **Later audit
  correction:** run48 contains two misses at 0x00442FCF inside memcpy. The
  race diagnosis was premature; see the follow-up entry below.


## Reference-port architecture comparison

| Port | Approach | Status |
|---|---|---|
| Burnout 3 (sp00nznet) | guest D3D8LTCG + software NV2A rasterizer | playable alpha, 2–14 fps |
| NFSU2/NFSC (antoxa2584x) | xboxrecomp fork; pb_exec → Vulkan/OpenGL + PGRAPH→D3D11 translator; GIL | playable end-to-end |
| halo-ce-universal (cybersecurity) | full decomp + clean-room XDK → host GL/SDL | full port (needs decomp) |
| doaxvb-re (NoRain211) | D3D8 APIs → native D3D11 at the binary boundary | fully playable |
| **MVP 06 (this project)** | xboxrecomp v0.11.0; software NV2A now, D3D11 PGRAPH planned | boot → audio init → config parse |

## 2026-10-10 (follow-up) — memcpy audit, complete XNET switch, intro-movie frontier

The handoff's race inference did not survive a log audit: run47 was clean,
but run48 contains **two `Failed to resolve VA 0x00442FCF`** messages before
the material-parse crash. This address is memcpy's zero-dword backward arm,
not a missing function. No GIL change was made.

**Four additional defects fixed:**

1. **Downward table index zero:** the old base-32/eight-dword probe stopped
   at base-4, omitting the target at base (0x442FCF). Scan downward from the
   base, including index zero, until the first non-target (256-entry bound).
2. **Overlapping decode:** resync deleted the instruction straddling its
   starting point but left later stale starts inside recovered instructions.
   The merged stream executed bogus `inc esp`, `inc ebp`, and memory writes
   between genuine memcpy instructions. `tools/recomp/disasm.py` now removes
   every stale start covered by each recovered instruction.
3. **Still-short boundary:** 0x443028 cuts through `mov eax,[ebp+8]` at
   0x443026 and drops the final epilogue. Correct exclusive end is
   **0x44302D**, after `ret` at 0x44302C. Local functions.json patched; the
   handoff and AGENTS reapplication instructions now use the correct end.
4. **Truncated longer tables:** after fixing memcpy, runs49/50 both crashed
   in XNET with one miss at 0x5BEFD2 and a write to guest 0xFFFFFFFC (16 path
   attempts). XNET checksum sub_005BEF58 jumps through a fifteen-arm table
   at 0x5BF054 whose unused index-zero dword is zero. The old fallback read
   only seven arms after that dword. The base+4 fallback now reads the full
   contiguous in-function run (256-entry bound). No internal arm was seeded.

**Validation:**

- Four synthetic regression tests reproduced the zero-index omission,
  overlapping decode, image-end probe overread, and long-table truncation
  before their respective fixes. `py -3 -m pytest tools/recomp -q`:
  **234 passed, 24 skipped, 44 subtests passed**.
- A local 32-bit MSVC harness executes the original x86 memcpy (relocated
  absolute table references), the newly lifted C using the real runtime
  header, and libc memmove. **281,124 cases passed**: lengths 0..512, all
  mod-4 alignments, both non-overlap directions, overlap displacements
  -64..64. Full-buffer bytes, return value, guest ESP, EBX/ESI/EDI, and DF
  checked. Decode: 299 non-overlapping instructions, final ret recovered.
  Harness files are local under `mvp06/build/`, not distributed game code.
- Only the two affected generated bodies (memcpy in chunk23 and XNET
  checksum in chunk36) were refreshed for this experiment; both Release
  builds succeeded (`build_log49.txt`, `build_log51.txt`). No disasm re-run
  was performed, so the existing boundary patches remain in place.
- **Run51: 180 s, no crash, 590 `[PATH]` attempts.** Material parsing passed;
  `frontend/title.big` and `frontend/movies/easports.vp6` opened. GPU logged
  25 draws, 48 rasterised triangles. Visible output was not inspected and
  is not established by these counters. Dispatch is not clean: new CRT
  jump thunk **0x432FBB** (two misses) and movie-path MMX routine
  **0x49E807** (repeated misses). Logging interleaves, so repeated-miss
  counts are unreliable. Neither memcpy's 0x442FCF nor XNET's 0x5BEFD2
  recurred.
- `tools.seed_from_log` accepted both new addresses as new functions and
  updated `config/seed_functions.json` (10 → 12 entries). **Their full
  disasm/lift/build cycle remains the next step**, with memcpy's 0x44302D
  boundary re-applied between disasm and lift. No game process left running.

**Frontier:** intro movie / new dispatch targets, with material parsing now
passed in a runtime test. A worker race is still possible later; the current
evidence does not establish it. No claim of a visible frame or playability.

## 2026-10-10 — Full seed cycle and first visible frame (run52)

**Requested cycle completed for 0x00432FBB and 0x0049E807.** Full disassembly
finished in 213.65 s. The database contains 37,743 functions, including the
five-byte CRT tail-jump thunk and the 1,034-byte movie-path MMX function
(final ret at 0x49EC10). New `mvp06/tools_patch_boundaries.py` reapplies
memcpy's exclusive end **0x0044302D**, removes any contained function starts,
and preserves the SEH range 0x00094B90..0x00094C80. It rejects internal
memcpy seeds and was checked for idempotence. README, AGENTS and handoff
commands now invoke it between disassembly and lift.

The full lift translated **37,741/37,741 functions, zero failures**, producing
4,270,500 lines in 38 source files (295 unresolved direct-call stubs; 387
unimplemented instruction comments elsewhere in the image). Regenerated
memcpy exactly matches the body that passed the 281,124-case differential
test. It has no dead arms; XNET checksum still has all fifteen switch targets;
the new movie MMX function has no unsupported instructions. Both new targets
are in the generated dispatch table. Release build succeeded with scoped
MSVC `/MP4`; thirteen C4293 shift-count warnings remain in generated
sub_005879C8, outside the newly seeded functions. Logs: `disasm52.txt`,
`lift52.txt`, `build_log52.txt`.

**Visible output is now established.** Added optional
`RECOMP_FB_WINDOW_DUMP_EVERY=<refreshes>` to the existing framebuffer
diagnostic. With `RECOMP_FB_DUMP=build/frame52/frame` and interval 300, it
saves `<prefix>.windowNNN.bmp` throughout startup; the default one-shot
capture is unchanged. A follow-up incremental Release build succeeded
(`build_log52_capture.txt`). No rendering/presentation semantics changed.

**Run52: 300 s, no crash, 590 `[PATH]` attempts** (153 distinct parsed path
strings; attempts are not a unique-file count). Window captures show the
MVP 06 loading screen and then legal artwork; successive GPU captures show
the loading bar advancing. The legal graphic's text is clipped at the right
edge, so this is first-frame bring-up, not a rendering-correctness claim.
Final GPU counters: 59 draws, 116 rasterised triangles, 11,363,346 pixels
written. Local artifacts: `build/frame52/first-visible.png` (converted from
`frame.window000.bmp`), 31 window BMPs, 35 GPU BMPs and `contact.png`.

The two requested targets no longer miss. Intro opens
`frontend/movies/easports.vp6` but repeatedly misses **0x0049E116**, the only
distinct unresolved VA parsed from `run52_stderr.txt` (4,544 complete records;
interleaved logging can hide others). It is a separate address-taken MMX
routine in the gap after sub_0049E0C2, referenced at 0x49AD54. Local decode:
533 non-overlapping instructions, final ret 0x49E806, exclusive end 0x49E807,
no dead arms or unimplemented instructions. Probe saved only under ignored
`build/movie_0049E116_probe.c`; it is **not installed or seeded yet**.
The immediate next intro step is to seed that entry and run the pipeline,
then verify progressing movie frames and a dispatch-clean log. A guest GIL
or a renderer port is not yet justified by this miss. The first-visible-frame
milestone is reached; complete intro playback, menus and playability are not.
Run52 was stopped at the timed limit; no game process left running.

## 2026-10-10 — Movie IDCT convergence and clean intro image (runs53–55)

Seeded 0x49E116 and its two peers installed by the On2 initializer:
0x49D0C0 (710 instructions, end 0x49DA06) and 0x49DA06 (513 instructions,
end 0x49E0C2). All three are separate, non-overlapping MMX routines with
no unsupported instructions. The address-taken probe's 64-instruction cap
missed these long, nonstandard-prologue bodies; the explicit initializer
pointers supply stronger evidence than a broad discovery heuristic change.
Full cycle53: disasm 194.41 s, boundary helper reapplied memcpy 0x44302D,
37,744/37,744 functions lifted with zero failures, 4,272,292 lines / 38 files.
Release build succeeded with `/MP4`, retaining thirteen C4293 warnings outside
the audited routines. Logs: `disasm53.txt`, `lift53.txt`, `build_log53.txt`.

A local 32-bit native differential harness tested all three IDCT bodies
against the original x86, including the title's original 0x49AC5F constant
initializer (mask FFFF, cosine FB15, round 0008). **6,144 cases passed**:
sparse/DC and random coefficient/quantization data, complete 2,048-byte
buffers/guards, saved EDX/ECX/EBX and ESP. This proves these routines, not
the entire movie decoder. Ignored local harness: `build/idct_audit53.*`.

Run53: 300 s, 590 path attempts, no crash, zero parsed dispatch misses;
last periodic GPU report 7,247 draws / 14,492 triangles. Sampling found the
main thread in D3D lock/present traffic; captures still showed legal art.
The movie textures were format 0x24 (YUY2), which fell through the sampler
and painted vertex colour. Added packed YUY2/UYVY BT.601 conversion.

OHCI simultaneously read zero EDs at physical 0x02EDE0C0. Live memory
showed the queue at 0x82EDE0C0, with the low-RAM copy entirely zero. Added
bus-to-contiguous translation for ED/TD/HCCA accesses. Run54 began actual
GET_DESCRIPTOR transfers but retried three times and disabled the port:
its reply buffer was static .data VA 0x006E0B4C, passed through by
MmGetPhysicalAddress. Preserving image VAs fixed that case. The runtime's
overlapping physical/virtual allocation ranges remain an architectural
limitation for arbitrary pass-through heap DMA; the modern reference moves
its contiguous arena above the image and tracks physical allocations.
Run54 still reached 300 s / 590 path attempts, no crash or parsed misses.

YUY2 revealed the EA Sports image beneath malformed legal triangles. NV097
primitive codes were one too low: triangle lists were treated as strips.
Corrected codes, separate quad groups and quad-strip assembly. Added the
common src-alpha / inverse-src-alpha blend and diffuse modulation for UI
fades; trace confirmed the legal overlay used diffuse 007F7F7F with alpha
zero and blend 0302/0303, while movie draws disabled blending.
**Run55 window012 shows a complete, unobscured EA Sports intro frame.**
Run55: 300 s / 590 paths, no crash; final periodic report 5,817 draws /
11,632 triangles. USB reaches SET_CONFIGURATION, then misses XID attach
callback **0x005E5684** once. No guest input reports yet. Movie completion
and menus remain unverified; it holds on the EA Sports frame.

Windows keyboard fallback is now in the input layer, merged with physical
port-zero input. Enter=Start, Z=A, X=B, A=X, S=Y, arrows=D-pad; focus loss
releases held keys. Generic default is off; title main enables it by default.
Native test covers packet edges/repeat, focus release, axes, capability/
vibration fallback and physical-pad merge. Optional `[PAD]` report tracing
distinguishes host key handling from guest USB consumption. Sample and stack
tools now accept `--pid`, use correct Win64 HANDLE prototypes, and leave an
attached run alive. Stack scanning also uses the actual module extent and
clips reads to the committed stack region.

## 2026-10-10 — XID callback cycle and audio DMA/output audit (cycle56)

Added attach 0x5E5684 and adjacent detach 0x5E5065 from the XPP table at
0x5E304C (17 seeds). Full disasm56: 232.18 s, 37,748 database functions;
boundary helper preserved 0x44302D and SEH. Lift56: **37,746/37,746,
zero failures**, 4,272,500 lines / 38 files. Release build succeeded, followed
by an incremental build of the final output change. All five CMake-built
input, GPU, OHCI, APU DMA and APU output regression executables passed.

Run55a's 90 s audio trace showed seven debug voices with BA/CBO/EBO all
zero. **Correction from run56:** those fields were never populated in the
debug structure, and its active flags were historical. The zero values did
not prove frozen cursors. The VP/SGE/sample helpers read physical addresses directly from low
RAM, despite DirectSound allocating those structures in the contiguous
window. Adopted the reference's physical-window rule via shared
`xbox_ContiguousIsPhysical()`, including streamed ADPCM block copies.
Added bounded `[APUPROGRESS]` cursor and `[APUOUTPUT]` sample/peak tracing.

Windows monitor audit found another defect: it cleared the DSP's accumulated
hardware mix, then submitted 1,024 software samples every eight 32-sample
APU frames. The XAudio2 path now preserves and submits the actual 256-sample
hardware block, adding software voices/test tone as appropriate. Its waveOut
fallback still uses the older path and needs separate validation if selected.
Run56 confirms the real voice tables exist in contiguous backing, while
their low-RAM aliases are empty. Output tracing still reports silent samples.
The cursor trace now reads live voice registers rather than the incomplete
debug structure; its first acceptance run is run57.

New native smoke tests cover real executor primitive assembly/video sampling/
diffuse-alpha fades, an actual OHCI control transfer across separately backed
physical queues and static reply buffers (including HCCA completion), and
APU byte/word/dword physical access and low-RAM fallback. All passed. The
XAudio2 monitor test also passed: the hardware block survives byte-for-byte,
submission is exactly 256 samples, and stopped/muted output is zero. It was
compiled separately while the generated-code build ran. No game code/assets
are in these tests. Regenerated memcpy and all three IDCT bodies still match
the differential-tested sources exactly.

Local container inspection found 107 video chunks (MV0K + 106 MV0F) and
107 audio chunks in `easports.vp6`. The early alternating movie-texture use
counts and later repeated end-card draws are consistent with reaching the
video's final frame while waiting on audio; this is an inference, not yet
proof that the full movie completed correctly.

Reference audit correction: the modern NFS executor uses GL/Vulkan backends
built outside Windows. Its `nv2a_pgraph_d3d11.c` remains a legacy path close
to ours. M5 requires a Windows D3D11 adapter for the modern backend interface;
copying that old D3D11 file is insufficient. Updated PLAYABILITY_PLAN.

Compiler-warning audit correction: twelve of the thirteen C4293 warnings are
in sub_005879C8 (its bytes begin ASCII `SIZE`), and one is in sub_0055E99A,
a false-looking hotpatch prologue inside pointer-table bytes before the real
body at 0x55E9C0. Neither has recorded callers. The latter's translated SHR
immediate 0x55 also exposes a latent immediate-count masking limitation, but
these data candidates have not been observed executing. Warnings remain
visible; do not attribute all thirteen to sub_005879C8 or suppress them.

## 2026-10-10 — Second intro movie and controller class requests (run56/57)

Run56 was stopped at 323 s: 591 parsed path attempts, no crash. XID attach
now executes and sends the vendor descriptor/capability requests. It also
sends class GET_REPORT (A1 01, 0100, 20 bytes) and SET_REPORT (21 09, 0200,
6 bytes). The old model stalled GET_REPORT. Added the reference's GET_REPORT
reply and explicit successful SET_REPORT handling (rumble data remains discarded).
The actual OHCI TD-walk regression passes both transactions without a stall.

After the owned-window Start test, run56 opens `frontend/movies/mvpintro.vp6`;
this is progress beyond the EA movie, not a complete-playback claim. The
window later goes black and dispatch misses expose 0x00432FC0 and 0x000E3020.
The former is a five-byte tail jump to 0x0042DD0C; the latter is a separate
111-instruction SEH body ending after ret 4 at 0x000E31B8. Both have no
unsupported instructions/dead arms and are now seeded (19 seeds).

For rapid acceptance, two translated preflight bodies are locally installed
in ignored `src/recomp/gen/recomp_9999.c`, with entries in the generated
dispatch table. This chunk is automatically removed by the next full lift;
do not distribute it. Run57 is testing the preflight build, controller class
requests and real-register APU trace. The two new seeds still need their
full disassembly/boundary/lift/build cycle after this preflight.

Run57 reached the MVP 06 title screen (window028) and 2,205 path attempts
without a crash. Its dialog text renders as white blocks. The two new misses
were 0x5E4C1F (40-instruction report callback) and 0x3DACEB (87-instruction
worker with all 23 switch arms live); adjacent report-copy callback 0x5E4C71
(17 instructions) was also added. Real-register tracing confirms voices244–246
advance, correcting the earlier interpretation of unpopulated debug fields.

Their preflight run58 reached 167 s / 2,214 paths, with no crash or parsed
misses. Start/A messages were posted to the owned framebuffer HWND, but no
guest report edge was observed. Run58a enabled verbose GPU tracing and crashed
in the movie path (guest fault FFFFFFFC, near return 0x49771F), after a new
callback miss at 0x42C31C. That is `xor eax,eax; ret`, also the fallthrough
after the call at 0x42C317: a legitimate secondary entry, not a switch arm.
The miss would normally return the same EAX=0, so it is not established as
the crash's cause. Output tracing did record 47,708 nonzero samples, peak
11,567, in its final interval; audible playback/pacing is still unverified.

Cycle58 disassembly finished in 279.91 s for 22 seeds; lifting was deferred
when 0x42C31C appeared. There are now 23 seeds. Run59 tests all six preflight
bodies, with keyboard-edge tracing and ASLR-correct crash diagnostics. The
native crash filter previously assumed the preferred 0x140000000 base and
therefore discarded return addresses after relocation. Boot now records the
actual executable base, and crashes record RVAs. Cycle59 disassembly is running
before the required boundary patch/lift/build replaces the preflight chunk.

## 2026-10-10 — OHCI completion ownership (run59/60, cycle59)

Run59 was stopped at **403.61 s / 590 path attempts**, with zero parsed
dispatch misses and no crash. Host Enter/Z down/up edges were logged, but no
changed USB reports followed. Stack sampling found the main thread waiting
inside XInput initialization (sub_005E4082/sub_005E50A0).

Reference comparison identified a concrete completion-queue defect:
`ohci_publish_done` replaced HccaDoneHead even while WDH was set. A native
regression failed with "an unacknowledged HCCA done list was overwritten".
Adopted the reference's held-queue/flush behavior: HCCA remains driver-owned
until acknowledgment, later batches chain newest-first in HcDoneHead, ACK
preserves that held queue, and the bus thread defers new TD walks while WDH
is pending. Extended the regression through three batches, ACK, flush and a
second ACK; it passes, along with the actual XID class-control TD transfers.
Incremental Release build succeeded. Run60 records Start (0010) and A (255)
USB report down/up edges and opens `mvpintro.vp6`; game-level menu input still
needs verification. This run passes the earlier XInput wait, but a single
run does not establish that all timing-dependent waits are resolved.

Cycle59 disassembly completed in **247.86 s**, followed by the mandatory
boundary helper (memcpy exclusive end **0x0044302D**, SEH entry). Database:
37,754 database functions; full lift completed in **169.3 s** after data
loading: **37,752/37,752, zero failures**, 4,273,165 C lines / 38 source files.
The local preflight chunk was removed automatically. Release build succeeded
at **20:03:42**, about **28.5 min** after launch, followed by the final USB
runtime rebuild at 20:03:49. All five fresh native regression executables pass.

Run60 was stopped at **347.62 s / 2,214 paths**, with no crash or parsed
dispatch misses. Changing EA intro frames and the MVP title/dialog are
captured. Input started working but later stopped: the log explicitly says
OHCI stopped after 200 repetitions of WDH. Reference audit identified a
second defect: the guard compared status values without checking whether
the driver acknowledged them between deliveries. A native test of the real
controller loop reproduced the cutoff at **201/250 acknowledged IRQs**.
The guard now tracks an atomic status-acknowledgment sequence. That test
passes 250 deliveries; a second case leaving WDH unacknowledged still trips
the guard at 201. The final runtime library must be rebuilt after the full
generated-code build, because that library compiled before this change.

Verbose draw tracing identifies the title's white-block glyphs as unsupported
**P8 (0x0B), 256x128** texture pages, including VA 0x81046380. Live texture
register 0x1B20 contains 0x0104E3C0; its actual palette at VA 0x8104E3C0 is
valid ARGB with transparent entries. Rendering captured indices through that
palette produces a readable font atlas. Added P8 sampling and palette-method
decoding (64-byte-aligned DMA offset, 256/128/64/32 entries). The real executor's
native regression passes index lookup, palette alpha, length bounds and a
textured quad whose transparent texels preserve the background. Game-level
readable text awaits the post-build run; do not claim it verified yet.

Post-lift audit confirmed regenerated memcpy and all three IDCT bodies match
the differential-tested sources exactly. All six post-run56 seeds have normal
generated bodies with no unsupported instructions or dead arms; the worker's
23 switch targets remain live. The 0x44302D boundary and preflight-chunk removal
were independently checked. Five native input/GPU/OHCI/APU checks pass, with
the newest OHCI test covering both continuous acknowledged interrupts and
the retained stuck-source guard.

Run61 began at **20:03:49**, testing the full regenerated executable plus P8
and both OHCI repairs. Live native snapshots pass **376 acknowledged status
writes** with advancing frames, beyond the old 201-delivery cutoff. Start
down/up produces USB report edges and is followed by opening `mvpintro.vp6`.
At this point there are no parsed dispatch misses, crashes or controller-stop
messages. Readable dialog/menu acceptance is still in progress.

## 2026-10-10 — Readable title and completed-thread polling (run61/62)

Run61 was stopped at **600.35 s / 2,214 path attempts**, with **zero dispatch
misses, crashes or controller stops**. Native snapshots passed **5,428 OHCI
status acknowledgments**, with frame progress. Window053 verifies the P8
repair in game: the save-check message is readable, with "Press START" behind
it. Start/A down/up edges reached the guest; Start preceded mvpintro.vp6.

The main thread spins in XAPI GetExitCodeThread, **sub_00320980**, called by
0x42082C/0x420854 while the save dialog remains visible. The wrapper references
a thread object, reads SignalState at +4 and ExitStatus at +0x120, then releases
the object. The bridge returned success and **NULL**, making the wrapper read
page zero and report STILL_ACTIVE forever. Direct inspection of guest token
**0x48000014** resolved host handle **0x7BC**, thread **8432**, already exited
with **status 1**. This is a specific object-semantics defect, not evidence for
the speculative EA GIL race.

Added guest thread-object views with real host exit/signal state, a stable view
per open handle, reference counts and a duplicated handle to retain the object
after NtClose. Object-taking waits and base-priority exports resolve these
views. Unknown/invalid handles fail rather than claiming success with NULL.
The new native regression passes live-thread polling, 1,000 repeated references,
an already-exited thread's first reference, invalid handles, and close/reference
lifetime. Incremental Release build succeeded; **run62 began at 20:14:59**.
Save-dialog completion and menus are under runtime acceptance.

Run62 progressed past the old save-check poll, opened **MENU10.ASF**, spawned
additional workers, and read **CACHE/LocalCache00.bin**. Its final APU interval
contained **70,011 nonzero samples, peak 5,273**; this is sample evidence, not
audible validation. At about **216 s / 2,231 path attempts**, it logged one
dispatch miss, **0x0007F052**, then crashed reading guest **0xAC8BFFFD** inside
sub_00089520. The miss's contribution to the crash still requires a clean run.

0x7F052 is inside **sub_0007F040**, whose old end 0x7F05A made the lifter emit
an indirect tail call rather than a switch. Its table at **0x7F078** has 28
entries and five targets: 0x7F052/5A/62/67/6F. Extending the exclusive boundary
to **0x7F077** recovers all 15 non-overlapping instructions, five live target
labels, and the final RET 4 at 0x7F074. Added this third range to the boundary
helper, without introducing an internal-arm seed. A local x86 differential
harness passes **4,099 cases**, including all table indexes, default inputs,
unsigned extreme values, return values, stack, callee-saved registers and DF.
Replaced only this body's generated code in chunk3; **Release build63** is
running. A future full lift retains the repair through the boundary helper.

Build63 succeeded. Run63 crashed during USB initialization, roughly **40 s /
590 paths**, with no dispatch miss. Fault RVA 0x2F17708 maps to **sub_005E9B62**;
the driver followed endpoint head **0xFFFFFFF0**, added the 0x80000000 alias,
and read guest **0x7FFFFFF8**. Run64 tested the same executable and passed this
site, the save check and the prior frontend switch failure. It was stopped at
**312.02 s / 2,567 paths**, with **zero dispatch misses or crashes**. Menu assets
continue loading and GPU batches continue, but window060 onward is black.
Many textured draws carry diffuse **0x007F7F7F**; the current sampler always
multiplies texture alpha by diffuse alpha, without interpreting the programmed
combiners. This is the next rendering lead; a combiner repair is not yet proven.

Reference comparison identified a separate kernel queue contract violation:
KeInsertQueueDpc always accepted the same pending KDPC again, and its ring had
no producer synchronization. Factored the queue into a locked module, tracking
**KDPC.Inserted at +2**, rejecting duplicates, implementing removal, preserving
FIFO/arguments, reclaiming cancelled capacity immediately and clearing Inserted
before callbacks. The new native regression passes **1,000 duplicate attempts,
4,000 concurrent insertions**, cancellation, wrap/capacity and argument checks.
Refreshed the thread-object test to cover explicit add-reference across close;
it passes. This establishes queue correctness, not the cause of run63's crash.
**Release build65 succeeded**; **run65 (PID11388) began 20:30:37** for acceptance.
An ignored MAP/ASLR combiner snapshot helper records live words for comparing
the movie and black frontend before changing the alpha/fade path.

Run65 now supplies stronger visual/input evidence. **Window056** shows
Sign In/Advance controls over the title. A two-second A press did not reach a
guest report during the slow frontend; a ten-second press did, with down/up
edges, and **window110** shows Gators/Longhorns plus Back/Random/Advance and
the menu track "Get Comfortable" / Wide Eyed. Another long A press transitions
to the stars background. Thus a game-level frontend response is verified;
full menu navigation and exhibition remain outstanding.

No combiner change was made. Live words still select diffuse*texture alpha,
so the earlier black capture alone does not justify disabling that modulation
or the already-verified movie fades. Later draws include offscreen geometry;
trace transforms/transitions and pacing next. OHCI snapshots advance slowly
past 2,611 acknowledgments; a one-second unchanged sample was not a permanent
controller stop. **At 550 s / 3,071 path attempts**, run65 remains live with
zero parsed dispatch misses/crashes. Audible playback is still unverified.

## 2026-10-10 — Completed frames, DMA command ownership, and IRQL (runs66–69)

Run65 was stopped at about **906 s / 4,019 path attempts**, with zero dispatch
misses/crashes. Repeated window captures showed the window following intermediate
clears and unfinished surfaces. Ported the reference's copy-at-flip presentation
path, retaining the last drawn target and its pitch. A shared/exclusive lock
protects the published snapshot; GDI and BMP capture use a separate copy.
The native regression verifies target reuse after a flip, RGB565 conversion,
the debug address pin, and preserving the completed target when the next target
is cleared. **Build66 succeeded.** Run66 shows stable completed frontend captures,
including Sign In/Advance and Gators/Longhorns with Back/Random/Advance.

Run66 was stopped at about **312.6 s / 7,511 paths**, with no crash and **one
dispatch miss at 0x0041FF34**. This address is the existing RET inside
sub_0041FF26 (RtlLeaveCriticalSection wrapper), not a newly discovered function.
Do not seed it blindly. Added guest registers/stack to the failure diagnostic
to identify its caller if it recurs. **Build67 succeeded**, but run67 reproduced
the earlier USB crash at roughly 40 s / 590 paths, zero dispatch misses:
sub_005E9B62 follows a freed endpoint/TD link 0xFFFFFFF0 and reads 0x7FFFFFF8.
The DPC insertion repair alone therefore did not eliminate that fault.

The pushbuffer audit found the original scanner still used previous PUT to
new PUT, stopped at jumps, ignored CALL bodies and skipped wrapped submissions.
It also published GET before executing the referenced commands, permitting
producer reuse while the executor was reading them. Replaced that walk with
persistent physical GET, jump/call/return following, reset detection and partial
method-packet state. GET advances only through consumed work. Ported the
reference's constant-time method inventory index too. A native synthetic-DMA
regression verifies split packets, no replay, calls/returns, both jump forms,
backward wraps, resets, invalid targets and GET ownership. Existing GPU and
framebuffer checks also pass. **Build68 succeeded**; run68 reaches frontend and
team selection with zero dispatch misses/crashes/parser desynchronizations at
the four-minute checkpoint. Complete navigation and audible output remain open.

USB code uses raised IRQL to protect its TD pools, but the runtime only stored
the level in TLS. ISRs/DPCs also entered without any shared exclusion, and
KeSynchronizeExecution ran its callback without a lock. Added a raised-IRQL
gate shared by protected guest regions, DPCs, synchronized callbacks and both
device ISR paths. It defers callbacks until the current protected region ends;
native DMA/render/audio execution remains independent. This is narrower than
a full guest GIL. The native IRQL check passes nested raise/lower, a deferred
contender, and **16,000 concurrent protected updates**. The real OHCI regression
also checks ISR level and passes. Game-level acceptance follows in run69;
the USB crash's elimination is not yet established.

Run68 was stopped at **363.3 s / 8,747 path attempts**, with zero dispatch
misses, crashes or parser desynchronizations. **Release build69 succeeded**
with the IRQL gate; run69 begins at **21:01:39** for acceptance.

Run69 was stopped at **318.1 s / 7,850 path attempts**, with zero dispatch
misses, crashes, parser desynchronizations or IRQL warnings. Start/A/Left
reports reach the guest. Completed frontend/team screens remain visible;
exhibition loading is not established. Live register capture confirms
**transform execution mode 6 (program)** and repeated vertex-program/constant
uploads, which the current executor ignores. Vertex-program interpretation is
the next concrete rendering gap. The native gate is validated and this run is
clean, but one run does not establish elimination of the intermittent USB fault.

Review caught a survey-only integration issue: mirroring GET before a read-only
survey would skip its submitted segment. The bus now advances GET after that
survey too, while the parser remains read-only without RECOMP_PB_EXEC. Added
survey-only regression coverage; it passes. **Release build70 succeeded.**
Run70 (**PID11308**) begins at **21:08:16**, using native file handles for logs
via the ignored `build/start_run.py` helper. It crashes during USB init at
590 paths, zero dispatch misses/parser desync, reading **0x7FFFFF1E** in
sub_005E9B62 through **sub_005E9D02's retired-endpoint processing**. Thus raised
IRQL exclusion also does not eliminate this fault. Trace that retirement path
and frame timing next, before treating the descriptor error as a done-queue bug.

## 2026-10-10 — USB lifetime, dispatcher state, and CPU scheduling (71–76)

Build71 adds temporary cancellation tracing at guest 0x5E9D2B/0x5E9D95.
Run71 crashes during USB init (590 paths, zero dispatch misses): retired
request **0x00F7EDA8** still points into the main thread's reused stack;
its endpoint becomes 0x80000000. CC=F is intentional cancellation, not proof
of a bad hardware completion. The generated hooks are diagnostic only and
disappear on a full lift; recomp_manual.c implements RECOMP_USB_CANCEL_TRACE.

HalGetInterruptVector returned IRQ1 as both vector and IRQL1, below
DISPATCH_LEVEL2. Corrected it to **vector=0x30+IRQ, IRQL=27-IRQ**, following
[Cxbx's mapping](https://raw.githubusercontent.com/Cxbx-Reloaded/Cxbx-Reloaded/master/src/core/kernel/init/CxbxKrnl.h)
and the local Mercenaries reference. USB uses vector0x31/IRQL26, GPU vector0x33;
private KINTERRUPT stores IRQL at +12. ISR paths and KeSynchronizeExecution
use it; disconnect clears the private vector table. Native mapping/nesting/
concurrency and OHCI checks pass. Build72 succeeds, but run72 still crashes
at 590 paths with a stale stack request.

The XDK initializes/clears dispatcher headers inline. Cached Windows events
retained a prior signal across stack reuse. Events and semaphores now use guest
SignalState, with a condition variable and bounded polling for inline stores.
Event type0/1,size4 and semaphore type5,size5 are recognized; set/reset return
previous state. Mixed WaitAll checks readiness without partial consumption.
Native checks cover 1,000 reuses, events, semaphore limits and mixed waits.
Build73 succeeds; run73 crashes at 590 paths after two valid cancellations,
proving this repair insufficient.

Build74 adds a passive wait-return barrier: a signalled waiter cannot resume
guest stack cleanup while a signalling raised-IRQL callback remains active.
Native wait/IRQL tests pass. **Run74 ends at 612.9 s / 37,308 path attempts,
zero dispatch misses/crashes**, with Start/A edges and visible title controls
and Gators/Longhorns selection. Window124 shows the team screen with readable
music-track text. Path attempts include repeated optional missing XSH probes;
they are not unique loaded assets.

Build75 ports the reference CPU vertex-program interpreter: program/constant
uploads, MAC+ILU paired issue, masks/swizzles/negation, relative constants,
diffuse and UV outputs. Program-mode draws use per-draw vertex caching and
perspective-correct UVs. Native tests upload a model-space quad through real
methods, checking transformed pixels, cache invalidation, simultaneous reads,
masked writes, relative constants and bounds. Existing P8 alpha, YUV,
primitives, fades and flip checks pass. Depth, full combiners, fixed-function
model transforms and near-plane clipping remain incomplete. Dispatcher checks
also cover pure-guest pulses, duplicate waits and abandoned native mutants.
Mixed host/guest pulse completion and full APC semantics remain unsupported.
Never acquire a native mutant on the pulsing thread.

**Run75 crashes again at 590 paths, zero dispatch misses.** Several valid
retired requests precede the same stack request being reused for video data.
Run74 therefore does not establish race elimination, and VP runtime acceptance
has not occurred.

Build76 is rebuilding with a reference-shaped single guest CPU lock. Lifted
entry points, workers, APCs and ISR/DPC/synchronized callbacks enter it.
Passive kernel calls park/restore nested depth; time queries/IRQL transitions
retain it, as do all kernel calls from raised guest regions. A contended IRQL
gate releases the CPU before waiting to avoid lock-order deadlock. FIFO tickets
and cooperative safe points prevent poll starvation. The translator emits
optional RECOMP_PREEMPT at entries/backward branch targets; the idempotent
scheduler_hints module instruments existing chunks too: **52,137 points in
38 chunks**. The title enables RECOMP_GUEST_SCHEDULER; RECOMP_GIL=0 disables
the lock for diagnosis. KeGetCurrentIrql returns TLS state; the SynchLevel
bridge raises to28 as in the reference. Native CPU exclusion, nested waits,
poll preemption, ISR lock order and raised-IRQL tests pass. Toolkit:
**236 passed, 24 skipped, 44 subtests passed**. Build/runtime acceptance pending.

The full generated rebuild reports C4293 oversized shifts inside sub_005879C8,
near outs/rcl and suspicious data-like constants. Inspect reachability and the
original bytes before changing boundaries; a zero-failure lift is not proof
that every decoded function is valid. This is a separate codegen/decode audit
item, not a diagnosed cause of the USB crash.

**Build76 succeeds at 22:00.** Run76 is an invalid long-run acceptance sample:
RECOMP_WATCHDOG_SECS=10 intentionally calls _exit(3), rather than only reporting
registers, and ended the process at its cutoff (302 paths). Run77 uses the same
exe without the watchdog and **reproduces USB's stale request crash at 590 paths,
zero dispatch misses** after two valid cancellations. Thus a serialized lifted
CPU alone also does not eliminate the fault. Do not label the USB race fixed.
Build78 adds optional RECOMP_WAIT_TRACE (caller/header/state/status/duration)
and succeeds; run78 starts at 22:07:53 for diagnosis. OHCI still exposes a
read-count register clock inconsistent with its HCCA counter and pauses its
hardware loop when MIE is masked. Those are concrete hardware-model leads.
Its timer increments the HCCA frame before DMA/interrupt work; retire versus
WDH delivery order also needs auditing. Do not add a CPU lock around all DMA:
guest code at raised IRQL can legitimately wait for hardware progress.

An early VP snapshot (`build/vp77a.txt`) decodes the actual eight-instruction
XDK program (DP4 rows, MUL, paired MOV/RCC, MOV, viewport MAD), confirming
uploads now reach the interpreter. Reserved c58/c59 were still zero during
initialization; inspect them at actual draw time before diagnosing a viewport
constant bug. Game-level VP output remains unverified.

### Runs 79–82: viewport, OHCI time, KPCR publication and reference audit

Build79 aliases SET_VIEWPORT_SCALE/OFFSET to VP constants 58/59, as xemu and
the current Burnout 3 toolkit do. A draw-time snapshot had confirmed these
constants stayed zero, collapsing the XDK viewport tail. Native tests execute
the actual MUL/RCC/MAD tail, check expected pixels, alias writes in both
directions, cache refresh and the unchanged constant-upload pointer. Run79
restores readable title/team screens (269.7 s / 14,447 path attempts, zero
dispatch misses/crashes); USB cancellations still occur.

Build80 removes the read-count FmNumber clock, shares elapsed-millisecond
time with HCCA, runs DMA while MIE is masked and publishes the next frame
after DMA/done-queue work. The controller tick is 4 ms. Native OHCI tests
cover masked DMA, pending IRQ delivery after unmask, wrap/FNO and invariant
time across repeated MMIO reads. Existing acknowledgement/stuck-source checks
pass. Run80: 269.8 s / 14,441 paths, no miss/crash, 16 cancellation trace lines.
FmRemaining still uses a read-count approximation; do not claim full OHCI
timing accuracy. QEMU's hcd-ohci.c supports the DMA/frame/IRQ ordering.

Build81 publishes each IRQL change as one byte to the current guest TIB at
fs:[0x24]. XDK inline reads of this byte had always seen zero despite the
native TLS IRQL. Main and newly allocated thread TIBs start at PASSIVE_LEVEL.
Native tests cover nested levels, separate thread TIBs, adjacent-byte integrity
and 16,000 updates, alongside scheduler exclusion/lock-order checks. The APU
also requests timeBeginPeriod(1), balanced by timeEndPeriod at shutdown,
following Mercenaries. Output now reaches 96,000 frames per two seconds,
nonzero samples and typically 0–2 dropped blocks, versus roughly 40–50 before.
Audible correctness is unverified. Run81: 254.9 s / 17,807 path attempts, zero
dispatch misses, crashes or USB cancellations. Both changes landed together;
do not attribute the intermittent race improvement uniquely to either one.

Build82 adds RECOMP_FRAME_TRACE=<flag path>: create the flag after reaching
the desired screen to log draw/texture/position/color state until the next
flip. Run82 repeats the clean USB result so far. Its team-screen trace shows
the actual 0x00011102 combiner, RGB scale-two output and stage program 1.
The old texel-times-diffuse approximation misses this scale. Many absent
widgets also arrive outside the viewport or with zero alpha; combiners alone
are not yet a diagnosis of the incomplete central frontend. No exhibition
has been completed. Preserve the P8, YUV, fade, physical-memory and VP tests
while upgrading the renderer.

The user's requested reference comparison used these primary repositories:

- NFSU2: reference/nfsu2-sw at 6e19726414e829d808f7acb0258c0902f04a923a,
  https://github.com/antoxa2584x/nfsu2-sw. Its FIFO guest CPU lock/TLS register
  model matches ours closely. Our build76 already follows its passive-call
  release and safe-point pattern; the lock alone did not fix run77. Its OHCI
  retains approximations, so hardware changes were checked against QEMU.
  Modern GL/Vulkan backend interfaces are useful for a future Windows backend;
  its older Windows D3D11 executor is not equivalent to that modern interface.
- Burnout 3: https://github.com/sp00nznet/burnout3/blob/main/docs/boot.md,
  current toolkit https://github.com/sp00nznet/xboxrecomp. Same XDK 5849;
  fixes 16/18 describe KPCR publication and viewport aliases, now applied.
  Fixes 22/23 identify register combiners, four texture stages and all sixteen
  immediate attributes as real renderer requirements. Current boot docs use
  the external toolkit; the older in-tree runtime and CLAUDE.md are stale.
  The reference still reports rendering and persistent-profile limitations.
- Mercenaries: reference/mercenaries-re at
  f513ae4e041401b7b032410016940971488d523b,
  https://github.com/KraftMacAndChee/Mercenaries-Recompiled. Windows D3D11
  surfaces/depth/presentation and balanced audio timer setup are useful.
  Its global-register, synchronous guest-thread/deferred-IRQ architecture
  differs from our TLS workers; do not copy its dispatcher wholesale.

Review-only current upstream combiner/executor sources are saved under the
ignored build/reference-toolkit directory. Their presence is not integration
or runtime acceptance. xemu's pgraph.c and glsl/psh.c remain primary references
for viewport aliases and combiner encodings.

### Build83: four texture stages and real register combiners

Ported the current upstream nv2a_combiner.c/.h with the NV097 method state,
four independent 2D/projective texture stages, interpolated diffuse/specular,
alpha testing and all sixteen immediate attributes. VP output retains oD1,
oFog and oT1–oT3. Immediate 4UB uses register-order RGBA, whereas array
D3DCOLOR still uses ARGB. Existing P8 palette lengths, YUV, physical-offset
resolution, fades and framebuffer selection remain in the shared sampler.
Cube/volume/bump modes, fog evaluation, depth and near-plane clipping are
still incomplete; this is not a complete hardware renderer.

Native pixel tests pass for the captured frontend RGB-times-two/alpha-times-one
program, alpha rejection, a diffuse gradient and a four-stage immediate quad
with distinct UV sets. Its correct output is red; reusing stage0 UVs would
produce black, and ignoring the other stages would produce white. Existing
VP/viewport/P8/YUV/fade/topology/flip checks still pass. The Release build
succeeds. Run83 reaches 17,171 path attempts with zero dispatch misses,
crashes or USB cancellation traces before deliberate termination. Legal and
team screens have corrected brightness. Central menu content remains
incomplete, and exhibition is unverified. Start/A/B/X, d-pad and analog-stick
reports reach the guest; this does not establish full navigation correctness.

### Build84 in progress: LAHF and unordered float flags

The requested Burnout comparison exposed 586 generated LAHF no-ops. MVP uses
both ucomiss/ucomisd and fucomip followed by LAHF/test AH,0x44, so guards read
unrelated stale AH. Ported upstream's materialization of tracked SF/ZF/PF/CF
into AH, preserving the rest of EAX and fixed bit1. SSE/x87 unordered branches
now use the actual ZF/PF/CF triple for NaNs. AF is still unmodeled in the
general LAHF path; do not claim full EFLAGS fidelity.

A new native test compares generated LAHF and eight branch predicates with
actual x86 UCOMISD/FUCOMIP flags over all 81 pairs of signed zero, finite,
subnormal, infinity and NaN inputs. MSVC x64 executes tiny instruction
sequences in private executable pages because inline assembly is unavailable;
GCC/Clang use inline assembly. The test passes. Toolkit suite: 237 passed,
24 skipped, 44 subtests. Full regeneration/rebuild and game acceptance are
pending; do not label LAHF as the diagnosed frontend cause yet. The boundary
helper confirms memcpy's exclusive end remains 0x0044302D before this lift.

Lift84 completes: 37,752/37,752 functions, zero failures, all 586 LAHF sites
materialized (500 UCOMISS, 83 FUCOMPI, one ADD, two OR), zero remaining LAHF
no-ops. Future lifts now emit 53,202 entry/backedge scheduler points; the prior
postprocessor had inserted 52,137. The executable bodies of memcpy, all three
IDCT entries and the frontend switch remain identical to their differential-
tested versions after excluding scheduler hints and blank lines. The first
identity check rejected newly emitted loop hints; inspection found only those
hints and a trailing blank-line difference, and the check now normalizes them.
The USB cancellation hooks were restored after regeneration. Build84 is
compiling all 38 chunks. Run83's final measured duration was 563.0 s, with
17,171 path attempts and zero misses/crashes/cancellation traces.

The full build caught one arithmetic LAHF site referencing an undeclared CF
temporary (sub_00079F62). LAHF had not been included in the translator's
carry-consumer analysis. Added that analysis and a regression check; the
function is being regenerated while the other chunks continue compiling.
This is a code-generation completeness repair, not a reason to truncate or
blindly reseed that suspicious internal decode. General AF remains partial.

The arithmetic follow-up also uses width-correct SF and low-byte PF snapshots
for LAHF and computes AF for CMP/ADD/SUB/INC/DEC. The native test now checks
the entire AH byte for all 65,536 ADD8 pairs, in addition to float/NaN checks;
it passes. Other uncommon flag producers remain an accuracy audit item.
The regenerated sub_00079F62 and all three integer LAHF statements were
applied to chunk3. The first build continues compiling the other chunks;
after it completes, rebuild chunk3 and link before running.

Updated suite: 238 passed, 24 skipped, 44 subtests. Additional native combiner
checks pass for MSB/LSB mux selection, simultaneous RGB/alpha reads, final E*F
and shared/per-stage constants. The four independent texture coordinates also
pass through actual oT0–oT3 VP outputs, and alpha comparison is eight-bit.

Added optional RECOMP_STUB_TRACE to generated unresolved direct-call bodies
(294 targets). It prints [UNLIFTED] once per executed target with guest ESP,
without changing return-stack cleanup. A clean indirect dispatch log alone
cannot establish that none of these silent direct stubs ran. The generator
preserves this diagnostic on future lifts; run84 should enable it.

Reference follow-up: upstream's x87 arithmetic also wraps results in
RECOMP_FP_PC, honoring PC=24. Our arithmetic is still unrounded double despite
tracking the guest control word. Burnout boot fix17 describes a float-versus-
double draw-list loop caused by that difference. This is a concrete later
audit lead if the LAHF run still stalls; it has not been applied or diagnosed
in MVP. Keep it separate until run84 establishes the effect of the flag repair.

Build84's follow-up succeeds at 23:19 after recompiling repaired chunk3 and
linking. The generated direct-stub diagnostic also compiles. Run84 is starting
with RECOMP_STUB_TRACE=1 and USB cancellation tracing, without a watchdog.

**Run84 frontend breakthrough:** EA Sports and baseball intro footage render,
followed by the title screen. After Start, the complete main menu now renders
(Play Now/Game Modes/Creation Zone/Options/Manage Rosters/Videos/Extras),
then Play Now opens the complete Florida-versus-Texas selector with logos,
ratings and controller assignment. Before LAHF it showed only team names and
bottom controls. Captures: build/mainmenu84.png and build/teams84.png.
At 23:23, no indirect dispatch misses, direct-stub hits, crashes or USB
cancellation traces; nonzero APU output reaches 96,000 frames/two seconds.
This strongly connects the missing frontend to stale AH/float guards.
Exhibition advancement is being tested; no playable claim yet.

### Run85 — exhibition loading exposes another truncated switch

Run84's final Start press advances from jersey selection to exhibition
loading: crowd/overlay audio banks, stadium/dishday.big and player textures
open. It ends at 23:28:48 (567.6 s / 2,874 path attempts), with one unresolved
0x0007AE92 followed by an AA55AA55 pointer crash. Zero direct-stub hits and
zero USB cancellation traces. The final capture shows the difficulty/loading
transition; no stadium frame yet. A presses had moved through team/jersey
selection; Start triggers this advancement.

Original-byte inspection identifies 0x7AE92 as the first of four arms in
sub_0007AE80, whose database end was 0x7AE92. It extracts three-bit fields
from [ecx+0xC0], shifting by 0/6/12/18 for indexes 0..3; defaults return zero.
The shared ret 4 ends at exclusive 0x7AECB, before the table at 0x7AECC.
No new seed is added. tools_patch_boundaries.py now preserves this range,
alongside memcpy 0x44302D, SEH 0x94C80 and frontend switch 0x7F077.

The complete lift has 20 non-overlapping instructions and all four arms live.
A native x86/lifted harness passes 45,056 comparisons over random bitfields,
all arms, ordinary/high/negative defaults, return value, stack cleanup,
callee-saved registers and DF. The body is installed in chunk3; build85 is
compiling. A dispatch miss at a ret-4 arm also omits argument cleanup, so
diagnose this decode failure before interpreting the later pointer crash.
The old ignored combiner_snapshot helper reads unhandled storage and is
stale after build83; draw-time FTRACE is the current combiner diagnostic.

Build85 succeeds at 23:33. Run85 PID26812 starts at 23:33:56 with direct-stub
and USB cancellation tracing, no watchdog. The installed player-switch body
matches the native-tested lift; prior memcpy/IDCT/frontend identity and
boundary checks still pass.

Run85 reaches 266.1 s / 3,067 path attempts before a new crash. It passes
the player-bitfield switch and prior AA55AA55 failure, loading stadium,
player and SIGP0000/SIGP0019 signature-animation assets. New indirect misses:
0x171D40 once and 0x2C8AD0 twice. Direct-stub tracing catches 0x288EB0 once,
immediately before the FFFFFFFE pointer read. No USB cancellation traces.
This remains a decode/dispatch frontier; do not infer a renderer or worker
race cause from the later poisoned pointer.

### Cycle86 — three measured exhibition-load entries

Original-byte/ownership inspection confirms three standalone bodies:
0x171D40 (stack frame, prior function ends 0x171D1B), 0x2C8AD0 (parameter
setup after 0x2C8A80 ends at the new entry), and 0x288EB0 (stack-frame/ret-4
routine after 0x288DF0 ends 0x288E53). The latter was a generated direct tail
stub; its trace closes the earlier dispatch-only blind spot. Added all three
to the seed file, now 26 entries, and started full disasm86. No interior
switch targets are seeded. Boundary helper also rejects accidental seeds
inside the protected memcpy/frontend/player-switch ranges.

Disasm86 finishes in 195.6 s. After reapplying all four boundaries, the
database adds exactly those three entries, removes none and changes no old
ranges. New exclusive ends: 0x17217A, 0x2C8CC6 and 0x2894C1. Full lift86 is
running. A native-x86 differential harness for sub_002C8AD0 passes 8,192
cases: all optional-pointer combinations, full object and unchanged source
bytes, return/scratch values, saved registers, stack cleanup and DF. Its
self-contained original bytes are relocated to a free low page (all branch
targets stay internal and memory operands are register-relative); this
avoids a test-process allocation collision at the original 0x2C0000 range.

Full lift86 succeeds: 37,755/37,755 translated functions, zero failures,
4,290,980 C lines in 38 chunks. Database total is 37,757 because two owned
aliases are excluded by the translator. Unresolved direct targets decrease
from 294 to 293; no additional unsupported-instruction category appears.
All three recovered bodies have no dead/unsupported arms, including
0x288EB0's four-entry/two-target switch. Native-tested player/parameter bodies
match generated C exactly; memcpy/IDCT/frontend checks still pass. Restored
the two generated USB cancellation diagnostic hooks and started full build86.

Read-only reference follow-up during compilation: Burnout fix21 covers scalar
SSE compares and packed sqrt/rsqrt/rcp. Our lifter still has gaps there, but
the current MVP generated code contains no corresponding compare no-op sites
and no packed "4-lane model TODO" sites. Keep that as a toolkit accuracy
audit item, not a diagnosed MVP frontier. PC=24 arithmetic rounding remains
the concrete CPU lead if exhibition later loops with clean dispatch/stub logs.

Input-mapping audit during build86 explains the visible P3 assignment:
[XQEMU's original mapping documentation](https://xqemu.com/input/#about-xid-and-qemu-usb)
maps front players 1/2/3/4 to root ports 3/4/1/2. Current
[xemu input code](https://github.com/xemu-project/xemu/blob/master/ui/xemu-input.c)
still uses that map, and its Xbox board exposes four ports per OHCI controller.
Our OHCI_PORTS=2 assumption and RECOMP_USB_PORT clamp to 0..1 cannot expose
player 1. NFSU2 already supports an NDP/port-selection diagnostic. This is a
measured later controls repair, separate from the cycle86 dispatch retry.
Audit all powered-port initialization and periodic-DMA gating when expanding
the root hub; changing only the insertion port would strand report transfers.

Build86 succeeds at 00:11 on 2026-10-11. Existing C4293 oversized-shift
warnings in sub_005879C8 remain; no build errors. Run86 is starting with
direct-stub/USB tracing and a dormant one-frame FTRACE flag, no watchdog.

Run86 intro acceptance: without any posted key, EA Sports transitions to
baseball footage in mvpintro.vp6. Successive window captures show different
batters/scenes (including window037); this verifies actual advancing video,
not only the static logo. No indirect misses, direct-stub hits, crashes or
USB cancellation traces at this checkpoint. The second movie is still
running; its natural completion is being checked before Play Now.

The movie progresses from historical baseball footage into prerecorded
gameplay footage with zero keyboard events. Do not label these images a live
stadium frame: only 591 path attempts and the intro asset is still active.
Its MVhd header reports 2,213 frames, time base 32767/983019 (~30 fps,
73.766 s), read using the layout in FFmpeg's electronicarts demuxer. Playback
takes several minutes under this software path; that is a measured pacing
issue for the later timing/performance phase, not a clean-dispatch freeze.
APU output is nonzero with low drops during the movie.

Both intro movies complete naturally in run86, without a single keyboard
event: baseball footage, prerecorded gameplay, the ending MVP/EA logo, then
the legal screen. Dispatch/stub/crash/USB-cancellation counts remain zero.
The full sequence takes roughly ten wall-clock minutes on the software
renderer, so completion is verified while original pacing is not. Proceeding
from the legal/title transition to the exhibition-load acceptance route.

### Cycle87 — later exhibition callbacks, scalar SSE rounding and P1 mapping

Run86 ends at 00:29:59 (1,107.5 s / 3,272 path attempts), after natural movie
completion and the complete team/jersey/difficulty route. Stadium, signature
animation and later .xsh/crowd-stream assets load. New indirect misses:
0x157ED0 nine times and 0x2CC3A0 ten times, then a FFFFFFF4 read with
0x26A4CF at the top of the guest return stack. Zero direct-stub hits or USB
cancellation traces. No live stadium frame yet; the last capture is the
exhibition loading graphic. Diagnose missing dispatch before the later crash.

Original bytes prove 0x157ED0 is a standalone player-parameter setup body,
ret 8 at 0x15805A, exclusive end 0x15805D. 0x2CC3A0 follows the preceding
balanced epilogue/ret at 0x2CC39F, starts its own push ecx and returns at
0x2CC598 (exclusive end 0x2CC599). It was inside an oversized tail-alias
range, not an internal switch arm. Added both seeds (28 total). Disasm87
finishes in 223.08 s: exactly two added entries, none removed, only old-range
change is splitting 0x2CC0A0 at 0x2CC3A0. Reapplied all four helper ranges,
including memcpy's 0x44302D end.

The recovered SSE callback's original-x86 comparison exposes another lifter
defect: CVTSS2SI and CVTTSS2SI both emitted a C cast, silently truncating the
rounded instruction. First measured mismatch uses phase 1.13671875/count125
under nearest rounding. Scalar single/double conversions now use SSE helpers
for rounded versus truncating semantics and integer-indefinite on invalid
inputs; double-register conversion reads the low double lane. This follows
the existing MMX conversion's host MXCSR convention; full guest MXCSR tracking
and unrelated double arithmetic are not implemented by this change.

The actual callback now passes 28,672 comparisons: four rounding modes,
active/state/index early returns, all five valid indexes, sparse point lists,
full object/source bytes and stack/saved registers. The self-contained native
body is relocated to a private RX page; Capstone checks branch ownership and
identifies the only two immutable constant addresses, which are relocated
identically in native/lifted test code. This avoids collisions with native
process allocations. Game data/harness are ignored. A shareable hardware
regression also covers scalar single/double rounded/truncating conversions,
ties, signs, subnormals, overflow, infinities and NaNs. Toolkit suite:
239 passed / 24 skipped / 44 subtests. Full lift87 is running.

The reference-derived input repair expands both OHCI root hubs to four
powered ports, implements port3/4 status requests, accepts zero-based port
0..3 and corrects the arrival trace's port number. MVP defaults to port2
(physical root port3/P1), preserving environment overrides. The real native
controller loop verifies arrival on each of four ports and interrupt ACK;
register tests cover enable/power/reset and simultaneous old-change ACK/new
reset. Existing DMA/done-list/MIE/frame-wrap/IRQ regressions still pass.
Periodic DMA has no hardcoded two-port gate. Game-level P1 acceptance waits
for the new build. The root-port mapping is from the primary xemu/XQEMU
sources linked above; NFSU2's port-selection model was the useful comparison.

Full lift87 succeeds: 37,757/37,757 functions, zero failures, 4,291,175 C lines
in 38 chunks. The same 293 unresolved direct targets and 387 unsupported
instruction comments remain, with no new unsupported category. All 641
scalar conversion sites use the repaired helpers (122 CVTSS2SI, 519 CVTTSS2SI;
no scalar-double conversion site in this title). Native-tested memcpy, IDCT,
frontend/player switches, parameter-copy and new SSE callback identities
still pass. Both new bodies have no dead or unsupported arms. Restored the
two generated USB cancellation trace hooks; Release build87 is compiling.

Read-only renderer follow-up while compiling: both local Mercenaries and
NFSU2 `src/nv2a/nv2a_pgraph_d3d11.c` define DEPTH_TEST_ENABLE as 0x0354.
Their `nv2a_regs.h` and NFSU2's raw `kernel/nv2a_pb_exec.c` instead identify
0x030C as DEPTH_TEST_ENABLE and 0x0354 as DEPTH_FUNC. Audit that path before
porting the D3D11 state switch. Our software executor currently has no depth
implementation; this discrepancy is a later renderer integration lead, not
a diagnosed cause of run86's missing callbacks. Preserve the already-correct
raw register identities rather than copying a backend label unexamined.

An exact-symbol constant comparison finds six discrepancies among the 25
shared Mercenaries backend/header constants: CLEAR_SURFACE (backend 0x1D0,
header 0x1D94), COLOR_CLEAR_VALUE (0x1D4/0x1D90), CLEAR_RECT_HORIZONTAL
(0x1D8/0x1D98), CLEAR_RECT_VERTICAL (0x1DC/0x1D9C), DEPTH_TEST_ENABLE
(0x354/0x30C) and SHADE_MODE (0x368/0x37C). Our current clear method values
already match the canonical header. This strengthens the need to audit the
reference backend's command-domain/integration path instead of treating its
state switch as a drop-in implementation.

A small read-only hardware probe also confirms the pending x87 PC=24 issue:
16777216+1 produces 16777216 under nearest PC24, but the current double-backed
arithmetic produces 16777217. All four rounding directions are observable.
PC24 retains x87's exponent range: 1e100+1e100 and 1e-100+1e-100 remain
finite/nonzero with 24-bit significands. A naive float cast would overflow or
underflow those, so a later PC24 port must preserve exponent range and audit
rounding directions, not merely copy a cast. No current game failure is
attributed to this probe; it remains a clean-dispatch logic/accuracy lead.
The ignored probe executes only its own instruction bytes in a private RX
page and preserves the native x87 control word.

Full Release build87 succeeds after about 24 minutes, with the existing
oversized-shift warning class and no build errors. Run87 PID1760 starts at
01:09:29 on 2026-10-11: dispatch/direct-stub/USB tracing, dormant frame trace,
no watchdog and no USB-port environment override. MVP's port2/P1 default is
therefore under game-level acceptance. Natural intro completion is already
verified by run86; this run may skip movies to retry the exhibition frontier.

Run87 enumerates the pad on physical root port3 (524 ms), skips both movies
with Start, completes the save check and accepts A through the main menu.
The Florida/Texas selector visibly labels the controller P1 (window107,
`build/teams87_p1.png`), replacing P3; game-level input/slot acceptance passes.
At 2,243 path attempts, indirect/direct-stub/crash/USB-cancellation counts
remain zero. Continuing through jersey/difficulty to exhibition loading.

Run87 passes the prior exhibition crash and renders the first live stadium
frame: Disch-Falk Field, Florida/Texas matchup overlays, stands, trees and
field (`build/frame87/frame.window175.bmp`, `build/stadium87_first.png`).
This follows stadium/signature/manager-huddle scripts and commentary audio
assets at 3,295 path attempts, not the prerecorded intro movie. All indirect,
direct-stub, crash and USB-cancellation counts remain zero. Triggered the
one-frame draw trace at this scene; it closes at flip691. First stadium
rendering is verified; first pitch and a completed exhibition are still pending.

Run87 continues through the pitcher card and manager-huddle animation, then
the batter introduction reaches unresolved 0x239420 ten times. Stopped at
739.1 s / 3,417 path attempts with zero crashes, direct-stub hits or USB
cancellations. First live stadium rendering and P1 acceptance stand; the
whole run is not dispatch-clean. The world is very dark while UI text/cards
remain readable. Do not diagnose the later black batter view before recovering
the newly missed callback.

### Preflight88 — batter-intro distance/state predicate

0x239420 is an independent 76-instruction body after ret4 at 0x23941D,
ending at exclusive 0x239540. No calls; a single immutable distance-threshold
constant at 0x61D088, two returns, SSE COMISS and unsigned hash comparisons.
It tests active/state, horizontal distance below 48 and one of six animation
hashes. Added seed29. Its complete lift passes 38,148 original-x86 comparisons
over valid/invalid unsigned hashes, distance boundaries, NaN/Inf/subnormal,
early returns, three coordinate origins, full object/source bytes, scratch
and saved registers, and stack cleanup. The native private RX page relocates
only the verified immutable constant, identically for both implementations.

Installed as local generated chunk9999 with an explicit sorted dispatch
entry, for a quick preflight before the next full consolidation cycle. The
database has the measured 0x239420..0x239540 range; no disasm rerun yet. Full
regeneration will remove the preflight chunk and generate the seeded body.
Expanded FTRACE to include active RC ICW/OCW/factors, final constants, decoded
textures and all four vertex texture coordinates plus diffuse/specular/fog.
This is diagnostic only; no world-brightness or depth workaround is applied.
First incremental invocation regenerates the VS project for the new chunk
but links without compiling it (LNK2001); retrying with the regenerated project.

Retry builds chunk9999 and links successfully. Run88 PID23524 starts at
01:28:05 on 2026-10-11 with the same diagnostics and dormant richer FTRACE.
This is an incremental preflight, not a claimed full cycle88. Preserve the
29th seed and all four boundary ranges during later consolidation.

### Cycle89 in progress — homogeneous texture defaults

The live stadium is dark, so compare the richer draw-time trace before
changing lighting. A separate, reproducible VP defect is already verified:
only oT0.Q was initialized to one; oT1..oT3.Q remained zero when a shader
wrote XY alone. [xemu's shader generator](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/glsl/vsh.c)
initializes all four texture outputs to (0,0,0,1). The four-stage native
pixel test now exercises XY-only shader writes: it fails before the repair
and passes with the expected opaque red pixel after initializing all four
Q values. The existing primitives/video/fades/P8/flips/VP/combiner tests
also pass (`build/nv2a89_before.txt`, `build/nv2a89_after.txt`). This proves
the defect and repair, not yet that it explains the whole stadium darkness.
Run88 still uses the prior executable while cycle89 disasm starts with all
29 seeds. Do not relink that executable until run88 is stopped.

Run88 is stopped at 01:40:26 (740.5 s, 2,824 file opens): zero unresolved
dispatches, direct-stub hits, crashes or USB cancellations, but parked on
a black exhibition-load screen before reaching the new predicate. Main-thread
samples are in critical-section/CPU-ticket code, workers in event waits;
audio output and device threads continue. Saved `build/sample88_wait.txt`
and `build/stack88_wait_final.txt`. This is not predicate acceptance or proof
of race elimination. The richer FTRACE flag is never consumed because no
new draw arrives. Cycle89 disasm completes in 275.97 s; its entry set and
ranges exactly match the preflight database. Reapplied all four protected
ranges, including memcpy's exclusive 0x0044302D; full lift is running.
Added a read-only, MAP-derived TLS/guest-stack and CPU-ticket snapshot helper
(`build/thread_state89.py`) to identify the owning thread in a repeat wait;
it still needs its first live acceptance.

Cycle89 lift completes: 37,758/37,758, zero failures, 4,291,310 C lines,
293 unresolved direct targets. All 29 seeds have unique sorted dispatch
entries. Consolidated player-bitfield, parameter-copy, SSE callback and new
predicate bodies match their original-x86 tested bodies exactly; memcpy,
IDCT and frontend identity checks also pass (`build/audit89.txt`). Removed
chunk9999 and restored both local USB hooks in regenerated chunk37. Full
Release build89 is running with /MP4; no game process remains alive.

### Runtime90 in progress — measured stadium blend states

The 153-draw stadium frame from run87 includes 18 SRC_ALPHA/ONE draws and
one ONE/CONSTANT_COLOR draw. The executor only blended SRC_ALPHA/
ONE_MINUS_SRC_ALPHA and silently replaced the destination for every other
combination. A native pixel check reproduces this independent defect.
[xemu's draw state](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/gl/draw.c)
and the NV097 definitions confirm separate factors, equation and ARGB blend
constant (method 0x34C; 0x350 is the equation). Implemented the 15 ordinary
factors, ADD/SUBTRACT/REVERSE_SUBTRACT/MIN/MAX, including alpha-saturate's
special alpha factor and RGB565 destination expansion. The existing common
fade keeps its fast path. Signed blend equations remain unsupported.

Native tests fail on the measured additive state before the repair and pass
after it: all factor-channel fixtures, both measured states, MIN/MAX ignoring
factors, subtraction/clamping, zero-alpha ONE writes and RGB565. The full
video/fade/P8/VP/four-texture/combiner smoke suite passes
(`build/blend90_final.txt`). FTRACE now also prints equation/blend constant.
The new state is appended to the GPU struct, preserving existing diagnostic
offsets. Runtime90 edits occurred after the kernel library compiled in full
build89; run a final incremental build before live acceptance. Its effect
on the stadium has not yet been measured.

NFSU2's current CLAUDE.md also documents APU-lock starvation while guest
workers hold GIL/IRQL, with atomic voice-lock bits as its repair. This is a
lead for the dispatch-clean run88 wait; do not port it as a claimed cause
without identifying the live owner. The new TLS snapshot is meant for that.
Run88's last pair of opens is AUDIO/AEMS/UMPC.ABK. At the same path-count
position run87 proceeds to SFXB.ABK, PAUSESFX.BNK and remaining stadium audio
banks. The complete sequences share only the first 591 opens (thread/movie
ordering differs), so this is a local loading checkpoint, not proof that
every preceding operation or timing was identical.

Build89's first invocation retains the old MSBuild item list after CMake's
glob regeneration: it reaches the removed chunk9999 and emits C1083, while
the regenerated .vcxproj no longer lists that file. Other generated objects
continue compiling. Retry after completion, with an explicit CMake configure
first; completed objects can be reused. Updated AGENTS' full-cycle command
to configure before MSBuild when generated chunk files change. The earlier
build88 new-chunk LNK2001 was the opposite side of the same stale-item trap.

The remaining build89 compiler work completes, returning failure only for
the stale chunk9999 item. Explicit CMake configure plus build90 succeeds in
6.4 s, reusing all generated objects and rebuilding nv2a_pb_exec.c. Both
renderer repairs are included. Run90 PID28888 starts at 02:12:36 on
2026-10-11, renders EA Sports and mvpintro.vp6, accepts Start, and initially
has zero misses/stubs/crashes/USB cancellation traces. The TLS helper reads
valid guest registers/IRQL/stacks; its initial separate ticket reads were
inconsistent during fast handovers, so it now reads/retries the adjacent
pair together and marks validity. Live intro check passes with next >= serving.

Run90 passes the run88 UMPC load checkpoint and reaches a bright live
Disch-Falk stadium, blue sky, trees, stands and field under the Florida/Texas
overlay (`build/stadium90_matchup.png`, window203). This is a substantial
visible improvement over run87's nearly black world. The repairs are tested
independently natively; this combined live run does not isolate their
individual contributions. P1 input again passes every frontend stage.
The richer one-frame trace captures 623 draws through flip677, including
78 with stage program0x21021 and 30 with0x20421; saved separately as
`build/frame90_trace.txt`. T0/T1.Q remain one. Negative repeat UVs are clamped
to zero by the existing sampler, a measured next texture-accuracy lead;
depth is still absent. The batter introduction is visible at 3,349 paths.

Run90 then reaches a pitching view with inning/count/aiming UI and a LATE
cue (`build/pitching90_first.png`, window307), but 0x256590 misses eleven
times. Stop at 02:23:40, 663.8 s / 3,351 paths, zero crashes/direct-stub hits/
USB cancellations. Do not call a successful pitch verified after a miss.

### Preflight91 — pitching callback

0x256590 is a standalone entry after INT3 padding, with own sub esp,0xFC
and balanced register saves. Complete range0x256590..0x256F4E contains
645 instructions and five returns; existing conditional alias0x256C6F is
internal. All immediate branches remain within the full range. Added seed30
and the fifth preserved range in tools_patch_boundaries.py. The complete
mechanical lift covers all instructions/returns without unsupported/dead
code; 22 direct callees already have dispatch entries and one call is virtual.
No whole-body native comparison is claimed. Installed in local chunk9999
with a local extern/sorted dispatch row (37,759 unique entries). Two initial
preparation attempts fail on the dispatch-row parser before changing the
dispatch; corrected the parser and verified the final sorted table. Explicit
CMake configure precedes incremental build91, now running. Full cycle91 is
pending; preserve seed30 and all five ranges during consolidation.

Explicit-configure build91 succeeds on its first invocation, compiling
chunk9999 and dispatch then linking. Run91 PID16680 starts at 02:29:15
on 2026-10-11 for acceptance; early intro path is clean and Start is posted.

### Runtime92 / consolidation in progress — signed repeat coordinates

Run91 passes the intermittent UMPC loading checkpoint, reaches the bright
stadium, batter introduction and live pitch selector without dispatch misses.
The first A press shows the pitching meter; a second press is posted for
release. A completed pitch and count change still require acceptance. MVP 06
uses the meter; Rock and Fire analog pitching belongs to the 2007 sequel.

The richer run90 trace contains negative repeated stadium UVs, e.g.
(0.146655,-0.689499,0,1). Both the projective combiner sampler and old
fixed-function raster path clamp negative coordinates before unsigned
conversion, smearing an edge across repeated textures. xemu's texture-address
map and the NFSU2/Mercenaries backends confirm mode1 is REPEAT. Preserve signed
coordinates, floor to the nearest texel's interval, then reduce repeat
coordinates before integer conversion; positive in-range coordinates keep a
fast path. Clamp behavior is retained; other address modes remain partial.

Native regression fails before this repair on the measured normalized UV and
passes after it, including swizzled/projective coordinates, negative fractions,
exact repeat boundaries, multiple periods, very large finite values and a
linear fixed-function pixel. Existing movie/fade/P8/VP/combiner/blend tests
pass (`build/sampler92_before.txt`, `build/sampler92_final.txt`). This runtime
edit is not in the running build91. No live texture improvement is claimed yet.

Full disasm92 starts with all 30 seeds while run91 remains alive; its MAP and
executable are unchanged. Reapply all five protected ranges before lifting,
then restore USB hooks and explicitly configure CMake before building after
run91 is stopped. Preserve the preflight DB in `build/functions91_preflight.json`.

Run91 throws an 82 MPH 4-seam fastball and changes the count from 0–0 to 1–0
(`build/pitch91_82mph.png`, window382). Immediately afterward it misses two
standalone entries, 0x32710 and 0x1D8240. Stop at 02:44:06 on 2026-10-11,
890.2 s / 3,325 paths, two misses, zero crashes/direct-stub hits/USB
cancellations. This proves visible pitch/count progress but does not accept a
dispatch-clean pitch. TLS/GIL snapshots are saved for stadium and post-pitch;
no repeat clean loading wait occurs. The late FTRACE captures nine postprocess/
UI draws through flip998 (274 lines), saved in `build/frame91_trace.txt`.

Both new entries pass executable/body probing and complete original-byte
audits (`build/audit_targets92.py`): 0x32710..0x32885 has 373 bytes, 112
instructions, two ret 4 paths and five known direct callees; 0x1D8240..0x1D8500
has 704 bytes, 161 instructions, one balanced return and six known direct
callees. Complete mechanical lifts have no overlapping/unsupported/dead code.
No whole-body native comparison is claimed. Seed-from-log's dry run also
offers two already-dispatched thread context entries; only the two actual
misses are seeded. Seeds32, seven protected ranges.

Disasm92 finishes in 247.85 s and, after restoring its five then-existing
ranges, matches the preflight91 DB entry/range sets exactly (37,761 DB entries).
It started before the post-pitch misses. Disasm93 is rerun with both new seeds
before the full lift/build; run91 is stopped. Its audits will also compare all
prior native-tested bodies and the three newly audited mechanical bodies.

Disasm93 completes in 212.15 s: exactly the two new entries, no changed
existing ranges after all seven patches. Full lift93 succeeds:
37,761/37,761, zero failed, 4,293,039 C lines in 38 chunks; 293 unresolved
direct targets remain. `build/audit_cycle93.py` passes all 32 dispatch rows,
unique/sorted entries, all seven ranges, prior native-tested body identities
and the three recent mechanical-body identities. Full lift removes chunk9999;
`build/restore_usb_hooks93.py` restores both probes in chunk37. Explicit CMake
configure succeeds and Release build93 is compiling with /MP4.

The final native renderer suite also passes with the signed-repeat repair and
new draw-time depth diagnostics (`build/sampler93_final.txt`). Diagnostics
retain zeta pitch and print still-unimplemented depth enable/function/mask,
zeta offset, clear value and control0 at actual draws. No depth rendering is
implemented by this diagnostic addition. A screen-checked local Play Now
driver (`build/route_frontend.py`) recognizes two distinct matching captures
before each key. Offline replay of run91 passes all title/name/menu/team/
jersey/difficulty stages without sending input; live acceptance is pending.

During build93, callback0x32710 additionally passes 13,440 original-x86/lifted
comparisons with controlled external callees (`build/postpitch93_test.txt`).
The harness relocates the original 373-byte body and two constants; six call
sites use five matching fixtures. It compares complete object memory, call
order/ECX/arguments, unsigned timer conversion, queue/list branches, float/
NaN comparison paths, return registers, stack cleanup and saved registers.
This covers the complete callback body with fixtures, not integration with
the real callees. The generated full93 body matches the tested source.
No runtime/lifter edit is required by this check. The other new callback
0x1D8240 still has only its complete original-byte/mechanical-lift audit.

Cross-run menu replay90 confirms title/name/menu/team recognition but does
not find two stable jersey captures before that historical run advances;
it stops without input. The title matcher was changed from blinking Press
Start to the static logo and reduced pixel-shift sensitivity. Run91 replay
is the complete offline route; live use must still be observed.

The second callback0x1D8240 also passes 3,840 original-x86/lifted comparisons
with controlled callees (`build/postpitch_second93_test.txt`): all object
bytes, its 18-call order/arguments and intermediate selected-state snapshots,
unsigned timer conversion, signed queue bounds, four virtual-base offsets,
return registers and stack/saved registers. The original 704-byte body and
three constants are relocated; six callee fixtures are shared between native
and lifted execution. This covers the full body with fixtures; real-callee
integration still requires the next game run. No runtime/lifter repair is
needed. The common harness is expanded for both bodies; rerun the first
comparison after that expansion before recording its final acceptance.

The expanded common harness rerun passes the first 13,440 cases too, and
the consolidated generated bodies still match both tested sources
(`build/audit93_final.txt`). `git diff --check` passes. Release compilation
is in the final chunks; existing C4293 oversized-shift warnings remain in
unreached decoded regions, with no new build failure observed so far.

Release build93 succeeds and links at 03:20 on 2026-10-11, with explicit
CMake configure before this first successful invocation. Both final native
callback suites pass after adding direct comparisons of all saved registers:
13,440 + 3,840 controlled-callee cases (`build/postpitch93_final.txt`,
`build/postpitch_second93_final.txt`). No callback/lifter repair was needed.
Full cycle93 is complete. Run94 is next for real-callee pitch integration and
live signed-repeat/depth-state acceptance; no game was alive during linking.

Run94 PID24856 starts at 03:20:51 on 2026-10-11. EA Sports movie pixels pass;
Start is posted and the screen-checked route accepts title/name/menu/team.
A persistent EA Trax banner obscures the jersey matcher's bottom rows; the
driver stops there, with zero game failures. Narrowed the ROI to menu labels,
verified the complete historical run90 route offline, stopped only the old
driver PID14972 and resumed the same live game at jersey. Jersey/difficulty
acceptance passes; stadium loading reaches 3,291 paths with zero misses/
stubs/crashes/USB cancellations. Armed the one-frame trace at the ending NCAA
loading transition for live signed-repeat/depth-state inspection. No game
relink or memory mutation occurs during this route adjustment.

Run94 reaches a bright live stadium and manager huddle with clean dispatch
(`build/stadium94_repeat.png`, window210). The frame trace captures 428 real
draws through flip528 (14,054 lines; `build/frame94_trace.txt`), with 88 stage
program0x21021, 30 program0x20421 and 300 program1. This accepts live execution
of the repaired repeat sampler; the camera differs from run90, so no exact
image A/B improvement is claimed. Native pixel tests establish its semantics.

Depth diagnostics now measure the next major renderer gap: 426/428 draws
enable depth, mostly LEQUAL(0x203), with depth writes either on or off. Main
zeta0x02D9C000/pitch2560, shadow zeta0x030E6000 or0x031E6000/pitch1024;
format0x128, clear0xFFFFFF00, control0=0x00100001. Typical screen-space VP Z
is ~16.5 million, in 24-bit viewport units, so do not treat it as normalized
0..1. Current renderer ignores these depth states. Manager faces/bodies and
field overlap visibly; correct depth is a measured next accuracy task.

### Runtime95 pending — partial surface clipping

Run94's 36 shadow-atlas draws use format0x128, pitch1024 and 128-wide clips
within a larger surface. `surface_bpp` divides pitch by clip width, reports
eight bytes per pixel and skips these draws/clears. A native partial-surface
pixel fixture fails before the repair (`build/surface95_before.txt`). Decode
storage width from NV097's color-format nibble instead, independently of
clip/layout/zeta. Channel packing beyond ARGB8888/RGB565 remains partial.

The fixture passes after repair: right-half clear/draw uses full surface
pitch and preserves the left half. All prior movie/P8/fade/blend/VP/combiner/
four-stage/repeat fixtures pass (`build/surface95_after.txt`). This edit is
not in the still-running build93/run94. Do not relink while run94 is alive.
Run94 reaches the pitch selector at 3,351 paths with all failure counts zero;
the first pitch-meter press is posted for real-callee acceptance.

Run94 advances the first pitch/count to 1–0 with no dispatch misses, direct
stubs, crashes or USB cancellation traces. It is stopped at 03:42:31 on
2026-10-11 after 1,300.0 s / 3,352 paths (`build/pitch94_clean_count.png`).
The post-pitch scene continues animating but does not return to the pitch
selector during this sample; a complete pitch-to-next-pitch loop/exhibition
is still unverified. Read-only TLS/ticket and host samples are saved in
`build/thread94_postpitch.txt` and `build_sample94_postpitch.txt`; tickets
advance and the main thread is active, so this is not evidence of an APU
lock deadlock. Both new callbacks are consolidated, native tested with
controlled callees and no longer produce the prior misses on the reached path.

### Runtime95 — integer pitched depth surfaces

The run94 trace establishes format0x128, integer Z24, LEQUAL and independent
color/depth pitches. Cross-checked method values/storage with NFSU2's current
kernel renderer and Mercenaries' NV097 header/clear path; xemu's programmable
VP tail confirms viewport Z is already projected and stays affine across the
screen. Added pitched integer Z16/Z24S8 clears, all eight comparisons, write
mask and guest-RAM zeta selection. Preserve stencil on depth writes/clears,
support stencil-only clear and inclusive clear rectangles, and perform depth
write only after texture/combiner discard and alpha testing. The repeat sampler
and partial-surface storage-width repair are retained.

Native regression fails before depth support (`build/depth95_before.txt`) and
passes after (`build/depth95_final.txt`). Real pixel fixtures cover 16.5M Z24
near/far occlusion, all comparison/equality cases, disabled tests/write masks,
different pitches, stencil preservation, partial clear bounds, Z16, alpha
discard, a real VP batch with unequal W and affine Z, separate zeta buffers
and repeated frame clears. Prior video/P8/fades/blends/VP/combiners/four-stage/
signed-repeat fixtures pass. Hardware-exact depth rounding, floating/swizzled
depth, W-buffering, stencil operations, polygon offset and near clipping
remain incomplete. No claim of complete depth emulation is made.

Run94 is stopped before any relink. Explicit configure95 passes and the
incremental Release build95 is started for fresh live acceptance.

Release build95 succeeds (03:43:06); run96 PID2180 starts at 03:43:35.
Both movie paths open and MVP intro pixels are verified, with a clean log.
The screen-checked frontend driver runs in the same game. Cross-check sources:
[NFSU2 renderer](https://github.com/antoxa2584x/nfsu2-sw/blob/main/xboxrecomp/src/kernel/nv2a_pb_exec.c),
[Mercenaries NV097 constants](https://github.com/KraftMacAndChee/Mercenaries-Recompiled/blob/main/src/nv2a/nv2a_regs.h),
[xemu programmable VP conversion](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/glsl/vsh-prog.c)
and [clear/depth state](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/gl/draw.c).
Use the current kernel/backend source rather than older reference state tables:
some old method labels disagree with NV097's header.

Run96's first menu driver expires during the movie/legal sequence without
sending menu input. Resume with a 360 s per-screen limit in the same game;
title, naming prompt and Play Now checks/input pass. Start is posted during
the MVP movie after its pixels are captured; this is not a full natural-playback
timing test. Zero failures at 2,243 paths. `build/audit95.txt` confirms full93's
32 sorted seed rows/seven ranges/body identities remain intact after runtime95.

Run96 reaches the bright Disch-Falk stadium at 3,291 paths with zero failures
(`build/stadium96_depth.png`). The transition FTRACE contains 116 real draws
through flip798, 114 with depth enabled (`build/frame96_trace.txt`). Explicit
clear rectangles are present: 256x512 followed by 640x480. Read-only actual
guest zeta RAM shows untouched `0xFFFFFF00` and drawn `0xFD66EF00` pixels
(`build/depth96_ram.txt`), confirming clear and writes execute in the game.
This trace precedes the player close-ups/shadow-atlas batches; capture a later
world frame before claiming those repaired. No complete exhibition yet.

Reference follow-up: xemu issue2403 reports the MVP series' pitch-meter
display freezing, closed by [PR2670](https://github.com/xemu-project/xemu/pull/2670),
a palette-byte-length/cache-invalidation repair. Our software P8 sampler reads
guest palette entries directly, without that texture cache, so this is not
evidence that our post-pitch loop has the same cause. Keep actual USB delivery,
simulation state and display state separate when testing the meter.

Run96's first player close-ups show incomplete body geometry and unusual
camera orientation; do not infer a complete visual fix from depth RAM alone.
The existing trace trigger is one-shot. Rearmed only `s_frame_trace_done`
through the matching loaded module/MAP (`build/rearm_trace96.py`, records in
`build/trace96_rearm*.txt`), without relinking or writing guest state. Three
consecutive captures include complete 379/385-draw frames through flips1171/
1172, saved in `build/frame96_full_trace.txt`. All draws use main zeta, explicit
640x480 clears and mostly LEQUAL. The captured frame has no shadow-atlas
batches, so their acceptance is still pending. At 3,410 paths dispatch,
direct-stub, crash and USB cancellation counts remain zero.

Run96 reaches the standard pitching view and a clean 1–0 count change
(`build/pitch96_depth_count.png`). Stopped at 04:03:51 on 2026-10-11 after
1,215.6 s / 3,413 paths, with zero misses/stubs/crashes/USB cancellations.
A full pitch-to-next-pitch loop/exhibition remains unverified. TLS/stack state
is saved in `build/thread96_postpitch.txt` before relinking.

### Runtime97 — reached packed vertex inputs and repeatable capture

Read-only live declarations show types5/6 unsupported by our fetcher. Repeated
sampling restricted to active primitives with at least three indices confirms
both types are used (`build/vertex_formats96_active.txt`): signed-short S32K
positions/indices and packed CMP 11/11/10 components. Existing float/byte-only
fetch silently returns zero values for these streams. NFSU2 already supports
them; Mercenaries' header and [xemu vertex decoding](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/vertex.c)
confirm the formats. Added signed-short conversion, sign extension and
normalization for packed 11/11/10, preserving default Z/W. Inline reads now
check the complete encoded element size, rather than always four bytes.

The native fixture fails before (`build/vertices97_before.txt`) and passes
after (`build/vertices97_final.txt`): signed extremes, actual indexed short
geometry, packed normal extremes and a real VP-driven pixel, plus truncated
inline elements. All prior renderer/depth tests pass. This does not yet prove
the player-mesh defects are resolved in the game.

Trace requests are now consumed by deleting their flag when armed; creating
a fresh file captures a later frame without a restart or diagnostic-memory
write. Two independent native requests pass (`build/trace97_native.txt`).
Old `build/rearm_trace96.py` is only for the matching old build96/95 MAP and
must not be used with build97 (its done flag is removed).

Explicit CMake configure and incremental Release build97 succeed (04:04:23).
Run98 PID2748 starts at 04:04:35 for packed-vertex live acceptance. Run96 was
stopped before linking; never relink while run98 is alive.

Run98 opens both movies and shows MVP intro pixels; Start is posted after
capture. The route's static-logo title match fires during asset loading,
before Press Start is ready, and its first input is lost. A single repeated
Start on the observed ready screen advances the same live game. The driver
now requires seeing the actual Press Start text once, as well as two stable
logo captures; full offline run91 replay still passes
(`build/route_replay91_ready97.txt`). No guest/runtime change is made for this
driver readiness issue. Dispatch/stub/crash/USB counts stay zero.

Run98 passes the full frontend route. A brief black loading interval at
2,824 paths is sampled before it resumes to 3,259+ without intervention:
`build/thread98_loading.txt`, `build_sample98_loading.txt` and
`build_stack98_loading.txt`. The ticket pair is valid/equal in that instant
and the wait clears; this does not establish NFSU2's APU-lock starvation as
the cause of a persistent park. Keep that lead separate from decoder work.

### 2026-10-11 — build99: concurrent kernel handle-table publication/removal

Run98's later 3,259-path loading park persists. Stop at 04:24:11 after
1,175.5 s, zero dispatch misses/direct stubs/crashes/USB cancellations. Both
movies and the full frontend route pass, but packed-vertex stadium acceptance
is still pending. Read-only frozen TLS/host samples and guest memory identify
`sub_0012C0F0` looping through `sub_0042082C`/XAPI GetExitCodeThread. Its guest
object `0x8013C790` retains token `0x4800000D`; table slot 13 is NULL
(`build/handles98_wait.txt`). CPU tickets advance and audio produces samples.
The main thread also periodically runs DirectSound recovery; current APU
readiness and prior DSP passthrough limitations remain separate leads.

`kernel_bridge.c`'s handle table has no lock, while bridges explicitly
suspend the guest CPU lock during host work. Concurrent file/event/thread
creation can both observe the same free slot and overwrite it; either close
then removes the other handle. Extracted the unchanged table implementation
to `kernel_handles.c` and ran 32 synchronized native workers with unique
handles, allocate/resolve/take batches: 58,871 wrong/missing-handle observations
in 128,000 allocations (`build/handles99_before.txt`). Shared/exclusive SRW
locking around all table accesses passes the same test with zero errors;
stable duplicate tokens, invalid/synthetic handles and existing real thread
object exit/ref/close lifetime tests pass (`build/handles99_final.txt`).

The bridge now uses this protected module. Thread-object close notification
remains outside the table lock. Optional `RECOMP_HANDLE_TRACE` logs publication,
removal and NtClose's guest caller, to distinguish future stale guest references
from lost runtime entries. Native stress proves the race, not causality for
every prior park. No APU atomic-lock port is made from the NFSU2 reference.

Explicit CMake configure and incremental Release build99 succeed, linked
04:27:00, 58,443,776 bytes. Full-cycle93 audit still passes: all 32 seeds,
37,761 sorted unique dispatch entries, seven protected ranges, zero failed
lifts, no preflight chunk (`build/audit99.txt`). Run99 PID3580 starts at
04:27:52 with handle tracing for movie/frontend/loading/vertex acceptance.
Run98 was stopped before relinking. The route driver now requires the actual
Press Start prompt, avoiding its earlier premature logo-only input.

Run99 renders both intro movies (`frame.window016.bmp` EA Sports,
`frame.window051.bmp` MVP), accepts Start to leave MVP and passes the entire
screen-checked frontend route. It advances beyond run98's 3,259-path park
and renders the bright stadium with the Florida/Texas card at 3,293 paths,
zero misses/stubs/crashes/USB cancellations
(`build/stadium99_handles_vertices.png`). Three flag-triggered frame traces
complete, 38/368/368 draws at flips749/750/751 (`build/frame99_trace*.txt`).
Active indexed declarations use both repaired vertex types 5/6; no sampled
format remains unsupported (`build/vertex_formats99_active.txt`). Handle
trace reconstruction at this checkpoint reports 265 publications, 245
removals, 20 live entries and zero identity errors. Player close-up and a
complete pitch loop remain to be checked; this successful rerun is not
causal proof that the reproduced table race caused run98's stale token.

### 2026-10-11 — native inline-layout repair prepared while run99 continues

The other packed-vertex path still counted S32K inputs as four-byte floats
in `draw_inline_array`. It also padded every attribute to a DWORD instead of
aligning to the component size. Cross-checked Mercenaries'
`prepare_inline_array_layout` and xemu's
`hw/xbox/nv2a/pgraph/gl/vertex.c::pgraph_gl_bind_inline_array`.
New complete pushbuffer pixel fixtures fail before
(`build/inline100_before.txt`) and pass after (`build/inline100_final.txt`):
adjacent one-short fields sharing a DWORD, byte RGB without invented padding,
and a following float aligned after a short, through a real VP. All previous
renderer/depth tests and two independent trace requests pass.

This source repair is not loaded in live build99. The live format sample
finds no packed inline inputs (`build/vertex_formats99_inline.txt`); do not
claim it caused the remaining player holes. Future FTRACE records array mode
and each input's offset/type/count/stride. Build100 is pending until run99
stops; no live EXE/MAP was replaced.

Run99 reaches the standard pitching UI at 3,412 paths, still clean. A matching
MAP read of `s_gpu.flips` measures only 7 completed game frames in 10 s
(`build/fps99_pitch.txt`), about 0.7 FPS. Native `--gpu-layout` verifies the
read offsets. Window refresh/capture rate is not the game frame rate. The
software renderer's throughput is now a measured playability limitation;
allow for this when interpreting post-pitch animation delays.

### 2026-10-11 — clean pitch acceptance, build100 inline layout, D3D11 prerequisite

Run99 throws an 82 MPH 4-seam fastball and advances the count to 1–0
(`build/pitch99_82mph.png`). Another A input does not establish return to the
next selector; Start subsequently opens the pause menu (`frame.window805.bmp`).
The game remains responsive. Stop at 04:56:34 after 1,721.3 s / 3,415 paths,
zero misses/direct stubs/crashes/USB cancellations. Final handle reconstruction
reports 273 publications, 253 removals, 20 live entries and zero identity
errors (`build/handles99_final_live.txt`). Post-pitch throughput independently
measures six flips/10 s (`build/fps99_postpitch.txt`). No complete pitch loop
or exhibition is claimed.

Explicit CMake configure and incremental Release build100 succeed, linked
04:56:46. Full93 audit still passes all seeds, tested bodies and protected
ranges (`build/audit100.txt`). Run100 PID10816 starts at 04:56:58 and renders
EA Sports (`frame.window016.bmp`) and MVP intro (`frame.window037.bmp`).
Start leaves the second movie; the complete screen-checked frontend route
passes (`build/route100.txt`), then the bright stadium/matchup card renders
(`frame.window219.bmp`), with zero failures at 3,293 paths. Inline-layout
acceptance preserves the earlier visible output. No live EXE/MAP was replaced.

The requested Mercenaries comparison exposes raw-word decoder defects in the
existing D3D11 combiner implementation. It reverses A/B/C/D, swaps AB/CD
destinations, reads final E/F/G from the wrong bytes, reverses special registers
14/15 and always selects alpha in the alpha path instead of blue/alpha by bit 4.
A new standalone D3D11 WARP fixture feeds asymmetric raw words and compares
floating-point rendered pixels with the portable evaluator: 1,136 channel
mismatches before, zero after in 320 draws / 1,280 channel comparisons
(`build/combiners101_before.txt`, `build/combiners101_after.txt`). Covers a
noncommutative final mix, G blue/alpha, asymmetric general dot/product outputs
and both final special registers. The existing A8 fixture's raw words had
mirrored the incorrect parser; corrected their packing. All 336 A8 plus four
RHW rendered draws still pass (`build/a8_101.txt`).

This is preparation for the hardware backend, not a live acceleration claim.
The physical DMA walker still invokes the software executor. Before wiring
D3D11, audit remaining combiner flags/simultaneous writes/clamping/constants,
homogeneous texture coordinates and surface/depth/guest-RAM coherence.
Mercenaries' expanded backend is a reference, not a drop-in replacement for
this title's validated DMA and guest scheduling paths. Main build101 is pending
until run100 stops; native tests alone do not replace that build/run acceptance.

Run100 stops at 05:06:15 after 557.4 s / 3,323 paths, zero misses/direct stubs/
crashes/USB cancellations. Both movies, the full frontend route and bright
stadium pass (`build/stadium100_inline.png`). Explicit CMake configure and
incremental Release build101 succeed; after removing an existing unused local
in the touched decoder, the final link is 05:06:46. Full93 audit still passes
(`build/audit101.txt`). The unrelated pre-existing C4132 device-vtable warning
remains; no new decoder warning remains in the final incremental log.

Run101 PID17740 starts at 05:06:47. Both intro movies visibly render
(`frame.window021.bmp` EA Sports, `frame.window083.bmp` MVP); two independently
requested complete traces end at flips173/174 with the new layout fields.
Stop at 05:10:59 after 252.0 s / 591 paths, zero misses/direct stubs/crashes/
USB cancellations. Its handle trace has zero identity errors. No game process
remains alive. All seven run99 frame traces, including the previously unsaved
209-draw pitching trace at flip1352, are extracted without altering the logs
under `build/frame99_traces_all/`; index.json records counts/flip IDs.

The requested first-frame task is achieved; the expanded playable/accuracy
goal is not. Current blocker to practical playability is measured software
rendering throughput, with a full pitch loop, exhibition/audio/save/stability
acceptance still outstanding. HANDOFF's next steps now bring the hardware
backend forward alongside those gameplay checks.

### 2026-10-11 — build102 decoded D3D11 rasterizer and exact integer depth

Added an opt-in `RECOMP_RENDERER=d3d11` backend at the existing executor's
decoded-triangle boundary. The physical DMA walker, CPU vertex programs and
presentation remain in place. Four homogeneous texture coordinates, palette
and packed-video decoding feed cached GPU textures. Raw NV2A combiner words
generate specialized HLSL, including simultaneous RGB/alpha reads, input and
output mappings, mux selection, shared/unique constants and final flags.
Animated constants do not recompile shaders. Cache snapshots detect source
and palette edits in place. Unsupported draws return to software before any
guest-RAM write.

The initial coherence contract uploads and reads back color/depth on every
batch. Both mappings must succeed before committing either guest surface.
This preserves CPU access and render-target-as-texture behavior but is an
expensive baseline; performance acceptance is still required. Texture cache
and shader cache capacities are 32 and 128 respectively. There is no extra
presentation window or command parser. The software renderer remains default.

The new differential fixture passes 1,795 GPU draws / 57 shader compiles on
both WARP and actual AMD hardware, with zero failures. It checks 56 combiner
structures across 32 signed input sets, projective Q, alpha discard and
in-place texture edits. Existing full-pushbuffer fixtures also execute through
the hardware path. An AMD-only Z24 failure appeared before live testing:
16,500,000 read back as 16,500,002 through normalized hardware depth, then
16,500,001 after preserving raw screen Z in a noperspective varying. WARP
alone had passed. Mercenaries' renderer independently documents extra D24
steps from host depth transforms; cross-checked that reference.

Use D32_FLOAT to store quantized guest integer Z divided by 2^16 or 2^24,
both exact binary fractions, and retain stencil bytes in guest RAM. This
avoids host UNORM conversion and reciprocal rounding without shifting guest
depth or weakening equality assertions. Raw screen Z interpolates without
perspective; quantization tests the fractional part instead of adding 0.5
at Z24 float magnitudes. Native tests now cover both range endpoints, adjacent
integers, fractional rounding, all eight comparisons, unequal-W affine depth,
separate pitches, masks and alpha discard. The complete suite passes with
72 GPU draws / zero fallbacks on both WARP and AMD; software also passes.
Evidence: `build/render102_{pb,diff}_{warp,hardware}_final.txt` and
`build/render102_software_final.txt`.

Explicit configure and Release build102 pass, final link 05:35:12,
58,464,256-byte EXE. No warnings appear in the final incremental log. The full93
audit still passes 32 seeds, 37,761/37,761 lifted entries, seven protected
ranges including memcpy's exclusive 0x0044302D end and no preflight chunk
(`build/audit102_final.txt`). Runtime-only integration needs no new lift.
Run102 PID19920 starts at 05:36:09 with actual hardware enabled. EA Sports
and MVP intro render (`frame.window003.bmp`, `frame.window010.bmp`) with a
clean dispatch log. Main menu, team/jersey/difficulty and bright stadium
subsequently render (`build/stadium102_hardware.bmp`). The old menu mask
driver did not accept its Play Now image; visually confirmed that menu and
continued with an owned-window A press, then resumed the screen-checked
driver at teams. Stop 05:42:01 after 352.5 s / 3,305 paths, zero dispatch
misses/stubs/crashes/USB cancellations. Thirty-three backend fallback attempts
are supported software draws, not dispatch misses. Menu measures 3.5 FPS;
stadium only 0.3–0.4 FPS. Whole-surface transfers make the initial integration
slower than software in that sequence; no performance success is claimed.

### 2026-10-11 — build103 coherent region readbacks and hardware pitch acceptance

The backend now retains resident surfaces while byte comparisons prove guest
RAM unchanged. CPU/software writes trigger uploads. Readback uses a
conservative projected vertex union, expanded for raster snapping and clipped
to the surface. Positive W bounds are required; unusual/nonfinite vertices
fall back. Only the region is committed to RAM and its snapshot. Read-only
depth omits readback. D3D11 requires a whole-resource copy from a depth-stencil
source, so copy into the non-DS upload resource before the region staging
transfer. Both modified surfaces still map successfully before either commits.

New native fixtures verify inside/outside CPU edits, adjacent integer depth,
reused surfaces, read-only depth and sampling the committed render target.
All previous fixtures pass. WARP and AMD each pass 1,800 differential/coherence
draws and 72 full-pushbuffer draws (`build/render103_coherence_*.txt`,
`build/render103_pb_*.txt`). Explicit configure/Release build103 links 05:47:13,
58,465,792 bytes; full93 audit passes. Run103 PID14084 starts 05:47:30.

Both movies pass (`frame.window003.bmp`, `frame.window018.bmp`) and the full
screen-checked frontend route passes without manual intervention
(`build/route103.txt`). Bright stadium/player close-ups render; stadium
measures 2.3 actual frames/s, player close-ups 1.2. Pitching controls appear,
two A presses operate the meter and a pitch changes the count to 1–0
(`build/pitch103_count.bmp`). Pitching remains 0.7 FPS, matching software;
the stadium improvement does not establish practical playability. Five traces
complete, with 47/495/495/222/494 draws. Full pitching trace has 490 draws to
32-bit color and four to 16-bit format 0x08080213. Its 32-bit draws include
383 LESS_EQUAL depth writers and 94 read-only LESS_EQUAL draws. No new seed
or dispatch repair is needed.

Read-only MAP counters during pitching show 3,269 accepted GPU draws, 32
backend fallback attempts, 792 texture uploads, zero shader compiles and
202,417,820 readback bytes in ten seconds (`build/renderstats103_pitch.txt`).
Texture residency and synchronous per-batch stalls need further tuning.
Stop 06:06:31 after 1,140.7 s / 3,413 paths, zero dispatch misses/stubs/crashes/
USB cancellations. No live EXE/MAP was replaced. Full pitch loop/exhibition
remains unverified.

### 2026-10-11 — build104 measured hybrid policy and bounded texture residency

Added a game-independent native benchmark through real method dispatch,
passthrough VP, six combiner stages, four textures and integer depth. The
benchmark explicitly forces GPU selection and warms the shader/resource
before timing. Software takes 0.038/0.078/0.278/0.863/3.230/13.293 ms for
4/8/16/32/64/128-square quads; AMD D3D11 takes
0.638/0.642/0.638/0.538/0.540/0.773 ms (`build/benchmark104.txt`). These are
local measured costs, not portable hardware guarantees.

The executor therefore leaves batches with fewer than 2,048 clipped triangle
bounding-box pixels in software. `RECOMP_GPU_MIN_PIXELS=0` forces hardware
for backend regression tests and comparisons; the threshold is configurable.
No shader/combiner semantics change. A native transition fixture verifies CPU
small draw, GPU overdraw, a later CPU edit of resident color, then GPU blending
that must preserve that edit. Full fixtures pass in hybrid, forced AMD, WARP
and software modes (`build/render104_*_final.txt`, `build/render104_software.txt`).

A forty-distinct-static-texture, two-pass fixture reproduces eviction churn
with 32 slots (`build/cache104_before.txt`). Expand to 128 slots and enforce
a 128 MiB content budget including encoded snapshots, palettes and decoded
GPU texels; evict oldest entries before exceeding it. One texture exceeding
the budget falls back. Scratch decode and driver overhead are separate from
that content budget. Second-pass uploads become zero. WARP and AMD each pass
1,880 differential/coherence/cache draws (`build/cache104_{warp,hardware}.txt`)
and all 72 forced-hardware pushbuffer draws. In-place edits still invalidate.
Periodic counters now include texture uploads, shader compiles and cache size.

Explicit configure/build104 links 06:06:38, 58,467,840 bytes, no warnings in
the incremental log. Full93 audit still passes (`build/audit104.txt`), including
memcpy 0x0044302D and all seven ranges. Run104 PID25372 starts 06:06:54 on
AMD hardware; both movies render (`frame.window003.bmp`, `frame.window037.bmp`)
with clean dispatch. The complete screen-checked frontend route passes
(`build/route104.txt`), followed by bright stadium and standard pitching
controls (`build/stadium104_hybrid.bmp`, `build/pitch104_hybrid.bmp`). Pitching
measures 1.3 actual frames/s, versus build103's 0.7, with 104 texture uploads
and zero shader compiles in ten seconds versus 792 uploads earlier. The
same sample still issues 1,345 surface uploads totaling 1,483,846,656 bytes
(`build/renderstats104_pitch.txt`): frequent CPU/GPU transitions invalidate
whole-target snapshots. The earlier stadium/player sequence separately
measures 1.0 FPS and 23 texture uploads/10 s; these are different scenes,
not a matched 35-fold performance claim. Stop 06:16:36 after 582.4 s / 3,412
paths, zero dispatch misses/stubs/crashes/USB cancellations. A full pitch
loop/exhibition remains unverified.

### 2026-10-11 — build105 partial uploads for CPU edits between GPU batches

Compare surface rows with the committed snapshots, then find first/last
changed pixels in differing rows. Upload the union of those bounds instead
of the whole target. Color snapshots update only that region. Integer depth
conversion likewise visits just changed pixels; the non-DS depth upload
resource retains the last GPU depth through the existing whole-resource
copy, then receives the partial CPU edit before copying back to the DSV.
New identities or failed prior batches still force complete uploads. CPU
reads retain committed RAM after every draw, and both modified results map
before either commits. No deferred-readback/coherence shortcut is introduced.

The native coherence fixture now asserts reduced upload bytes while checking
CPU edits inside/outside the next draw, unchanged pixels, exact depth and
render-target sampling. WARP and AMD each pass all 1,880 differential/
coherence/cache draws, all 72 full-pushbuffer draws and the hybrid transition
fixture (`build/render105_*.txt`). Explicit configure/Release build105 links
06:16:42, 58,467,840 bytes, no incremental warnings. Full93 audit passes
(`build/audit105.txt`), including all 32 seeds and seven boundaries.
Run105 PID20484 starts 06:17:32 on AMD hardware. EA Sports and MVP intro
visibly render (`frame.window003.bmp`, `frame.window019.bmp`), clean dispatch.
The complete screen-checked frontend route subsequently passes
(`build/route105.txt`), followed by bright stadium/matchup card and pitcher
introduction (`build/stadium105_partial_upload.bmp`,
`build/players105_partial_upload.bmp`). At 3,361 paths the dispatch/stub/crash/
USB-cancellation log remains clean. Three full traces complete, each 692
draws, at flips1707/1708/1709 (`build/frame105_traces_all/`). The initial
ten-second timing overlaps those verbose captures (0.1 FPS) and is not a
clean comparison. Repeating after tracing finishes measures 3.5 completed
frames/s in the stadium/player sequence (`build/fps105_stadium_untraced.txt`).
Its counters show 1,509 accepted GPU draws, 24 fallback attempts, 134 texture
uploads, zero shader compiles, 654 surface uploads / 267,820,376 bytes in
ten seconds. This is a different scene from build104's pitching sample;
matched pitching acceptance remains. Do not relink the live game; its MAP
and executable match. Full pitch-loop/exhibition acceptance continues.

Run105 subsequently reaches the standard pitching selector. The untraced
matched scene measures 1.6 completed frames/s (`build/fps105_pitch_untraced.txt`),
versus build104's 1.3. Ten-second counters record 1,547 accepted GPU draws,
45 fallback attempts, 128 texture uploads, zero shader compiles, 1,438 surface
uploads / 240,554,588 bytes (`build/renderstats105_pitch.txt`), down from
build104's 1.48 GB of surface uploads in its pitching sample. An 83 MPH
fastball changes the count to 1–0 (`build/pitch105_count.bmp`). Post-pitch
players animate, but next-pitch controls do not return during observation;
later A/Y report edges alone do not prove game-level acceptance. Main stacks
include D3D submission/flush polling (`build/stack105_postpitch.txt`). Stop
06:40:06 after 1,353.5 s / 3,423 paths, zero dispatch misses/stubs/crashes/USB
cancellations. Full pitch loop remains unverified.

### 2026-10-11 — build106 removes environment scans from command dispatch

Extend `tools_sample.py` with actual DLL names/RVAs and per-thread CPU time;
its attach mode leaves the game running, and suspension always resumes in a
finally block. The render worker's sixty-sample profile includes 21 samples
in ucrtbase, 17 at RVA 0x315D7 (`build/sample105_modules.txt`). Inspection of
the CRT loop and a suspended native stack identify environment-string scans,
with `nv2a_pb_exec_method+0x102` above `nv2a_pb_scan+0x240`. The executor was
calling getenv for its verbose flag on every parameter, including inline
vertex words, while that startup flag is unchanged.

Cache `RECOMP_PB_EXEC_VERBOSE` on first use, including the draw/flip checks.
Skip the survey getenv when execution is already enabled, and move the
renderer-mode getenv after the existing initialized check. Renderer shutdown
still resets initialization, preserving mode changes in native fixtures.
No draw, RAM coherence, guest timing or input semantics change. Software,
AMD/WARP full pushbuffer suites, hybrid CPU/GPU transitions, DMA packet/GET
fixtures and 1,880 decoded draws on each device pass (`build/render106_*.txt`).
Verbose-enabled execution still prints its method/draw diagnostics.

Explicit configure/build106 links 06:40:14, 58,467,840 bytes, no incremental
warnings. Full93 audit passes (`build/audit106.txt`). Run105 is stopped before
linking. Run106 PID7556 starts 06:40:21 on AMD hardware; EA Sports and MVP
movies visibly render (`frame.window003.bmp`, `frame.window009.bmp`). The
screen-checked frontend route and matched pitching comparison continue; no
build106 speed gain is claimed at this checkpoint. Do not relink this live game.

The complete screen-checked frontend subsequently passes (`build/route106.txt`),
followed by bright stadium, batter introduction and standard pitching controls
(`build/stadium106_dispatch.bmp`, `build/pitch106_dispatch.bmp`). Matched
untraced pitching measures 2.2 FPS, versus build105's 1.6
(`build/fps106_pitch_untraced.txt`). Ten-second counters show 2,459 accepted
GPU draws, 66 fallback attempts, 184 texture uploads, zero shader compiles,
477,632,232 readback bytes and 2,367 surface uploads / 352,044,784 bytes.
These per-second transfers rise with throughput; the cache change does not
relax coherence (`build/renderstats106_pitch.txt`). The matched sixty-sample
profile has no recurring ucrtbase environment hotspot on the render worker
(`build/sample106_pitch_modules.txt`). Main-thread samples still include
`sub_0057D530+0x1F3` and guest-lock waits. Its PFB write-combine flush spin is
the same 0x100410 bit16 handshake documented by NFSU2; our acknowledgment
loop still shares the synchronous renderer worker. NFSU2 separates flag
servicing and acknowledges that flush at poll hints. This is the next measured
lead; any repair must preserve DMA_GET-after-consumption and render completion
semantics. At 3,376 paths, zero dispatch/stub/crash/USB-cancellation failures;
full pitch-loop acceptance continues.

Run106 subsequently completes a pitch/count change to 1–0
(`build/pitch106_count.bmp`). Start still opens the pause menu
(`build/pause106_postpitch.bmp`), so the missing next-pitch controls do not
establish a general input stop. Start resumes; next-pitch acceptance remains
open. Read-only inspection of the matching loaded machine code confirms
`sub_0057D530+0x1F3` is `bt ecx,0x10` after the 0x100410 load, with a backward
branch to its preemption point (`build/flush106_disasm.txt`). The PFB lead is
therefore observed in the actual executable, not only inferred from the XBE.

Run106 later returns to the next-pitch selector at count 1–0
(`build/nextpitch106_selector.bmp`, frame.window475). A further A press delivers
an 84 MPH fastball and advances the count to 2–0
(`build/nextpitch106_count.bmp`, frame.window512). This accepts the complete
pitch-to-next-pitch loop and a second pitch, including pause/resume after the
first. The reset phase takes minutes at this throughput; earlier observations
ended too soon to establish a gameplay/input stop. Full exhibition remains
unverified. Stop 06:59:07 after 1,125.7 s / 3,380 paths, zero dispatch misses/
stubs/crashes/USB cancellations.

### 2026-10-11 — builds107/108 separate flag servicing and consume real fences

Following NFSU2's independent flag worker, extract the existing six busy and
two idle register semantics into `nv2a_ack.c`. A separate thread services them
while the executor renders. Masked compare/exchange preserves unrelated bits
and supplies the barrier before a flush acknowledgment; unchanged words use
volatile reads. DMA_GET, fences and frame completion remain outside this
worker. The executor also ticks flags as a fallback. Shutdown joins both
workers before freeing MMIO, guest RAM or KeTickCount backing; the earlier
one-second executor join could expire during a long batch.

New `nv2a_ack_smoke` reproduces the old combined-worker stall as a negative
control, then completes 128 flush requests while its rendering worker stays
blocked. DMA_GET/PUT and unrelated registers remain unchanged; start/stop,
duplicate-start rejection and restart with a new aperture pass. DMA walker
ownership and guest CPU/IRQL regressions pass (`build/ack107_*.txt`). Full93
audit107 passes. Build107 links 06:59:15, 58,468,864 bytes. The recompiled
memory-layout file reports its existing alignment/shadow/unreachable warnings.

Run107 starts 06:59:26 (PID7904), but parks on an early EA movie frame, clean
at 590 paths. Ten-second sampling verifies flips43/draws61 do not advance.
Main polls `sub_0057D3C0`, comparing PGRAPH_PATT_COLOR0 at 0xFD400B10 with
the semaphore's low progress bits. The registered semaphore 0x82815000 reads
201, while the MMIO marker remains zero. The consumed ring tail contains
subchannel5 method0x0310 = 0x507CB09C followed by method0x1D70 = 199
(`build/pb107_tail.bin`, `build/thread107_intro.txt`, `build/stack107_intro.txt`).
The executor ignored both progress methods, and the old mirror copied the
producer's count two releases ahead. NFSU2 documents the exact pattern-color
latch plus backend semaphore release. Stop107 at 07:07:34 after 488.4 s,
zero dispatch misses/stubs/crashes/USB cancellations. A frontend-helper timeout
was an observation result; the game was inspected alive before being stopped.

Build108 consumes the non-3D pattern 0x0310 into PGRAPH_PATT_COLOR0 and the
3D 0x1D70 release into the registered device-chain target. The periodic
producer-count mirror stands down with RECOMP_PB_EXEC; compatibility mode
without execution retains it. A 128-sequence/wrapped-marker fixture fails
before the pattern repair (256 routing failures) and passes afterward, with
ordered releases and subchannel isolation. Software and all 72 AMD/WARP
pushbuffer draws pass (`build/pattern108_*.txt`). Target resolution still uses
the registered device chain, like NFSU2's configured semaphore pointer; general
DMA-context semaphore addressing is not established by this test.

Explicit configure/build108 links 07:07:46, 58,469,888 bytes. Full93 audit108
passes, including all seven ranges and memcpy's 0x0044302D end. Run108 PID9180
starts 07:08:12 on AMD hardware. Both movies visibly render
(`frame.window003.bmp`, `frame.window011.bmp`), passing the run107 park with
zero dispatch/stub/crash/USB-cancellation entries at 591 paths. Frontend,
matched pitching and continuous gameplay acceptance continue. Do not relink
this live game; its MAP must remain matched.

### 2026-10-11 — build109 delivers PGRAPH software-method interrupts

Run108 reaches the title logo but never displays Press Start. At 2,186 paths,
all dispatch/stub/crash/USB-cancellation counters are zero. Flips5835 and
draws12109 remain unchanged over ten seconds (`build/fps108_title.txt`). The
main thread waits in `sub_0057D710` through KeWaitForSingleObject, while both
native GPU workers remain alive (`build/stack108_title.txt`,
`build/sample108_title.txt`). That original D3D routine submits WAIT_FOR_IDLE
then NOP(5), and waits on its device event. The executor ignored nonzero NOPs.
NFSU2's software-method interrupt repair documents this exact handshake.
Stop108 at 07:27:24 after 1,152.2 seconds, 2,186 paths, zero failures.

Build109 routes subchannel-zero NO_OPERATION(nonzero) to a hardware trap.
Publish TRAPPED_ADDR/DATA, NSOURCE, PGRAPH_INTR and PMC's PGRAPH source; the
existing timer's guest stack/TIB delivers vector0x33 through the connected ISR
and original queued DPC. The GPU remains independent, observing the handler's
PGRAPH_FIFO=0/1 entry/exit. Retire the aggregate source on entry to avoid
duplicate handling of plain-RAM W1C status, and stall command consumption
until handler exit. Never signal the game's event directly or time out a
pending trap. Shutdown cancels the wait before joining the GPU worker.
Nested, synchronized PGRAPH holds prevent the flag worker from clearing this
source; unrelated interrupts and PFB flush acknowledgment keep progressing.
Timer initialization uses INIT_ONCE because it now has a native GPU caller.

The new routing fixture fails before (two omitted nonzero traps) and passes
after, isolating NOP(0) and other subchannels. Thirty-two delayed asynchronous
handler fixtures verify pending-source retention, entry/exit retirement,
flush progress, unchanged DMA_GET/PUT and cancellation without fake completion.
AMD's 72 pushbuffer draws, DMA walker, guest CPU/IRQL ordering and concurrent
DPC queue fixtures pass (`build/trap109_*.txt`). Existing bridge/memory-layout/
USB warnings remain; no zero-warning claim. Explicit configure/build109 links
07:27:32 (58,470,912 bytes); full93 audit109 passes with all seven boundaries.
Run109 PID3008 starts 07:27:45 on AMD hardware. EA Sports and MVP movies
visibly render (`frame.window003.bmp`, `frame.window016.bmp`), and actual guest
software traps are handled. Frontend and matched pitching acceptance continue.
Do not relink the live game.

Run109 passes the title NOP(5) wait: Press Start appears
(`build/title109_ready.bmp`), Start opens jersey-number naming and A closes it.
The old frontend helper misses the pulsing selected Play Now lettering, so
idle attract mode starts. Start exits that demo and exposes six misses at
0x0013D210, no crash/stub/USB cancellation. A corrected screen check uses the
adjacent fixed Game Modes lettering. The resumed Play Now/teams/jersey/
difficulty route passes (`build/route109_resume.txt`), followed by the actual
player exhibition and pitching selector (`build/pitch109_selector.bmp`). A
fastball advances the count to 1–0 (`build/pitch109_count.bmp`). Main-menu flips
measure 13.7 FPS; pitching during concurrent disassembly measures 2.3 FPS,
which is not a contention-free matched performance comparison.

### 2026-10-11 — cycle110 recovers the attract-exit destructor thunk

The missed 0x0013D210 callback is an independent 11-byte/two-instruction body:
MOV [ECX],0x0061960C; JMP 0x0013B640. The preceding function ends exactly at
its entry and INT3 padding begins at exclusive end 0x0013D21B. The tail target
is already translated. A complete mechanical lift matches 10,000 original-x86
executions with a controlled tail callee, checking memory guards, registers
and guest stack (`build/callback110_native.txt`). Add seed33 and run the full
disasm/boundary-patch/lift cycle; build109 stays alive for gameplay acceptance
and must stop before the next link. Disassembly completes in 283.29 s and all
seven boundary patches are reapplied (`build/boundaries110.txt`), including
memcpy's 0x0044302D end. Full lift succeeds: 37,762/37,762 functions, zero
failed, 4,293,056 C lines and 38 chunks. Restore both USB cancellation probes;
audit110 verifies 33 sorted seed entries, all seven ranges and every previously
tested body, including the new destructor thunk, with no preflight chunk.
Run109 stops before the new build at 07:49:43 after 1,317.6 s / 6,269 paths,
six known destructor misses, zero stubs/crashes/USB cancellations. Pause and
resume pass (`build/pause109_postpitch.bmp`); second pitch/full exhibition
remain unverified. Explicit configure/build110 is compiling; Release build
outcome is pending here.

Read-only build109 profiling during concurrent disassembly shows the main
thread mostly waiting, rather than burning in PFB flush polling. The render
worker has 16/60 samples in win32u+0x60A4; the local DLL's export table maps
that to NtGdiDdDDIWaitForSynchronizationObjectFromCpu
(`build/win32u109_profile_symbol.txt`). Per-draw GPU readback synchronization
is the next measured renderer lead. Ten-second counters show 2,110 GPU draws,
58 fallback attempts, zero shader compiles, 162 texture uploads, 408,315,832
readback bytes and 2,015 uploads / 309,017,676 bytes. This does not authorize
dropping RAM coherence: any batching must synchronize at actual GPU/CPU/fence/
texture dependencies and pass native coherence comparisons first.

NFSU2 also preserves PCRTC/PMC vblank status until its queued DPC has the guest
CPU, then retires that source on the original handler's entry. Our independent
flag worker currently preserves only the PGRAPH trap bits. The original MVP
0x00580B60 handler increments its +0x1C0 counter and spins on PMC bit24 after
the W1C acknowledgment. Prepare `build/read_vblank110.py PID` to measure that
counter at ISR context0x0058B668, independently of rendered frames and window
refresh. Dropped vblanks are a code-supported timing lead; no causal or live
rate claim until measured.

Full build110 reports existing generated C4293 warnings in 0x005879C8.
Read-only inspection shows an imm_ref_target with no callers/prologue whose
bytes begin ASCII "SIZE", followed by floating constants (4.1588831,
1.4426950 and a negative coefficient table). This is a data-derived function
candidate, rather than evidence that the reached interrupt repair has shifted
incorrectly. Keep it recorded for disassembler/build-quality audit; no filter
or generated-body change is made during this acceptance cycle.

### 2026-10-11 — build111 fixes duplicate traps and lost guest vblanks

Full build110 succeeds and links 08:13:02 (58,470,400 bytes), with audit110
passing. Run110 PID16848 starts 08:14:49 on AMD hardware; both movies are
visible (`frame.window003.bmp`, `frame.window011.bmp`), and the title/naming
route passes (`build/route110_to_menu.txt`). Idle enters the batting minigame
demo. A 2.5-second Start is posted, but the latest capture still shows that
demo; this does not establish coverage of all six earlier baseball-demo
destructor call sites. Stop 08:35:21 after 1,231.9 s / 4,984 paths, zero misses,
stubs, crashes or USB cancellations. Preserve the clean run and its matching
MAP before compiling/linking the next build.

`build/vblank110_intro.json` measures the original driver's +0x1C0 counter:
18 vblanks in 10.001 s, only 1.8/s. Its retired index stays at two, while the
target/queue grow from 239218/239217 to 287576/287575. NFSU2's original-handler
wrappers document precisely this failure: plain-RAM W1C status lets the DPC
consume a single trap thousands of times before the GPU thread clears it.
FIFO=1 also precedes the handler's full return, and an empty call which began
before publication can write it without taking the new trap. This replaces
the earlier speculative timing lead with a measured defect.

Build111 opts this title into an explicit trap handoff. Publication and the
complete original PGRAPH handler share a lock; the handler acknowledges a
trap it found only after its original logic finishes, clearing NSOURCE,
PGRAPH bit0 and PMC bit12 before the DPC loops. A completion ticket, rather
than FIFO alone, releases the executor. Event waits avoid a busy core; stop
still cancels without signaling the title's event or faking completion. Both
original D3D handlers run at DISPATCH_LEVEL, excluding direct busy-wait calls
from racing their DPC counterparts. The runtime remains title-agnostic and
keeps the FIFO fallback for projects without original-handler hooks.

Vblank publication retains PCRTC bit0 and PMC bit24 until the driver's actual
W1C store. Service all three original PCRTC acknowledgment stores, including
the masked-vblank branch and initialization; this avoids a guessed grace
timeout and lets the original spin end immediately. A GPU ISR's queued KDPC
is tracked under the existing queue contract; absent/unresolved handlers drop
the undeliverable source. Independently held PGRAPH/vblank sources preserve
each other while unrelated flags and PFB flushes continue. The timer uses
rational 60 Hz deadlines with adaptive 1–10 ms sleeps, retaining phase after
ordinary delay and restarting without catch-up bursts after a missed period.

Native regression `trap111_native_before.txt` fails when FIFO=1 falsely
releases an unfinished handler. `vblank111_native_before.txt` fails when
flag servicing erases a queued vblank. The repaired suite passes 64 explicit
trap races/duplicate-loop checks, overlapping empty-call publication, 32
delayed legacy traps, protected-source cancellation, 10,000 deterministic
vblank handoffs and another 10,000 against the live flag worker, plus 60,000
phase-correct clock periods. Concurrent queue, guest CPU/IRQL ordering, DMA
walker and AMD's 72 full-pushbuffer draws pass (`build/*111_native*.txt`,
`build/pb111_amd.txt`). Fresh mechanical lifts of 0x00580B60, 0x005810D0,
0x00581310 and 0x005844BC match after undoing only the declared hooks
(`build/audit111_d3d_final.txt`); all 33 seeds, seven ranges and previously
tested bodies still pass the full cycle audit. Existing generated warnings
remain; no warning-free claim.

`mvp06/tools_patch_d3d.py` is now a required, idempotent, shareable post-lift
step. It finds definitions across chunks, renames only the two handler
definitions to `_gen`, and converts the three PCRTC stores to the MMIO helper.
Calls and dispatch entries retain original names. `--check` verifies without
writing. README, AGENTS and HANDOFF record this regeneration requirement.
Explicit configure/build111 is compiling; live movie/timing/pitch-loop
acceptance remains pending at this entry.

Release111 finally links 08:43:56 (58,472,448 bytes), all audits pass. Run111
PID10728 starts with the default software renderer, discovered from its worker
profile rather than assumed from the intended backend. Both movies visibly
render; original driver sampling records 600 vblanks / 10.005 s = 59.97 Hz,
with retirement advancing and targets near the clock. Its 4.0 movie FPS is
not comparable to previous hardware pitching. Stop after 221.9 s / 591 paths,
zero failures. The private acceptance launcher now explicitly selects and
records D3D11. No code or EXE change between these two runs.

Hardware run112 PID28596 starts 08:47:39. Both movies visibly render
(`frame.window003.bmp`, `frame.window026.bmp`), and the complete screen-checked
Play Now route passes (`build/route112.txt`). Bright stadium and player
close-ups render (`frame.window090.bmp`, `frame.window109.bmp`). At 3,414 paths
all four failure counters remain zero. The original driver now measures 598
vblanks / 10.000 s = 59.8 Hz; retirement advances and flip targets stay near
the current clock (`build/vblank112_intro.json`). This accepts the interrupt
timing repair on hardware. Stadium renders at 2.2 FPS; driver vblank rate is
not rendered FPS. Matched pitching, a second pitch and full exhibition remain
to be checked. Never relink this live game.

Run112 reaches the standard pitching selector (`build/pitch112_selector.bmp`).
An untraced, compilation-free ten-second sample measures **2.5 FPS**
(`build/fps112_pitch_untraced.txt`), versus run106's 2.2 in that view; treat
this short sample as the current rate, not a broad performance guarantee.
A 2.5-second A press throws an 83 MPH fastball and changes the count to 1–0
(`build/pitch112_count.bmp`). Start opens the original pause menu afterward
(`build/pause112_postpitch.bmp`); resume and the next pitch remain in progress.
At 3,417 paths all failure counters are still zero.

The matched view's native profile has 22/60 render-worker samples in win32u
(18 in the previously mapped CPU synchronization wait), while the main thread
mostly waits. Ten-second backend counters show 2,712 GPU draws, 80 fallbacks,
zero shader compiles, 231 texture uploads, 595,945,080 readback bytes and 2,485
surface uploads / 435,784,248 bytes (`build/sample112_pitch_modules.txt`,
`build/renderstats112_pitch.txt`). The remaining renderer bottleneck is still
per-batch synchronization. A safe first experiment is mapping the last
submitted staging copy first, so color/depth need only one completion wait;
retain both successful maps before committing either guest surface. This is
a proposed native benchmark, not yet a code change or accepted gain.

Run112 resumes from pause and continues rendering, measuring 2.9 FPS in a
later post-pitch sample. The original driver still records 600 vblanks /
10.006 s = 59.964 Hz with retirement advancing. A second 2.5-second A press
and an eight-second hold produce guest USB report edges, but the next-pitch
selector/count change remain unverified. Do not claim a full exhibition or
this build's full pitch loop. Stop 09:05:49 after **1,090.4 s / 3,425 paths**,
zero misses/stubs/crashes/USB cancellations; preserve the frontier in
`build/postpitch112_unverified.bmp`, `build/stack112_postpitch.txt` and the
matching logs/counters.

The proposed map-order change passes 1,880 decoded draws on each of AMD and
WARP, but paired uncontended depth benchmarks show no consistent gain (128²:
0.338/0.270 ms before, 0.368/0.424 after). Revert that experiment; do not relink
or claim a performance build for it. All results remain under `build/map113_*`.

The next build113 candidate packs drawn color and integer depth into one GPU
buffer and maps it once, enabled only by `RECOMP_PACKED_READBACK=1`. It still
resolves every batch before returning, preserves CPU edits/stencil/untouched
regions and retains the legacy staging path on failure. There is no deferred
RAM coherence or synthetic frame completion. Initial AMD/WARP tests pass
2,392 decoded draws each, including 512 exact random ARGB/Z16/Z24 pixels and
guard/stencil preservation; AMD's full pushbuffer suite passes too. Large-
surface benchmarks and main-build/live acceptance are pending here. No game
is live while benchmarking.

Repeated uncontended large-surface benchmarks run legacy→packed→packed→legacy
in the same native binary. On a 640×480 target, 32² depth draws measure
1.224/0.962 ms legacy and 0.960/0.823 ms packed; 128² draws measure 1.689/1.129
ms legacy and 1.250/1.022 ms packed. Both pairs favor packing (mean roughly
18–19%); small 128² targets remain noisy/mixed. AMD/WARP each pass 2,392
decoded draws and 72 full-pushbuffer draws; legacy AMD and hybrid transitions
also pass. Explicit configure/build113 links 09:21:16, 58,476,032 bytes.
The full-cycle and four fresh D3D body audits pass (`audit113_cycle.txt`,
`audit113_d3d.txt`, `hooks113.txt`). Run113 PID26492 starts 09:22:33 with
D3D11 and RECOMP_PACKED_READBACK=1 explicitly recorded. Both movies render
(`frame.window003.bmp`, `frame.window021.bmp`), the complete screen-checked
frontend passes, and stadium/pitching render. Matched untraced pitching is
3.0 FPS versus run112's 2.5; original vblanks measure 600 / 10.001 s = 59.994
Hz. Ten-second counters record 3,102 GPU draws, 94 fallbacks, zero shader
compiles and 2,838 packed readbacks. The render worker still has 20/60 win32u
samples, 15 in the mapped CPU synchronization wait. A first pitch changes the
count to 1–0 (`build/pitch113_count.bmp`), zero failures at 3,419 paths.

### 2026-10-11 — build114 preserves GPU depth around partial CPU uploads

Review of the packed path finds a coherence gap missed by earlier fixtures:
packed readback updates RAM and the depth shadow, but leaves depth_upload
stale. A later partial CPU edit updates only its dirty box, then CopyResource
replaces the entire GPU depth surface from that stale intermediate. The
existing test edited both inside and outside the preceding triangle, causing
its bounding box to refresh the observed pixel and hide the defect.

The new regression first draws a near red triangle, edits one depth pixel
far outside it, then draws a farther blue triangle. LESS must reject blue
against the unchanged near depth. Build113's packed AMD path instead renders
blue (`build/partial114_packed_before.txt`, exit1), reproducing the defect.
The source now seeds depth_upload from current GPU depth before partial CPU
updates; whole-surface edits avoid this extra copy. Verify Z16/Z24, inside
edits, legacy/packed AMD/WARP and pushbuffer/hybrid paths before relinking.
Run113 remains alive at this point, so main relinking must wait for its stop.

The repaired fixture passes Z16 and Z24 both outside/inside edits. Packed AMD,
packed WARP, legacy AMD and legacy WARP each pass **2,398 decoded draws**;
both devices pass 72 full-pushbuffer draws, and hybrid transitions pass
(`build/partial114_*.txt`). Run113 stops 09:36:58 after **864.4 s / 3,422 paths**,
zero misses/stubs/crashes/USB cancellations; pause and resume pass after the
first pitch, second pitch unverified. Explicit configure/build114 links
09:37:06 (58,476,032 bytes); cycle114, hook and fresh D3D body audits pass.
Run114 PID30564 starts 09:37:44 with D3D11/packed readback. Both movies render
(`frame.window003.bmp`, `frame.window021.bmp`) and the full screen-checked
frontend passes (`build/route114.txt`). Stadium/pitching acceptance is pending.

Read-only CRT sampling maps VCRUNTIME140+0x1E4D8 to the local memcmp AVX loop
(its export at +0x1F010 jumps to +0x1E490). A targeted 400-sample render-worker
profile catches 50 memcmp samples, 44 returning to changed_bounds with 2,560-
byte rows (`build/memcmp114_callers.txt`). Surface comparison is a measured
secondary cost alongside GPU synchronization; any scan optimization needs
benchmarks for unchanged and partially CPU-edited surfaces, not just a fast
clean-surface case.

Run114 reaches standard pitching (`build/pitch114_selector.bmp`) at **2.5 FPS**
untraced, with 2,534 packed readbacks in ten seconds. Its original driver
records 600 vblanks / 10.001 s = **59.994 Hz** after the first pitch. The first
pitch advances to 1–0 (`build/pitch114_count.bmp`). The next-pitch selector
returns at window440 (`build/nextpitch114_selector.bmp`); another 2.5-second A
press advances the count to 2–0 (`build/nextpitch114_count.bmp`, window480).
Pause/resume pass afterward (`build/pause114_postpitch.bmp`,
`build/resume114_postpitch.bmp`). Stop 09:56:53 after **1,148.9 s / 3,425 paths**,
zero misses/stubs/crashes/USB cancellations. This accepts the repeated pitch
loop with the corrected depth path and original 60 Hz clock. The reset phase
takes minutes at the current throughput; earlier shorter waits did not prove
a gameplay stop. Full exhibition/audio acceptance remain open, and the
corrected 2.5 FPS does not confirm build113's pre-fix apparent speedup.

### 2026-10-11 — reject scan and depth-copy timing experiments

A whole-surface memcmp shortcut reduces clean comparisons but can nearly
double a late partial-edit case. Sixteen-row comparisons bound that repeated
work, but paired timings on identical buffers, pinned to one CPU with thread
cycle counts, show only a small/mixed benefit (`build/bounds115_paired_*.txt`).
Both scan changes are reverted; no main performance build is made for them.

The next candidate preserves depth_upload immediately before the packed
readback staging wait, removing the next partial edit's preservation copy.
Packed AMD/WARP each pass 2,398 draws, including the new outside-edit depth
regression. A new `--benchmark-large-edits` mode in the native pushbuffer
fixture alternates a CPU depth edit outside the GPU draw and checks both
regions. Repeated uncontended control→candidate→candidate→control timing on
640×480 targets shows no consistent gain: 32² draws are 1.124/1.115 ms control
versus 1.105/1.093 candidate, while 128² draws are 1.361/1.292 control versus
1.506/1.297 candidate (`build/schedule115_*_edits_*.txt`). Revert that candidate
too. Build114's explicit partial-edit preservation remains the production
source; no main build115 or live speedup is claimed.

The whole-surface depth copy is required by D3D11's depth-stencil copy rules;
using a cropped CopySubresourceRegion directly on the depth target would
violate the API contract. See [Microsoft's copy documentation](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copysubresourceregion).
A future partial depth-upload draw or dependency-aware rendering batch must
retain the outside-edit/CPU/texture/fence coherence checks. No game is live
after run114's accepted stop.

### 2026-10-11 — bounded readback polling and target-size selection evidence

The previous turn made progress: build114 repairs a reproduced depth bug and
accepts two pitches/pause/resume in a clean 1,148.9-second run. No game is live
at the next continuation's start; the main EXE still links 09:37:06.

Native candidate116 adds an opt-in RECOMP_READBACK_POLL_US budget (default0,
maximum2,000 µs): Map(DO_NOT_WAIT), submit once if pending, poll until the
deadline, then use the ordinary blocking map. RAM commit and dual-map failure
behavior are unchanged. AMD passes all 2,398 draws with a 200 µs budget,
including 1,313 nonblocking completions and 1,085 blocking fallbacks. On a
640×480 target with a partial CPU depth edit per draw, 200 µs catches none
of 3,078 maps; 500 µs catches587 and 1,000 µs catches2,722. Initial 1,000 µs
timings favor polling, but reverse comparisons and CPU cost remain pending.
See [Map](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map)
and [Flush](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-flush)
for the actual API completion/submission contract. No main build116 yet.

The same large-target benchmark reveals that 32² six-stage/four-texture depth
draws take ~0.713 ms in software versus ~1.384 ms on the GPU. The 2,048
triangle-bounds cutoff came from 128² targets, where the fixed cost was lower.
Test a 4,096 cutoff on the unchanged accepted build114 before combining it
with polling. The private launcher now explicitly sets/records both cutoff
and poll budget, keeping runtime comparisons attributable.

Packed/legacy AMD/WARP each pass2,398 draws with polling or a1µs fallback
budget (`build/poll116_*render*.txt`, `poll116_legacy_*.txt`). Reverse timings
weaken the initial speedup:32² is1.094ms off versus1.090ms with1,000µs polling,
and128² is1.295 versus1.288ms, despite2,844/3,078 maps completing in the poll.
The extra spinning has no repeatable material benefit. Fully revert polling,
its counters and test changes; no main build116 is made.

Run116 PID11564 starts10:11:09 on the unchanged accepted Release114 EXE/MAP
(still09:37:06,58,476,032 bytes), with D3D11/packed readback and a4,096 cutoff.
This changes only CPU/GPU batch selection. The private launch metadata records
the cutoff, and the screen-checked frontend helper is waiting for the title.
Matched pitching and gameplay acceptance remain pending. Do not relink it.

### 2026-10-11 — cutoff acceptance and a measured grouped-readback foundation

Run116 on unchanged Release114 passes both movies (window003/window021), the
screen-checked frontend (`build/route116.txt`), stadium/players and pitching.
Matched untraced pitching records 28 flips / 10.002 s = **2.799 FPS** versus
114's 2.5. This short comparison does not justify changing the 2,048 default.
The next-pitch selector returns at count 1–0 (window488), another A hold changes
the count to 2–0 (window518), and pause/resume pass (window618/window661).
Stop 10:34:41 after **1,412.0 s / 3,429 paths**, zero dispatch misses, direct
stubs, crashes or USB cancellations. Evidence: `build/summary116_final.json`,
`nextpitch116_selector.bmp`, `nextpitch116_count.bmp`, `pause116_postpitch.bmp`,
`resume116_postpitch.bmp` and `fps116_pitch_untraced.txt`. No main build116.

Add an explicit single-consumer renderer begin/flush/end scope. Ordinary draws
still commit color/depth synchronously. Within a caller-owned exclusion scope,
draws retain GPU surfaces and union their conservative readback boxes; end
commits the union once. Target/layout changes, color/depth source and palette
aliases, overlapping color/depth storage and rejected later batches resolve
earlier work first. Read-only depth after earlier writes retains the pending Z
commit. CPU edits after an explicit flush are uploaded on the next draw,
including the accepted partial-depth preservation repair. Failed ordinary
readback invalidates/discards that one draw for software fallback; a failed
scoped flush remains pending and is fatal to its owner. Pointers must use the
canonical RAM mapping. Scopes cannot nest.

Native fixtures compare 32 queued draws byte-for-byte with the synchronous
reference, including blending, Z16/Z24, pitched depth and stencil/guard bytes.
Additional pixel checks cover target switches, interior color/depth texture
sources, palette dependencies, CPU edits after flush, overlapping color/depth
RAM and fallback. The first added fallback check accidentally retained LESS
against an equal existing Z and correctly failed; disable depth for that
color-only fixture. Final AMD/WARP, packed/legacy each pass **2,545 draws**
(`build/group118_{packed,legacy}_{amd,warp}.txt`); full pushbuffer each passes
72 draws and hybrid transitions pass. Cycle117 and D3D117 audits preserve all
33 seeds, 37,762 functions, seven boundaries and declared driver hooks.

On 640×480 targets, groups of 32 six-stage/four-texture repeated draws cost
0.029–0.049 ms per draw versus synchronous 0.782–1.060 ms in the final native
pair (`build/group118_bench_{scopes,sync}.txt`). Earlier forward/reverse pairs
also reproduce roughly 20× lower cost. All timing samples run with no game
alive. This establishes a backend opportunity, **not a live game speedup**.

Source review finds the integration constraint: the current CPU GIL is released
around passive kernel bridges, including guest buffer access. NFSU2's GIL has
the same boundary and deliberately leaves native GPU/audio workers independent
(`reference/nfsu2-sw/xboxrecomp/src/kernel/kernel_bridge.c`, GIL comment and
thunk dispatch). Mercenaries keeps GPU surface/alias records and uses depth
RAM download as a diagnostic fallback, but its different scheduling and
surface model do not establish safety for this runtime. Do not simply hold
this GIL around an entire DMA drain or disable readbacks. HANDOFF records the
needed memory, vertex/command, fence, software and interrupt barriers.

Explicit configure/build117 links 10:40:41 (58,477,056 bytes). Run117 accepts
both movies (window003/window021), full frontend and bright stadium (window087)
on the ordinary synchronous path, then stops 10:45:17 after **233.9 s / 3,357
paths**, zero failures. Build118 adds the final native overlapping-target
barrier, explicitly configures/builds and links 10:45:27 with the same EXE size.
Run118 PID4184 starts 10:47:09, D3D11/packed readback/cutoff 4,096. The game has
no call to renderer begin and retains synchronous commits. Live regression
acceptance is in progress; never relink it. Full exhibition/audio and playable
frame rate remain open.

Run118 visibly renders both movies (window003/window021), the screen-checked
frontend (`build/route118.txt`) and stadium (window092). Pitching controls appear
at window149; matched untraced pitching records **28 flips / 10.000 s = 2.800
FPS**, consistent with the cutoff comparison. A first A hold changes the count
to 1–0 at window200. The original driver records **600 vblanks / 10.000 s =
60.0 Hz**, with retirement advancing and targets near the current clock
(`build/vblank118_pitch.json`). Pause opens at window266. Final cycle118 and
D3D118 audits pass (`build/audit118_{cycle,d3d}.txt`). Resume/stop acceptance
is in progress; grouping remains unused by the game.

Resume passes at window305 (`build/resume118_postpitch.bmp`). Stop run118 at
10:58:26 after **677.3 s / 3,420 paths**, zero misses/stubs/crashes/USB
cancellations. Its repeated pitch loop/exhibition is not re-accepted; run116
remains the latest two-pitch acceptance. The final compiler log exposes a new
C4701 flow warning on dirty bounds behind the pending short circuit. The bounds
helper initializes its output on every call; explicitly zero-initialize the
local rectangle to make that invariant clear to MSVC. Configure/build119 links
10:58:34 (58,477,568 bytes), with no warning in this incremental build (not a
claim about all generated C). Final AMD renderer 2,545 and pushbuffer 72 checks
pass (`build/final119_{render,pb}_amd.txt`). Run119 starts a movie smoke.

Run119 PID27312 starts 11:00:17, D3D11/packed readback/cutoff 4,096. Both movies
visibly render (window003/window021/window049); title screen/Start passes the
screen helper at window059 (`build/route119_title.txt`). Stop 11:03:01 after
**164.2 s / 2,214 paths**, zero misses/stubs/crashes/USB cancellations
(`build/summary119_final.json`). No game is live. Final source/build119 has the
same synchronous game path; broad gameplay acceptance is118 and the latest
accepted two-pitch loop is116. Integrating scopes under complete target-memory
exclusion is the next measured performance lead. Full exhibition, playable
frame rate and audible audio acceptance remain open.

### Continuation120 — integrate bounded GPU scopes under independent RAM exclusion

The CPU GIL was insufficient because passive bridges retain buffer pointers
while suspending lifted execution. Add a separate shared RAM lease to the
lifted CPU, bridge bodies, APU/OHCI DMA, framebuffer copies and callback
preambles. Host/dispatcher waits copy arguments and drop all RAM nesting;
reacquire before touching guest storage. Dispatcher waiters release their
mutex before reacquiring RAM. The GPU only tries exclusive access and falls
back to ordinary synchronous draws if busy, including raised-IRQL CPU polls.
Never queue a GPU writer behind a CPU waiting for GPU completion.

Wire scopes into the actual DMA walker behind RECOMP_GPU_SCOPES=1 (default
off). Retain packet/CALL state across 4,096-word / 2-ms slices, scan unchanged
PUT backlogs, and stop before nonzero NOP parameters. Commit and release RAM
before invoking software traps/guest interrupt handlers. Flush before command/
vertex aliases, fallback, clear, pattern/semaphore release, flip and diagnostic
reads; backend texture/palette/target aliases retain their previous barriers.
Small batches can use hardware within a scope, amortizing the fixed readback.
Failure to commit is fatal, never permission to publish successful completion.

Native memory_scope_smoke passes bridge retention, wait release/resumption,
actual guest dispatcher waits, callback progress, a foreign GPU try while CPU
IRQL is raised and 2,000 two-field DMA commits with deliberate scheduling
between writes. Repeat with GIL off passes. Scheduler/IRQL/dispatcher/OHCI/
APU-output/framebuffer checks pass enabled; scheduler/IRQL/dispatcher repeat
disabled. Four AMD/WARP packed/legacy runs pass 2,546 renderer and 78 full
pushbuffer draws each, including pending command/vertex reads, fences, flips,
clears and software traps. Hybrid and bounded packet/CALL/trap tests pass.
Cycle120 retains all33 seeds/37,762 lifts/seven ranges, memcpy 0x44302D; D3D120
fresh-body audit passes. Explicit configure/build120 links 11:30:07,
58,479,616 bytes. Incremental build contains existing runtime warnings.

Default-off run120 PID10672 passes both movies and screen-checked title/Start
(window063), stops 11:33:23 after **149.7 s / 2,214 paths**, zero misses/stubs/
crashes/USB cancellations. Opt-in run121 PID16112 starts 11:33:24 for full
gameplay and matched untraced performance validation. No game FPS gain claimed
from the native scope benchmark or from this movie-only baseline.

Run121 passes both movies, the complete screen-checked Play Now route
(difficulty window071), stadium and pitching selector window149. Matched
untraced pitching is **3.000 FPS** (`build/fps121_pitch_untraced.txt`) against
118's 2.800, a small short-sample gain. A separate stadium sample is5.100 FPS;
do not relabel it pitching. Player-intro vblanks are59.77 Hz. First pitch
changes the count to1–0 at window204, with a clean log. Ten-second pitching
renderer stats still show2,239 accepted draws/1,846 packed readbacks, so most
draws pay individual synchronization. Source inspection: a failed GPU RAM
try drained the whole submission synchronously, delaying the next grouping
attempt. Bound that fallback to the same quantum when scopes are enabled;
keep default-off behavior unchanged. Also preserve unknown-word/runaway-ring
limits across bounded slices; native single-word unknown continuations and
4-million-word self-jump recovery pass (`build/native121_scan.txt`). These
source refinements are not in run121's loaded build120 executable yet.

Run121 pause/resume passes at windows221/242; stop11:42:07 after **523.3 s /
3,418 paths**, zero misses/stubs/crashes/USB cancellations. Repeated pitch
loop remains unverified in121. Explicit configure/build121 links11:42:17,
58,479,616 bytes, including bounded synchronous retry and retained runaway
limits. The POSIX host layer gains the corresponding try-exclusive SRW
primitive (Windows build unchanged; no Linux validation claimed). Run122
PID7652 starts11:42:30 with the same D3D11/packed/cutoff4096/scopes settings
to compare the retry refinement and continue pitch-loop acceptance.

Run122 passes both movies/full frontend (difficulty window069) and bright
stadium, then parks on the stadium overview. Stop11:53:20 after **649.8 s /
3,294 paths**, zero misses/stubs/crashes/USB cancellations. A ten-second
sample is **0 flips / 0 draws**; last drawn count56,367, GET=PUT=0x02859170.
The walker has no pending packet/CALL, no unknown words, posted/completed trap
tickets both5,321, last trapped method0x100 parameter5. Main waits at original
0x0057D86F on device event0x0058B80C, SignalState0. A frozen per-thread snapshot
shows no lifted CPU owner, all sampled RAM depths0 and no GPU writer. This
is not evidence of a RAM-lock deadlock. It may expose a command/event timing
race; causality is not established. Capture files: runtime122_park.json,
thread122_park.jsonl, fps122_stadium_untraced.txt and park122_stadium.bmp.

Reject and revert the bounded synchronous-retry change; retain the first
try-only scoped integration and the native-tested corrupt-stream protection.
No speedup or pitching acceptance claimed for122. Build122 explicitly
configures/links11:53:31 (58,479,616 bytes). Five actual Nt/Ke host-wait entry
points are now covered by memory_scope_smoke, including nested lease release
and blocking resumption until GPU commit; GIL-on/off both pass. Its first
fixture compile used the wrong local multiple-wait signature; corrected and
rebuilt successfully, no production-code change. Run123 PID4960 starts11:53:50
with scopes off for final source/build movie/title regression. Default remains
off. Next performance work must reproduce the event/command-patch ordering
under original 0x57D6xx / 0x580E60 before increasing scope opportunities.

Final run123 on build122 passes both movies (window003/window021), the
screen-checked title/Start at window061 (`build/route123_title.txt`), and stops
11:56:46 after **175.5 s / 2,214 paths**, zero misses/stubs/crashes/USB
cancellations (`build/summary123_final.json`). No game is live. Final source/
EXE122 retains synchronous default and optional first-scope integration;
latest broad scoped gameplay acceptance is121, latest two-pitch acceptance116.
Docs and git diff whitespace check pass (existing CRLF-conversion notices).

### Continuation124 — queued command publication versus GPU exclusion

Compare NFSU2's XDK5849 `D3D_BlockOnTime` wrapper and PGRAPH NOP(5) handler
(`reference/nfsu2-sw/src/recomp_manual.c`, device-context technical notes).
MVP's original 0x57D710 reads GET, clears device+0x1DD0, retrofits WAIT_FOR_IDLE
and NOP(5) into submitted commands, flushes and rechecks an 8 KB safety margin
before waiting at 0x57D86F. A 4,096-word exclusive GPU slice can advance16 KB
while forcing a preempted CPU to remain stopped. This introduces a command-
publication hazard that synchronous concurrent execution did not force.

New `nv2a_pb_race_smoke` uses the actual walker, guest CPU/RAM gates and
dispatcher wait. Force suspension after the producer checks GET: without an
outer reader, exclusive GPU work passes the patch slot and the real wait
times out; with a retained reader, try-exclusive declines, the synchronous
path permits publication, NOP(5) executes and the dispatcher wait succeeds.
Both outcomes are asserted, with GIL on and off (`native124_race*.txt`).
This reproduces the race class; it does not by itself establish that run122
experienced this exact interleaving.

Add generic `xbox_MemoryCall` to retain a reader across lifted preemption.
The title's 0x57D710 wrapper calls its mechanically lifted `_gen` body through
that helper; real host waits still suspend every nested RAM reference. The
required post-lift patch now renames three definitions (0x57D710, 0x580B60,
0x5810D0), retaining original direct calls/dispatch symbols, plus the same
three PCRTC W1C hooks. Fresh D3D124 audit compares all five bodies. Cycle124
passes33 seeds/37,762 unique lifts/seven boundaries, memcpy end0x44302D, zero
failed. Actual memory/wait and bounded scanner regressions pass again.
Reintroduce bounded synchronous retry only behind experimental scopes for
live validation with the publication guard. Default-off behavior is unchanged.

Build124 explicitly configures/links12:11:59,58,480,128 bytes. Run124 passes
both movies (windows003/009), title/Start062, full Play Now route through
difficulty073, stadium/team introductions092, pitcher card115 and selector144.
Then it parks:10 seconds0 flips/0 draws at295,894 draws, clean log. STOP12:20:42
after504.4 s/3,415 paths. Frozen CPU/RAM state is idle again; original caller
0x57D86F waits on event0x58B80C, SignalState0; GET=PUT=0x029D64B4, all7,698
traps completed. Device fence0xBBE9, semaphore0xBBE7, requested0xBB95. Saved
descriptor index10 is [0xBB95,0x828B4564]; its submitted words still contain
WAIT_FOR_IDLE0 and NOP5 at+0x10/+0x18. Capture full command buffer/device,
runtime/thread snapshots and fence124_park.json. The publication guard alone
does not make bounded retry reliable; no FPS gain or live pitch accepted.

### Continuation125 — preserve waits satisfied by SetEvent

The dispatcher previously only set SignalState and woke a condition variable.
A waiter blocked resuming RAM could observe a subsequent XDK inline clear
and lose an already satisfied wait. New dispatcher fixture deterministically
holds a GPU RAM writer across SetEvent and inline clear, preventing the
registered waiter from inspecting state until afterward. Before repair it
returns STATUS_TIMEOUT(type0,exit2); afterward notification and synchronization
event cases pass. Complete registered all-guest waits under the dispatcher
mutex when setting an event or releasing semaphore tokens, reusing the pulse
completion path. Already completed waits are immutable; future waits still
observe current guest SignalState. Mixed native/guest waits retain the prior
periodic path so native mutant ownership stays on the waiting thread.
This follows [KeSetEvent's wait satisfaction semantics](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kesetevent).

Dispatcher scopes-on/default-off, actual memory/wait fixtures and command-
publication fixture (GIL on/off) pass (`native125_*`). Explicit configure/
build125 links12:22:40,58,480,128 bytes. Run125 starts12:23:16 with the same
D3D11/packed/cutoff4096/scopes settings. The saved run124 fence evidence is
consistent with a lost wake, but does not establish this as its live cause.

Run125 still parks, at jerseys before the difficulty step completes. The
initial Start and a manual retry do not advance;10 seconds0 flips/0 draws at
21,368 draws. STOP12:28:26 after310.1 s/2,255 paths, zero misses/stubs/crashes/
USB cancellations. Original0x57D86F waits on the same unsignalled event; all
3,846 traps complete, GET=PUT=0x02890F44, idle CPU/RAM. Device fence0x525D,
semaphore0x523F, requested0x5245: unlike124, the semaphore is behind. Descriptor
index34 [0x5245,0x8285477C] still contains release0x5245 and the NOP5 patch.
Saved device/ring/thread/runtime/summary/fence files allow comparison without
assuming every stall has the same cause. The dispatcher fix is a native-proven
repair, not an accepted fix for this live failure.

Reject bounded synchronous retry again. Explicit configure/build126 links
12:28:35,58,480,128 bytes, restoring unbounded fallback while retaining both
native-tested correctness fixes and opt-in bounded exclusive scopes. Run126
starts12:29:56 with D3D11/packed/cutoff4096 and scopes OFF. Both movies visibly
render003/021; title/Start059 and full frontend through difficulty070 pass.
The final dispatcher fixture also covers registered notification/synchronization
WaitAlls: incomplete waits consume nothing, earlier cleared signals cannot
satisfy a later incomplete WaitAll, and genuinely completed waits survive
immediate inline clears. Enabled/default-off regressions pass again.

Final run126 advances through the stadium/team introductions (117 huddle,
145 pitcher card) and batter introduction179. A ten-second sample starting
there is2.100 FPS; it is renamed `fps126_batter_intro_untraced.txt` and excluded
from pitching comparisons because the capture was inspected too late. The A
input during the transition is accepted: windup196, first pitch/count1–0 at223.
Pause244 and resume267 pass. A separate post-resume ten-second sample shows
33 flips/15,155 draws,3.300 FPS (`fps126_postpitch_untraced.txt`); this is not
a matched selector comparison or claimed gain. Save count/pause/resume BMPs.
STOP12:39:59 after **604.0 s/3,416 paths**, zero misses/stubs/crashes/USB
cancellations (`summary126_final.json`, `stop126.json`). No game is live.
Final source/EXE126 retains the two native-proven correctness fixes, unbounded
synchronous fallback and default-off scopes. Latest two-pitch acceptance116,
latest broad scoped gameplay acceptance121. Full exhibition/playable FPS/
audible audio remain open. Final whitespace check passes apart from existing
line-ending conversion notices.

### Continuation127 — observe the actual failed fence wait

Add opt-in `RECOMP_MEMORY_TRACE=1` history:65,536 fixed64-byte records, serialized
sequence/ticks/thread/GPU-writer state, no per-record printing. Capture consumed
NOP/pattern/semaphore parameters before and after execution, scope boundaries,
parser resync, original D3D GET results, inline clear and NOP5 publication,
watched event sets and registered wait/completion/return. Three GET observations
and two store-preserving hooks in original0x57D710 are now restored by the
required patch helper and undone for fresh-lift comparison. Original logic and
preemption points match the mechanical lift. Cycle127 preserves33 seeds/37,762
lifts/seven boundaries, zero failed; D3D127 body audit passes. Native dispatcher,
scanner, publication and memory tests pass with tracing on; publication/memory
repeat GIL off. Explicit configure/build127 final links12:59:42,58,481,664 bytes.

The rejected retry is enabled only by explicit diagnostic `RECOMP_GPU_RETRY=1`,
still requiring scopes. Default retains unbounded synchronous fallback. Remove
the diagnostic retry branch again if the live cause is not repaired/accepted.
Run127 PID22512 starts13:00:36 with D3D11/packed/cutoff4096/scopes/retry/trace.
Both movies003/021 and full screen-checked frontend (difficulty069) pass. The
private reader successfully validates a stable wrapped history and watches
the canonical host event0x59B80C. Do not measure performance from this traced
run. `read_memory_trace127.py` needs the loaded EXE's matching MAP; analyzer
correlates the last watched wait with its requested fence/patch and actual
consumed parameters. Live failure diagnosis is in progress.

Run127 parks at difficulty:10 seconds zero flips/draws; GET=PUT0x02845DDC,
all4,877 software traps complete. Its final wait requests fence0x6495 while
semaphore0x648D is complete. The original CPU reads GET0x029FB1B8, clears the
event and publishes NOP5 at0x82A0BEB8, then registers its wait. Trace sequence
521676 resyncs parser GET0x029FC938 to the previous submission PUT0x02828F30,
skipping that fence and patch; subsequent releases begin0x64A3. This is not
evidence of an external GET writer: stderr identifies the parser's own
`lost sync #1 ... word00000000` recovery. STOP13:11:59 after682.9 s/2,255 paths,
zero misses/stubs/crashes/USB cancellations. Save memory127_frontier.json,
analysis127_frontier.txt, order127_skip.txt and full device/ring snapshots.

### Continuation128 — scope runaway detection to the current submission

The bounded scanner accumulated its four-million-word runaway budget until
GET caught PUT. A continuously busy producer advances PUT first, so valid
traffic eventually exhausted the lifetime budget and resynced over unexecuted
commands. Native continuous-producer regression keeps two valid words queued
while consuming one. Before repair, at exactly4,194,304 words it skips expected
GET0x104 to PUT0x108 and exits2 (`native128_scan_before.txt`). Reset only the
walk budget when the submitted PUT changes, retaining method/CALL state and
unknown-word accounting. Afterward the regression consumes over four million
words and retires the final semaphore0x6495/NOP5; unchanged-PUT closed-loop,
unknown-stream and invalid-target protections still pass, with tracing/scopes
enabled and disabled. NFSU2's reference walker uses a per-invocation word
budget, confirming that this limit originally bounded a fixed submission;
its synchronous implementation did not need to carry a budget across slices.

Explicit configure/build128 links13:12:10,58,481,664 bytes. Cycle128 and fresh
D3D128 audits pass (33 seeds/37,762 lifts/seven ranges, memcpy0x44302D).
Run128 PID19660 starts13:12:42 with the same D3D11/packed/cutoff4096/scopes/
retry/trace settings as127. Both movies003/021 visibly render; full frontend
through difficulty069 passes and advances into exhibition loading074, beyond
the prior stall. Explicit A advances loading into stadium introductions091.
Live acceptance is in progress; do not measure FPS with this history enabled.

Run128 reaches the pitching selector133 and accepts A. Pitch/count1–0 at166,
pause180 and resume227 visibly pass. Native trace verification finds242 fully
captured original clear/publish/wait, requested-semaphore release, consumed
NOP5, SetEvent, registered completion and successful return chains in the
last65,536-record history (`verified128_postpitch.json`). Runtime snapshot
reaches139,891,671 consumed words, no lost-sync entries. STOP13:21:13 after
**511.4 s/3,327 paths**, zero misses/stubs/crashes/USB cancellations. This
traced run establishes correctness progress; no FPS claim or two-pitch
acceptance. Saved pitch128_count.bmp, pause128.bmp and resume128.bmp.

### Continuation129 — accept bounded retry under opt-in GPU scopes

Remove the separate diagnostic RECOMP_GPU_RETRY branch/launcher flag. With
RECOMP_GPU_SCOPES=1, bounded synchronous slices retry RAM exclusion on the
next worker iteration; the corrected submission budget prevents valid busy
rings from being skipped. Default-off keeps the original unbounded fallback.
Explicit configure/build129 links13:21:22,58,481,664 bytes. Run129 uses the same
D3D11/packed/cutoff4096/scopes settings with memory history disabled, for live
acceptance and a correctly matched pitching measurement. Acceptance pending.

Run129 starts13:21:45 (PID24040), passes full frontend/difficulty069 and team
introductions094. Pitching selector132 and147 bracket the untraced10.001-second
sample:31 flips/15,414 draws, **3.100 FPS**. This is close to121's3.000 sample,
not an established speedup. A input is accepted; pitch/count1–0 at165. Second
pitch and final pause/resume acceptance are still pending. No lost-sync or
miss/stub/crash/USB-cancellation entries so far. Cycle129 and D3D129 audits pass.

Post-pitch performance evidence (not the matched selector FPS sample):10.001
seconds produces7,659 accepted GPU draws/111 fallbacks,1,734 packed readbacks,
623,409,056 readback bytes and522,274,072 surface-upload bytes. Roughly4.4
accepted draws per packed resolve leaves substantial synchronization/transfer
overhead. A separate60-round sample finds the command worker in ntdll23,
win32u14, VCRUNTIME6, packed_readback4 and AMD-driver4 of60 samples. Treat these
as leads for the next performance experiment, not a measured improvement or
proof of one dominant cause. Keep the accepted build unchanged through the
second-pitch check; compare any future slice/batching change in the same
pitching-selector phase and preserve RAM, alias and software-trap barriers.

Final129 second-pitch selector returns at360; the screen-checked helper posts
A after two matches (earlier no-selector controls score37.7 versus match1.65).
The83 MPH second pitch visibly advances the count to2–0 at375. Pause388 and
resume395 pass. Final runtime snapshot reaches280,510,683 consumed words;
there are zero lost-sync entries. STOP13:35:57 after **851.4 s/3,420 paths**,
zero misses/stubs/crashes/USB cancellations. Save nextpitch129_selector.bmp,
nextpitch129_count.bmp, pause129.bmp, resume129.bmp, runtime129_final.json,
summary129_final.json and stop129.json. This is the latest full two-pitch
acceptance on the corrected opt-in scoped path. Full exhibition, playable FPS
and audible audio acceptance remain open;3.1 FPS is not a claimed speedup.
Build129 is unchanged. Run130 checks its default-off movies/title path next.

Default-off130 starts13:36:32 (PID29828) on unchanged build129, memory history
off. Both movies003/021 visibly render and title/Start059 passes the two-capture
screen check. STOP13:39:04 after **151.8 s/2,214 paths**, zero misses/stubs/
crashes/USB cancellations/lost-syncs. No game is live. Final source/EXE129,
58,481,664 bytes, links13:21:22. Whitespace check passes (existing CRLF notices
only); all five handoff documents record the actual repair, accepted129
two-pitch path, default-off130 smoke and remaining performance/exhibition/audio
work. No commit or PR created.

### Continuation131 — measure actual batch-ending causes

Add single-consumer counters for committed resolves and callers: scope end,
external observer, command/vertex memory alias, target change, texture/palette
alias, unscoped draw and rejected batch. Scanner counters distinguish word,
time, software-trap and PUT endpoints. No per-draw logging. Remove the worker's
redundant require-flush immediately before end: end performs the same mandatory
commit, and failure still aborts before releasing RAM. Native AMD/WARP each
pass2,546 renderer draws in both packed/legacy modes, including all RAM/alias/
fallback barriers. Scanner busy-producer/corrupt-stream tests pass. Explicit
configure/build131 links13:42:30,58,481,664 bytes; cycle/D3D131 audits pass.
Untraced131 PID2560 starts13:43:10 with accepted129 settings for profiling.

Code inspection identifies unconditional zero-NOP resolves as a candidate.
NFSU2's corresponding handler traps only nonzero NOPs. New native pushbuffer
fixture queues a GPU draw, requires NOP0 to leave pixels queued, then checks
NOP5 commits before the software observer. Before repair it exits2:Zero NOP
unexpectedly resolves queued GPU pixels (`native132_nop_before.txt`). Preserve
all nonzero NOP, fence, pattern, flip, clear and vertex-alias barriers. Measure
the actual live distribution before attributing a performance gain.

Baseline131 selector172–184 measures2.900 FPS; repeated selector through222
measures3.100. The two10-second render samples have2,124/1,865 actual resolves:
617/523 scope ends,598/491 external barriers,909/851 unscoped draws, and zero
memory/target/texture aliases in either interval. Repeated scanner sample ends
750 slices at word limits,566 at time limits,129 before software traps, and
zero at PUT. Roughly27% external/28% scope/45% unscoped resolves; do not blame
aliases in this scene. STOP13:51:37 after507.3 s/3,418 paths, zero failures.

### Continuation132 — keep zero NOPs inside GPU batches

Only nonzero NO_OPERATION parameters now require a render commit and invoke
the original software-method path. Zero padding publishes no completion and
observes no target RAM. Retain pattern/semaphore/flip/clear/vertex-alias and
scope-end barriers, along with unchanged4,096-word/2 ms limits. The new native
fixture fails before, then passes after: NOP0 keeps pixels/readback bytes
unchanged; NOP5's observer sees committed pixels. All79 pushbuffer GPU draws
pass on AMD/WARP in packed/legacy modes, plus hybrid selection. Original
publication/preemption/actual dispatcher-wait regressions pass GIL on/off.
Explicit configure/build132 links13:51:50,58,481,664 bytes. Untraced132 PID22568
starts13:52:21 with identical settings to131; matched repeat measurements and
live pitch-loop acceptance are in progress. Do not claim an FPS gain yet.

The user independently controlled132, selected Kentucky/Texas A&M and advanced
the frontend; cancel the automated Florida selector gate. At14:03 the user
explicitly hands testing back. Read-only gameplay sample is2.100 FPS with
1,504 resolves/10 s (488 scope ends,319 external,43 target,653 unscoped),
552.8 MB readback and439.6 MB upload. Different teams/phase: this cannot
establish a gain against131. By window332 the count is1–0. PID22568 remains
live and clean (3,424 paths); further pitch/pause checks are pending.

After control is returned, selector433/443 visibly returns. Press A from that
screen; second pitch advances count2–0 at461. Pause491 and resume507 pass.
STOP14:10:24 after1,083.8 s/3,427 paths, zero misses/stubs/crashes/USB
cancellations/lost-syncs;339,903,630 words consumed. Save nextpitch132_selector,
nextpitch132_count, pause132_after_second and resume132_after_second BMPs.
This is user-assisted two-pitch acceptance (user selected teams/first pitch),
not a controlled FPS comparison or fully automated frontend run. Run133
PID10680 starts14:10:25 on unchanged132 to repeat Florida/Texas measurements.

### Continuation133 — matched selector and Windows audio meter

Unchanged132 passes the full screen-checked default frontend (title058,
names060, menu062, teams064, jersey066, difficulty068), stadium and Florida/
Texas pitching selector146. Untraced matched samples are2.900 and3.400 FPS;
no consistent zero-NOP speedup established. The second10-second render sample
has2,133 resolves (625 scope ends,450 external,74 target,984 unscoped),
692.1 MB readback/605.2 MB upload. Longer repeat pending.

User did not check audio. A private read-only Core Audio session tool finds
PID10680 active (state1), volume1.0, unmuted, max peak0.127807617 over2 s.
This establishes a nonzero signal at the Windows mixer, not audible quality
or correct commentary/music timing. Save audio133_meter.txt and its source.
The scanner's GetTickCount64 cutoff is coarse on this host: sampled increments
are15/16 ms. The nominal2 ms check is not a precise2 ms deadline; word limits
still bound slices. Keep this distinction in future timing/performance work.

Third matched selector sample133 is3.500 FPS over20.000 s. Combined40 s from
the three samples averages3.325 FPS; ranges2.9–3.5, versus131's two2.9/3.1
samples. This suggests a possible benefit but does not establish a stable
gain across matched repeats. Long render interval has4,292 resolves/20 s
(1,282 scope ends,906 external,150 target,1,954 unscoped). STOP14:19:00 after
515.3 s/3,413 paths, zero failures/lost-syncs;155.8M words. Measure-only run,
no pitches thrown. Save before/mid/after/long-after selector captures.

### Continuation134 — experimental larger exclusive word budget

Increase only successful GPU-exclusive batch slices from4,096 to16,384
words; keep the existing coarse2 ms clock check, nonzero software-trap stop,
all coherence barriers and shorter4,096-word synchronous retries. Default-off
path unchanged. This is an opt-in performance experiment, not yet accepted.
Expand the actual command-publication/preemption/dispatcher regression across
both word limits: unsafe publication reproduces a missed NOP5, protected
publication completes correctly. Both limits pass GIL on/off, alongside RAM
bridge/wait/DMA tests and scanner continuous-producer/corrupt-stream cases.
Explicit configure/build134 links14:19:37,58,481,664 bytes. Cycle/D3D134 and
whitespace audits pass. Matched live performance and pitch-loop checks pending.

Run134 PID2100 starts14:20:42, passes movies003/021, title059, full frontend/
difficulty069, stadium and pitching selector. The conservative image gate
rejects an actual selector at error8.96 versus threshold8; visual inspection
at172/178 confirms it. Cancel only the read-only gate, retain a current
selector reference and allow explicit --reference in the private helper.
Unified FPS/resolve sampling now uses the same interval, native-verified
GPU struct offsets (game draws270872/flips271000), with no history enabled.

Two matched20.004-second samples are2.550/2.699 FPS,51/54 flips. They have
2,689/2,842 resolves:544/570 scope ends,775/799 external,117/141 target,
1,254/1,332 unscoped. Resolves/frame fall to52.7/52.6 from133's61.3, but
throughput regresses. Do not attribute a precise cause without further trace.
STOP14:29:46 after544.3 s/3,410 paths, zero failures/lost-syncs. No pitch-loop
acceptance attempted after this performance rejection. REVERT the16,384-word
runtime experiment; retain4,096 and the expanded native publication fixture.
Build135 restores accepted132 behavior, then repeats the controlled route
and unified measurements. Latest user-assisted two-pitch acceptance remains132.

### Continuation135 — restore accepted budget and repeat acceptance

Explicit configure/build135 links14:30:27,58,481,664 bytes. Successful GPU
slices return to4,096 words with the original coarse clock check; runtime
behavior matches132. Retain the native4096/16384 publication coverage.
Cycle/D3D135 audits pass. Run135 PID20728 starts14:30:55, untraced, with the
same D3D11/packed/cutoff4096/scopes settings. Controlled unified performance
sampling and a fully automated two-pitch/pause repeat are pending.

135 passes movies003/021, full frontend/difficulty075, longer coach/player
introductions and confirmed selector182 (error3.74). Unified20.001-second
samples are2.450/2.750 FPS,49/55 flips, with3,353/3,338 resolves. This reverse
repeat is also slower than133 on the same runtime behavior, so the previous
claim that134's word limit caused the FPS regression is NOT established.
134 did reduce resolves/frame, but135's variable workload/timing prevents a
stable gain/regression conclusion. Keep the experiment reverted for lack of
an established benefit. Need direct CPU/GPU wait and RAM-scope duration
measurements before changing budgets again. Begin the screen-checked private
two-pitch/pause helper on PID20728 after performance samples finish.

First automated pitch135 passes count1–0 at257; wait for the returned selector.
After FPS measurements, a60-round/50 ms thread sample finds command worker
tid4968 in ntdll29, win32u14, VCRUNTIME6, sample_texture_for5, nv2a_vp_run2.
CPU time is1,984 ms over roughly3 s; flag worker consumes2,469 ms in busy
Sleep(0). Resolve against the actual local DLL exports: command-worker's
15-sample ntdll offset162664 is ZwCreateFile+14; offset161CC4 is ZwWriteFile+14.
win32u60A4 is NtGdiDdDDIWaitForSynchronizationObjectFromCpu+14. Save profile135
postpitch and system-symbol JSON. No claimed FPS result from debugger sampling.

Source inspection exposes measurement overhead: trace_current_batch opens the
missing RECOMP_FRAME_TRACE flag on EVERY draw. The private launcher enabled
this flag even in earlier runs called untraced (no active trace records or
memory history, but file polling remained). Change launcher default to omit
the flag, with explicit --frame-trace for captures, and record frame_trace in
launch metadata. Runtime135 is unchanged. After the135 pitch-loop acceptance,
repeat selector/profile on136 with flag polling disabled. Earlier FPS samples
must retain this qualification; do not compare them as fully tracing-disabled.

Automated135 returns selector461, posts second A, confirms count2–0 at472,
pause475 and resume481. Save six pitchloop135 BMPs/log. STOP14:48:44 after
1,068.9 s/3,416 paths, zero misses/stubs/crashes/USB cancellations/lost-syncs;
343,063,400 words. This is latest fully automated two-pitch/pause acceptance.
Source/EXE135 remains unchanged and native/cycle/D3D tests remain valid.

### Continuation136 — remove per-draw diagnostic polling from measurement

Run136 PID13868 starts14:48:45 on unchanged135, same rendering settings,
memory_trace=false/frame_trace=false explicitly recorded in launch metadata.
Only private launcher default changes; native trace capability remains explicit
via --frame-trace. Repeat screen-checked frontend, selector FPS/resolve sample
and system-export-resolved thread profile. Acceptance/measurement pending.

136 passes both movies/full frontend/difficulty071, stadium and selector139
(error1.86). Two unified20.001-second samples with both tracing flags false
measure3.500/3.450 FPS (70/69 flips),4,577/4,114 resolves. Readback1,417/1,390 MB,
surface upload1,217/1,190 MB per20 s. This is the corrected baseline, not a
claimed runtime speedup: EXE135 is unchanged and older FPS included flag I/O.
Core Audio still reports active/unmuted/gain1.0, peak0.192352295 over2 s.

After measurement,60-round selector profile command worker tid28624 has
22/60 win32u samples,11/60 ntdll, no previous15-sample CreateFile hotspot.
Improve tools_sample.py with optional --system-exports: parse installed
ntdll/win32u exports and attribute only matching<32-byte syscall stubs by RVA;
other internal addresses retain module labels. Live post-pitch136 verification
reports11 GPU synchronization waits,7 NtWaitForSingleObject,6 vertex-program,
6 VCRUNTIME,4 NtWriteFile and4 packed_readback samples out of60. Flag worker
has48 NtDelayExecution samples and2,219 ms CPU over roughly3 s. These guide
direct wait/scope timing and quieter diagnostics; no precise bottleneck share
or FPS claim from debugger samples. Save profile136_selector/syscalls.txt.

Automated136 confirms first selector195, first pitch/count1–0 at205 and waits
for returned controls. Second-pitch/pause/resume acceptance pending. No main
build136; source/EXE135, 4,096-word budget and all native-proven barriers remain.

Final136 returns selector366, second A advances count2–0 at376, pause379 and
resume384 pass. Save six pitchloop136 BMPs/log. STOP15:03:21 after876.2 s/
3,420 paths, zero misses/stubs/crashes/USB cancellations/lost-syncs;
344,856,329 words. This is latest fully automated two-pitch/pause acceptance,
with memory history and per-draw frame-flag polling disabled. No game is live.
Source/EXE135 unchanged; no main build136. All five handoff documents updated.
Next measure GPU commit/map and RAM-scope durations; separate verbose NV2A
diagnostics from execution, profile the busy flag worker, and compare any
future budget changes only with frame-trace polling disabled. Full exhibition,
playable FPS, sound quality/timing and original-Xbox comparison remain open.
No commit or PR created; persistent goal remains active.

## Continuation137–138: measure commit and RAM costs; separate debug output

Release137 links15:12:50,58,484,736 bytes after explicit configure/build.
Add default-off RECOMP_GPU_TIMING=1 aggregate QPC counters to the single-
consumer D3D11 renderer and atomic reader-acquisition/GPU-lease counters to
guest_memory.c. No history, per-draw I/O, new GPU wait or public stats ABI.
Readbacks split into submit/map/commit; preparation, texture, surface compare/
upload and dispatch are measured separately. Draw/flush totals overlap;
texture is inside preparation and compare inside surface. Reader time sums
across threads. Maxima are lifetime values, not maxima within a sample.
Default-off performs no QPC calls. RAM/ISR/coherence behavior stays unchanged.

Native137 passes2,546 renderer draws on AMD/packed and WARP/legacy,79 packed
AMD PB draws and software/WARP-off PB checks. RAM/bridge/wait/raised-IRQL/
2,000-DMA-commit and4,096/16,384-word publication/dispatcher cases pass GIL
on/off with timing enabled; RAM fixture also passes with timing disabled.
Cycle/D3D137 audits pass,33 seeds/37,762 full lifts/seven boundaries, memcpy
exclusive44302D. No disasm or lift rerun is needed for these runtime changes.

137 PID24516 starts15:13:49 with timing on, memory history/frame-flag polling
off, packed/scopes on, cutoff4,096. Movies visibly pass; frontend title057,
names059/menu062/teams064/jersey066/difficulty068 and selector150 pass.
Movie-only5 s sample29.77 FPS is not a gameplay comparison. Six20 s selector
samples toggle only s_nv2a_trace in the owned process using the matching MAP:

| Logging | FPS | GPU Map seconds | GPU RAM lease seconds | Reader acquisition seconds (summed) |
|---|---:|---:|---:|---:|
| On1 |3.150|6.079|9.885|28.731|
| Off1 |3.750|6.447|7.985|27.395|
| Off2 |3.650|6.409|7.665|25.419|
| On2 |3.200|5.988|9.627|30.759|
| Off3 |3.298|6.357|8.135|28.659|
| On3 |3.399|6.066|9.917|30.372|

The last pair reverses slightly; do not claim a reliable FPS improvement.
Across these diagnostic windows commits take7.15–7.60 s/20, including5.99–
6.45 s Map waits and1.01–1.08 s RAM copies. Preparation3.82–4.04 s includes
texture2.41–2.56 s; surface work2.52–2.75 s includes compare1.52–1.73 s.
Scope lifetime max219 ms/reader218 ms includes earlier scenes and does not
demonstrate a gameplay maximum. Existing2 ms check is coarse GetTickCount64,
not a strict deadline. Keep4,096 words/all barriers. Texture/surface work,
synchronous commits and CPU scheduling all matter; removing one wait cannot
alone establish playable throughput. Save render137_logging_*.json and
timing137_summary.json. The private reader now indexes MAP symbols once,
avoiding repeated regex passes before measurement.

NFSU2 PERF_NOTES.md/xbox_memory_layout.c confirms its busy-poll concern and
documents direct vertices/threaded raw rendering. Its Windows pause still
uses Sleep(0). This is a reference lead, not authorization to weaken this
runtime's RAM/observer barriers or transplant its old D3D11 backend wholesale.
137 STOP15:25:58 after728.1 s/3,422 paths, zero misses/stubs/crashes/USB
cancellations/lost-syncs,266,758,898 words. This is a selector/timing run;
no pitches thrown. The public sampler is unchanged this continuation.

Release138 links15:26:06,58,484,736 bytes. Decouple RECOMP_PB_EXEC from
verbose NV2A logging; the poll already enables execution independently.
RECOMP_NV2A_TRACE or RECOMP_PB_SCAN retains diagnostics. Do not infer a
speedup from this cleanup. Private launcher clears inherited survey/trace
flags and adds explicit --nv2a-trace/--gpu-timing, recording both in metadata.
Run138 PID7884 starts15:26:15 with timing/history/frame polling/NV2A tracing
off; a live counter read confirms every timing counter/frequency zero and no
verbose DMA/CRTC/checksum lines. Cycle/D3D138 audits pass. Both movies and
title064 pass; frontend/gameplay/performance acceptance continues.

138 full frontend names066/menu068/teams070/jersey072/difficulty074 and
selector146(error3.74) pass. Two20.001 s selector windows measure3.500/3.450
FPS (70/69 flips), identical to136's corrected baseline.4,366/4,398 resolves,
1.370/1.375 GB readback and1.222/1.211 GB surface uploads. All timing fields
remain zero. No FPS gain established; retain the execution/diagnostics
separation as a quiet-default cleanup. Automated test starts15:33:34:
selector207, first A/count1–0 at217, then wait for returned controls.
Core Audio session active/unmuted/gain1.0, peak0.126617432 (audio138_meter.txt).

Audio follow-up from source/read-only trace, not an audible-quality verdict:
APUOUTPUT counters reset approximately every2 s, not cumulative.136/137/138
often report18–27 rejected256-frame submissions in a roughly350-block
window. xa2_submit_samples returns false at three queued buffers (or a host
failure); its buffers allocate1,024 frames but monitor submits only256.
Thus three actual queued submissions cover16 ms at48 kHz, not64 ms. Audit
coalescing/pacing and queue saturation with a native sample-order/backpressure
fixture before changing audio. Do not classify these counters alone as
audible underruns or as proof of a RAM-scope cause. Audio quality/timing open.

Reference cross-check while138 resets after pitch: NFSU2's vendored
src/apu/apu_xaudio2.c already uses XA2_NUM_BUFS=12, explicitly12×256=64 ms,
plus separate full-queue/underrun counters. This is a concrete next candidate,
with a fake-source-voice burst/order/wrap/backpressure regression before live
validation; it is NOT applied in138. NFSU2 GL tex_get samples a byte hash and
checks once per frame. Mercenaries' nv2a_pgraph_d3d11.c validates compressed/
swizzled textures once per FLIP_STALL generation while checking linear/video
textures every draw. Do not copy either heuristic as an exact coherence
guarantee. First split this renderer's texture timer into compare/decode/create
costs. Any validation reuse needs proven complete RAM exclusion and epoch
invalidation at writes/commits, including aliases, clears and palette edits;
the generic renderer scope API alone does not promise all texture RAM is
immutable. Mercenaries' optional CPU depth-readback policy cannot replace the
mandatory RAM commits required by this hybrid renderer's observer tests.

Final138: returned selector391 at15:40:06, second A/count2–0 at402,
pause405 and resume411 PASS15:40:49. Visually inspect pause/resume captures;
both show Florida/Texas,0–0 score,2–0 count. Save six pitchloop138 BMPs/log.
STOP15:41:07 after891.7 s/3,426 paths, zero misses/stubs/crashes/USB
cancellations/lost-syncs;329,550,218 words, software posted/completed19,067.
This is latest fully automated two-pitch acceptance on source/EXE138. No
game is live; no C changes after138 build. Native137 and cycle/D3D138 audits
pass; four existing xbox_memory_layout compiler warnings remain. Update all
five handoff files. No commit/PR. Persistent goal active: full exhibition,
comfortable FPS, audio quality/timing and original-game comparison still open.

Read-only APUOUTPUT aggregation at15:39:08:136 rejects7,472/154,260 attempted
blocks(4.844%);1375,860/127,073(4.612%); live1386,213/136,116(4.564%). Save
audio138_output_summary.json;138 figures are a prefix before final stop.
These count rejected host submissions, not directly measured audible gaps.
The next bounded fixture should test real sink buffer ownership/order/wrap
under a fake source voice, before changing3 to12 slots and measuring live
submission failures/host signal. Do not apply reference gain/limiter filters
without separate evidence; retain the monitor's exact256-sample cadence.

## Continuation139: XAudio2 queue headroom with real-sink regression

Revalidate138 stopped/no game live and source queue3×256=16 ms. Add
tests/xaudio2_queue_smoke, compiling the actual apu_xaudio2.c against a fake
IXAudio2SourceVoice; no device/audio is opened. A64 ms scheduling burst fails
before repair on the fourth256-frame block (queued3/submitted3, exit1), saved
native139_queue_before.txt. Increase only XA2_NUM_BUFS3→12, matching NFSU2's
12×256=64 ms headroom; fix the storage-size comment (1,024 is capacity per
slot, not actual monitor submission size). No sample filtering, rate, mixing
or cadence change. Reference audio safety filters are deliberately not ported.

After repair, the same actual-sink fixture preserves3,246 FIFO sample blocks
through500 variable completion/backpressure/wrap cycles and simulated host
submission failures. It verifies copied payloads remain unchanged until
completion, no queued slot is reused, full/failing submissions retain buffer
ownership and a retry succeeds. APU hardware mix preservation,256-frame
cadence, stop/mute and voice/sample DMA/low-RAM fixtures also pass. Logs
native139_queue_after/output/dma.txt. Explicit configure/incremental build139 succeeds
without warnings; Release139 links15:46:50 (58,484,736 bytes). Cycle/D3D139
pass:33 seeds/37,762 full lifts/seven boundaries, memcpy exclusive44302D.

Run139 PID18648 starts15:48:26 with the accepted packed/scopes/cutoff4,096,
history/frame polling/GPU timing/NV2A tracing off. Initial movie windows show
375 blocks/96,000 frames per2 s and zero rejects. Both movie files open;
frontend, matched FPS and gameplay/drop-rate acceptance continue. Do not infer
audible quality or frame-rate gains from queue capacity or these early windows.

139 visibly passes movies003/021 and full frontend title058/names060/menu062/
teams064/jersey066/difficulty068; selector137(error3.74). Two20.001 s matched
windows with timing off measure3.950/4.150 FPS (79/83 flips), versus138's
3.50/3.45. No controlled reverse repeat: record observed FPS, not an established
causal speedup. Add --audio-log to the private read_render_stats.py to collect
complete APUOUTPUT records ending during the same FPS interval. Their~2 s
windows need not align to interval edges. Each sample has10 windows,3,531/
3,523 attempted blocks, zero rejected (7,054 combined), peak5,473/6,525.
Native timing counters/frequencies stay zero. Save render139_pitch_first/
repeat.json. Core Audio session active/unmuted/gain1.0, peak0.125396729.

Automated139 starts15:55:44, selector209, first A/count1–0 at218(15:56:04);
returned controls/second pitch/pause/resume acceptance pending. Rejection
windows remain zero after pitch. The generated frames per trace window can
still fall below nominal48 kHz; these records have no exact elapsed field.
Queue headroom fixes rejection pressure, not necessarily APU scheduling or
continuous audible delivery. Next audio diagnostics should measure actual
window duration/source queue and playback clock before claiming sound
timing/quality. Keep sample content/cadence and original voices unchanged.

Final139: returned selector370 at16:01:27, second/count2–0 at379, pause382,
resume387 PASS16:02:02. Visually inspect saved pause/resume BMPs: Florida/
Texas,0–0 score,2–0 count. STOP16:02:53 after867.9 s/3,427 paths, zero misses/
stubs/crashes/USB cancellations/lost-syncs; last snapshot371,857,489 words,
software posted/completed18,988. Full audio trace435 windows/153,762 attempted
blocks has ZERO rejected. Save audio139_final_summary.json, runtime/summary/
stop139 files, six pitchloop139 captures/log. This is latest fully automated
two-pitch acceptance on source/EXE139. No game is live; no source changes after
139 build. All five handoff files updated; no commit/PR. Goal remains active.

Do not equate this repair with complete audio accuracy: some trace windows
contain fewer frames than nominal production suggests, but current logs lack
exact elapsed/playback clocks and do not prove a scheduling cause. Measure those
before claiming continuous delivery, quality or original timing. Frames still
need texture-stage breakdown/optimization and broader rendering work; full
exhibition, comfortable FPS, saves and original-game comparison remain open.

## Continuation140/141: measured exact linear-ARGB bulk decoding

Extend default-off RECOMP_GPU_TIMING diagnostics with separate texture lookup,
exact source/palette comparison, decode (including destination allocation) and
resource/upload wall times, plus hits/changes/misses, requested comparison
bytes and decoded pixels by format. Requested comparison ranges are not actual
memory traffic because memcmp can stop early. Per-format counts record successful
decodes before resource creation. Timers overlap enclosing draw/prepare work;
no per-draw I/O or public stats ABI changes. The private reader also reports
retained texture-cache bytes. Default-off still makes no QPC calls.

Explicit configure/build140 succeeds; AMD/packed and WARP/legacy each pass
2,546 renderer draws, AMD/packed passes79 pushbuffer draws. Run140 PID29308
starts16:13:05, timing on, packed/scopes/cutoff4,096, history/frame polling/
NV2A tracing off. Movies003/020 visibly render; frontend title063/names065/
menu067/teams069/jersey071/difficulty073 and selector143(error5.90) pass.
STOP16:20:48 after462.7 s/3,417 paths, zero failures/lost-syncs;154,590,065
words and software posted/completed10,323. Audio231 windows/81,679 attempts,
zero rejects. No pitches attempted; latest two-pitch acceptance remains139.

Two matched20.001 s selector windows measure3.65/3.15 diagnostic FPS. Texture
decode1.377/1.309 s dominates lookup0.013/0.013, exact compare0.505/0.481 and
resource/upload0.376/0.387. Cache remains38.226 MiB with ZERO misses in both;
586/509 content changes decode111.277/96.333 million pixels. Linear ARGB0x12
accounts for106.427/92.138 million (512/444 decodes); swizzled R5G6B5 accounts
for the rest. Map remains6.297/6.611 s. A loading-only sample has many misses,
but that does not establish selector eviction pressure: retain128 cache slots
and128 MiB budget, discard the unbuilt larger-cache fixture experiment.

For valid linear ARGB0x12 only, render_texture_decode now copies width*4 bytes
per row from the existing pitched source into the decoded destination. These
are already identical ARGB bytes; skip repeated scalar wrapping/format/address
work. All other formats retain the scalar path; cache validation, resource
updates, target/palette aliases and guest-RAM/original ISR barriers are unchanged.
Add native bulk-decode comparisons against the unchanged scalar sampler:
625,422 texels across1x1,3x7,257x17,640x480,640x1 and1x480, tight and padded
rows, both addressing modes, zero/varied alpha, destination guards and a second
decode after CPU mutation. Invalid and unsupported descriptors retain failure.
The same512-iteration padded640x480 native benchmark measures2.450441 ms per
texture before and0.036920 ms after, identical checksum00000200. This is a
decoder microbenchmark, not a game-frame-rate gain.

Explicit configure/build141 links16:23:06 (58,485,248 bytes), no warnings.
AMD/packed and WARP/legacy retain2,546 exact renderer draws; both pushbuffer
paths pass79 draws and all new scalar comparisons, including timing-off WARP.
Cycle/D3D141 audits pass33 seeds/37,762 lifts/seven ranges, memcpy exclusive
44302D. Run141 PID24708 starts16:24:23 with timing on for matched live costs;
acceptance and default-off FPS/gameplay verification remain pending.

Run141 visibly passes both movies/title058/names060/menu062/teams064/jersey066/
difficulty068 and selector141(error4.04). Its two20-second selector samples
measure3.90/4.10 diagnostic FPS. Decode falls to0.545418/0.555467 s while
decoded work rises to119.596/123.253 million pixels; total texture work falls
to1.444/1.415 s. Exact compare0.500/0.445, resource/upload0.381/0.397 and Map
6.629/6.682 s remain. Both have zero cache misses and zero audio rejects
(7,164 attempts). The native before/after benchmark and live stage costs
support a decoder improvement; FPS causality is not established by these
varying runs, especially with diagnostics on. Remaining formats in these
windows are swizzled R5G6B5 (5.177/5.308 million pixels); measure its cost
separately before further bulk decoding. Do not change cache size based on
the loading-only misses.

STOP141 at16:31:30 after427.2 s/3,419 paths, zero failures/lost-syncs;
152,070,631 words, software posted/completed9,809. Audio214 windows/76,211
attempts/zero rejects. No pitches attempted. Same Release141 starts run142
PID25216 at16:31:59 with timing/history/frame polling/NV2A tracing off for
matched FPS and automated two-pitch/pause/resume acceptance; still pending.

142 passes movies/title058/names060/menu062/teams064/jersey066/difficulty068
and selector145(error3.74). Two20.002 s default-off samples are3.70/3.65 FPS;
all renderer/memory timing counters/frequencies remain zero and per-format
arrays empty. This overlaps earlier corrected baselines and falls below139's
3.95/4.15: do not claim a reliable FPS improvement. They capture7,028 audio
attempts/zero rejects. Keep the exact native/live-proven CPU-work reduction;
overall playability remains limited. Core Audio active/unmuted/gain1.0,
peak0.099822998. Automated helper confirms selector186, first A16:38:55 and
count1–0 at195(16:39:12); second selector/pitch/pause/resume remain pending.

After FPS sampling, a60-round read-only tools_sample attach briefly suspends
each thread; do not mix this interval into FPS. profile142_reset.txt records
flag worker2,093.75 CPU ms (47/60 RIPs in NtDelayExecution, source usesSleep(0));
GPU worker1,625 CPU ms (18/60 in the actual win32u driver synchronization stub,
11/60 in VCRUNTIME,4/60 packed_readback); main296.875 CPU ms (54/60 in
NtWaitForAlertByThreadId). This is sampled RIP attribution, not stack unwinding
or exact waiting duration. Keep readback/RAM/IRQ contracts. The remaining
Map6.6 s/20, surface work and polling worker deserve measurement before
another unproven global budget/sleep/cache change. At16:41:53 the reset
snapshot has214,538,985 words and software issued/completed12,404, continuing.

Final142: returned selector352 at16:45:03, second A16:45:05, count2–0 at362
(16:45:24), pause365(16:45:31), resume370(16:45:41), PASS. Visually inspect
saved pause/resume BMPs: Florida/Texas0–0 score,2–0 count. STOP16:46:14 after
855.8 s/3,420 path attempts, zero dispatch misses/direct-stub hits/crashes/USB
cancellations/lost-syncs. Snapshot324,153,610 words, software issued/completed
17,439. Full audio trace428 windows/151,634 attempts/zero rejects. Save six
pitchloop142 BMPs/log, runtime142_final.json, summary142_final.json, stop142.json,
audio142_final_summary.json and host meter. Latest fully automated two-pitch
acceptance is142 on unchanged Release141. No game live, no runtime source
edits after141 build. All five handoff documents updated. Goal remains active:
full exhibition, comfortable FPS, saves, sound quality/timing and original
comparison remain open. Keep measured decode savings separate from unproved
FPS improvement; prioritize the observed readback/RAM/polling costs next.

## Continuation143–146: readback attribution and GPU safe-point requests

Add default-off successful-readback counts/pixels/Map wall time/total wall
time by scope end, external observer, RAM alias, target change, texture alias,
unscoped draw, fallback and direct API flush. Power-of-two area histogram
retains the existing GPU/RAM/IRQ behavior. Native AMD/packed and WARP/legacy
each pass2,546 renderer draws; AMD pushbuffer79 draws. Release143 links16:51:15
(58,484,736 bytes), cycle/D3D143 pass. Run143 PID11468 starts16:52:13 with
timing on, history/frame polling/NV2A tracing off, packed/scopes/cutoff4,096.
Both movies/frontend and selector144(error1.93) pass. Two20-second samples
are3.15/3.10 FPS, Map7.802/7.514 s. Of4,302/4,143 readbacks, unscoped draws
cause2,343/2,263 and4.142/3.884 Map seconds; scope ends1.983/1.902, external
observers1.410/1.463, target changes0.267/0.265. Histogram/call totals and
Map attribution agree with aggregate counts. Many copies cover a few thousand
pixels. Diagnostics on FPS is not a product baseline.

Private matching-MAP setter changes only the running render_small_batch
cutoff, validates the executable path and expected old value, reads back the
new value and records every transition. Same-process20-second selector samples:
8,192a3.90→4,096a3.75;16,384a3.249→4,096b3.05;8,192b4.20→4,096c3.70;
8,192c/d4.10/4.30→4,096d/e4.25/3.90. Higher thresholds reduce unscoped maps
but the reverse samples overlap: NO reliable cutoff FPS gain. Restore and
verify4,096; retain existing default/recommendation. The native hybrid fixture
now accepts the three experimental thresholds and tests CPU→GPU→CPU color/
blend coherence with40 overlapping quads. Its initial64-quad expansion
overlapped its own render target; correct that fixture layout before accepting
the native results.2,048/4,096/8,192/16,384 all pass afterward.

STOP143 at17:09:59 after1,066.0 s/3,417 paths, zero failures/lost-syncs;
442,455,751 words and software posted/completed24,429. Audio533 windows/
189,524 attempts/zero rejects. No pitches attempted; latest two-pitch acceptance
remains142 on Release141. Save readback/cutoff143 samples, transition JSONL,
runtime/summary/stop/audio143 artifacts.

Investigate safe GPU cooperation instead of weakening the native-proven
BlockOnTime publication reader. A new native memory-scope case runs one guest
CPU and a GPU writer, with no other guest CPU to request preemption. Existing
code times out the GPU (native144_yield_before.txt, assertion/exitC0000409).
On failed exclusive try, set advisory g_guest_gpu_contended; clear on success.
Extend the existing generated/template safe-point macro to call preemption
for CPU or GPU contention. GPU-only yielding is allowed only with the sole
lifted-CPU RAM reader (depth1), retaining publication/bridge references and
raised-IRQL exclusion. Use the existing GuestSuspend/Resume path: do not drop
all nested RAM references, block an upgrade, remove a fence or skip a commit.
Default scopes-off never requests GPU yielding.

Afterward32 handoffs pass with GIL on/off, extra-reader and DISPATCH_LEVEL
guards, coherent paired writes and2,000 DMA commits. Existing missed-NOP
publication regression still passes4,096/16,384 words with GIL on/off; guest
scheduler/host-wait/ISR-order and APU DMA tests pass. Template and current
generated header both updated; cycle/D3D144 pass. Full Release144 rebuild is
in progress (build144.txt); no game live. Live behavior/FPS and default-off
acceptance remain unverified. Do not call this an FPS repair before measuring.

The first main144 invocation compiles generated chunks serially. Object
timestamps show successive multi-minute chunks; deliberately stop only its
verified compiler/MSBuild/CMake processes and preserve build144_serial_partial.txt.
Explicitly configure again and use invocation-local CL=/MP4 for four compiler
workers, retaining the same source and/O2 settings. This is a measured build
bottleneck decision, not a restart on an observation timeout; other projects'
builds are untouched. Replacement build144.txt is still in progress.

While compiling, extend the private read-only matching-MAP stats helper to
sample apu_xaudio2's existing g_xa2_frames_written counter over the FPS interval.
That misleadingly named counter increments once per accepted block; the sole
runtime caller supplies256 frames. Report active state, blocks and frames/s
alongside the existing approximately2-second trace-window reject counts.
No runtime instrumentation/source changes. This measures production accepted
by the sink, not SamplesPlayed, sound quality or continuous audible output.

The full header rebuild surfaces13 C4293 shift-count warnings in generated
chunk0035, bodies0055E99A and005879C8. Exact same count appears in builds110,
111,124 and127 (warnings144_history.json); incremental141 did not compile
this chunk. Record these as existing decoder/lifter issues for separate
ownership/immediate-count auditing, not a new GPU-yield warning. Do not silence
or change those bodies during this frozen-executable comparison.

Build144 succeeds, links17:59:23 (59,190,784 bytes). Total15 warnings are13
C4293 plus2 C4333, the same shift warnings/count as110; warnings144_history.json
also covers111/124/127. No new runtime source changes. Run144 PID9852 starts
18:01:06 with timing on, packed/scopes/cutoff4,096, history/frame polling/
NV2A tracing off. Visually inspect movie003 (EA SPORTS) and021 (baseball
intro); both render, no failures at591 paths. Frontend/selector/FPS pending.

144 completes title060/names062/menu073/teams075/jersey077/difficulty079 and
selector142(error1.93), visually inspected. Two20-second diagnostic samples
2.75/2.50 FPS show only1,209/2,684 and1,115/2,508 successful GPU lease attempts
(45.0/44.5%); Map10.080830/10.552759 s, unscoped5.909982/6.113256. Compared
with143's earlier samples this establishes no improvement; host/driver timing
also varies, so require a same-process reverse before causal claims. Exact
sink acceptance is3,445/3,372 blocks,44,092.339/43,156.993 frames/s (91.859/
89.910% of48k), with active sink and zero trace rejects. This demonstrates
why zero rejects cannot establish correct producer/playback timing.

STOP14418:08:53 after466.745 s/3,435 paths, zero failures/lost-syncs;
126,435,366 words, software posted/completed8,416. Audio233 windows/81,740
attempts/zero rejects. No pitches attempted; latest two-pitch acceptance142.

Add experimental RECOMP_GPU_YIELD=1 gate, default OFF, to the request path and
eligibility guard; no generated-header change or new disassembly/lift needed.
Native fixture now uses32 distinct paired values and also proves request-off
safe points retain the reader until an explicit GuestSuspend. Both modes pass
with GIL on/off, including extra-reader/IRQL guards,2,000 DMA commits and
publication4K/16K. Scheduler passes. Explicit configure/build145 links18:11:51
(59,190,784 bytes), no new incremental warnings. Private launcher defaults
GPU_YIELD off; --gpu-yield enables. Matching-MAP setter validates own PID,
executable path, loaded PE header versus disk, expected old0/1 value and
readback, changing only the advisory gate for the reverse comparison.
Run145 PID15980 starts18:12:50 with timing on, GPU_YIELD off, otherwise the
same packed/scopes/cutoff4,096 settings. Reverse and default-off acceptance pending.

145 passes movies003/021 (visually inspected), title063/names065/menu067/
teams069/jersey071/difficulty073 and selector132(error5.90). Interleaved
off/on/off/on/off phases each collect two20-second samples after2-second
settling; every0/1 change and restoration is verified/recorded. Off FPS pairs
3.10/3.00,3.05/3.25,3.25/2.90; on3.00/2.95,3.05/3.05. Weighted3.09146 off
versus3.01224 on, overlapping ranges: no reliable FPS benefit, keep default
OFF. Lease success49.185% off/44.739% on; readbacks61.658/60.544 per flip,
unscoped34.469/36.004, Map0.157843/0.164407 s per flip. Exact accepted audio
43,192.854/43,362.606 frames/s, zero trace rejects; no playback-quality claim.
Save gpu_yield145_comparison.json, ten sample JSONs and transition JSONL.

Restore/verify gate0. STOP14518:23:24 after634.237 s/3,416 paths, zero
failures/lost-syncs;218,665,927 words, software posted/completed15,129. Audio
317 windows/110,460 attempts/zero rejects. No pitches. Cycle/D3D145 pass33
seeds/37,762 lifts/seven ranges, memcpy exclusive44302D. Same Release145 starts
run146 PID22284 at18:23:43, timing/history/frame polling/NV2A tracing and
GPU_YIELD off, for matched FPS and automated two-pitch/pause/resume acceptance.
No runtime source edits after145 link. Full exhibition/comfortable FPS,
audio source/playback clocks, saves and original-game comparison remain open.

146 visibly passes movies003/021, title062/names064/menu066/teams068/jersey070/
difficulty072, selector133(error1.93). Two20-second default-off samples are
4.30/4.30 FPS; all renderer/memory timing counters/frequencies remain zero,
format/histogram arrays empty. This is a useful normal-run observation, not
a controlled build-to-build FPS gain. Exact accepted audio45,716.931/45,615.592
frames/s (95.244/95.032% of48k), zero trace rejects; host active/unmuted/gain1,
peak0.101348877. Playback-clock/listening quality remains open.

Initial pitch-helper wrapper redirects stdout to pitchloop146.txt, which the
helper itself opens: Windows rejects that file lock before any input. Preserve
traceback in pitchloop146_start_failure.txt and rerun using separate external
pitchloop146_helper.txt. Native/game source untouched; do not repeat FPS for
this harness-only repair. Helper confirms first selector19618:30:42, first A
18:30:45 and count1–0 at20518:31:01 (visually inspected). Return/second pitch
and pause/resume remain pending. Current reset still retires commands/fences;
do not infer a park from the several-minute animation at this frame rate.

Final146: returned selector36518:36:54, second A18:36:56, count2–0 at375
18:37:15, pause37818:37:22, resume38418:37:35, PASS. Visually inspect pause/
resume captures: Florida/Texas0–0, count2–0. STOP18:38:27 after884.243 s/
3,420 path attempts, zero misses/stubs/crashes/USB cancellations/lost-syncs;
367,699,808 words, software posted/completed20,983. Audio442 windows/156,776
attempts/zero rejects. Save six pitchloop146 BMPs/log, runtime/summary/stop/
audio146 files and source/EXE/MAP hashes. Latest fully automated two-pitch
acceptance is146 on unchanged Release145, GPU_YIELD default off. No runtime
source edits after145 link. All five handoffs updated; no game live.

Goal remains active. No reliable GPU-yield/cutoff FPS gain established; keep
both conservative defaults. Next measure remaining readback synchronization/
polling and audio RAM waits/throttle overruns/actual playback clock, preserving
publication, DMA, alias, CPU-observer and original ISR contracts. Full
exhibition, comfortable FPS, saves, sound quality/timing and original-game
comparison remain open. Existing generated shift warnings need a separate
ownership/immediate-count audit; do not suppress them as a performance fix.


## Continuation147–148: measure and repair APU device-clock pacing

Release147 links18:51:42 (59,191,808 bytes). Default-off RECOMP_APU_TIMING
records block deadlines, discarded lateness, throttle/RAM/pipeline wall time,
queue occupancy observations, accepted frames and a sequenced XAudio2 voice
clock every two seconds. SamplesPlayed is queried only with GetState flags0;
queue-only calls intentionally leave it unspecified. Microsoft documents
SamplesPlayed as processed samples and BuffersQueued as including the current
buffer: https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/ns-xaudio2-xaudio2_voice_state.
No listening-quality claim. Native fake clocks/voice validate default-off
versus enabled identical sample bytes, cadence and ownership;3,247 FIFO blocks,
500 wrap/backpressure cycles,14 host-failure retries and DMA pass. An initial
empty-observation assertion incorrectly assumed the fixture drained only once;
correct it to the measured zero-queue histogram minus initial startup.

Run147 PID26252 starts18:53:03 with only APU timing on. Intro production/playback
48,011/48,005 frames/s, zero resets/empty observations during20 seconds. Stadium
measurement39,652/39,676 frames/s,265 resets discard3.481116 seconds in20 seconds,
RAM acquisition7.954116 seconds and390 zero-queue observations, zero rejected
submissions. Repeat44,657/44,935 frames/s loses1.389774 seconds/132 resets,
RAM4.723402 seconds and243 empty observations. These are observations rather
than a direct gap-duration measure. Movie003/021 and stadium083 visually pass;
full frontend reaches difficulty070; selector142(error3.74).

The NFSU2 reference src/apu/apu_core.c paces its Windows producer by eight
queued256-frame blocks (~43 ms), within twelve64-ms storage slots. It yields
the APU mutex once per block to admit register writers even during catch-up;
RAM remains acquired only after throttle. Port that bounded device-clock
pacing with queue-only xa2_queued()/xa2_queue_target() helpers. Retain the
non-device wall-clock fallback, sample rate/content, monitor cadence, RAM
leases and DSP/IRQ contracts. New fake-queue regression fails against the old
producer (it sleeps despite an empty queue), then passes after repair for
empty-queue catch-up, full-queue drain and pause. All APU/sink/DMA native checks
pass with timing0/1; no new native warnings. Never relink the live147 process.
Selector baseline and live repair acceptance are pending below.

Selector14720s baseline:45,566 accepted/45,634 processed frames/s,102 resets
lose1.012089 s; RAM waits5.040182 s,235 empty observations, zero rejects.
Diagnostic-on FPS3.6, renderer/memory diagnostics off. STOP14718:59:13 after
369.565 s/3,413 paths, zero failures/lost-syncs;113,270,461 words and8,887
software commands completed.185 audio windows/65,497 attempted blocks/zero
rejects, active/unmuted/peak0.134949. No pitches in this diagnostic run.

Release148 links18:59:19 (59,192,320 bytes), no new warnings. Runtime-only
change is device-queue pacing; no generated/lifter/seed changes. Cycle/D3D148
pass33 seeds/37,762 lifts/seven ranges, memcpy exclusive44302D and all original
D3D mechanical bodies/declared hooks. Run148 PID7900 starts18:59:32, APU timing
on, other timings/history/frame polling/NV2A trace/GPU_YIELD off. Intro20s
47,999 accepted/47,999 processed frames/s, no lost time/rejections/empty
observations, queue mostly6–7 pre-submit blocks. Stadium and selector still
pending; do not confuse diagnostics with normal gameplay acceptance146.

User asks to reach a pausing point before148 reaches the selector. Finish
the in-flight stadium measurement and close the owned game and selector
helper.148 stadium20s:46,103 accepted/46,102 processed frames/s, zero deadline
resets/discarded time/rejections; RAM waits4.761299 s, pipeline0.422238 s,
throttle14.813234 s,14 empty pre-submit observations. This remains below48k;
queue pacing removes the deadline-reset defect but long stalls can still
exhaust headroom. Do not claim continuous sound or a controlled stage-to-stage
performance gain. Next measure long RAM-acquisition stalls before changing
leases/headroom. Default-off two-pitch acceptance is also still required.

STOP14819:03:16 after224.089 s/3,325 paths, zero misses/stubs/crashes/USB
cancellations/lost-syncs;36,129,492 words, software posted/completed4,462.
Both movies003/021/full frontend through difficulty072/stadium104 visually
pass.112 audio trace windows/41,287 attempted blocks/zero rejects; host
active/unmuted/gain1/peak0.124511719. No selector, pitches or148 FPS acceptance
is claimed. Latest full two-pitch/pause acceptance remains146 on older145.
Release148 is current, native-tested and partially live-tested; no runtime
source edits after its18:59:19 link. Stop metadata, summaries, voice timings,
CoreAudio meter, source/EXE/MAP hashes and captures saved. No game/helper live.

All five handoffs updated for the user-requested pause. Resume unchanged148
with normal run149: packed readback/scopes/cutoff4096, all timing/history/frame
polling/NV2A tracing/GPU_YIELD off. Route through selector, measure FPS and
accepted production, then two-pitch/count2–0/pause/resume acceptance. Preserve
all RAM/publication/CPU observer/alias/IRQ barriers. Full exhibition, comfortable
FPS, saves, sound continuity/quality and original-game comparison remain open.


## Continuation149–150: normal queue-pacing acceptance and remaining APU stalls

Resumed2026-10-12 per user goal continuation. Previous turn made progress:
measured deadline loss, implemented native-tested queue pacing and saved a
clean partial148 run, then paused at user request. Revalidate the actual
worktree, absence of a live game and all source/EXE/MAP148 hashes. They match.
Run149 PID23444 starts08:06:26 on unchanged Release148, packed readback, GPU
scopes, cutoff4096; APU/GPU timing/history/frame polling/NV2A tracing/GPU_YIELD
off. Full two-pitch/pause acceptance and matched FPS/production are pending.
No relink while149 is live.

Prepare unbuilt150 diagnostics in apu_core.c during149: default-off maxima
and20 logarithmic bins for raw32-frame RAM-acquisition wall time and completed
256-frame production intervals, plus pipeline wall-time maximum. Reuse the
existing diagnostic timestamps; no new clocks or histogram writes when off.
These are wall times including descheduling, not exact lock-held or CPU-time
attribution. The live149 EXE remains148; do not confuse new source with it.

149 visibly passes movies003/021, frontend through difficulty070 and
selector135(error5.90). Two20-second normal samples are3.90/3.649 FPS, accepted
audio47,969.716/48,005.793 frames/s, zero rejects; all renderer/memory timing
counters and frequencies zero. read_audio_timing147.py --seconds0 verifies
APU counters/voice snapshots/sink diagnostics zero as well. This is a useful
normal-run measurement, not a controlled FPS gain claim or a playback proof.

Pitch helper confirms first selector20008:13:31, sends A down/up08:13:33 and
visibly confirms count1–0 at21208:13:57 (83 MPH fastball). User asks for a
pausing point while waiting for second selector. Stop the owned helper between
inputs; it is explicitly terminated, not failed on an assertion or deadline.
STOP14908:15:07 after521.363 s/3,417 paths, zero misses/stubs/crashes/USB
cancellations/lost-syncs;188,023,527 words. Software posted11,226/completed11,225
in the pre-stop snapshot; one pending observation is not a park. Audio260
windows/96,886 attempted blocks/zero rejects, active/unmuted/gain1/peak0.129150391.
Second pitch/pause/resume on148 remain unverified; latest full acceptance146
uses145. Save first-selector/count captures, FPS/audio data and stop/summary.

After closing149, explicitly configure and link Release15008:15:15
(59,192,320 bytes), no new warnings. All APU output/sink/DMA native cases pass
timing0/1; after adding diagnostic thread ID, output fixture passes again
both modes. Cycle/D3D150 pass33 seeds/37,762 lifts/seven ranges, memcpy end
44302D, original mechanical bodies/declared hooks retained.150 only adds
diagnostic counters/maxima/histograms/thread identity; no live150 test yet.
No runtime source edits after150 link. No game/test helper remains live.

NFSU2 also raises its APU thread to THREAD_PRIORITY_HIGHEST (relative2 in a
normal process); Microsoft documents relative thread priorities and required
query/set rights: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority
and https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadpriority.
Private set_apu_priority150.py is prepared but untested: verifies loaded PE/MAP,
owned diagnostic APU TID, normal process class and expected old priority. No
priority change made. Measure histograms first; only perform a same-process
0/2/0 reverse if they support the lead, restoring0. Diagnostics measure wall
time including descheduling, not exact CPU/lock-time attribution.

All five handoffs updated for requested pause. Next unchanged150 normal
run150, full two-pitch/count2–0/pause acceptance and selector measurements;
then151 --apu-timing with read_audio_timing150.py for the remaining audio
stalls. Keep4,096/coarse2 ms,128 slots/128 MiB, GPU_YIELD off and every RAM,
publication, DMA, CPU-observer, alias and original ISR barrier. Full exhibition,
comfortable FPS, saves, audio continuity/quality and original accuracy remain
open. Goal paused at user request, not complete.


## Continuation150 (2026-10-19): revalidation and normal pitch-loop acceptance

Resume per user goal continuation. Previous turn made progress: normal149
first pitch/count1–0 and near48k accepted production, plus built/native-tested
default-off timing histograms150; paused at user request. Revalidate current
worktree and absence of a live game. Saved150 source/EXE/MAP hashes all match
before explicit configure/build. The build relinks14:39:44 (59,192,320 bytes)
without any source recompilation. Original source hashes still match, EXE/MAP
change; preserve original150 hashes and record build150_resume_hashes.json.
Cycle/D3D150_resume pass33 seeds/37,762 lifts/seven preserved ranges, memcpy
exclusive44302D and original mechanical bodies/declared hooks. No runtime edits.

A launch hash guard correctly stops on the new binary before creating a game.
The wrapper mistakenly continues to the route helper with no PID; argparse
rejects before any input. Preserve route150_start_failure.txt, then fix the
wrapper to require launch metadata/PID before routing. The read-only selector
helper remains live waiting for150 captures; no restart on observation timeout.
Normal150 PID14068 starts14:41:02, packed readback/scopes/cutoff4096, all APU/
GPU timings/history/frame polling/NV2A tracing/GPU_YIELD off. Never relink live.
Full two-pitch/pause acceptance remains pending.

150 visually passes movies003/021 and frontend through difficulty051.
Selector107(error1.72) confirmed. Two normal20-second samples4.35/4.00 FPS;
accepted audio47,996.038/47,970.253 frames/s, zero rejects. All APU fields/
histograms/thread identity and GPU timing counters/frequencies remain zero
when off. No causal FPS gain claim. First selector14914:46:19, A down/up
14:46:21, count1–0 at15714:46:35 visibly passes (83 MPH). Waiting for next
selector; no second pitch/pause acceptance yet.

Extend only apu_output_smoke with --state-layout: native offsetof/sizeof JSON
for MCPXAPUState fields avoids mirroring a packed C layout in Python. Its
normal output/mute/stop/pacing case still passes; main game EXE untouched.
Private read_apu_state150.py checks PE/MAP and stable g_state, samples modes
without suspension or writes. Run after FPS and first pitch:3,901 samples
over20 s (~195 Hz actual), all FECTL100F/mode0, SECTLF3,724 samples/7 in177,
pause_requested0;30,000 raw32-frame iterations (~1,500/s). This does not
rule out brief unsampled traps or prove audible quality. No trap/priority/
RAM or sample behavior changed based on the observation.

Native layout CLI now exports FECTL mode maskE0 and SECTL XCNTMODE mask18
as constants, rather than duplicating them in the reader. SECTL7 has XCNTMODE
off;F has it on. Remove an overly narrow initial SECTL0/F sanity assertion
that would reject a legitimate7 on a later invocation; first probe startedF
and its recorded7 observations remain valid. Second20s probe3,401 samples
(~170 Hz), all FECTL100F/mode0, SECTLF3,311/7 in90 (~2.65% pipeline-off
observations),30,008 iterations (~1,500/s). These are externally sampled
states, not exact generated-frame/silence counts or proof of a sound gap.
No FECTL, SECTL, priority, RAM or sample behavior changed. Native normal case
still passes after adding the layout constants; no main executable relink.
During the between-pitch wait, snapshots show41,876,203 more command words
and2,069 more software commands posted/completed: processing progresses.

150 final: second selector30714:52:04, A down/up14:52:06, second count2–0
at31614:52:23, pause31914:52:29, resume32414:52:39 PASS. Pause/resume captures
visually inspected (Florida/Texas0–0, count2–0). STOP14:53:49 after766.605 s/
3,420 paths, zero misses/stubs/crashes/USB cancellations/lost-syncs;347,889,013
words, software posted17,197/completed17,196 in pre-stop snapshot.383 audio
windows/143,046 attempts/zero rejects. CoreAudio active/gain1/peak0.109954834
but SESSION MUTED; do not claim listening or unmuted output. Preserve that
user/session state. Latest fully automated two-pitch/pause acceptance is150.


## Continuation151: preserve mixed active/off APU subframes

Code inspection follows the live SECTLF/7 transitions: DSP passthrough writes
one32-sample stereo slice per raw frame, but the monitor decided whether to
clear the whole256-frame block from the final register state. If only the
last slice is stopped, seven valid slices are discarded. If an earlier slice
is stopped and the final slice runs, the skipped slice retains the previous
block bytes. This is separate from device-clock pacing or a front-end trap.

Extract the existing frame branch into apu_run_audio_frame with unchanged
behavior, then inject deterministic VP mixbins into the real DSP/monitor in
existing apu_output_smoke. Four baseline patterns: fully active0 mismatches;
last-slice off448 channel mismatches; mid-block off128 mismatches; all off0.
Native151_before exits1. Repair by preparing/zeroing only the current32-frame
slice before processing, and tracking all eight freshly prepared slices. A
complete fresh block preserves earlier valid hardware audio even if the final
slice is off. Standalone stopped/stale monitor calls still clear all; mute
and tone retain whole-block behavior. Software mixing,256-frame cadence,
sample rate, queue ownership/pacing, RAM leases, scheduler/IRQs unchanged.
Append one freshness byte at the end of monitor state: sizeof+8 due padding,
all existing field offsets preserved; native --state-layout confirms this.

All256 active/off patterns now pass131,072 PCM channel comparisons; fresh
muted blocks and original stop/cadence controls pass. Sink3,247 FIFO blocks/
500 wrap/backpressure cycles/host-failure retries and DMA pass timing0/1.
Default-off counters record actual off/front-end-stopped raw frames and
partial blocks whose valid nonzero hardware audio the old monitor erased.
The native pattern sweep checks exactly1,024 off frames and127 preserved
partial blocks when enabled, zero diagnostic counters when off. No extra
clock or buffer scans when timing is off. read_audio_timing151.py reads these
counters plus150 histograms/voice clock, preserving the old150 reader.

Release151 links14:58:23 (59,192,832 bytes). Explicit configure before build;
no new warnings. Cycle/D3D151 pass33 seeds/37,762 lifts/seven preserved ranges,
memcpy exclusive44302D and original D3D mechanical bodies/declared hooks.
Normal full acceptance150 is preserved;151 live acceptance still pending.
Diagnostic151 PID16520 starts14:59:08 with only APU timing on; packed readback/
GPU scopes/cutoff4096, other diagnostics/GPU_YIELD off. Never relink live.
NFSU2 priority candidate remains untested; no priority change. --layout allows
the read-only state probe to use fresh native metadata after header changes.

151 intro003/021 visually pass;20s47,982 accepted/47,998 processed frames/s,
zero empty observations/rejections,6 preserved partial blocks,6,934 off raw
frames/30,008, no front-end-stopped frames or discarded deadline time.
Follow-up filename audio151_stadium.json was premature: it actually covers
frontend/transition, so preserve it and use audio151_frontend.json alias.
Do not label it a pure stadium comparison. That20s47,996 accepted/48,018
processed,22 preserved partial blocks,6,831 off/30,000 raw frames, no front-end
stops/empty observations/rejections; RAM0.472147 s/pipeline0.348219 s. Maxima
are lifetime (including earlier scenes), not per-window maxima.
Frontend through difficulty078 passes; selector096(error3.74) confirmed.
Now measure selector audio/interval histograms before any priority experiment.
Session is still at its existing user mute preference; no listening claim.

Selector151 first20s:46,487 accepted/46,485 processed frames/s,93 empty
observations, zero rejects/resets,1 preserved partial block. RAM acquisition
2.842081 s/pipeline0.409995 s,1,046 off/29,056 raw frames, no front-end stops.
Window RAM histogram has17 waits in16–32 ms and none above32; block intervals
have2 in32–65 ms and none above65. Lifetime RAM/block maxima263.579/263.839 ms
include earlier scenes and must not be assigned to this window. Diagnostic-on
FPS3.05, not a normal-run baseline.

This supports the prepared NFSU2 APU-priority experiment: only the owned
registered diagnostic APU thread changes relative priority0/2; normal process
class and PE/MAP/ownership/expected old priority verified. Same-process
normal_a (above), raised_a, normal_b, raised_b, normal_c;5 s settle then
20 s audio and FPS samples per phase. experiment_apu_priority151.py has
finally restoration to0 and bounded child readers, no guest-memory writes.
Runtime source scheduling policy unchanged; do not adopt priority based on
a single sample. Experiment is now live, transitions/results in private files.

Priority phases complete: raised_a8.799 FPS/47,967 processed/2 empty; normal_b
5.50/47,923/1; raised_b3.95/47,982/0; normal_c4.15/48,004/0. The initial
normal_a3.05/46,485/93 and wide scene variance cannot establish causality.
Key audit finds122 keyboard transitions in151 versus12 route edges, including
letters/arrows never sent by the helper. User confirms playing and now done.
Discard these as controlled priority/FPS comparisons, retain functional and
exact audio-counter evidence. Normal priority restored at15:07:19 (0-to0
verification after the final phase); no source scheduling policy adopted.

Inspect150 input provenance too:22 transitions, including two extra Enter
pairs before the expected route and missing Z key-up traces (focus reset
clears states without logging key-up). Its recorded two pitches/count2–0 and
pause/resume remain real, but do not label the entire run strictly unattended.
This corrects earlier fully-automated wording; screenshots and actual sequence
results are unchanged. Next normal repeat audits key-down sequence because
focus clearing legitimately suppresses some key-up traces.

STOP15115:09:55 after646.801 s/3,384 paths, zero misses/stubs/crashes/USB
cancellations/lost-syncs;244,989,269 words, software13,146 posted/completed.
Audio323 windows/120,392 attempts/zero rejects, active/gain1/peak0.123229980
but still session muted. Human changed game state (score1–0 visible), not a
controlled rules/sequence verification. No listening/full-exhibition claim.
User hands control back. Verify all151 source/EXE/MAP hashes match. Normal152
PID13452 starts15:10:14 on unchanged151 with all timings/history/frame polling/
NV2A tracing/GPU_YIELD off. Full normal two-pitch acceptance pending; no relink
while live. Priority is the default0; no preference changes persist.

Normal152 frontend through difficulty068 passes, selector124(error1.72)
confirmed. Movies003/021 and selector124 visually inspected. All APU151
fields/counters/histograms/thread identity and GPU timing counters/frequencies
remain zero with flags off. Before and after two20-second FPS samples, the
key-down trace exactly matches six completed route commands; no extra inputs.
Normal152 FPS4.549/4.65, accepted audio48,020.237/47,996.295 frames/s, zero
rejected submissions. Do not claim a causal FPS gain from different runs.
Readback bytes/game flip20.46/20.36 MB; earlier timing attributes synchronization
costs, not simply bulk-copy throughput. Keep current budgets/barriers.
After these measurements start test_pitch_loop152 on owned PID13452; external
redirect is pitchloop152_helper.txt, helper-owned log pitchloop152.txt.
Audit_input152.py derives expected downs from completed helper commands;
focus resets can legitimately omit key-up log edges. Full sequence pending.

### Continuation152 — controlled acceptance and user-requested pause (2026-10-19)

Unchanged Release151 completes normal152: first selector17015:16:10,
first count1–0 at17815:16:26, second selector30215:20:44, second count2–0
at31015:21:01, pause31315:21:07 and resume31715:21:16. Helper PASS; count,
selector, pause/resume captures visually inspected (acceptance152.png).
audit_input152.py finds exactly the ten expected key-downs:
0D 5A 5A 5A 0D 5A 5A 5A 0D 0D. No unexplained user input in this run.

At the user's repeated request to reach a pausing point, stop owned PID13452
after verifying its executable path. STOP15:22:07.918 after713.483 s;3,427
path attempts, zero dispatch misses/direct stubs/crashes/USB cancellations/
lost-syncs. Final pre-stop runtime snapshot355,170,383 command words;
software18,700 posted/completed.357 audio windows/133,363 block attempts,
zero rejects. CoreAudio active/gain1/peak0.106567383, session muted; preserve
user preference and make no listening-quality claim. No game/helper remains
live. This is the latest complete controlled acceptance on Release151.

Normal selector samples4.549/4.65 FPS and accepted48,020.237/47,996.295 audio
frames/s remain the baseline; all timing diagnostics off/zero. No causal FPS
gain inferred. Native151 exhaustive audio-pattern/ownership/pacing/DMA tests,
cycle/D3D audits and memcpy exclusive44302D evidence stand; no runtime edits
after the151 link. Resume with readback synchronization and remaining audio
stalls; retain current GPU budgets, texture limits and all publication/CPU
observer/alias/RAM/IRQ barriers. Full exhibition, saves, comfortable FPS,
audio quality and original-game comparison remain open. Goal paused, not done.

### Continuation153 — retain compatible depth allocations across HUD draws (2026-10-19)

User resumes in-game FPS/performance work. Unchanged151 diagnostic run153
PID26116 starts15:38:36, stops15:44:03 after327.401 s/3,414 paths, no dispatch
misses/direct stubs/crashes/USB cancellations. Both movies/full frontend and
selector130 pass; selector visually inspected. Six key-downs exactly match
route commands before/after measurements. Timing-on selector3.800/3.849 FPS,
Map6.419/6.611 s and surface handling2.987/2.757 s per20 s. Uploads4,584/4,125,
1.367/1.353 GB for76/77 flips. Native tests did not overlap these samples.

Inspection finds surfaces(width,height,0) destroys compatible color/depth
resources whenever a HUD disables depth, forcing allocation and full uploads
on both transitions. New depth_toggle_residency fails on original code:
three uploads instead of one, while independently checking pixels. Retain
existing surfaces when dimensions match and the new draw disables depth.
Compare pending draw's active depth state rather than retained allocation's
depth format, preserving grouped no-depth draws and actual transition barriers.
CPU depth revalidation remains forced after disabling; all existing RAM/
alias/fence/observer/software/IRQ barriers remain. This changes no guest state,
GPU budgets, texture limits or synchronization ownership.

Native regression covers Z16/Z24, scoped/unscoped transitions, exact readback
volume/grouping, CPU color edits and CPU depth edits changing later visibility.
AMD/WARP packed/legacy each pass2,570 draws; PB AMD packed/WARP legacy each
pass79 draws and the prior primitive/VP/blending/texture/depth/flip checks.
Separate640x480 depth/HUD benchmark checks drawn and edited outside pixels,
100 pairs/pass, three passes/phase, control→candidate→candidate→control with
no game live. Control7.853–10.773 ms/pair, candidate2.529–3.017; control300
uploads/368,640,000 bytes per100 pairs, candidate100–101/~122,880,000 bytes.
First candidate pass includes a four-byte CPU color edit. Candidate restored
in finally and rebuilt. Logs depth153_*.txt; this is not an in-game FPS claim.

Explicit configure/build Release153 links15:45:05,59,192,832 bytes, no warnings;
matching source/EXE/MAP hashes saved. Cycle/D3D audits pass33 seeds/37,762 lifts,
seven ranges/memcpy exclusive44302D and original mechanical D3D bodies/hooks.
Diagnostic154 PID17008 starts15:46:08, only GPU timing on, scopes/packed/cutoff
4096; all history/frame polling/APU timing/NV2A tracing/GPU_YIELD off. Route/
selector measurements and live acceptance pending; never relink while live.

154 both movies/full frontend/difficulty078 and selector139 pass; early
scoreboard capture138 is mid-draw, later161 visibly complete and correct.
GPU timing-on selector4.60/4.75 FPS versus153's3.80/3.849; Map6.439/6.344 s,
surface2.927/2.850 s per20 s. Uploads16.489/16.313 MB per flip versus
17.989/17.567. All six key-downs match route before/after measurements.
STOP15415:51:55 after347.234 s/3,425 paths; zero dispatch misses/direct stubs/
crashes/USB cancellations. This supports the allocation repair in the matched
selector scene; normal flags-off and full gameplay acceptance remain pending.

### Continuation155 — reuse allocation capacity across shorter clip heights

Surface height is derived from the current clip extent; a shorter HUD can
recreate the same resources even after153's disabled-depth repair. Reuse a
same-width, compatible-format allocation whose height is at least the current
height. Pending batches compare their logical draw height, preserving actual
height transition barriers without confusing it with allocation capacity.
All RAM comparisons/uploads remain bounded by current draw dimensions.

Extend the Z16/Z24 scoped/unscoped residency regression with shorter HUDs;
153 fails three uploads versus one. Capacity reuse alone initially fails the
new independent depth EQUAL test below a shortened draw: a full CPU edit of
the shorter extent is partial relative to the retained allocation, so a
whole-resource depth copy would erase prior GPU depth below it. Compare the
dirty rectangle against allocated gpu.height before skipping preservation.
This correction passes the EQUAL test and existing depth/stencil/CPU/alias
checks. No guest pacing, RAM leases, queue ownership, budgets or IRQ changes.

Private compare_capacity155.py compares153 (same-height reuse only) with155
in control/candidate/candidate/control order, no game live.640x480/64-height
HUD,100 pairs/pass, three passes/phase; drawn and outside-edited pixels checked.
Full logs capacity155_*.txt; candidate restored/rebuilt in finally. Build155
and the expanded renderer/PB suite now pending; normal156 follows with GPU
timing off to avoid treating diagnostic throughput as normal performance.

Clipped-height benchmark153 control6.524–7.135 ms/pair;155 candidate2.219–2.735,
300 uploads/262,144,000 bytes →100–101/~122,880,000 per100 pairs, across both
orders. Expanded AMD/WARP packed/legacy renderer suite each passes2,600 draws,
including the shortened-height EQUAL/outside-pixel test. PB AMD packed/WARP
legacy each pass79 draws. Explicit configure/build155 links15:53:20,
59,192,832 bytes, zero warnings; matching runtime/test source/EXE/MAP hashes.
Cycle/D3D155 pass33 seeds/37,762 lifts/seven ranges, memcpy44302D and original
mechanical D3D bodies/hooks. No runtime changes after link.

Normal156 PID13900 starts15:53:57, all diagnostics/history/frame polling/NV2A
tracing/GPU_YIELD off. Queued helper waits for visually checkable selector,
audits six route key-downs before/after two20-second FPS samples, verifies
timing counters off, then attempts two pitches/count2–0 and pause/resume.
No relink while live. No normal155 FPS or full gameplay claim yet.

Normal156 exposes a live regression:2.498/1.850 selector FPS with all timing
frequencies/counters zero; accepted audio47,944/47,828 frames/s, zero rejects.
Uploads16.254/16.286 MB per flip, readback20.385/20.735 MB. Six expected route
downs before/after FPS. Default selector template rejects legitimate100%-stamina
UI at8.72; visually inspect156/164 and independently compare with previously
verified unchanged151 selector153/130:2.10. Preserve initial gate log, stop only
read-only helper and rerun with that additional reference, same threshold8 and
two distinct captures. Game never restarted. Save verified current selector
as pitch156_selector_before.bmp for the pitch helper; no threshold loosening.

First selector20416:01:11, A16:01:13, count1–0 at22016:01:46. Input audit
has exactly seven expected downs. Reject height-capacity reuse despite native
speedups; stop helper deliberately while waiting for second selector, then
owned game PID13900 at16:02:39 after521.226 s/3,418 paths. Zero dispatch misses/
direct stubs/crashes/USB cancellations;179,489,986 words, software11,867 posted/
completed before stop. CoreAudio active/gain1/peak0.103424072, session muted.
Do not claim second-pitch/pause or audible quality acceptance. Host sample has
five of eight logical CPUs idle, game205% CPU; no obvious competing CPU load.

### Continuation157 — reject height-capacity reuse, isolate depth-toggle repair

Revert all155 capacity changes; runtime source is byte-for-byte153 candidate,
confirmed against saved renderer153_candidate.c. Keep shortened-height pixel
oracle and benchmark, but assert three uploads for actual height changes and
one for same-height depth toggles. Do not generalize native HUD savings to
in-game speed. Explicit build/native157 and normal158 will test the smaller
allocation-only change independently, retaining all current synchronization.

157 links16:03:08,59,192,832 bytes, no warnings; AMD/WARP packed/legacy each
pass2,600 renderer draws with actual-height changes recreated. New runtime
source/EXE/MAP/test hashes saved. Normal158 PID7364 starts16:03:50, all
diagnostics/history/frame polling/NV2A tracing/GPU_YIELD off. Selector gate
uses verified153/130 plus129 reference, unchanged threshold8/two captures;
copies its confirmed image for later pitch testing. Three20-second selector
samples queued before any pitch input; assess FPS before committing another
full pitch loop. Latest complete acceptance remains152 on151.

158 selector135 pass, three normal samples4.45/4.50/4.65 FPS. Six exact route
downs before/after, all timing diagnostics zero; no reliable normal FPS gain
from the smaller resource fix. Accepted audio47,970/48,035/47,996 frames/s,
zero rejects. Attempted profile failed before sampling because tools_sample.py
expects cwd=mvp06; preserve error log, no stack evidence claimed. STOP16:11:45
after474.511 s/3,421 paths, clean, no pitch inputs. Keep the smaller allocation
repair's native work reduction, but do not claim diagnostic FPS uplift proves
a normal-mode speedup. Height-capacity experiment remains rejected.

### Continuation159 — controlled synchronous fallback retry granularity

Map stalls and unscoped resolves remain. Test whether shorter synchronous
fallback slices find more opportunities for existing try-exclusive grouping.
Only the failed-exclusion fallback gets MAP-visible s_gpu_fallback_words,
default4096 and defensive clamp64..4096; exclusive slices remain4096/coarse2 ms,
software methods/GET publication and RAM/ISR/CPU gates unchanged. No waiting
for exclusion or skipped commands. Existing walker tests cover one-word partial
packets and continuous producers; publication fixture now adds a protected
256-word continuation, retaining unprotected4K/16K missed-patch controls.
Walker, publication GIL0/1 and memory/host-wait/raised-IRQL/DMA handoffs pass.
An initial memory-scope executable path was wrong (no test ran); corrected to
src/kernel/memory_scope_smoke and reran both GIL modes successfully.

Explicit159 build links16:12:30,59,192,832 bytes. Four memory-layout warnings
C4324/C4456(x2)/C4702 appear on recompile at unchanged code; check older logs.
Normal160 PID23988 starts16:13:23, all diagnostics/GPU_YIELD off. Private
setter validates process ownership, loaded PE/disk/MAP timestamp, ASLR, aligned
DWORD within module image and expected old budget; verifies write readback.
Selftest4096→4096 passes. No guest-memory edits, process/thread priority changes
or input. Planned same-process4096/256/4096/256 comparison,5 s settle+20 s each,
audit key-downs per phase and finally restore4096. No speedup claimed yet.

Confirm all four warning sites already in build107 (alignment, both old_protect
locals, unreachable block); only line numbers shift, no new warning sites.
Normal160 both movies/full frontend through difficulty069 pass. Live selector/
controlled comparison pending; no pitch inputs queued.

### Continuation160–161 — select512 fallback and pause after promotion

Completed the controlled experiment on159, all timing/history/frame polling/
NV2A trace/GPU_YIELD off. Private setter validates owned process, loaded PE/
MAP timestamp, ASLR, aligned module DWORD, expected value and write readback;
no guest RAM or priority edits. Each phase settles5 s, samples20 s, then audits
input; all12 phases retain exactly six route downs. Each experiment restores
4096 in finally. Original4096/256/4096/256 gives4.75/5.30/5.15/5.35 FPS;
neighbor256/512/256/512 gives5.299/5.35/5.35/5.35 FPS. Final warmed direct
4096/512/4096/512 gives5.149/5.350/5.150/5.400: mean5.1495→5.375 (+4.38%).
Evidence: fallback160_experiment.txt, fallback160_w512_*,
fallback160_final512_*, transitions and per-phase JSON/input audits in build/.

Select512 rather than256: indistinguishable nearby FPS and fewer budget retries.
Unscoped resolves fall about36→23.8–23.9 per flip, but TOTAL resolves do NOT
fall (about62–63→63–64). Scoped hardware draws rise about174→222–226 per flip;
the existing scoped path also accelerates tiny draws otherwise handled by CPU.
This is a plausible contributor, not proven attribution. Surface upload volume
16.24→16.06 MB/flip. Final reverse audio submissions47,932–48,021 frames/s,
zero rejects. Claim a modest selector improvement only; no broad game-speed
guarantee or readback-count reduction. No source yielding/priority change.

Explicitly hold512 afterward; fallback160_pitch_setting.json and final setter
readback record selection. Full160 acceptance: selector32616:24:40, A16:24:42,
count1–0 at33316:24:55; second selector44116:28:37, A16:28:40, count2–0 at448
16:28:52, pause45116:28:58/resume45516:29:06 PASS. Visually inspected collated
frame160/acceptance160.png. input160_closed.json matches all ten helper downs:
0D 5A 5A 5A 0D 5A 5A 5A 0D 0D. This supersedes152 as latest complete acceptance.

Owned160 PID23988 started2026-10-19 16:13:23.009968 and stopped21:00:42.160009,
17,239.150 s, before relink. summary160.json:3,763 path attempts, zero dispatch
misses/direct stubs/crashes/USB cancellations. evidence160_final.json confirms
8,353 captures spanning17,236.183 s, maxgap3.089 s;8,618 audio windows/3,231,807
attempts/zero rejects/lost-syncs. Software536,261 posted/completed. Final words
snapshot2,866,295,674 is uint32 and MAY HAVE WRAPPED: do not call it lifetime
total. Host active/gain1/peak0.251678467, SESSION MUTED; preserve preference,
no audible-quality/full-exhibition claim. Large later tool-response gaps do not
extend game runtime beyond the saved stop. Captures support continuous cadence
within this run; they do not establish complete-game correctness.

Release161 promotes only synchronous failed-exclusion fallback default4096→512;
clamp64..4096 remains, invalid values use the named512 default. Exclusive scopes
stay4096/coarse2 ms, no waiting for exclusion, and original software/GET/ISR/
publication/RAM contracts remain. Publication fixture covers guarded256 AND512
continuations, retains unguarded/guarded4K/16K reproductions. GIL0/1 pass161;
159 walker/continuous-producer/memory/host-wait/raised-IRQL/DMA tests and157
AMD/WARP packed/legacy2,600-draw renderer tests apply to unchanged code.
Explicit CMake configure/build161 succeeds07:59:42 on2026-10-20,59,192,832 bytes.
Four old memory-layout warnings C4324/C4456(x2)/C4702 match107 sites; no new
warning sites. build161_hashes.json records EXE/MAP/memory/renderer identity.
Renderer still exactly153 allocation-only candidate;155 height reuse rejected.
No disasm/lift changes;33 seeds/37,762 functions/seven patched ranges and
memcpy exclusive0x0044302D remain from prior audited cycle.

Normal162 on161 starts08:02:27 PID19708 with packed readback, scopes, cutoff4096
and all diagnostics/GPU_YIELD off. Intro capture01108:02:55 visibly passes.
Last stderr08:03:06 has591 paths/zero logged misses/stubs/crashes/cancels; route
only reaches "Waiting for title" and sends no input. At requested stopping
point, game and helper are already absent; exec session no longer exists and
exit status/reason was not retained. DO NOT label162 clean completed startup,
full161 acceptance or a diagnosed regression. No inference from absent crash
signature. Application events saved in events162_checkpoint.json refer to
another process and older watchdog reports; inspected OS boot time10/14 rules
out a contemporaneous reboot. No established cause. Save summary162.json,
input162_checkpoint.json and intro checkpoint.png. No later runtime edits.

Reference review, requested by user: inspect local Mercenaries D3D11 surface
serial/range-download contracts (nv2a_pgraph_d3d11.c around2637–2765); overlapping
color/zeta views require compatible interpretation, not just newest serial.
[Mercenaries architecture](https://github.com/KraftMacAndChee/Mercenaries-Recompiled/blob/master/ARCHITECTURE.md)
separates guest RAM, host surfaces and scanout. [xemu texture handling](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/vk/texture.c)
uses QEMU dirty-page tracking; direct adoption needs equivalent CPU observers.
[NFSU2](https://github.com/antoxa2584x/nfsu2-sw) local source is under
reference/nfsu2-sw/xboxrecomp; [Burnout3](https://github.com/sp00nznet/burnout3)
review and xemu surface review remain incomplete. No external code copied.
Larger potential win is GPU-resident targets with exact CPU/alias synchronization;
do not skip existing RAM commits or copy sampled-hash/frame-epoch shortcuts.
First attribute external flushes to clears/fences/software/flip before choosing
a change. A fully covered clear might eliminate a dead readback only if every
pending color/depth plane, mask, target, pitch and rectangle is covered; no such
optimization implemented or validated yet.

PAUSED2026-10-20 at user's request. No owned game or test helper running. Next:
fresh normal163 on161, retain process handle/exit status, finish frontend/selector/
two-pitch/pause acceptance, then profile external readbacks. Private
experiment_fallback159.py assumes/restores4096; adapt to initial/final512 before
reuse. Full exhibition/save-reload/comfortable FPS/accuracy/audio listening open.

## 2026-10-20 — GPU residency, latency fixes, texture filtering (runs 201–226)

Baseline201 (unchanged imported state, scopes + packed readback): 3.45/4.30 FPS.
Per flip ~69 flushes, 20.7 MB readback, 16 MB upload. Per-thread CPU showed
no thread saturated (guest 18%, command worker 56%, flag worker spinning
Sleep(0) at 96%): latency-bound ping-pong, not compute.

Residency (RECOMP_GPU_RESIDENT=1) replaces per-commit readback with page
protection (NOACCESS while GPU newer, READONLY while equal) and a renderer VEH
that materializes on fault. With scopes off, 7.25 FPS. Diagnostics then drove
each step: single bound surface forced readback on ~8 target switches/flip →
12-slot surface cache; 256-wide targets drawn at clip heights 256 and 512 →
height-agnostic keys; three back buffers share one depth → GPU depth transfer;
render-to-texture → direct SRV binding, bound-target snapshot, sub-rect views;
depth buffer sampled as LIN_A8R8G8B8 → compute pass reproducing (z24<<8)|stencil
bytes; presenter memcpy fault per flip → 2-deep staging ring.

Two latency defects outside the renderer dominated afterwards:
- NV2A software-method traps waited for the kernel timer thread, which slept a
  vblank/10 ms deadline at the default 15.6 ms timer resolution: ~4 traps/flip,
  20.6 ms/flip. Event wake on trap request and KeInsertQueueDpc: trap wait
  0.16 ms/flip; 12.4 → 16.95 FPS. vblank/timers stay deadline-checked.
- The dynamic vertex buffer was WRITE_DISCARDed whole on every draw (~460/flip):
  ~20% of worker samples in GPU-sync waits. 16 MB NO_OVERWRITE ring: renderer
  28.8 → 15.7 ms/flip; 18.45 → 23.5 FPS.

Accuracy: SET_TEXTURE_FILTER/BORDER_COLOR and MIPMAP_LEVELS now reach the
renderer. Per-flip draw log (RECOMP_DRAW_LOG_FLIP) shows the depth-of-field
chain sampling with filter 0x0206 (TENT_TENT_LOD/TENT) and half-texel tap
offsets, i.e. designed for bilinear. Scene textures use trilinear; full mip
chains are uploaded. Run226: 23.15 FPS, two pitches/count2–0/pause/resume PASS.
Native smoke tests (21) pass, the two env-gated ones with GIL 0/1.

## 2026-10-21 — Pacing, accuracy, block VP, between-pitch stall (runs 227–252)

Flip pacing on PGRAPH_INCREMENT: menus at exactly 60, gameplay up to 60 (30–37
measured). GPU clears for partial/depth-only targets, flag-worker backoff,
16-bit/swizzled render targets on the GPU (player shadows), NV2A fog, clear
channel mask. 20-minute soak (run247) stable at 26–36 FPS.

Pitch input: one long A only selects the pitch; three short presses ~0.75 s
apart select, start and stop the meter (meter_probe run249, soak.py
pitch_sequence). Runs 249/250 throw 5 pitches each, including a walk, a hit
and fielding. Every pitch is then followed by a long wait (~25–60 s, pitcher
wandering) before the selector returns; with a runner on first it can wait
indefinitely (run249, >10 min). No dispatch misses or unlifted stubs
(RECOMP_STUB_TRACE=1), no blocked guest threads, pad idle, clocks real-time.
Open; see HANDOFF for leads.

Block VP: nv2a_vp_run_block runs the decoded program over 8 lanes with
SoA registers, identical float order; pool chunks and small batches use it and
unread inputs are not fetched. 191 M lanes bit-identical (RECOMP_VP_BLOCK_CHECK).
Worker build time 10.8–12.2 → 9.4–10.1 ms/flip (run252 vs run251).

## 2026-10-23 — Documentation reorganized

AGENTS.md, HANDOFF.md, PLAYABILITY_PLAN.md and the root README each had grown a
run-by-run checkpoint wall (runs 52–162, the 2–5 FPS software/readback era) ahead
of their current content. Those sections moved verbatim to mvp06/docs/archive/.
HANDOFF now opens with one current-state table, prioritized open issues and the
measurement helpers. AGENTS.md keeps the rules, commands, debugging loop,
coherence model and traps. PLAYABILITY_PLAN gains a milestone status table and
the residency, pacing and reference-check decisions.
