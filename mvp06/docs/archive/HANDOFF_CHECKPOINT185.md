# MVP 06 handoff — checkpoint185

2026-10-31. Stopped at the user's request for documentation, commit and push. The full objective remains **steady 30 FPS in gameplay, then rendering and accuracy**. It is not achieved. Resume deliberately; do not treat this checkpoint as a completed port.

## State to resume from

- No game or test helper from this session remains running. Runs178,181,182 were deliberately retired;184 crashed and its exit was observed. Recheck process state before any build or launch.
- Normal `build/Release/mvp06_recomp.exe` and its matching MAP are build185,59,198,464 bytes, PE timestamp1793460453. EXE SHA256: `3bd995fddaebff8ebded5713c97cbf9488d7c72b59f9387c877ce6c1a3a31735`. Private `build/checkpoint185_hashes.json` records source and binary hashes.
- Build185 links successfully and includes the retained heap/stack synchronization, centralized kernel CPU policy, and passive CS-wait repair. It has not been launched. CS fast path defaults off; do not label185 runtime-accepted.
- Retail files, generated game C, executables, MAPs, logs and captures stay private. The current branch is `main`, tracking `mvp/main` at the dedicated MVP repository. Do not push this game checkpoint to the unrelated `origin` toolkit fork or `upstream`.

## What changed and what passed

1. Normal179/run180 crashed in a TLS lookup. A native fixture using the actual allocator reproduced overlapping live allocations in round0. Heap selection, zero-fill, metadata, free and size queries are now serialized; stack capacity/reclamation is separately locked. RAM gate on/off each pass102,400 disjoint allocations,51,200 intact TLS blocks and32 concurrent stack cycles.
2. A completed-bridge lease-release experiment passed a narrow native check but failed to establish a gameplay benefit in both measurement orders. It was removed; private `build/experiment182/` preserves the experiment. Keep the old return order.
3. The whole-system audit found repeated CPU ticket handoffs around uncontended CS calls and RAM retained during real CS waits. `xbox_GuestKernelCall` centralizes the prior dispatch policy; optional `RECOMP_CS_GUEST_FASTPATH=1` retains CPU for277/278/294/295. Actual passive contention now releases CPU/RAM while waiting. Native fastpath0/1 × GIL0/1 all pass FIFO, real contention, recursion, coherent GPU commits and raised-IRQL checks. Existing memory-scope and scheduler tests pass. The fast path remains unpromoted.

## Latest runtime evidence and failure

Run182 (heap repair, rejected bridge binary switched back to old order0) reaches exhibition, completes one pitch/count1–0, opens pause, and resumes; the resumed published framebuffer was saved. Second pitch and complete exhibition remain unverified. Final audit:3451 PATH, zero logged crashes; deliberately stopped for this handoff. GPU/memory timing was enabled one way for the later diagnostic profile, so those rates are not a normal baseline.

Run184 explicitly enables the CS fast path. It passes startup and several idle-demo/frontend transitions, then **crashes during exhibition loading after contiguous-arena exhaustion**. Observed exit3221225477 (0xC0000005), fault RVA0x308EB63=`sub_005E99A1+0x103`, guest EAX/EBX=`0xBB55BB55`, fault VA0xBB55BBD5. Before the fault, the64 MB contiguous arena reports66,960,000 bytes consumed and cannot allocate1,228,800 /131,072-byte buffers; later tiny allocations also fail. No FPS comparison was completed on184. The selector gate timing out was an observation failure; the exit record and crash log prove terminal failure.

The allocator is still bump-only. `bridge_MmFreeContiguousMemory` routes graphics addresses to `xbox_HeapFree`, whose table contains ordinary-heap allocations; contiguous allocations have no reclamation metadata. `MmQueryAllocationSize` also queries only that table. This is a demonstrated lifetime defect and a strong explanation for exhaustion after repeated demos/loading. The exact poison-pointer path and whether the CS policy contributes remain to be isolated; do not claim a controlled attribution.

