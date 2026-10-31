# Readback pipeline experiments172–173 (2026-10-30)

Goal remains steady30FPS in gameplay, then rendering/accuracy. Previous turn
made progress (exact surface checks and verified3.90/3.95FPS baseline), not
completion. At this turn's start171 PID17020 was confirmed live. It remains
live with normal input, timing off, poll100; do not relink its executable/MAP.

## Thread profile

Private build/profile171_threads.txt uses171's matching MAP,160 rounds. Main
thread145/160 RIP samples are NtWaitForAlertByThreadId. Renderer has43 samples
in GPU synchronization waits,33 in NtWaitForSingleObject,22 in VCRUNTIME,
8 in sample_texture_for, plus driver/residency work. Sampled CPU time is
1,125ms main and4,578ms renderer. These are observations, not exact stall
percentages. Backend/ownership code still commits before releasing exclusion
and before pattern/semaphore/software-method publication. No barrier relaxed.

## Rejected driver-thread option

Native opt-in D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS
worked (flag8) but depth-toggle ABBA timings overlap substantially: control
roughly3.9–4.6ms/pair and candidate3.4–5.4. Removed the runtime option; do not
reintroduce without stronger evidence. Private driver172_*.txt retain results.
171 was suspended only during each isolated native experiment and resumed
in the helper's finally block; logs confirm resumed=True each time.

## Default texture mapping probe

New native `nv2a_uma_probe` queries Device3/Options2 and validates all640x480
red pixels through ReadFromSubresource, without a staging texture. Host says
UMA1/MapOnDefaultTextures1/StandardSwizzle0. Capability does not imply speed.
Warm ABBA clear/read tests32²/64²/128²/full640x480 show no consistent small
read advantage and substantially slower full reads (direct2.46–2.88ms versus
staging0.96–1.84ms in the repeated run). Do not adopt mapped color textures.
Private uma172_compare.txt/runner.txt retain output. The native probe is a
capability/performance investigation, not an in-game acceptance test.

## Candidate173: directly map packed UAV results

This separate path uses MapOnDefaultBuffers, not mapped color/depth textures.
`RECOMP_PACKED_DIRECT_READBACK=1` opts into a CPU-readable DEFAULT raw UAV result
buffer, eliminating its intermediate CopySubresourceRegion to staging. The
same packing dispatch, blocking map, exact color/depth/stencil conversion,
RAM shadows and observer ordering remain. Unsupported capability or buffer
creation falls back to staging; map failure retains the existing ordinary
color/depth fallback. Default OFF. No extra clock polling, fences skipped or
new outstanding guest memory are introduced. A counter proves actual use.

Hardware and WARP each pass2,605 draws, including512 exact ARGB/Z16/Z24 cases
that each report direct=512. Both legacy packed-off modes pass2,605 draws as
well. Tests include target/palette/source aliases, scoped draws, CPU edits,
partial depth preservation, stencil/guard values and fallback barriers.
Evidence: direct173_hw/warp.txt, direct173_legacy_hw/warp.txt.

Separate173 executable/MAP builds successfully; not launched or promoted.
Native depth-toggle ABBA timings overlap (control5.16–6.13ms/pair, direct
4.73–6.20). New --benchmark-readback isolates256² draw/readback without CPU
surface uploads. ABBA control2.03–3.08ms/draw versus direct2.22–2.80 also
overlaps, with a warming trend. No reliable performance improvement established.
Private direct173_abba.txt and direct173_readback_abba.txt retain every phase.
No173 gameplay FPS claim. Keep this as an opt-in prototype pending a matched
gameplay comparison; steady30FPS remains unachieved. Normal169 and live171
executables are unchanged. All new work remains uncommitted.

API sources: [D3D11 creation flags](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_create_device_flag),
[default texture mapping](https://learn.microsoft.com/en-us/windows/win32/direct3d11/default-texture-mapping),
[DEFAULT SRV/UAV buffer mapping, section5.6.1.3](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm).
