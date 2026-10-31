# Heap and TIB concurrency regression

Windows native fixture using the actual allocator and TIB constructor from
`xbox_memory_layout.c`. It allocates synthetic RAM and starts no loader,
device workers or game code. Unused loader/device entry points abort if called.

Build target: `heap_concurrency_smoke`. Run its Release executable with
`RECOMP_GPU_SCOPES=1`, then `0`. Each case checks 102,400 disjoint allocations,
51,200 intact per-thread TLS blocks, freed-block reuse, size queries, and 32
concurrent worker-stack capacity/reclamation cycles.

Before allocator serialization, the fixture found overlapping live allocations
in its first round. The shared RAM gate and guest CPU gate cannot serialize
host thread startup allocations; the allocator needs its own metadata lock.
This is a correctness test, not a gameplay benchmark. Contiguous graphics
allocation/free lifetime is a separate unresolved issue.