**Next action:** repair and test contiguous allocation/free/query lifetime, preserving alignment, pinned physical ranges, low64 KB and GPU-instance reservations, true contiguous/tiled aliases, and distinction from low RAM. Do not grow the arena or strip address bits to hide the leak. First reproduce repeated allocation/free exhaustion in a native fixture, then compare fresh normal runs and demo→exhibition cycles. Keep the CS fast path off until the failure is isolated and matched comparisons plus gameplay acceptance pass.

## Whole-system performance findings

[SYSTEM_PERFORMANCE_AUDIT184.md](SYSTEM_PERFORMANCE_AUDIT184.md) contains the pipeline, host constraints and measured costs. Single-instance diagnostic182:20.002s/35 flips=1.750 FPS;2068 resolves (59.09/flip),16.75 MB readback and16.33 MB surface upload per flip;8.209s in readback Map wall time. Main/renderer threads mostly wait; frame presentation sleeps most samples. Timers overlap and include host descheduling. CPU load repeatedly reached99–100% with unrelated work; the requested ten-minute wait did not remove contention.

Historical controlled178 selector3.77→4.05 FPS and its two-pitch/pause pass remain valid scene-specific evidence. Recent noisy1–4 FPS results establish no further speedup. Reaching30 FPS needs fewer GPU/RAM round trips and scheduling waits, not just copy-byte reductions. Preserve all completion/alias/IRQ contracts.

## Build, run and acceptance

From the root, after confirming no live normal executable:

```powershell
cmake -S mvp06 -B mvp06/build
cmake --build mvp06/build --config Release --target mvp06_recomp
$env:RECOMP_CS_GUEST_FASTPATH = '0'
Push-Location mvp06
.\build\Release\mvp06_recomp.exe
Pop-Location
```

Run with a retained process handle and exit record. Main defaults select D3D11/packed/scopes/cutoff4096/yield1/handoff2000/vblank; explicit overrides win. Toolkit handoff defaults off. Exclusive slices4096/coarse2ms and synchronous fallback512 remain mandatory. Normal test-key hold0, tracing/timing off for FPS comparisons, user mute preference preserved.

Use the computer-use skill for inputs. Read-only private `capture_present182.py` / `capture_present184.py` validate loaded PE/MAP and two identical published pixel snapshots; they avoid unreliable desktop capture. Update strict executable/MAP guards for any new candidate. Private `gate_present184.py` compares two captured selector signatures; it sent no input. The184 gate failed before the crash; do not cite it as selector acceptance.

After the lifetime fix: verify startup, idle-demo return, fresh Play Now selector, two pitches/count2–0, pause/resume, full exhibition and save/reload. Compare PATH/fault signatures and saved images, then run controlled FPS samples in both orders with input and host-load provenance. Steady30 FPS requires sustained gameplay and frame pacing, not menu/demo FPS.

## Native checks

```powershell
cmake --build mvp06/build --config Release --target heap_concurrency_smoke critical_section_smoke memory_scope_smoke guest_scheduler_smoke
$env:RECOMP_GPU_SCOPES = '1'
foreach ($gil in @('0','1')) {
  foreach ($fast in @('0','1')) {
    $env:RECOMP_GIL = $gil
    $env:RECOMP_CS_GUEST_FASTPATH = $fast
    & .\mvp06\build\xboxrecomp\src\kernel\critical_section_smoke\Release\critical_section_smoke.exe
    if ($LASTEXITCODE) { throw 'critical-section regression' }
  }
}
```

Heap fixture also passes scopes0/1. Memory-scope tests use scopes1 and cover yield/handoff, publication, host waits, IRQL and2,000 coherent DMA commits. Latest logs: `build/cs184_gil*_fast*.txt`, `memory184.txt`, `heap181_stacks_gateon.txt`, `heap181_stacks_gateoff.txt`, `build185_log.txt`. Existing walker and614,784-case exact surface-bound checks passed earlier this session. No new lifter changes were made; earlier lifter/render suites are historical, not rerun185.

Full regeneration and protected-boundary instructions are in [../AGENTS.md](../AGENTS.md). [README.md](README.md) indexes all evidence and archived checkpoints. Do not repeat a long sequence of stale checkpoint instructions before reading this current state.
