> Historical checkpoint report. Its process/default/next-action statements apply to that run. Current state: [HANDOFF.md](HANDOFF.md); report index: [README.md](README.md).

# Toward steady 30 FPS: runs170–171 (2026-10-30)

Goal remains steady30 FPS in gameplay, followed by rendering and accuracy work.
Neither intro/menu speed nor a native microbenchmark proves this goal complete.
Previous goal work made progress: committed runtime fixes and validated168's
two-pitch/pause route. At this continuation's start, git was clean at61a93f2
and no game process was live; historical PID10620 was no longer present.

## Current measurements

Fresh170 uses unchanged169 normal executable, packed readback/scopes/4096,
adaptive flag poll100, GPU timing on. First20-second sample25.9 FPS is the
video intro, NOT gameplay. Second sample48.9 FPS is title/frontend and overlaps
native renderer testing, so is NOT a matched performance benchmark. Its file
is misleadingly named `build/profile170_game.json`; do not interpret that
filename as scene evidence. Captured frame070 visibly shows the title screen.
Diagnostic files remain private under build/.170 deliberately stopped with
exitFFFFFFFF before launching171; exit170.json records the retained handle.

## Exact surface comparison candidate

`changed_bounds` previously performed two variable-size memcmp calls per equal
pixel while finding dirty edges in a changed row. The new internal helper
skips64-byte equal blocks, then resolves exact pixel edges. Every relevant RAM
byte is still checked; no hashes, sampling, dirty-page assumptions, GPU/RAM
observer changes, fence bypass or relaxed IRQ publication are introduced.
This targets CPU comparison work seen in prior gameplay profiles, not just
Map waits. No in-game FPS improvement is claimed yet.

The independent byte oracle passes614,784 cases covering16/32-bit pixels,
four pointer alignments, zero/short/long widths, block boundaries, every byte
within a pixel and two separate edits. The sparse640-pixel row microbenchmark
on this host: old895ms, block126ms for500,000 iterations, checksum641000000.
This is approximately7x faster FOR THAT OPERATION, not whole-game speedup.
All four renderer modes (hardware/WARP times packed/legacy) pass2,605 accepted
draws with zero failures, including partial CPU edits and depth preservation.
Evidence: build/surface_diff171.txt, render171_hw0/1.txt, render171_warp0/1.txt.

Separate Release/mvp06_recomp171.exe/MAP links successfully; normal169 binary
is unchanged.171 PID17020 launched with timing off, hold2500, poll100 and the
usual packed/scopes/4096 policy. Frontend route selected default Florida–Texas,
home jersey and Varsity. Selector gate and gameplay measurements are pending.
Selector gate passed frame097, error1.72. Two timing-off20-second selector
samples measured3.900 and3.950 FPS, about18.89/18.86MB readback per flip.
Both audio windows report zero rejects; seven exact frontend downs unchanged.
At audit,171 has3,416 PATH entries and zero logged crash/direct stub/dispatch
miss/lost sync/USB cancellation. Prior168 reached the same gameplay route;
170 ended at the title (not comparable PATH coverage). This verifies startup
and selector only, not two pitches or full exhibition. There is no proven
whole-game speedup; steady30FPS remains far away. Native comparison speed
alone does not address the dominant repeated readback/handoff cost.

Private evidence: profile171_selector_a/b.json. Test hold was set2500→0 with
matching executable/MAP/PE validation, so ordinary keyboard behavior is restored.
171 remains live at poll100/hold0; normal169 EXE remains unchanged. Next work
must investigate batching/coherence architecture for substantially fewer GPU/
CPU round trips, with actual CPU/alias/fence visibility preserved; do not merely
remove progress-marker observers. Before any speedup claim, use same-process
control or a matching scene/build comparison. Never relink live171 EXE/MAP.
