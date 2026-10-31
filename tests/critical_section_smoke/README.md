# Critical-section scheduling regression

Uses the real host critical-section implementation and kernel CPU policy,
with synthetic guest objects and no game data. Build target:
`critical_section_smoke`.

Set `RECOMP_GPU_SCOPES=1`. Run all four combinations of `RECOMP_GIL=0/1`
and `RECOMP_CS_GUEST_FASTPATH=0/1`. The fixture checks FIFO handoff in the
old policy, CPU retention for the optional uncontended policy, real lock
contention, recursive Enter/Leave and Region variants, coherent GPU writes
while waiters are parked, and raised-IRQL ownership.

Before the passive-wait repair, a blocked critical section retained RAM and
prevented GPU exclusion. The repaired wait releases CPU/RAM while blocked
and restores them before guest access. The optional CPU policy remains off
by default; passing this fixture does not establish gameplay FPS or complete
runtime acceptance.
