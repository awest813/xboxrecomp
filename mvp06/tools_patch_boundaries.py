"""Reapply MVP 06 function ranges after tools.disasm regenerates its database."""

import json
from pathlib import Path


PROJECT = Path(__file__).resolve().parent
FUNCTIONS = PROJECT.parent / "tools/disasm/output/functions.json"
MEMCPY_START, MEMCPY_END = 0x00442CF0, 0x0044302D
SEH_START, SEH_END = 0x00094B90, 0x00094C80
MENU_SWITCH_START, MENU_SWITCH_END = 0x0007F040, 0x0007F077
PLAYER_SWITCH_START, PLAYER_SWITCH_END = 0x0007AE80, 0x0007AECB
PITCH_CALLBACK_START, PITCH_CALLBACK_END = 0x00256590, 0x00256F4E
POSTPITCH_EVENT_START, POSTPITCH_EVENT_END = 0x00032710, 0x00032885
POSTPITCH_CALLBACK_START, POSTPITCH_CALLBACK_END = 0x001D8240, 0x001D8500


def main():
    seeds = json.loads((PROJECT / "config/seed_functions.json").read_text())
    protected = ((MEMCPY_START, MEMCPY_END),
                 (MENU_SWITCH_START, MENU_SWITCH_END),
                 (PLAYER_SWITCH_START, PLAYER_SWITCH_END),
                 (PITCH_CALLBACK_START, PITCH_CALLBACK_END),
                 (POSTPITCH_EVENT_START, POSTPITCH_EVENT_END),
                 (POSTPITCH_CALLBACK_START, POSTPITCH_CALLBACK_END))
    internal = [s["start"] for s in seeds
                if any(a < int(s["start"], 16) < b for a, b in protected)]
    if internal:
        raise ValueError(f"Remove internal switch arms from the seed file: {internal}")

    original = FUNCTIONS.read_text()
    functions = json.loads(original)
    memcpy = next(f for f in functions if int(f["start"], 16) == MEMCPY_START)
    memcpy.update(end=f"0x{MEMCPY_END:08X}", size=MEMCPY_END - MEMCPY_START)
    kept = [f for f in functions
            if not MEMCPY_START < int(f["start"], 16) < MEMCPY_END]
    removed = len(functions) - len(kept)

    seh = next((f for f in kept if int(f["start"], 16) == SEH_START), None)
    if seh is None:
        seh = dict(start=f"0x{SEH_START:08X}", name=f"sub_{SEH_START:08X}",
                   section=".text", confidence=0.9, detection_method="manual_seed",
                   num_instructions=0, has_prologue=True, calls_to=[], called_by=[])
        kept.append(seh)
    seh.update(end=f"0x{SEH_END:08X}", size=SEH_END - SEH_START)
    # Run62 reached this 28-entry switch after the save check. Its first RET
    # split off the other constant-return arms and prevented switch lifting.
    menu_switch = next(f for f in kept if int(f["start"], 16) == MENU_SWITCH_START)
    menu_switch.update(end=f"0x{MENU_SWITCH_END:08X}",
                       size=MENU_SWITCH_END - MENU_SWITCH_START)
    # Run84's exhibition load reached 0x7AE92: the first of four packed-player
    # bitfield switch arms, not a callable entry. Keep every arm and the shared
    # ret 4 at 0x7AEC8 in the containing function; table data begins at 0x7AECC.
    player_switch = next(f for f in kept if int(f["start"], 16) == PLAYER_SWITCH_START)
    player_switch.update(end=f"0x{PLAYER_SWITCH_END:08X}",
                         size=PLAYER_SWITCH_END - PLAYER_SWITCH_START)
    # Run90 reached this independent pitching callback. Its five returns
    # share one frame; the existing 0x256C6F conditional alias is internal.
    pitch_callback = next(f for f in kept if int(f["start"], 16) == PITCH_CALLBACK_START)
    pitch_callback.update(end=f"0x{PITCH_CALLBACK_END:08X}",
                          size=PITCH_CALLBACK_END - PITCH_CALLBACK_START)
    # Run91 reached these standalone callbacks after its first fastball. The
    # first body has two ret 4 paths; the second ends immediately before the
    # next function. Original-byte audits cover every instruction and branch.
    for start, end in ((POSTPITCH_EVENT_START, POSTPITCH_EVENT_END),
                       (POSTPITCH_CALLBACK_START, POSTPITCH_CALLBACK_END)):
        callback = next(f for f in kept if int(f["start"], 16) == start)
        callback.update(end=f"0x{end:08X}", size=end - start)
    kept.sort(key=lambda f: int(f["start"], 16))
    updated = json.dumps(kept)
    if updated != original:
        FUNCTIONS.write_text(updated)
    print(f"memcpy: 0x{MEMCPY_START:08X}..0x{MEMCPY_END:08X} "
          f"(exclusive end, {MEMCPY_END - MEMCPY_START} bytes); "
          f"removed {removed} contained entries")
    print(f"SEH: 0x{SEH_START:08X}..0x{SEH_END:08X}; total {len(kept)} functions")
    print(f"menu switch: 0x{MENU_SWITCH_START:08X}..0x{MENU_SWITCH_END:08X}")
    print(f"player switch: 0x{PLAYER_SWITCH_START:08X}..0x{PLAYER_SWITCH_END:08X}")
    print(f"pitch callback: 0x{PITCH_CALLBACK_START:08X}..0x{PITCH_CALLBACK_END:08X}")
    print(f"post-pitch event: 0x{POSTPITCH_EVENT_START:08X}..0x{POSTPITCH_EVENT_END:08X}")
    print(f"post-pitch callback: 0x{POSTPITCH_CALLBACK_START:08X}..0x{POSTPITCH_CALLBACK_END:08X}")


if __name__ == "__main__":
    main()
