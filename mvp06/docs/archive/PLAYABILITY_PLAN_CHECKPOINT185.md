# Playability acceptance gates

Current checkpoint185,2026-10-31. Operational handoff: [HANDOFF.md](HANDOFF.md). Original milestones and old checkpoint notes are preserved in [the archive](archive/PLAYABILITY_PLAN_PRE185.md).

| Requirement | Evidence at checkpoint | Still required |
| --- | --- | --- |
| Build and boot |185 links; earlier builds boot and render movies/frontend |Fresh185 runtime acceptance |
| Stable loading/resource lifetime |184 exposes contiguous-arena exhaustion |Native allocation/free regression and repeated demo→exhibition pass |
| Interactive gameplay |178 two pitches/count2–0/pause/resume;182 one pitch/count1–0 and pause/resume |Acceptance on final lifetime-fixed source |
| Steady30 FPS |Not achieved; historical controlled selector about4 FPS |Sustained measured gameplay, frame pacing and original simulation rate |
| Complete exhibition |Not verified |Pitching, batting, fielding, inning/game transitions |
| Audio |182 diagnostic accepts about99.4% nominal frames |Audible continuity, timing, mixing and long-session stability |
| Saves |Paths exist; full persistence unverified |Save, exit, restart, reload and state comparison |
| Original rendering/gameplay accuracy |Local pixel/lift fixtures pass; visual defects remain |Original-console/scenario comparisons |

Do not use title/demo FPS as gameplay evidence, a native speedup as whole-game evidence, or accepted audio frames as listening evidence. Normal185 has no separate gameplay pass. The optional CS fast path is off by default and has no accepted FPS comparison. Current resource-lifetime failure precedes further promotion.

After completing the ordered plan in [../PLAN.md](../PLAN.md), declare each gate only from current artifacts with enough scope to prove it. Full objective completion requires every requested behavior, not just absence of a logged crash.
