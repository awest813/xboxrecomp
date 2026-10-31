# Playability acceptance gates

Checkpoint 186, 2026-10-31. Operational handoff: [HANDOFF.md](HANDOFF.md). Earlier
milestone plans are archived: [resident-renderer lineage](archive/PLAYABILITY_PLAN_FAST254_2026-10-23.md),
[checkpoint-185 lineage](archive/PLAYABILITY_PLAN_CHECKPOINT185.md).

| Requirement | Evidence at checkpoint 186 | Still required |
| --- | --- | --- |
| Build and boot | Merged build boots through both intro movies and the Play Now frontend (runs 256–258) | — |
| Stable resource lifetime | Contiguous frees reuse memory; 20×32 MB native cycle passes | Repeated idle-demo → exhibition cycles on the merged build |
| Interactive gameplay | Scripted three-tap pitching; balls, strikes, walks, hits, fielding (runs 249/250, 256, 258) | Batting, full innings |
| Gameplay FPS ≥ 30 | Selector 38–40 FPS at 54% host load (run 258); gameplay soak samples in HANDOFF | Sustained frame pacing under ordinary load; batting/fielding scenes |
| Between-pitch flow | Selector returns after 25–70 s; never with a runner on first (run 249) | Stall fixed (HANDOFF open issue 1) |
| No crashes | Run 256 crashed after pitch 6 (failed animation lookup) | Root cause fixed, long soak clean |
| Complete exhibition | Not verified | Pitching, batting, fielding, inning/game transitions |
| Rendering | Resident D3D11, DOF composite correct, shadows, fog, filtering | Pale-blue cut flash; original-console comparisons |
| Audio | 48 kHz output, software-mixed ring | Audible continuity, timing, mixing over long sessions |
| Saves | Paths exist; persistence unverified | Save, exit, restart, reload and state comparison |

Do not use title/demo FPS as gameplay evidence, a native speedup as whole-game
evidence, or accepted audio frames as listening evidence. Declare a gate only from
current artifacts with enough scope to prove it.
