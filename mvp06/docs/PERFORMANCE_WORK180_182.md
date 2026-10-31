# Concurrent heap repair and completed bridge RAM ownership

Final checkpoint185: all game/test processes are stopped for the requested
handoff. Build185 links but is unlaunched; see [HANDOFF.md](HANDOFF.md).
Run182 later completed one pitch/count1–0, observed pause and resumed published
output; the second pitch was not completed. Final3451 PATH/no logged crash,
deliberate exitFFFFFFFF. The whole-system184 audit found a separate contiguous
allocation/free lifetime defect and loading failure; no30FPS claim is made.
The chronological notes below describe state at their own measurement times.

2026-10-31. The goal remains steady 30 FPS in gameplay, followed by rendering
and accuracy work. The previous turn committed the performance checkpoint as
b4ecc1a; that preserved the evidence and the unresolved normal179 startup crash.

## Normal180 startup failure

Normal179 run180 exited with 0xC0000005 in sub_0031D5BF's guest TLS lookup.
Thread startup allocates a TIB outside the guest CPU lock. The RAM gate allows
concurrent readers, so neither gate serializes the heap's bump pointer or free
table. The new Windows native heap fixture uses the actual allocator and TIB
constructor without loading game data. Before the fix it finds overlapping
live allocations in round0: a 320-byte block and a 276-byte TIB both start at
0x00FA65C0. This establishes an allocator race capable of destroying TLS data;
the failed game's exact interleaving was not captured.

An allocator SRW lock now covers selection, zero-fill, table publication,
free/coalescing and size queries. A separate stack lock protects the capacity
counter through allocation and reclamation. Lock order is stack then heap;
neither allocator lock acquires the guest CPU or RAM gate.

After repair, RAM scopes on and off each pass 102,400 disjoint allocations,
51,200 intact TLS blocks and 32 concurrent stack-capacity/reclamation cycles.
Private logs: heap180_before.txt and heap181_stacks_gateon/gateoff.txt.
Normal181 builds and runs through movies, frontend, stadium and the interactive
0-0 pitching selector (PID29412). It runs without the old diagnostic traces;
optional owner records were enabled temporarily. Full two-pitch acceptance
of this build remains pending. Normal181 EXE/MAP and separate live178 must
not be relinked while live.

## Rejected completed-bridge experiment

Owner sampling found long-lived readers acquired by host-wait return paths.
Source inspection shows the passive bridge resumes the guest CPU before
dropping its extra RAM reference. Once the native bridge has finished all
output writes, its CPU-ticket wait can therefore continue excluding the GPU.

The experimental passive bridge uses xbox_GuestBridgeCall. Its candidate path drops
only the completed bridge reference before rejoining the CPU. Nested caller
publication references remain intact; raised-IRQL dispatch keeps its existing
direct path. Host-wait argument copying, RAM restoration and output accesses
retain their protection. A MAP-visible comparison switch retains the old
return order for matched runtime measurements.

A native regression holds the CPU on a separate thread with no active RAM
operation, finishes a bridge's outputs, then tries GPU exclusion while that
bridge is queued for the CPU. The old return order fails; the candidate
passes. The same fixture verifies unfinished outputs remain protected and
the bridge cannot resume guest execution before the CPU owner releases it.
Existing publication, IRQL, host waits, 32 handoffs and 2,000 coherent DMA
commits pass with GIL1/owner tracing1 and GIL0. The completed bridge also
preserves a separate outer publication reference in a direct native check.
Scheduler exclusion, host waits, ISR lock order and raised-IRQL tests pass.
GPU budgets, GET publication,
readback completion, alias visibility and interrupt contracts are unchanged.

Separate candidate182 EXE/MAP builds using BuildProjectReferences=false and
TargetName=mvp06_recomp182 after rebuilding the kernel library, preserving
the live normal181 files. Runtime comparison and candidate gameplay
acceptance initially remained pending; no FPS gain was claimed from native tests.

Normal181's two isolated 20-second selector samples, after disabling owner
tracing and test key hold, are 1.25 and 2.90 FPS. Both samples pause/resume
the verified178 process through a finally guard. Earlier samples (2.25/1.499)
overlap build/native work and are unsuitable as a baseline. This variability
precludes a between-process speedup claim. Candidate182 must use a matched
old/new return-order comparison in the same stationary selector; other test
instances must be suspended only during the bounded sample and always resumed.

Candidate182 reaches the same Florida-Texas, Varsity, 0-0 pitching selector.
With timing/owner tracing and test hold off, matched 20-second samples are:

| Return order sequence | FPS in order |
| --- | --- |
| old/new/new/old | 3.850, 3.549, 3.249, 1.199 |
| new/old/old/new | 1.100, 1.192, 2.000, 1.049 |

The result is unstable and does not establish a gain. No automated game input
was sent during the phases, but there is no trace-based external-input audit
for this non-diagnostic run. Scene verification is visual, not a saved-frame
two-capture gate. Private bridge182_abba/baab files contain the samples; each
sample validates matching PE/MAP and suspends both known test instances through
a finally guard that records their resumption. Do not compare these noisy
samples directly against earlier stable178 results as a regression estimate.

Reject the runtime experiment: its guest scheduler/bridge/header/test changes
are removed from source. The private build/experiment182 snapshot preserves
the mechanism and its native before/after tests. Live182 retains that binary
but its comparison switch is restored to old order0; owner tracing remains0.
The heap/TIB repair and its native fixture are retained. Final acceptance and
the new resource-lifetime blocker are recorded in the handoff; steady30FPS
is not achieved.
